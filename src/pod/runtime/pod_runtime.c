#if __has_include("pod_runtime.h")
#include "pod_runtime.h"
#else
#include "../../../inc/pod/runtime/pod_runtime.h"
#endif

#if __has_include("pod_camera_plugin.h")
#include "pod_camera_plugin.h"
#else
#include "../../../inc/pod/plugin/pod_camera_plugin.h"
#endif

#include <string.h>

#if __has_include("pod_rk3588_platform.h")
#include "pod_rk3588_platform.h"
#else
#include "../../../inc/platform/pod_rk3588_platform.h"
#endif

#if __has_include("zf_assert.h")
#include "zf_assert.h"
#else
#include "../../../inc/core/zf_assert.h"
#endif

#if __has_include("zf_malloc.h")
#include "zf_malloc.h"
#else
#include "../../../inc/core/zf_malloc.h"
#endif

#define POD_STREAM_SLOT_COUNT 4u

typedef struct _PodRuntime
{
    /* 插件管理器：负责多相机插件的生命周期和查找。 */
    PodPluginManager *pPluginManager;
    /* 运行配置快照：创建后按该配置运行。 */
    PodProfile Profile;
    /* 每路视频流对应一个帧池，采集线程写入 ready 队列。 */
    PodFramePool *pFramePools[POD_STREAM_SLOT_COUNT];
    /* 统计每路在运行时的丢帧事件，用于监控与后续自适应策略。 */
    uint32_t StreamDropCount[POD_STREAM_SLOT_COUNT];
} PodRuntime;

static uint8_t StreamToIndex(uint8_t stream_id)
{
    /*
     * 约定 stream_id 直接映射槽位。
     * 越界统一回落到 0，避免数组越界造成崩溃。
     */
    if (stream_id >= POD_STREAM_SLOT_COUNT)
    {
        return 0u;
    }

    return stream_id;
}

static bool Runtime_createPool(PodRuntime *pRuntime, uint8_t stream_id)
{
    uint8_t idx = StreamToIndex(stream_id);

    /*
     * 每路流第一次真正产帧前才创建帧池。
     * 这样做的好处是：
     * 1. 未启用的视频流不占内存。
     * 2. 运行时对象创建阶段尽量轻量，失败点更少。
     */
    if (pRuntime->pFramePools[idx] != NULL)
    {
        return true;
    }

    return PodFramePool_create(&pRuntime->pFramePools[idx],
        pRuntime->Profile.FramePoolBlockCount,
        pRuntime->Profile.FramePoolBlockSize);
}

static void Runtime_disposePools(PodRuntime *pRuntime)
{
    uint32_t i;

    /* 退出时统一释放所有流对应的帧池。 */
    for (i = 0; i < POD_STREAM_SLOT_COUNT; i++)
    {
        if (pRuntime->pFramePools[i] != NULL)
        {
            PodFramePool_dispose(pRuntime->pFramePools[i]);
            pRuntime->pFramePools[i] = NULL;
        }
    }
}

bool PodRuntime_create(PodRuntime **ppRuntime, const PodRuntimeInit *pInit)
{
    PodRuntime *pRuntime;
    PodCpuPlan cpu_plan;

    ZF_ASSERT(ppRuntime != (PodRuntime **)0)
    ZF_ASSERT(pInit != (PodRuntimeInit *)0)
    ZF_ASSERT(pInit->pPluginManager != (PodPluginManager *)0)

    pRuntime = (PodRuntime *)ZF_MALLOC(sizeof(PodRuntime));
    if (pRuntime == NULL)
    {
        *ppRuntime = NULL;
        return false;
    }

    memset(pRuntime, 0, sizeof(PodRuntime));
    pRuntime->pPluginManager = pInit->pPluginManager;
    pRuntime->Profile = pInit->Profile;

    /*
     * 运行时创建时顺手把 profile 中的 CPU 规划下发到平台层。
     * 这样后续采集、编码、网络线程如果需要绑核，就有统一来源。
     */
    cpu_plan.CaptureCpu = pRuntime->Profile.CaptureCpu;
    cpu_plan.EncodeCpu = pRuntime->Profile.EncodeCpu;
    cpu_plan.NetworkCpu = pRuntime->Profile.NetworkCpu;
    cpu_plan.ControlCpu = pRuntime->Profile.ControlCpu;
    PodPlatform_setCpuPlan(&cpu_plan);

    *ppRuntime = pRuntime;
    return true;
}

void PodRuntime_dispose(PodRuntime *pRuntime)
{
    if (pRuntime == NULL)
    {
        return;
    }

    Runtime_disposePools(pRuntime);
    ZF_FREE(pRuntime);
}

