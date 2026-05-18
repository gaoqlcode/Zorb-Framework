/**
  *****************************************************************************
  * @file    zf_task.h
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
  * @brief   任务管理的头文件
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
  *    Modification:建立文件
  *
  *****************************************************************************
  */

#ifndef __ZF_TASK_H__
#define __ZF_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"
#include "zf_list.h"
#include "zf_timer.h"
#include "zf_event.h"

/*
 * 任务系统是对事件系统之上的一层调度抽象：
 * - 任务有独立堆栈。
 * - 调度器总是选择“可运行且优先级最高”的任务执行。
 * - 数字越小优先级越高，这和很多 RTOS 的约定一致。
 */

/* 任务最高优先级(用户不可用) */
#define TASK_HIGHEST_PRIORITY EVENT_HIGHEST_PRIORITY

/* 任务最低优先级(用户不可用) */
#define TASK_LOWEST_PRIORITY EVENT_LOWEST_PRIORITY

/* 开启任务调度 */
#define TASK_SCHEDULE_ENABLE() Task_scheduleSwitch(true)

/* 关闭任务调度 */
#define TASK_SCHEDULE_DISABLE() Task_scheduleSwitch(false)

/* 获取任务调度标志 */
#define TASK_IS_SCHEDULE_ON() mIsScheduleOn

/* 获取任务系统运行标志 */
#define TASK_IS_SYSTEM_RUN() mIsTaskSystemRun

/* 获取空闲任务 */
#define TASK_GET_IDLE_TASK() pIdleTask

/* 获取空闲任务事件处理器 */
#define TASK_GET_IDLE_TASK_HANDLER() pIdleTaskEventHandler

/* 获取任务列表 */
#define TASK_GET_TASK_LIST() Task_getTaskList()

/* 获取任务数量 */
#define TASK_GET_TASK_COUNT() TASK_GET_TASK_LIST()->Count

/* 创建任务定时器 */
#define TASK_TIMER_CREATE(ppTimer_) do                     \
{                                                          \
    Timer_create(ppTimer_);                                \
    if (*ppTimer_ != NULL)                                 \
    {                                                      \
        *ppTimer_->pEventHandler = pIdleTaskEventHandler;  \
    }                                                      \
} while(0)

typedef void (*ITaskProcess)(void *pArg); /* 程序任务类型 */

/* 任务状态 */
typedef enum _TaskState
{
    TASK_STATE_STOP = 0,          /* 停止 */
    TASK_STATE_RUNNING            /* 运行 */
} TaskState;

/*
 * Task 结构既保存运行期状态，也暴露面向对象风格的方法指针。
 * 学习时建议重点看 Priority、State、DelayTime、RunTime 四个字段，
 * 这四个字段基本解释了调度器为什么会选中某个任务。
 */
typedef struct _Task
{
    uint32_t *pStkPtr;            /* 当前上下文保存/恢复时使用的栈顶指针 */
    uint32_t *pStkBase;           /* 整个任务栈的起始地址，用于释放与调试 */
    uint32_t StkSize;             /* 栈空间大小，单位字节 */
    uint32_t DelayTime;           /* 延时剩余 tick，大于 0 时不会被调度运行 */
    uint8_t Priority;             /* 任务优先级，值越小越容易抢占 CPU */
    uint8_t State;                /* 任务当前状态：运行或停止 */
    uint32_t RunTime;             /* 累计运行 tick，可用于简单统计 */
    
    /* 开始任务 */
    bool (*Start)(struct _Task * const pTask);
    
    /* 停止任务 */
    bool (*Stop)(struct _Task * const pTask);
    
    /* 销毁任务 */
    void (*Dispose)(struct _Task * const pTask);
    
    /* 延时任务 */
    void (*Delay)(struct _Task * const pTask, uint32_t tick);
} Task;

extern Task *pTopPriorityTask;              /* 最高优先级任务 */
extern Task *pCurrentTask;                  /* 当前任务 */
extern Task *pIdleTask;                     /* 空闲任务 */
extern EventHandler *pIdleTaskEventHandler; /* 空闲任务事件处理器 */
extern bool mIsScheduleOn;                  /* 任务调度开的标志 */
extern bool mIsTaskSystemRun;               /* 任务系统是否开始的标志 */

/* 获取系统任务链表，便于调试或做运行态统计。 */
List *Task_getTaskList(void);

/* 创建任务：分配 Task 对象、栈空间，并插入任务链表。 */
bool Task_create(Task **ppTask, ITaskProcess taskProcess, void *pArg,
    uint8_t priority, uint32_t stkSize);

/* 将任务标记为可运行。 */
bool Task_start(Task * const pTask);

/* 将任务标记为停止，不再参与调度。 */
bool Task_stop(Task * const pTask);

/* 销毁任务。实际释放动作由空闲任务异步完成，避免在当前上下文直接删自己。 */
void Task_dispose(Task * const pTask);

/* 当前任务主动延时指定 tick。 */
void Task_delay(struct _Task * const pTask, uint32_t tick);

/* 开关调度器，用于临界区或批量修改任务状态。 */
void Task_scheduleSwitch(bool on);

/* 触发一次调度决策，必要时执行上下文切换。 */
void Task_schedule(void);

/* 在系统时钟节拍中调用，维护 DelayTime 与 RunTime。 */
void Task_timeUpdate(void);

/* 启动任务系统并切入调度循环，正常情况下不会返回。 */
void Task_run(void);

#ifdef __cplusplus
}
#endif

#endif /* __ZF_TASK_H__ */

/******************************** 文件结束 ********************************/

