#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include("pod_plugin_manager.h")
#include "pod_plugin_manager.h"
#else
#include "../../inc/pod/plugin/pod_plugin_manager.h"
#endif

#if __has_include("pod_profile.h")
#include "pod_profile.h"
#else
#include "../../inc/pod/runtime/pod_profile.h"
#endif

#if __has_include("pod_runtime.h")
#include "pod_runtime.h"
#else
#include "../../inc/pod/runtime/pod_runtime.h"
#endif

#if __has_include("pod_encoder.h")
#include "pod_encoder.h"
#else
#include "../../inc/pod/codec/pod_encoder.h"
#endif

#if __has_include("pod_adaptive_rate.h")
#include "pod_adaptive_rate.h"
#else
#include "../../inc/pod/codec/pod_adaptive_rate.h"
#endif

#if __has_include("pod_udp_streamer.h")
#include "pod_udp_streamer.h"
#else
#include "../../inc/pod/transport/pod_udp_streamer.h"
#endif

#if __has_include("pod_tcp_control.h")
#include "pod_tcp_control.h"
#else
#include "../../inc/pod/control/pod_tcp_control.h"
#endif

#if __has_include("pod_camera_plugin.h")
#include "pod_camera_plugin.h"
#else
#include "../../inc/pod/plugin/pod_camera_plugin.h"
#endif

#if __has_include("pod_camera_plugin_template.h")
#include "pod_camera_plugin_template.h"
#else
#include "../../inc/pod/plugin/pod_camera_plugin_template.h"
#endif

#if __has_include("pod_perf_queue.h")
#include "pod_perf_queue.h"
#else
#include "../../inc/pod/data/pod_perf_queue.h"
#endif

#if __has_include("pod_rk3588_platform.h")
#include "pod_rk3588_platform.h"
#else
#include "../../inc/platform/pod_rk3588_platform.h"
#endif

#if __has_include("zf_buffer.h")
#include "zf_buffer.h"
#else
#include "../../inc/core/zf_buffer.h"
#endif

#if __has_include("zf_event.h")
#include "zf_event.h"
#else
#include "../../inc/core/zf_event.h"
#endif

#if __has_include("zf_fsm.h")
#include "zf_fsm.h"
#else
#include "../../inc/core/zf_fsm.h"
#endif

#if __has_include("zf_list.h")
#include "zf_list.h"
#else
#include "../../inc/core/zf_list.h"
#endif

#if __has_include("zf_time.h")
#include "zf_time.h"
#else
#include "../../inc/core/zf_time.h"
#endif

#if __has_include("zf_timer.h")
#include "zf_timer.h"
#else
#include "../../inc/core/zf_timer.h"
#endif

typedef struct _DemoSdkCamUser
{
    uint8_t IsInited;
    uint8_t IsOpened;
    uint8_t IsStreaming;
    uint32_t NextFrameId;
    uint8_t FrameBuf[512];
    uint32_t Exposure;
    uint16_t Width;
    uint16_t Height;
    uint8_t StreamId;
    uint8_t Format;
} DemoSdkCamUser;

typedef struct _DemoCoreCounters
{
    uint32_t EventExecCount;
    uint32_t TimerFireCount;
    uint32_t FsmEnterCount;
    uint32_t FsmExitCount;
    uint32_t FsmWorkCount;
} DemoCoreCounters;

static DemoCoreCounters gCoreCnt = {0u, 0u, 0u, 0u, 0u};

static void DemoEventProcess(List *pArgList)
{
    (void)pArgList;
    gCoreCnt.EventExecCount++;
}

static void DemoTimerProcess(void)
{
    gCoreCnt.TimerFireCount++;
}

static void DemoFsmIdle(Fsm * const pFsm, FsmSignal const signal);

static void DemoFsmWork(Fsm * const pFsm, FsmSignal const signal)
{
    if (signal == FSM_ENTER_SIG)
    {
        gCoreCnt.FsmEnterCount++;
        return;
    }

    if (signal == FSM_EXIT_SIG)
    {
        gCoreCnt.FsmExitCount++;
        return;
    }

    gCoreCnt.FsmWorkCount++;
    (void)pFsm;
}