int32_t PodRuntime_start(PodRuntime *pRuntime)
{
    int32_t ret;

    ZF_ASSERT(pRuntime != (PodRuntime *)0)

    /*
     * 启动顺序必须固定：Init -> Open -> StartStream。
     * 理解方式可以类比“先配参数，再打开设备，最后启动数据流”。
     */
    ret = PodPluginManager_initAll(pRuntime->pPluginManager, "");
    if (ret != 0)
    {
        return ret;
    }

    ret = PodPluginManager_openAll(pRuntime->pPluginManager);
    if (ret != 0)
    {
        return ret;
    }

    return PodPluginManager_startAll(pRuntime->pPluginManager);
}

int32_t PodRuntime_stop(PodRuntime *pRuntime)
{
    int32_t ret;

    ZF_ASSERT(pRuntime != (PodRuntime *)0)

    /* 停止顺序与启动顺序相反，先停流，再关设备。 */
    ret = PodPluginManager_stopAll(pRuntime->pPluginManager);
    if (ret != 0)
    {
        return ret;
    }

    return PodPluginManager_closeAll(pRuntime->pPluginManager);
}

int32_t PodRuntime_pollOnce(PodRuntime *pRuntime, uint32_t timeout_ms)
{
    uint32_t i;

    ZF_ASSERT(pRuntime != (PodRuntime *)0)

    /*
     * 轮询每个插件拉帧，并进入对应流的 ready 队列。
     * 这是采集侧的核心入口，保持“失败即跳过”的实时策略：
     * 某一路偶发超时，不应该阻塞其他流继续工作。
     */
    for (i = 0; i < PodPluginManager_getCount(pRuntime->pPluginManager); i++)
    {
        PodCameraPlugin *pPlugin;
        PodFrameMeta meta;
        const uint8_t *pPayload = NULL;
        uint32_t payload_len = 0u;
        uint8_t idx;
        PodFrameBlock *pBlock;

        pPlugin = PodPluginManager_getAtIndex(pRuntime->pPluginManager, i);
        if (pPlugin == NULL || pPlugin->ops.GetFrame == NULL)
        {
            continue;
        }

        if (!Runtime_createPool(pRuntime, pPlugin->stream_id))
        {
            return -10;
        }

        /*
         * 从插件拉取一帧。
         * 这里没有重试循环，因为运行时更偏实时系统：宁可丢一帧，也不在单次 poll 中卡太久。
         */
        if (pPlugin->ops.GetFrame(pPlugin->ctx, &meta, &pPayload,
            &payload_len, timeout_ms) != 0)
        {
            continue;
        }

        idx = StreamToIndex(meta.stream_id);
        pBlock = PodFramePool_acquireFree(pRuntime->pFramePools[idx]);
        if (pBlock == NULL)
        {
            /*
             * 当前流已经没有空闲块，说明下游消费速度跟不上上游产帧速度。
             * 此时只能丢帧，但不会拖慢其他流。
             */
            pRuntime->StreamDropCount[idx]++;
            continue;
        }

        /* 超出帧块容量时截断并计入丢失统计 */
        if (payload_len > pBlock->Capacity)
        {
            payload_len = pBlock->Capacity;
            pRuntime->StreamDropCount[idx]++;
        }

        /* 把插件缓冲区内容拷贝到运行时自有帧块中，实现跨模块解耦。 */
        if (pPayload != NULL && payload_len > 0u)
        {
            memcpy(pBlock->pData, pPayload, payload_len);
        }

        pBlock->Length = payload_len;
        pBlock->TimestampUs = meta.ts_mono_us;
        pBlock->FrameId = meta.frame_id;
        pBlock->StreamId = meta.stream_id;

        /* ready 队列压入失败通常意味着消费者处理不过来。 */
        if (!PodFramePool_pushReady(pRuntime->pFramePools[idx], pBlock))
        {
            /* ready 队列满：记录丢帧并把帧块归还 free 队列。 */
            pRuntime->StreamDropCount[idx]++;
            (void)PodFramePool_releaseFree(pRuntime->pFramePools[idx], pBlock);
        }
    }

    return 0;
}

PodFrameBlock *PodRuntime_popReady(PodRuntime *pRuntime, uint8_t stream_id)
{
    uint8_t idx;

    ZF_ASSERT(pRuntime != (PodRuntime *)0)

    /* 运行时只负责路由到对应流的帧池。 */
    idx = StreamToIndex(stream_id);
    if (pRuntime->pFramePools[idx] == NULL)
    {
        return NULL;
    }

    return PodFramePool_popReady(pRuntime->pFramePools[idx]);
}

bool PodRuntime_releaseBlock(PodRuntime *pRuntime, uint8_t stream_id,
    PodFrameBlock *pBlock)
{
    uint8_t idx;

    ZF_ASSERT(pRuntime != (PodRuntime *)0)
    ZF_ASSERT(pBlock != (PodFrameBlock *)0)

    /* 调用方在消费完成后必须归还，否则 free 队列会越来越少。 */
    idx = StreamToIndex(stream_id);
    if (pRuntime->pFramePools[idx] == NULL)
    {
        return false;
    }

    return PodFramePool_releaseFree(pRuntime->pFramePools[idx], pBlock);
}
