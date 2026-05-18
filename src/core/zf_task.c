/**
  *****************************************************************************
  * @file    zf_task.c
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
 * @brief   任务管理实现
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
    *    Modification:建立文件
  *
  *****************************************************************************
  */

#include "zf_task.h"
#include "zf_task_schedule.h"
#include "zf_critical.h"
#include "zf_malloc.h"
#include "zf_debug.h"
#include "zf_assert.h"
#include "zf_list.h"

/* 空闲任务栈大小。空闲任务主要负责处理异步销毁等后台动作。 */
#define IDLE_TASK_STK_SIZE 512  /* 4 字节对齐 */

List *pmTaskList = NULL;        /* 系统内全部任务。 */
Task *pTopPriorityTask = NULL;  /* 本轮调度选择出的最高优先级任务。 */
Task *pCurrentTask = NULL;      /* 当前正在运行的任务。 */
Task *pIdleTask = NULL;         /* 空闲任务，没有其他任务可运行时执行。 */
/* 空闲任务内置的事件处理器，专门处理异步销毁等后台事件。 */
EventHandler *pIdleTaskEventHandler;

/* 调度器总开关。 */
bool mIsScheduleOn = true;

/* 任务系统是否已经正式启动。 */
bool mIsTaskSystemRun = false;

/* 空闲任务入口函数声明。 */
static void Task_idleTaskProcess(void *pArg);

/******************************************************************************
 * 功能  获取任务链表
 * 说明  主要用于调试、监控或遍历现有任务。
 * 返回  任务链表指针
******************************************************************************/
List *Task_getTaskList(void)
{
    return pmTaskList;
}

/******************************************************************************
 * 功能  创建任务
 * 说明  完成三件事：
 *       1. 分配 Task 对象和任务栈。
 *       2. 初始化任务初始上下文，使其第一次被调度时能进入 taskProcess。
 *       3. 把任务挂入全局任务链表，参与后续调度。
 * 参数  (out)-ppTask        输出任务指针
 *       (in)-taskProcess    任务入口函数
 *       (in)-pArg           传给任务入口函数的参数
 *       (in)-priority       任务优先级
 *       (in)-stkSize        任务栈大小
 * 返回  true 成功，false 失败
******************************************************************************/
bool Task_create(Task **ppTask, ITaskProcess taskProcess, void *pArg,
    uint8_t priority, uint32_t stkSize)
{
    Task *pTask;
    uint32_t *pStkBase;
    ListNode *pNode;
    
    ZF_ASSERT(ppTask != (Task **)0)
    ZF_ASSERT(taskProcess != (ITaskProcess)0)
    ZF_ASSERT(stkSize > 255)
    ZF_ASSERT(priority <= TASK_LOWEST_PRIORITY)
    
    /* 先分配任务对象和链表节点，任务对象放在节点数据区里。 */
    if (!List_mallocNode(&pNode, (void **)&pTask, sizeof(Task)))
    {
        ZF_DEBUG(LOG_E, "malloc task space error\r\n");
        
        return false;
    }
    
    pTask->Priority = priority;
    pTask->State = TASK_STATE_RUNNING;
    pTask->DelayTime = 0;
    pTask->RunTime = 0;
    
    /* 再为任务单独分配栈空间。 */
    pStkBase = ZF_MALLOC(stkSize);
    if (pStkBase == NULL)
    {
        ZF_DEBUG(LOG_E, "malloc task stack space error\r\n");
        List_freeNode(pNode);
        
        return false;
    }
    
    pTask->pStkBase = pStkBase;
    pTask->StkSize = stkSize;
    
    /* 初始化任务栈，让第一次切换进来时像“从中断恢复”一样开始执行任务函数。 */
    ZF_initTaskStack(pTask, taskProcess, pArg);
    
    /* 绑定面向对象风格接口。 */
    pTask->Start = Task_start;
    pTask->Stop = Task_stop;
    pTask->Dispose = Task_dispose;
    pTask->Delay = Task_delay;
    
    /* 首次创建任务时顺便初始化全局任务链表。 */
    if (pmTaskList == NULL)
    {
        List_create(&pmTaskList);
        
        if (pmTaskList == NULL)
        {
            ZF_DEBUG(LOG_E, "malloc task list space error\r\n");
            
            /* 清空输出，表示创建失败。 */
            *ppTask = NULL;
            ZF_FREE(pStkBase);
            List_freeNode(pNode);
            
            return false;
        }
    }
    
    /* 插入链表时暂时关闭调度，避免链表修改过程中被抢占。 */
    TASK_SCHEDULE_DISABLE();
    
    /* 新任务创建后立即进入任务集合；是否运行由 State 决定。 */
    pmTaskList->Add(pmTaskList, pNode);
    
    /* 恢复调度。 */
    TASK_SCHEDULE_ENABLE();
    
    /* ��� */
    *ppTask = pTask;
    
    return true;
}

