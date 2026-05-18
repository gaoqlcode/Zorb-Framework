/**
  *****************************************************************************
  * @file    zf_time.h
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
  * @brief   系统时间的头文件
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
  *    Modification:建立文件
  *
  *****************************************************************************
  */

#ifndef __ZF_TIME_H__
#define __ZF_TIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

/* 系统滴答周期(ms)。整个时间、定时器、任务延时都以它为基础。 */
#define ZF_TICK_PERIOD 1

/* 获取系统滴答数 */
#define ZF_SYSTICK() ZF_getSystemTick()

/* 获取系统时间(ms) */
#define ZF_SYSTIME_MS() ZF_getSystemTimeMS()

/*
 * 系统延时(ms)。
 * 如果任务系统在运行，会转成任务延时并让出 CPU；
 * 否则退化为忙等。
 */
#define ZF_DELAY_MS(ms_) do                            \
{                                                      \
    if (ms_ % ZF_TICK_PERIOD)                          \
    {                                                  \
        ZF_delayTick((ms_ / ZF_TICK_PERIOD) + 1);      \
    }                                                  \
    else                                               \
    {                                                  \
        ZF_delayTick(ms_ / ZF_TICK_PERIOD);            \
    }                                                  \
} while(0)

/* 获取系统滴答数 */
uint32_t ZF_getSystemTick(void);

/* 获取系统时间(ms) */
uint32_t ZF_getSystemTimeMS(void);

/* 系统延时 */
void ZF_delayTick(uint32_t tick);

/* 系统滴答程序(需挂在硬件的时间中断里)。 */
void ZF_timeTick (void);

#ifdef __cplusplus
}
#endif

#endif /* __ZF_TIME_H__ */

/******************************** 文件结束 ********************************/

