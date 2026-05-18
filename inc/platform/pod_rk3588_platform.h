#ifndef __POD_RK3588_PLATFORM_H__
#define __POD_RK3588_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

typedef struct _PodCpuPlan
{
    /* 采集、编码、网络、控制线程绑定 CPU 核编号 */
    uint8_t CaptureCpu;
    uint8_t EncodeCpu;
    uint8_t NetworkCpu;
    uint8_t ControlCpu;
} PodCpuPlan;

void PodPlatform_setCpuPlan(const PodCpuPlan *pPlan);
const PodCpuPlan *PodPlatform_getCpuPlan(void);

/* 绑定当前线程到指定 CPU 核 */
bool PodPlatform_bindCurrentThread(uint8_t cpu_id);

/* 设置当前线程名称（尽力而为） */
void PodPlatform_setCurrentThreadName(const char *name);

/* 获取高精度单调时钟，单位微秒 */
uint64_t PodPlatform_monotonicUs(void);

#ifdef __cplusplus
}
#endif

#endif /* __POD_RK3588_PLATFORM_H__ */
