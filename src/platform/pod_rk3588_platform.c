#if defined(__linux__)
#define _GNU_SOURCE
#endif

#if __has_include("pod_rk3588_platform.h")
#include "pod_rk3588_platform.h"
#else
#include "../../inc/platform/pod_rk3588_platform.h"
#endif

#if defined(__linux__)
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#endif

static PodCpuPlan gCpuPlan = {4u, 5u, 6u, 2u};

void PodPlatform_setCpuPlan(const PodCpuPlan *pPlan)
{
    /* 保存一份全局 CPU 规划，供后续线程创建或启动时查询。 */
    if (pPlan != NULL)
    {
        gCpuPlan = *pPlan;
    }
}

const PodCpuPlan *PodPlatform_getCpuPlan(void)
{
    return &gCpuPlan;
}

bool PodPlatform_bindCurrentThread(uint8_t cpu_id)
{
#if defined(__linux__)
    cpu_set_t mask;

    /* Linux 下使用线程亲和性接口把当前线程绑到目标 CPU。 */
    CPU_ZERO(&mask);
    CPU_SET((int)cpu_id, &mask);

    return pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) == 0;
#else
    (void)cpu_id;
    return false;
#endif
}

void PodPlatform_setCurrentThreadName(const char *name)
{
#if defined(__linux__)
    if (name == NULL)
    {
        return;
    }

    /* pthread_setname_np 长度有限，这里截断到 15 个字符。 */
    {
        char short_name[16];
        size_t len = strlen(name);
        if (len > 15u)
        {
            len = 15u;
        }
        memcpy(short_name, name, len);
        short_name[len] = '\0';
        (void)pthread_setname_np(pthread_self(), short_name);
    }
#else
    (void)name;
#endif
}

uint64_t PodPlatform_monotonicUs(void)
{
#if defined(__linux__)
    struct timespec ts;
    /* 单调时钟不会受系统时间回拨影响，适合做超时和窗口统计。 */
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    {
        return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
    }
#endif

    return 0ull;
}
