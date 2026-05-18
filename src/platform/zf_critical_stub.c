#if __has_include("zf_critical.h")
#include "zf_critical.h"
#else
#include "../../inc/platform/zf_critical.h"
#endif

uint32_t ZF_SR_Save(void)
{
    return 0u;
}

void ZF_SR_Restore(uint32_t sr)
{
    (void)sr;
}

void INTERRUPT_DISABLE(void)
{
}

void INTERRUPT_ENABLE(void)
{
}

void ZF_initTaskStack(Task *pTask, ITaskProcess taskProcess, void *parg)
{
    (void)taskProcess;
    (void)parg;

    if (pTask == (Task *)0)
    {
        return;
    }

    /* Linux 兼容场景下不执行真实堆栈初始化，仅保证字段有效�?*/
    pTask->pStkPtr = pTask->pStkBase;
}

void SF_readyGo(void)
{
    /* Linux 兼容场景下不触发 PendSV�?*/
}

/******************************** 文件结束 ********************************/
