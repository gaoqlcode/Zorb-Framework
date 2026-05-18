#ifndef __POD_RUNTIME_H__
#define __POD_RUNTIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

#include "../data/pod_frame_pool.h"
#include "../plugin/pod_plugin_manager.h"
#include "pod_profile.h"

typedef struct _PodRuntime PodRuntime;

typedef struct _PodRuntimeInit
{
    PodPluginManager *pPluginManager;
    PodProfile Profile;
} PodRuntimeInit;

bool PodRuntime_create(PodRuntime **ppRuntime, const PodRuntimeInit *pInit);
void PodRuntime_dispose(PodRuntime *pRuntime);

int32_t PodRuntime_start(PodRuntime *pRuntime);
int32_t PodRuntime_stop(PodRuntime *pRuntime);

/* 轮询一次所有插件，并将帧压入就绪队列 */
int32_t PodRuntime_pollOnce(PodRuntime *pRuntime, uint32_t timeout_ms);

/* 从指定流队列读取一帧（编码后或原始帧） */
PodFrameBlock *PodRuntime_popReady(PodRuntime *pRuntime, uint8_t stream_id);

/* 发送或录像完成后，将帧块归还到空闲队列 */
bool PodRuntime_releaseBlock(PodRuntime *pRuntime, uint8_t stream_id,
    PodFrameBlock *pBlock);

#ifdef __cplusplus
}
#endif

#endif /* __POD_RUNTIME_H__ */
