/**
  *****************************************************************************
  * @file    zf_task_schedule.c
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
 * @brief   任务调度算法实现
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
    *    Modification:建立文件
  *
  *****************************************************************************
  */

#include "zf_task_schedule.h"
#include "zf_assert.h"
#include "stdlib.h"

/******************************************************************************
 * 功能  获取最高优先级可运行任务
 * 说明  采用抢占式优先级策略：
 *       在所有 RUNNING 且未延时的任务中，选择优先级数值最小的那个。
******************************************************************************/
Task *Task_getTopPriorityTask(void)
{
    Task *pTask = NULL;
    List *pTaskList;
    
    pTaskList = TASK_GET_TASK_LIST();
    if (pTaskList == NULL)
    {
        return NULL;
    }
    
    if (pTaskList->Count > 0)
    {
        Task *pTaski = NULL;
        uint32_t i;
        
        pTask = TASK_GET_IDLE_TASK();
        if (pTask == NULL)
        {
            return NULL;
        }
        
        /* 遍历所有任务，找出最该运行的那个。 */
        for (i = 0; i < pTaskList->Count; i++)
        {
            pTaski = (Task *)pTaskList->GetElementDataAt(pTaskList, i);
            if (pTaski != NULL)
            {
                /* 只有运行态且不在延时中的任务才有资格参与调度。 */
                if (pTaski->State == TASK_STATE_RUNNING
                    && pTaski->DelayTime == 0)
                {
                    /* 数字更小意味着优先级更高。 */
                    if (pTask->Priority > pTaski->Priority)
                    {
                        pTask = pTaski;
                    }
                }
            }
        }
        
        /* 如果连空闲任务都不可运行，说明系统状态异常。 */
        if (pTask->State != TASK_STATE_RUNNING || pTask->DelayTime > 0)
        {
            pTask = NULL;
        }
    }
    
    return pTask;
}

/******************************** 文件结束 ********************************/