static void DemoFsmIdle(Fsm * const pFsm, FsmSignal const signal)
{
    if (signal == FSM_ENTER_SIG)
    {
        gCoreCnt.FsmEnterCount++;
        return;
    }

    if (signal == FSM_EXIT_SIG)
    {
        gCoreCnt.FsmExitCount++;
        return;
    }

    /* 收到任意工作信号就切到 WORK 状态。 */
    pFsm->TransferWithEvent(pFsm, DemoFsmWork);
}

static int32_t DemoSdk_Init(void *pUser, const char *cfg_text)
{
    DemoSdkCamUser *pCtx = (DemoSdkCamUser *)pUser;
    (void)cfg_text;

    pCtx->IsInited = 1u;
    pCtx->NextFrameId = 1u;
    pCtx->Exposure = 100u;
    return 0;
}

static int32_t DemoSdk_Open(void *pUser)
{
    DemoSdkCamUser *pCtx = (DemoSdkCamUser *)pUser;
    if (!pCtx->IsInited)
    {
        return -1;
    }
    pCtx->IsOpened = 1u;
    return 0;
}

static int32_t DemoSdk_Start(void *pUser)
{
    DemoSdkCamUser *pCtx = (DemoSdkCamUser *)pUser;
    if (!pCtx->IsOpened)
    {
        return -1;
    }
    pCtx->IsStreaming = 1u;
    return 0;
}

static int32_t DemoSdk_Stop(void *pUser)
{
    DemoSdkCamUser *pCtx = (DemoSdkCamUser *)pUser;
    pCtx->IsStreaming = 0u;
    return 0;
}

static int32_t DemoSdk_GetFrame(void *pUser, PodFrameMeta *meta,
    const uint8_t **ppPayload, uint32_t *pPayloadLen, uint32_t timeout_ms)
{
    DemoSdkCamUser *pCtx = (DemoSdkCamUser *)pUser;
    uint32_t i;

    (void)timeout_ms;

    if (!pCtx->IsStreaming)
    {
        return -1;
    }

    for (i = 0u; i < 128u; i++)
    {
        pCtx->FrameBuf[i] = (uint8_t)((pCtx->NextFrameId + i) & 0xFFu);
    }

    meta->frame_id = pCtx->NextFrameId;
    meta->ts_mono_us = PodPlatform_monotonicUs();
    meta->ts_sync_us = 0u;
    meta->width = pCtx->Width;
    meta->height = pCtx->Height;
    meta->stream_id = pCtx->StreamId;
    meta->format = pCtx->Format;

    *ppPayload = pCtx->FrameBuf;
    *pPayloadLen = 128u;
    pCtx->NextFrameId++;
    return 0;
}

static int32_t DemoSdk_SetParam(void *pUser, const char *key,
    const char *value)
{
    DemoSdkCamUser *pCtx = (DemoSdkCamUser *)pUser;
    if (key == NULL || value == NULL)
    {
        return -1;
    }

    if (strcmp(key, "exposure") == 0)
    {
        pCtx->Exposure = (uint32_t)strtoul(value, NULL, 10);
        return 0;
    }

    return -2;
}

static int32_t DemoSdk_GetParam(void *pUser, const char *key,
    char *value_buf, uint32_t value_buf_size)
{
    DemoSdkCamUser *pCtx = (DemoSdkCamUser *)pUser;
    if (key == NULL || value_buf == NULL || value_buf_size == 0u)
    {
        return -1;
    }

    if (strcmp(key, "exposure") == 0)
    {
        (void)snprintf(value_buf, value_buf_size, "%lu",
            (unsigned long)pCtx->Exposure);
        return 0;
    }

    return -2;
}

static int32_t DemoSdk_GetHealth(void *pUser, uint32_t *pHealthCode)
{
    DemoSdkCamUser *pCtx = (DemoSdkCamUser *)pUser;
    if (pHealthCode == NULL)
    {
        return -1;
    }

    *pHealthCode = (pCtx->IsStreaming ? 0u : 1u);
    return 0;
}

static int32_t DemoSdk_Close(void *pUser)
{
    DemoSdkCamUser *pCtx = (DemoSdkCamUser *)pUser;
    pCtx->IsOpened = 0u;
    return 0;
}

static int32_t DemoGimbalHook(const char *cmd, const char *args, void *pUser)
{
    (void)pUser;
    printf("[demo gimbal] cmd=%s args=%s\n", cmd, args ? args : "");
    return 0;
}