/******************************************************************************
 * 功能  启动任务
 * 说明  这里只是把任务状态设为 RUNNING，真正开始执行还要等调度器选中它。
******************************************************************************/
bool Task_start(Task * const pTask)
{
    ZF_ASSERT(pTask != (Task *)0)
    
    /* 修改任务状态前暂时关闭调度。 */
    TASK_SCHEDULE_DISABLE();
    
    pTask->State = TASK_STATE_RUNNING;
    
    /* 恢复调度器。 */
    TASK_SCHEDULE_ENABLE();
    
    return true;
}

/******************************************************************************
 * 功能  停止任务
 * 说明  如果停止的是正在运行中的普通任务，会立即重新触发一次调度。
******************************************************************************/
bool Task_stop(Task * const pTask)
{
    ZF_ASSERT(pTask != (Task *)0)
    
    /* 空闲任务不能被停止，否则系统失去兜底执行流。 */
    if (pTask == pIdleTask)
    {
        return false;
    }
    
    /* 如果正在运行中停止，状态变化后需要立即重调度。 */
    if (mIsTaskSystemRun && pTask->State == TASK_STATE_RUNNING)
    {
        /* 先冻结调度，避免中间态被切走。 */
        TASK_SCHEDULE_DISABLE();
        
        pTask->State = TASK_STATE_STOP;
        
        /* 再恢复调度。 */
        TASK_SCHEDULE_ENABLE();
        
        /* 立刻让调度器重新选择可运行任务。 */
        Task_schedule();
    }
    else
    {
        /* 未运行系统时只更新状态即可。 */
        TASK_SCHEDULE_DISABLE();
        
        pTask->State = TASK_STATE_STOP;
        
        /* 恢复调度。 */
        TASK_SCHEDULE_ENABLE();
    }
    
    return true;
}

/******************************************************************************
 * 功能  在空闲任务上下文中真正销毁任务
 * 说明  不直接在调用者上下文释放任务，是为了避免“任务把自己删掉”时破坏当前栈。
******************************************************************************/
static void Task_disposeByIdleTask(List *pArgList)
{
    /* 保存并恢复中断上下文的局部变量。 */
    ZF_SR_VAL();
    
    ListNode *pNode;
    Task *pTask;
    
    ZF_ASSERT(pArgList != (List *)0)
    ZF_ASSERT(pmTaskList != (List *)0)

    if (pArgList->Count == 0)
    {
        return;
    }
    
    /* 进入临界区，安全地从任务链表移除目标任务。 */
    ZF_CRITICAL_ENTER();
    
    /* 参数列表的第 0 项保存着待销毁任务指针。 */
    pTask = *((Task **)pArgList->GetElementDataAt(pArgList, 0));

    if (pTask == NULL)
    {
        /* 无效任务指针，直接退出临界区返回。 */
        ZF_CRITICAL_EXIT();
        return;
    }

    /* 空闲任务自身不能被销毁。 */
    if (pTask == pIdleTask)
    {
        /* 退出临界区。 */
        ZF_CRITICAL_EXIT();
        return;
    }
    
    /* 从任务链表中删掉该任务节点，同时释放节点及任务对象。 */
    while(pmTaskList->GetElementByData(pmTaskList, pTask, &pNode))
    {
        if (pNode == NULL)
        {
            break;
        }
        
        if (!pmTaskList->Delete(pmTaskList, pNode))
        {
            ZF_DEBUG(LOG_E, "delete task node from list error\r\n");
            
            break;
        }
    }
    
    /* 退出临界区。 */
    ZF_CRITICAL_EXIT();
}

