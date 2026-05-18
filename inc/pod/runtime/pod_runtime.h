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
    /* 外部构建好的插件管理器，运行时通过它拉起所有视频源。 */
    PodPluginManager *pPluginManager;
    /* 运行时配置快照，创建时复制一份，后续按该配置运行。 */
    PodProfile Profile;
} PodRuntimeInit;

/* 创建运行时对象，完成配置快照与平台资源规划。 */
bool PodRuntime_create(PodRuntime **ppRuntime, const PodRuntimeInit *pInit);
/* 销毁运行时对象，同时释放内部帧池等运行期资源。 */
void PodRuntime_dispose(PodRuntime *pRuntime);

/* 依次完成插件 Init/Open/StartStream，真正进入采集状态。 */
int32_t PodRuntime_start(PodRuntime *pRuntime);
/* 依次停止采集并关闭插件资源。 */
int32_t PodRuntime_stop(PodRuntime *pRuntime);

/*
 * 轮询一次所有插件，并将帧压入对应流的 ready 队列。
 * 这是采集侧主入口：插件负责“产帧”，运行时负责“缓存和分发准备”。
 */
int32_t PodRuntime_pollOnce(PodRuntime *pRuntime, uint32_t timeout_ms);

/* 从指定流的 ready 队列读取一帧，供编码、传输或录像模块继续消费。 */
PodFrameBlock *PodRuntime_popReady(PodRuntime *pRuntime, uint8_t stream_id);

/* 消费完成后，将帧块归还到 free 队列，供下一帧继续复用。 */
bool PodRuntime_releaseBlock(PodRuntime *pRuntime, uint8_t stream_id,
    PodFrameBlock *pBlock);

#ifdef __cplusplus
}
#endif

#endif /* __POD_RUNTIME_H__ */
