#include <stdio.h>

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

int main(void)
{
    PodPluginManager *pMgr = NULL;
    PodRuntime *pRuntime = NULL;
    PodEncoder *pEncoder = NULL;
    PodAdaptiveRate *pAdaptive = NULL;
    PodRuntimeInit init;
    PodEncoderConfig enc_cfg;
    PodAdaptiveRateConfig ar_cfg;

    PodProfile_setDefaults(&init.Profile);

    if (!PodPluginManager_create(&pMgr))
    {
        printf("PodPluginManager_create failed\n");
        return 1;
    }

    init.pPluginManager = pMgr;

    if (!PodRuntime_create(&pRuntime, &init))
    {
        printf("PodRuntime_create failed\n");
        PodPluginManager_dispose(pMgr);
        return 2;
    }

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

    PodAdaptiveRate_setApplyHook(pAdaptive, PodAdaptiveRate_applyToEncoder,
        pEncoder);

    /* 演示一次丢包回调和周期评估流程�?*/
    PodAdaptiveRate_udpDropHook(1u, 1u, 1u, pAdaptive);
    PodAdaptiveRate_tick(pAdaptive);

    if (PodRuntime_start(pRuntime) != 0)
    {
        printf("PodRuntime_start failed\n");
        PodRuntime_dispose(pRuntime);
        PodPluginManager_dispose(pMgr);
        return 3;
    }

    (void)PodRuntime_pollOnce(pRuntime, 1u);

    if (PodRuntime_stop(pRuntime) != 0)
    {
        printf("PodRuntime_stop failed\n");
        PodAdaptiveRate_dispose(pAdaptive);
        (void)PodEncoder_close(pEncoder);
        PodEncoder_dispose(pEncoder);
        PodRuntime_dispose(pRuntime);
        PodPluginManager_dispose(pMgr);
        return 4;
    }

    PodAdaptiveRate_dispose(pAdaptive);
    (void)PodEncoder_close(pEncoder);
    PodEncoder_dispose(pEncoder);

    PodRuntime_dispose(pRuntime);
    PodPluginManager_dispose(pMgr);

    printf("zorb_pod_demo ok\n");
    return 0;
}