/******************************************************************************
 * 功能  异步销毁任务
 * 说明  通过给空闲任务投递事件，把真正释放动作延迟到安全上下文执行。
******************************************************************************/
void Task_dispose(Task * const pTask)
{
    /* 保存并恢复中断上下文的局部变量。 */
    ZF_SR_VAL();
    
    Event *pEvent;
    
    ZF_ASSERT(pTask != (Task *)0)
    ZF_ASSERT(pmTaskList != (List *)0)
    
    /* 空闲任务不能被异步销毁。 */
    if (pTask == pIdleTask)
    {
        return;
    }
    
    /* 创建一个高优先级事件，让空闲任务尽快完成资源释放。 */
    Event_create(&pEvent);
    
    pEvent->Priority = TASK_HIGHEST_PRIORITY;
    pEvent->EventProcess = (IEventProcess)Task_disposeByIdleTask;
    /* 事件参数里保存待销毁任务指针。 */
    pEvent->AddArg(pEvent, (void *)&pTask, sizeof(Task *));
    
    /* 投递给空闲任务事件处理器。 */
    EVENT_POST(pIdleTaskEventHandler, pEvent);
    
    /* 临界区内更新任务状态和空闲任务优先级。 */
    ZF_CRITICAL_ENTER();
    
    /* 先把任务标记为停止，避免它再次被调度。 */
    pTask->State = TASK_STATE_STOP;
    
    /* 提升空闲任务优先级，确保销毁事件能尽快被处理。 */
    pIdleTask->Priority = TASK_HIGHEST_PRIORITY;
    
    /* 退出临界区。 */
    ZF_CRITICAL_EXIT();
    
    /* 主动触发一次调度，让系统尽快切到合适任务。 */
    Task_schedule();
}

/******************************************************************************
 * 功能  设置任务延时
 * 说明  DelayTime 由系统节拍递减到 0 后，任务才重新具备运行资格。
******************************************************************************/
void Task_delay(struct _Task * const pTask, uint32_t tick)
{
    ZF_ASSERT(pTask != (Task *)0)
    
    pTask->DelayTime = tick;
}

/******************************************************************************
 * 功能  调度器开关
 * 说明  用于在短时间内冻结调度行为，保护任务链表或状态批量修改。
******************************************************************************/
void Task_scheduleSwitch(bool on)
{
    mIsScheduleOn = on;
}

/******************************************************************************
 * 功能  任务调度
 * 说明  这是任务系统的核心：
 *       1. 找出当前可运行的最高优先级任务。
 *       2. 如果它不是当前任务，就执行上下文切换。
******************************************************************************/
void Task_schedule(void)
{
    /* 保存并恢复中断上下文的局部变量。 */
    ZF_SR_VAL();
    
    /* 系统未启动或调度被显式关闭时，直接返回。 */
    if (!mIsTaskSystemRun || !mIsScheduleOn)
    {
        return;
    }
    
    /* 没有任何任务可调度。 */
    if (pmTaskList == NULL || pmTaskList->Count == 0)
    {
        return;
    }
    
    /* 临界区内读取和更新全局调度状态。 */
    ZF_CRITICAL_ENTER();
    
    /* 从全部任务里选出当前最应该运行的任务。 */
    pTopPriorityTask = Task_getTopPriorityTask();
    
    /* 理论上空闲任务始终存在，因此这里失败通常说明内部状态异常。 */
    if (pTopPriorityTask == NULL)
    {
        /* 异常路径下先退出临界区。 */
        ZF_CRITICAL_EXIT();
        
        ZF_DEBUG(LOG_D, "\r\n");
        ZF_DEBUG(LOG_E, "get top priority task error\r\n");
        ZF_DEBUG(LOG_E, "task schedule stop\r\n");
        
        /* 发现内部异常后，直接关闭调度避免继续运行在未知状态。 */
        TASK_SCHEDULE_DISABLE();
        
        return;
    }
    
    /* 如果最高优先级任务就是当前任务，则无需切换。 */
    if (pTopPriorityTask == pCurrentTask)
    {
        /* 无需切换时先退出临界区。 */
        ZF_CRITICAL_EXIT();
        
        return;
    }
    
    /* 正常路径下退出临界区。 */
    ZF_CRITICAL_EXIT();
    
    /* 进入平台相关的上下文切换逻辑。 */
    TASK_SWITCH();
}

