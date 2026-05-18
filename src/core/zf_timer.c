/**
  *****************************************************************************
  * @file    zf_timer.c
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
 * @brief   软件定时器实现
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
    *    Modification:建立文件
  *
  *****************************************************************************
  */

#include "zf_timer.h"
#include "zf_assert.h"
#include "zf_debug.h"
#include "zf_list.h"
#include "zf_malloc.h"
#include "zf_time.h"
#include "zf_task.h"

/* 打开定时器扫描过程。 */
#define TIMER_PROCESS_ENABLE() mIsTimerProcessOn = true
/* 关闭定时器扫描过程。 */
#define TIMER_PROCESS_DISABLE() mIsTimerProcessOn = false

/* 是否启用 Timer_process 扫描。 */
static bool mIsTimerProcessOn = false;

/* 系统内所有软件定时器。 */
static List *pmTimerList = NULL;

/******************************************************************************
 * 功能  刷新下一次触发时间
******************************************************************************/
static void RefreshAlarmTime(Timer * const pTimer)
{
    ZF_ASSERT(pTimer != (Timer *)0)
    
    pTimer->AlarmTime = ZF_SYSTIME_MS() + pTimer->Interval;
}

/******************************************************************************
 * 功能  创建定时器
 * 说明  定时器对象会被加入全局定时器链表，等待 Timer_process 扫描。
******************************************************************************/
bool Timer_create(Timer **ppTimer)
{
    ListNode *pNode;
    Timer *pTimer;
    
    ZF_ASSERT(ppTimer != (Timer **)0)
    
    if (!List_mallocNode(&pNode, (void **)&pTimer, sizeof(Timer)))
    {
        ZF_DEBUG(LOG_E, "malloc timer space error\r\n");
        
        return false;
    }
    
    pTimer->Priority = EVENT_LOWEST_PRIORITY;
    pTimer->Interval = 0;
    pTimer->IsAutoReset = true;
    pTimer->IsRunning = false;
    pTimer->pEventHandler = NULL;
    pTimer->TimerProcess = NULL;
    
    pTimer->Start = Timer_start;
    pTimer->Stop = Timer_stop;
    pTimer->Restart = Timer_restart;
    pTimer->Dispose = Timer_dispose;
    
    /* 首次创建定时器时顺带初始化全局定时器链表。 */
    if (pmTimerList == NULL)
    {
        if (!List_create(&pmTimerList))
        {
            ZF_DEBUG(LOG_E, "malloc timer list space error\r\n");
            
            /* ��� */
            *ppTimer = NULL;
            return false;
        }
        
        /* 链表创建成功后打开后台扫描。 */
        TIMER_PROCESS_ENABLE();
    }
    
    /* ���ӵ���ʱ���б� */
    pmTimerList->Add(pmTimerList, pNode);
    
    /* ��� */
    *ppTimer = pTimer;
    
    return true;
}

/******************************************************************************
 * 功能  启动定时器
 * 说明  启动后并不会立刻触发，而是在到达 AlarmTime 时触发。
******************************************************************************/
void Timer_start(Timer * const pTimer)
{
    ZF_ASSERT(pTimer != (Timer *)0)
    
    if (!pTimer->IsRunning)
    {
        RefreshAlarmTime(pTimer);
        pTimer->IsRunning = true;
    }
}

/******************************************************************************
 * 功能  停止定时器
******************************************************************************/
void Timer_stop(Timer * const pTimer)
{
    ZF_ASSERT(pTimer != (Timer *)0)
    
    pTimer->IsRunning = false;
}

/******************************************************************************
 * 功能  重启定时器
 * 说明  等价于 Stop 后重新按当前时刻计算 AlarmTime。
******************************************************************************/
void Timer_restart(Timer * const pTimer)
{
    ZF_ASSERT(pTimer != (Timer *)0)
    
    Timer_stop(pTimer);
    
    Timer_start(pTimer);
}

