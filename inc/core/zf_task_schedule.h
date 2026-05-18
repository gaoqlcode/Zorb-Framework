/**
  *****************************************************************************
  * @file    zf_task_schedule.h
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
  * @brief   任务调度算法的头文件
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
  *    Modification:建立文件
  *
  *****************************************************************************
  */

#ifndef __ZF_TASK_SCHEDULE_H__
#define __ZF_TASK_SCHEDULE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"
#include "zf_task.h"

/*
 * 获取当前可运行的最高优先级任务。
 * “可运行”同时要求：
 * 1. 状态是 RUNNING。
 * 2. DelayTime 已经归零。
 */
Task *Task_getTopPriorityTask(void);

#ifdef __cplusplus
}
#endif

#endif /* __ZF_TASK_SCHEDULE_H__ */

/******************************** 文件结束 ********************************/