/******************************************************************************
 * 功能  节拍更新时间
 * 说明  通常由系统时钟中断周期调用，用来维护任务延时和运行时间统计。
******************************************************************************/
void Task_timeUpdate(void)
{
    /* 保存并恢复中断上下文的局部变量。 */
    ZF_SR_VAL();
    
    uint32_t i;
    ListNode *pNode;
    Task *pTask;
    
    /* 系统未启动，不更新。 */
    if (!mIsTaskSystemRun)
    {
        return;
    }
    
    /* 没有任务时无需更新。 */
    if (pmTaskList == NULL || pmTaskList->Count == 0)
    {
        return;
    }
    
    /* 在临界区内遍历全部任务并更新统计字段。 */
    ZF_CRITICAL_ENTER();
    
    /* 遍历所有任务，递减延时并累计当前任务运行时间。 */
    for (i = 0; i < pmTaskList->Count; i++)
    {
        if (pmTaskList->GetElementAt(pmTaskList, i, &pNode))
        {
            pTask = (Task *)pNode->pData;
            
            /* 延时中的任务每个 tick 递减一次。 */
            if (pTask->DelayTime > 0)
            {
                pTask->DelayTime--;
            }
            
            /* 只给当前运行任务累加运行时间。 */
            if (pTask == pCurrentTask)
            {
                pTask->RunTime++;
            }
        }
    }
    
    /* 退出临界区。 */
    ZF_CRITICAL_EXIT();
}

/******************************************************************************
 * 功能  空闲任务主循环
 * 说明  空闲任务有两个职责：
 *       1. 处理异步投递过来的后台事件，例如销毁任务。
 *       2. 在系统空闲时占位，避免调度器找不到可运行任务。
******************************************************************************/
void Task_idleTaskProcess(void *pArg)
{
    Event *pEvent;
    
    while(1)
    {
        /* 先执行事件处理器，让后台事件得到处理机会。 */
        pIdleTaskEventHandler->Execute(pIdleTaskEventHandler);
        
        /* 若还有待处理事件，则把空闲任务临时提升到事件优先级。 */
        if (pIdleTaskEventHandler->GetEventCount(pIdleTaskEventHandler) > 0)
        {
            /* 取出队头事件，观察其优先级。 */
            pEvent = (Event *)pIdleTaskEventHandler->pEventList
                ->GetElementDataAt(pIdleTaskEventHandler->pEventList, 0);
            
            if (pEvent == NULL || pEvent->EventProcess == NULL)
            {
                ZF_DEBUG(LOG_E, "idle task get event error:event is null\r\n");
                while(1);
            }
            
            /* 让空闲任务尽快再次被调度，从而持续处理高优先级后台事件。 */
            pIdleTask->Priority = pEvent->Priority;
            
            /* 重新触发调度。 */
            Task_schedule();
        }
        /* 没有后台事件时，空闲任务恢复最低优先级。 */
        else
        {
            /* 空闲任务正常情况下应该永远是最低优先级。 */
            if (pIdleTask->Priority != TASK_LOWEST_PRIORITY)
            {
                pIdleTask->Priority = TASK_LOWEST_PRIORITY;
                
                /* 恢复后再次触发调度，让正常任务抢回 CPU。 */
                Task_schedule();
            }
            
            /* 这里可以扩展为低功耗等待。 */
        }
    }
}

/******************************************************************************
 * 功能  启动任务系统
 * 说明  会自动创建空闲任务、创建空闲任务事件处理器，然后切入首轮调度。
 *       成功后程序不再回到调用点。
******************************************************************************/
void Task_run(void)
{
    /* 至少要先有一个用户任务，否则系统没有实际意义。 */
    if (pmTaskList == NULL || pmTaskList->Count == 0)
    {
        ZF_DEBUG(LOG_E, "run task system error\r\n");
        return;
    }
    
    /* 创建空闲任务，保证调度器永远有兜底任务。 */
    if (!Task_create(&pIdleTask, Task_idleTaskProcess, NULL,
        TASK_LOWEST_PRIORITY, IDLE_TASK_STK_SIZE))
    {
        ZF_DEBUG(LOG_E, "create idle task error\r\n");
        
        while(1);
    }
    
    /* 空闲任务内部用事件处理器来承接异步销毁等后台工作。 */
    EventHandler_create(&pIdleTaskEventHandler);
    
    /* 标记系统已启动。 */
    mIsTaskSystemRun = true;
    
    /* 选出第一批将要运行的最高优先级任务。 */
    pTopPriorityTask = Task_getTopPriorityTask();
    
    /* 交给平台层启动第一次任务切换。 */
    SF_readyGo();
}

/******************************** 文件结束 ********************************/