/******************************************************************************
 * 功能  销毁定时器
 * 说明  从全局链表移除，但不主动关闭 Timer_process 总开关。
******************************************************************************/
bool Timer_dispose(Timer * const pTimer)
{
    ListNode *pNode;
    
    ZF_ASSERT(pTimer != (Timer *)0)
    ZF_ASSERT(pmTimerList != (List *)0)
    
    Timer_stop(pTimer);
    
    /* 从全局定时器链表中移除目标定时器。 */
    while(pmTimerList->GetElementByData(pmTimerList, pTimer, &pNode))
    {
        if (pNode == NULL)
        {
            break;
        }
        
        if (!pmTimerList->Delete(pmTimerList, pNode))
        {
            ZF_DEBUG(LOG_E, "delete timer node from list error\r\n");
            
            break;
        }
    }
    
    return true;
}

/******************************************************************************
 * 功能  投递定时器事件
 * 说明  当定时器绑定了事件处理器时，不直接执行回调，而是转成一个事件。
******************************************************************************/
static void Timer_postEvent(Timer *pTimer)
{
    /* 创建一个事件，把 TimerProcess 包装进去。 */
    Event *pEvent;
    Event_create(&pEvent);
    pEvent->Priority = pTimer->Priority;
    pEvent->EventProcess = (IEventProcess)pTimer->TimerProcess;
    pEvent->pArgList = NULL; 
    
    /* 投递到目标事件处理器。 */
    EVENT_POST(pTimer->pEventHandler, pEvent);
    
    /* 如果目标是空闲任务处理器，则提升空闲任务优先级，确保事件尽快执行。 */
    if (pTimer->pEventHandler == TASK_GET_IDLE_TASK_HANDLER())
    {
        /* 提升空闲任务优先级，让该定时事件尽快得到执行。 */
        if (TASK_GET_IDLE_TASK()->Priority > pTimer->Priority)
        {
            TASK_GET_IDLE_TASK()->Priority = pTimer->Priority;
        }
    }
}

/******************************************************************************
 * 功能  定时器后台扫描
 * 说明  建议每 1ms 调用一次。
 *       它会遍历全部定时器，检查是否到期，并决定直接执行还是转成事件。
******************************************************************************/
void Timer_process(void)
{
    /* 总开关关闭时直接返回。 */
    if (!mIsTimerProcessOn)
    {
        return;
    }
    
    if (pmTimerList != NULL && pmTimerList->Count > 0)
    {
        int i = 0;
        ListNode *pNode;
        Timer *pTimer;
        
        /* 逐个扫描所有定时器。 */
        for (i = 0; i < pmTimerList->Count; i++)
        {
            if (pmTimerList->GetElementAt(pmTimerList, i, &pNode))
            {
                pTimer = (Timer *)pNode->pData;
                
                /* 只处理处于运行中的定时器。 */
                if (pTimer->IsRunning)
                {
                    /* 到达或超过触发时间时执行。 */
                    if (ZF_SYSTIME_MS() >= pTimer->AlarmTime)
                    {
                        /* 自动重装定时器会继续下一轮，否则只触发一次。 */
                        if(pTimer->IsAutoReset)
                        {
                            pTimer->Restart(pTimer);
                        }
                        else
                        {
                            pTimer->Stop(pTimer);
                        }
                        
                        /* 到点后执行回调：本地直接执行，或转事件异步执行。 */
                        if (pTimer->TimerProcess != NULL)
                        {
                            if (pTimer->pEventHandler == NULL)
                            {
                                /* 未绑定事件处理器时，直接在当前上下文执行回调。 */
                                pTimer->TimerProcess();
                            }
                            else
                            {
                                /* 绑定了处理器时，把回调包装成事件异步执行。 */
                                Timer_postEvent(pTimer);
                            }
                        }
                    }
                }
            }
        }
    }
}

/******************************** 文件结束 ********************************/