static void DemoRunCoreModules(void)
{
    RingBuffer *pRb = NULL;
    List *pList = NULL;
    ListNode *pNode = NULL;
    uint8_t *pNodeData = NULL;
    PodSpscQueue *pQ = NULL;
    EventHandler *pEventHandler = NULL;
    Event *pEvent = NULL;
    Fsm *pFsm = NULL;
    Timer *pTimer = NULL;
    uint8_t popByte = 0u;
    uint8_t buf[4] = {1u, 2u, 3u, 4u};
    void *qItem = NULL;

    printf("\n========== demo: core modules =========="
        "\n");

    /*
     * 1) 环形缓冲区：演示写入、读取、丢弃等基础字节流操作。
     */
    if (RB_create(&pRb, 16u))
    {
        (void)pRb->SaveRange(pRb, buf, 4u);
        (void)pRb->GetByte(pRb, &popByte);
        printf("[core/rb] first byte=%u remain=%lu\n",
            (unsigned)popByte, (unsigned long)pRb->GetCount(pRb));
    }

    /*
     * 2) 链表：演示节点创建、插入、读取、清理。
     */
    if (List_create(&pList) && List_mallocNode(&pNode, (void **)&pNodeData, 4u))
    {
        pNodeData[0] = 10u;
        pNodeData[1] = 20u;
        pNodeData[2] = 30u;
        pNodeData[3] = 40u;
        (void)pList->Add(pList, pNode);
        printf("[core/list] count=%lu first=%u\n",
            (unsigned long)pList->Count,
            (unsigned)((uint8_t *)pList->GetElementDataAt(pList, 0))[0]);
    }

    /*
     * 3) 无锁 SPSC 队列：演示单生产者 push、单消费者 pop。
     */
    if (PodSpscQueue_create(&pQ, 8u))
    {
        (void)PodSpscQueue_push(pQ, (void *)"alpha");
        if (PodSpscQueue_pop(pQ, &qItem))
        {
            printf("[core/spsc] pop=%s\n", (const char *)qItem);
        }
    }

    /*
     * 4) 事件系统：创建事件并执行一次，验证回调链路。
     */
    if (EventHandler_create(&pEventHandler) && Event_create(&pEvent))
    {
        pEvent->Priority = 1u;
        pEvent->EventProcess = DemoEventProcess;
        (void)pEventHandler->Add(pEventHandler, pEvent);
        pEventHandler->Execute(pEventHandler);
        printf("[core/event] exec_count=%lu\n",
            (unsigned long)gCoreCnt.EventExecCount);
    }

    /*
     * 5) 状态机：演示 idle -> work 的带事件转移。
     */
    if (Fsm_create(&pFsm))
    {
        pFsm->SetInitialState(pFsm, DemoFsmIdle);
        (void)pFsm->Run(pFsm);
        (void)pFsm->Dispatch(pFsm, FSM_ENTER_SIG);
        (void)pFsm->Dispatch(pFsm, FSM_USER_SIG_START);
        (void)pFsm->Dispatch(pFsm, FSM_USER_SIG_START + 1u);
        printf("[core/fsm] enter=%lu exit=%lu work=%lu\n",
            (unsigned long)gCoreCnt.FsmEnterCount,
            (unsigned long)gCoreCnt.FsmExitCount,
            (unsigned long)gCoreCnt.FsmWorkCount);
    }

    /*
     * 6) 定时器 + 时间系统：
     *    - 创建一次性定时器
     *    - 通过 ZF_timeTick 推进系统时间触发回调
     */
    if (Timer_create(&pTimer))
    {
        pTimer->Interval = 2u;
        pTimer->IsAutoReset = false;
        pTimer->TimerProcess = DemoTimerProcess;
        pTimer->Start(pTimer);
        ZF_timeTick();
        ZF_timeTick();
        ZF_timeTick();
        printf("[core/timer] timer_fire_count=%lu systick=%lu\n",
            (unsigned long)gCoreCnt.TimerFireCount,
            (unsigned long)ZF_SYSTICK());
    }

    if (pTimer != NULL)
    {
        (void)pTimer->Dispose(pTimer);
    }
    if (pFsm != NULL)
    {
        (void)pFsm->Dispose(pFsm);
    }
    if (pEventHandler != NULL)
    {
        (void)pEventHandler->Dispose(pEventHandler);
    }
    if (pQ != NULL)
    {
        PodSpscQueue_dispose(pQ);
    }
    if (pList != NULL)
    {
        (void)pList->Dispose(pList);
    }
    if (pRb != NULL)
    {
        (void)pRb->Dispose(pRb);
    }
}

