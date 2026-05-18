#ifndef __ZF_CRITICAL_H__
#define __ZF_CRITICAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "../core/zf_task.h"

/* Linux/RK3588 兼容实现：不使用 PendSV 切换 */
#define TASK_SWITCH() ((void)0)

typedef uint32_t ZF_STK_TYPE;

/* 兼容旧代码中的临界区变量声明 */
#define ZF_SR_VAL() uint32_t ZF_SR = 0u

/* Linux 用户态下默认不屏蔽中断，做空实现 */
#define ZF_INT_DIS() do { (void)ZF_SR; } while (0)
#define ZF_INT_EN() do { (void)ZF_SR; } while (0)
#define ZF_CRITICAL_ENTER() do { ZF_INT_DIS(); } while (0)
#define ZF_CRITICAL_EXIT() do { ZF_INT_EN(); } while (0)

/* 兼容函数：由 zf_critical_stub.c 提供空实现 */
uint32_t ZF_SR_Save(void);
void ZF_SR_Restore(uint32_t sr);
void INTERRUPT_DISABLE(void);
void INTERRUPT_ENABLE(void);

void ZF_initTaskStack(Task *pTask, ITaskProcess taskProcess, void *parg);
void SF_readyGo(void);

#ifdef __cplusplus
}
#endif

#endif /* __ZF_CRITICAL_H__ */

/******************************** 文件结束 ********************************/