int main(void)
{
    /*
     * 这个 demo 不是“最小可跑样例”，而是“功能覆盖型样例”。
     *
     * 目标：在单一入口里覆盖项目关键能力，方便学习时逐段断点观察。
     * 覆盖范围：
     * 1) core 层：ringbuffer/list/spsc/event/fsm/timer/time
     * 2) pod 层：plugin manager/runtime/frame flow/encoder/adaptive rate
     *             /udp streamer/tcp control
     *
     * 说明：本 demo 使用一个“模拟相机插件”产帧，以保证整个链路可在
     * Linux 环境下独立运行，无需真实摄像头硬件。
     */
    PodPluginManager *pMgr = NULL;
    PodRuntime *pRuntime = NULL;
    PodEncoder *pEncoder = NULL;
    PodAdaptiveRate *pAdaptive = NULL;
    PodUdpStreamer *pStreamer = NULL;
    PodTcpControlServer *pCtrlServer = NULL;
    PodFrameBlock *pInBlock = NULL;
    PodFrameBlock outBlock;
    uint8_t outBuf[2048];
    uint8_t isKey = 0u;
    PodCpuPlan cpuPlan;
    PodUdpTarget udpTarget;
    PodTcpControlConfig ctrlCfg;
    DemoSdkCamUser visUser;
    DemoSdkCamUser nirUser;
    DemoSdkCamUser tirUser;
    PodSdkCameraCtx visTplCtx;
    PodSdkCameraCtx nirTplCtx;
    PodSdkCameraCtx tirTplCtx;
    PodCameraPlugin visPlugin;
    PodCameraPlugin nirPlugin;
    PodCameraPlugin tirPlugin;
    PodSdkCameraHooks hooks;
    uint8_t streamIds[3] = {POD_STREAM_VIS, POD_STREAM_NIR, POD_STREAM_TIR};
    const char *streamNames[3] = {"demo_cam_vis", "demo_cam_nir", "demo_cam_tir"};
    uint32_t streamIdx;
    uint32_t encodedCount = 0u;
    char pluginParam[64];
    FILE *fp = NULL;

    PodRuntimeInit init;
    PodEncoderConfig enc_cfg;
    PodAdaptiveRateConfig ar_cfg;
    int32_t ret;

    memset(&visUser, 0, sizeof(visUser));
    memset(&nirUser, 0, sizeof(nirUser));
    memset(&tirUser, 0, sizeof(tirUser));
    memset(&visTplCtx, 0, sizeof(visTplCtx));
    memset(&nirTplCtx, 0, sizeof(nirTplCtx));
    memset(&tirTplCtx, 0, sizeof(tirTplCtx));
    memset(&visPlugin, 0, sizeof(visPlugin));
    memset(&nirPlugin, 0, sizeof(nirPlugin));
    memset(&tirPlugin, 0, sizeof(tirPlugin));
    memset(&hooks, 0, sizeof(hooks));
    memset(&outBlock, 0, sizeof(outBlock));

    /* 先演示 core 层能力，再进入 pod 业务链路。 */
    DemoRunCoreModules();

    printf("\n========== demo: pod modules =========="
        "\n");

    /* 先加载默认配置，后续编码器和运行时都复用这份 profile。 */
    PodProfile_setDefaults(&init.Profile);

    /*
     * 演示 profile 文本解析：
     * 通过 key=value 文本覆盖默认值，可用于配置热加载前的解析验证。
     */
    (void)PodProfile_parseText(&init.Profile,
        "width=1280\n"
        "height=720\n"
        "fps=25\n"
        "bitrate_kbps=2048\n"
        "gop=25\n"
        "tcp_control_port=19001\n");

    /*
     * 演示从文件加载 profile：
     * 真实项目可直接指向部署配置文件。
     */
    fp = fopen("/tmp/zorb_demo.profile", "wb");
    if (fp != NULL)
    {
        (void)fputs("udp_vis_port=21001\n", fp);
        (void)fputs("udp_nir_port=21002\n", fp);
        (void)fputs("udp_tir_port=21003\n", fp);
        (void)fclose(fp);
        (void)PodProfile_loadFromFile(&init.Profile, "/tmp/zorb_demo.profile");
        (void)remove("/tmp/zorb_demo.profile");
    }

    /*
     * 演示平台层接口：
     * - 设置/读取 CPU 规划
     * - 设置当前线程名
     * - 尝试绑定当前线程 CPU（失败不致命）
     */
    cpuPlan = (PodCpuPlan){2u, 3u, 4u, 5u};
    PodPlatform_setCpuPlan(&cpuPlan);
    cpuPlan = *PodPlatform_getCpuPlan();
    PodPlatform_setCurrentThreadName("demo-main");
    (void)PodPlatform_bindCurrentThread(cpuPlan.ControlCpu);

    if (!PodPluginManager_create(&pMgr))
    {
        printf("PodPluginManager_create failed\n");
        return 1;
    }

    /*
     * 使用模板插件装配三路相机（VIS/NIR/TIR）：
     * 真实项目里你只需要把 hooks 指向厂商 SDK 适配函数。
     */
    hooks.SdkInit = DemoSdk_Init;
    hooks.SdkOpen = DemoSdk_Open;
    hooks.SdkStart = DemoSdk_Start;
    hooks.SdkStop = DemoSdk_Stop;
    hooks.SdkGetFrame = DemoSdk_GetFrame;
    hooks.SdkSetParam = DemoSdk_SetParam;
    hooks.SdkGetParam = DemoSdk_GetParam;
    hooks.SdkGetHealth = DemoSdk_GetHealth;
    hooks.SdkClose = DemoSdk_Close;

    visUser.StreamId = POD_STREAM_VIS;
    visUser.Width = init.Profile.Width;
    visUser.Height = init.Profile.Height;
    visUser.Format = POD_FMT_RAW;
    nirUser.StreamId = POD_STREAM_NIR;
    nirUser.Width = init.Profile.Width;
    nirUser.Height = init.Profile.Height;
    nirUser.Format = POD_FMT_RAW;
    tirUser.StreamId = POD_STREAM_TIR;
    tirUser.Width = init.Profile.Width;
    tirUser.Height = init.Profile.Height;
    tirUser.Format = POD_FMT_RAW;

    if (!PodSdkCameraPlugin_setup(&visPlugin, &visTplCtx,
            streamNames[0], streamIds[0], &hooks, &visUser) ||
        !PodSdkCameraPlugin_setup(&nirPlugin, &nirTplCtx,
            streamNames[1], streamIds[1], &hooks, &nirUser) ||
        !PodSdkCameraPlugin_setup(&tirPlugin, &tirTplCtx,
            streamNames[2], streamIds[2], &hooks, &tirUser))
    {
        printf("PodSdkCameraPlugin_setup failed\n");
        PodPluginManager_dispose(pMgr);
        return 7;
    }

    if (!PodPluginManager_register(pMgr, &visPlugin) ||
        !PodPluginManager_register(pMgr, &nirPlugin) ||
        !PodPluginManager_register(pMgr, &tirPlugin))
    {
        printf("PodPluginManager_register failed\n");
        PodPluginManager_dispose(pMgr);
        return 7;
    }

    (void)PodPluginManager_findByName(pMgr, "demo_cam_vis");
    (void)PodPluginManager_findByName(pMgr, "demo_cam_nir");
    (void)PodPluginManager_findByName(pMgr, "demo_cam_tir");
    (void)PodPluginManager_findByStream(pMgr, POD_STREAM_VIS);
    (void)PodPluginManager_findByStream(pMgr, POD_STREAM_NIR);
    (void)PodPluginManager_findByStream(pMgr, POD_STREAM_TIR);
    (void)PodPluginManager_getAtIndex(pMgr, 0u);
    (void)PodPluginManager_getAtIndex(pMgr, 1u);
    (void)PodPluginManager_getAtIndex(pMgr, 2u);
    printf("[pod/plugin] count=%lu\n",
        (unsigned long)PodPluginManager_getCount(pMgr));

    /* 演示插件参数读写（通过模板插件接口表桥接）。 */
    (void)visPlugin.ops.SetParam(visPlugin.ctx, "exposure", "180");
    (void)visPlugin.ops.GetParam(visPlugin.ctx, "exposure", pluginParam,
        (uint32_t)sizeof(pluginParam));
    printf("[pod/plugin] exposure=%s\n", pluginParam);

    /* 运行时并不自己管理插件集合，而是显式依赖外部注入的插件管理器。 */
    init.pPluginManager = pMgr;

    if (!PodRuntime_create(&pRuntime, &init))
    {
        printf("PodRuntime_create failed\n");
        PodPluginManager_dispose(pMgr);
        return 2;
    }

    /*
     * 编码器配置来自 profile。
     * 当前 demo 里编码器更像“占位骨架”，但接口已经具备真实平台接入的形状。
     */
    enc_cfg.StreamId = 1u;
    enc_cfg.Codec = POD_CODEC_H264;
    enc_cfg.Width = init.Profile.Width;
    enc_cfg.Height = init.Profile.Height;
    enc_cfg.Fps = init.Profile.Fps;
    enc_cfg.BitrateKbps = init.Profile.BitrateKbps;
    enc_cfg.Gop = init.Profile.Gop;

    if (!PodEncoder_create(&pEncoder, &enc_cfg) || PodEncoder_open(pEncoder) != 0)
    {
        printf("PodEncoder init failed\n");
        PodRuntime_dispose(pRuntime);
        PodPluginManager_dispose(pMgr);
        return 5;
    }

    /*
     * 自适应码率控制器根据丢包统计动态调节码率。
     * 这里给出的参数更偏演示用途：窗口短、步长固定，便于观察行为。
     */
    ar_cfg.StreamId = 1u;
    ar_cfg.InitBitrateKbps = init.Profile.BitrateKbps;
    ar_cfg.MinBitrateKbps = init.Profile.BitrateKbps / 4u;
    ar_cfg.MaxBitrateKbps = init.Profile.BitrateKbps * 2u;
    ar_cfg.WindowMs = 1000u;
    ar_cfg.CooldownMs = 5000u;
    ar_cfg.DropThreshold = 5u;
    ar_cfg.StepDownPercent = 20u;
    ar_cfg.StepUpPercent = 10u;

    if (!PodAdaptiveRate_create(&pAdaptive, &ar_cfg))
    {
        printf("PodAdaptiveRate_create failed\n");
        (void)PodEncoder_close(pEncoder);
        PodEncoder_dispose(pEncoder);
        PodRuntime_dispose(pRuntime);
        PodPluginManager_dispose(pMgr);
        return 6;
    }

    /*
     * 把“控制决策”绑定到“编码器参数更新”上。
     * 这样 AdaptiveRate 只负责决策，不直接依赖编码器内部实现。
     */
    PodAdaptiveRate_setApplyHook(pAdaptive, PodAdaptiveRate_applyToEncoder,
        pEncoder);

    /* UDP 发送器：演示分片发送与丢包回调桥接。 */
    if (!PodUdpStreamer_create(&pStreamer, 1200u))
    {
        printf("PodUdpStreamer_create failed\n");
        PodAdaptiveRate_dispose(pAdaptive);
        (void)PodEncoder_close(pEncoder);
        PodEncoder_dispose(pEncoder);
        PodRuntime_dispose(pRuntime);
        PodPluginManager_dispose(pMgr);
        return 8;
    }

    udpTarget.Ip = "127.0.0.1";
    udpTarget.VisPort = init.Profile.UdpVisPort;
    udpTarget.NirPort = init.Profile.UdpNirPort;
    udpTarget.TirPort = init.Profile.UdpTirPort;
    (void)PodUdpStreamer_setTarget(pStreamer, &udpTarget);
    PodUdpStreamer_setDropHook(pStreamer, PodAdaptiveRate_udpDropHook,
        pAdaptive);
    ret = PodUdpStreamer_start(pStreamer);
    if (ret != 0)
    {
        printf("PodUdpStreamer_start failed, ret=%ld\n", (long)ret);
    }

    /* TCP 控制服务：演示控制面生命周期（不要求外部客户端接入）。 */
    ctrlCfg.ListenPort = init.Profile.TcpControlPort;
    ctrlCfg.pPluginManager = pMgr;
    ctrlCfg.GimbalHook = DemoGimbalHook;
    ctrlCfg.pGimbalUser = NULL;
    if (PodTcpControlServer_create(&pCtrlServer, &ctrlCfg))
    {
        ret = PodTcpControlServer_start(pCtrlServer);
        if (ret == 0)
        {
            (void)PodTcpControlServer_pollOnce(pCtrlServer);
        }
    }

    if (PodRuntime_start(pRuntime) != 0)
    {
        printf("PodRuntime_start failed\n");
        if (pCtrlServer != NULL)
        {
            (void)PodTcpControlServer_stop(pCtrlServer);
            PodTcpControlServer_dispose(pCtrlServer);
        }
        if (pStreamer != NULL)
        {
            (void)PodUdpStreamer_stop(pStreamer);
            PodUdpStreamer_dispose(pStreamer);
        }
        PodRuntime_dispose(pRuntime);
        PodPluginManager_dispose(pMgr);
        return 3;
    }

    /* 轮询一次采集侧，把插件输出帧压入运行时管理的 ready 队列。 */
    (void)PodRuntime_pollOnce(pRuntime, 1u);

    /*
     * 拉取 ready 帧后，串联编码和发送：
     * 1) runtime popReady
     * 2) encoder encode
     * 3) udp streamer sendFrame
     * 4) runtime releaseBlock
     */
    /* 三路流逐路取帧，演示多插件并行时的统一处理模式。 */
    for (streamIdx = 0u; streamIdx < 3u; streamIdx++)
    {
        pInBlock = PodRuntime_popReady(pRuntime, streamIds[streamIdx]);
        if (pInBlock != NULL)
        {
            outBlock.pData = outBuf;
            outBlock.Capacity = (uint32_t)sizeof(outBuf);
            outBlock.Length = 0u;
            outBlock.StreamId = pInBlock->StreamId;

            ret = PodEncoder_encode(pEncoder, pInBlock, &outBlock, &isKey);
            if (ret == 0)
            {
                printf("[pod/pipeline] stream=%u encoded len=%lu key=%u\n",
                    (unsigned)outBlock.StreamId,
                    (unsigned long)outBlock.Length,
                    (unsigned)isKey);
                (void)PodUdpStreamer_sendFrame(pStreamer, &outBlock);
                encodedCount++;
            }

            (void)PodRuntime_releaseBlock(pRuntime, pInBlock->StreamId, pInBlock);
        }
    }

    printf("[pod/pipeline] encoded stream count=%lu\n",
        (unsigned long)encodedCount);

    /*
     * 手动注入一次丢包并执行自适应评估，观察码率控制闭环是否联通。
     */
    PodAdaptiveRate_udpDropHook(POD_STREAM_VIS, 1u, 1u, pAdaptive);
    PodAdaptiveRate_tick(pAdaptive);
    printf("[pod/adapt] bitrate=%lu kbps gop=%lu drops=%lu\n",
        (unsigned long)PodEncoder_getBitrateKbps(pEncoder),
        (unsigned long)PodEncoder_getGop(pEncoder),
        (unsigned long)PodUdpStreamer_packetDropCount(pStreamer));

    if (pCtrlServer != NULL)
    {
        (void)PodTcpControlServer_pollOnce(pCtrlServer);
    }

    if (PodRuntime_stop(pRuntime) != 0)
    {
        printf("PodRuntime_stop failed\n");
        if (pCtrlServer != NULL)
        {
            (void)PodTcpControlServer_stop(pCtrlServer);
            PodTcpControlServer_dispose(pCtrlServer);
        }
        if (pStreamer != NULL)
        {
            (void)PodUdpStreamer_stop(pStreamer);
            PodUdpStreamer_dispose(pStreamer);
        }
        PodAdaptiveRate_dispose(pAdaptive);
        (void)PodEncoder_close(pEncoder);
        PodEncoder_dispose(pEncoder);
        PodRuntime_dispose(pRuntime);
        PodPluginManager_dispose(pMgr);
        return 4;
    }

    if (pCtrlServer != NULL)
    {
        (void)PodTcpControlServer_stop(pCtrlServer);
        PodTcpControlServer_dispose(pCtrlServer);
    }

    if (pStreamer != NULL)
    {
        (void)PodUdpStreamer_stop(pStreamer);
        PodUdpStreamer_dispose(pStreamer);
    }

    /* 释放顺序与创建顺序相反，避免依赖对象提前失效。 */
    PodAdaptiveRate_dispose(pAdaptive);
    (void)PodEncoder_close(pEncoder);
    PodEncoder_dispose(pEncoder);

    PodRuntime_dispose(pRuntime);
    PodPluginManager_dispose(pMgr);

    printf("zorb_pod_demo ok\n");
    return 0;
}
