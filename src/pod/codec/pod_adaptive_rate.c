#if __has_include("pod_adaptive_rate.h")
#include "pod_adaptive_rate.h"
#else
#include "../../../inc/pod/codec/pod_adaptive_rate.h"
#endif

#if __has_include("pod_rk3588_platform.h")
#include "pod_rk3588_platform.h"
#else
#include "../../../inc/platform/pod_rk3588_platform.h"
#endif

#if __has_include("pod_encoder.h")
#include "pod_encoder.h"
#else
#include "../../../inc/pod/codec/pod_encoder.h"
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

typedef struct _PodAdaptiveRate
{
    /* 控制参数（阈值、窗口、步长）。 */
    PodAdaptiveRateConfig Config;
    /* 当前生效的码率与 GOP。 */
    uint32_t CurrentBitrateKbps;
    uint32_t CurrentGop;

    /* 滑动窗口统计状态。 */
    uint64_t WindowStartUs;
    uint64_t LastAdjustUs;
    uint32_t WindowDropEvents;

    /* 外部应用钩子：把新参数下发到编码器。 */
    PodAdaptiveRateApplyHook ApplyHook;
    void *ApplyUser;
} PodAdaptiveRate;

static uint32_t ClampU32(uint32_t v, uint32_t min_v, uint32_t max_v)
{
    /* 限幅函数用于防止调整后的码率跑出安全区间。 */
    if (v < min_v)
    {
        return min_v;
    }
    if (v > max_v)
    {
        return max_v;
    }
    return v;
}

static void TryApply(PodAdaptiveRate *pCtrl)
{
    /* 策略层只负责算结果，真正下发给谁由回调决定。 */
    if (pCtrl->ApplyHook != NULL)
    {
        (void)pCtrl->ApplyHook(pCtrl->Config.StreamId,
            pCtrl->CurrentBitrateKbps, pCtrl->CurrentGop, pCtrl->ApplyUser);
    }
}

bool PodAdaptiveRate_create(PodAdaptiveRate **ppCtrl,
    const PodAdaptiveRateConfig *pConfig)
{
    PodAdaptiveRate *pCtrl;

    ZF_ASSERT(ppCtrl != (PodAdaptiveRate **)0)
    ZF_ASSERT(pConfig != (PodAdaptiveRateConfig *)0)

    /* 这些约束保证控制器一创建就处于可计算状态。 */
    if (pConfig->MinBitrateKbps == 0u || pConfig->MaxBitrateKbps == 0u
        || pConfig->MinBitrateKbps > pConfig->MaxBitrateKbps
        || pConfig->WindowMs == 0u)
    {
        *ppCtrl = NULL;
        return false;
    }

    pCtrl = (PodAdaptiveRate *)ZF_MALLOC(sizeof(PodAdaptiveRate));
    if (pCtrl == NULL)
    {
        *ppCtrl = NULL;
        return false;
    }

    pCtrl->Config = *pConfig;
    pCtrl->CurrentBitrateKbps = ClampU32(pConfig->InitBitrateKbps,
        pConfig->MinBitrateKbps, pConfig->MaxBitrateKbps);
    /* 默认从正常 GOP 开始，出现持续丢包再拉长。 */
    pCtrl->CurrentGop = 30u;

    pCtrl->WindowStartUs = PodPlatform_monotonicUs();
    pCtrl->LastAdjustUs = pCtrl->WindowStartUs;
    pCtrl->WindowDropEvents = 0u;

    pCtrl->ApplyHook = NULL;
    pCtrl->ApplyUser = NULL;

    *ppCtrl = pCtrl;
    return true;
}

void PodAdaptiveRate_dispose(PodAdaptiveRate *pCtrl)
{
    if (pCtrl == NULL)
    {
        return;
    }

    ZF_FREE(pCtrl);
}

void PodAdaptiveRate_setApplyHook(PodAdaptiveRate *pCtrl,
    PodAdaptiveRateApplyHook hook, void *pUser)
{
    ZF_ASSERT(pCtrl != (PodAdaptiveRate *)0)

    pCtrl->ApplyHook = hook;
    pCtrl->ApplyUser = pUser;
}

void PodAdaptiveRate_udpDropHook(uint8_t stream_id, uint32_t frame_id,
    uint32_t drop_count, void *pUser)
{
    PodAdaptiveRate *pCtrl = (PodAdaptiveRate *)pUser;
    (void)frame_id;
    (void)drop_count;

    if (pCtrl == NULL)
    {
        return;
    }

    /* 只统计目标流，避免多路互相干扰。 */
    if (stream_id != pCtrl->Config.StreamId)
    {
        return;
    }

    /* 当前策略先按“事件次数”统计，不区分一次事件里丢了多少包。 */
    pCtrl->WindowDropEvents++;
}

void PodAdaptiveRate_tick(PodAdaptiveRate *pCtrl)
{
    uint64_t now_us;
    uint64_t elapsed_ms;

    ZF_ASSERT(pCtrl != (PodAdaptiveRate *)0)

    now_us = PodPlatform_monotonicUs();
    if (now_us == 0u)
    {
        return;
    }

    /* 单位换算到毫秒后再和窗口参数比较，避免接口层混用单位。 */
    elapsed_ms = (now_us - pCtrl->WindowStartUs) / 1000u;
    /* 未到窗口边界时不做决策，保持控制回路稳定。 */
    if (elapsed_ms < pCtrl->Config.WindowMs)
    {
        return;
    }

    /* 丢包超阈值：快速降码率并拉长 GOP，优先保连通性。 */
    if (pCtrl->WindowDropEvents >= pCtrl->Config.DropThreshold)
    {
        /* 按百分比下降，而不是固定值下降，便于适配不同码率区间。 */
        uint32_t down = pCtrl->CurrentBitrateKbps
            * (uint32_t)pCtrl->Config.StepDownPercent / 100u;
        if (down == 0u)
        {
            down = 1u;
        }

        pCtrl->CurrentBitrateKbps = ClampU32(pCtrl->CurrentBitrateKbps - down,
            pCtrl->Config.MinBitrateKbps, pCtrl->Config.MaxBitrateKbps);

        pCtrl->CurrentGop = 45u;
        pCtrl->LastAdjustUs = now_us;
        TryApply(pCtrl);
    }
    /* 无丢包且冷却结束：缓慢升码率，逐步恢复画质。 */
    else if (pCtrl->WindowDropEvents == 0u)
    {
        uint64_t cool_ms = (now_us - pCtrl->LastAdjustUs) / 1000u;
        if (cool_ms >= pCtrl->Config.CooldownMs)
        {
            /* 升码率比降码率更保守，避免网络刚恢复就再次拥塞。 */
            uint32_t up = pCtrl->CurrentBitrateKbps
                * (uint32_t)pCtrl->Config.StepUpPercent / 100u;
            if (up == 0u)
            {
                up = 1u;
            }

            pCtrl->CurrentBitrateKbps = ClampU32(pCtrl->CurrentBitrateKbps + up,
                pCtrl->Config.MinBitrateKbps, pCtrl->Config.MaxBitrateKbps);

            pCtrl->CurrentGop = 30u;
            pCtrl->LastAdjustUs = now_us;
            TryApply(pCtrl);
        }
    }

    /* 窗口结束后清零统计，开始下一轮观测。 */
    pCtrl->WindowDropEvents = 0u;
    pCtrl->WindowStartUs = now_us;
}

uint32_t PodAdaptiveRate_getCurrentBitrateKbps(PodAdaptiveRate *pCtrl)
{
    ZF_ASSERT(pCtrl != (PodAdaptiveRate *)0)
    return pCtrl->CurrentBitrateKbps;
}

int32_t PodAdaptiveRate_applyToEncoder(uint8_t stream_id,
    uint32_t bitrate_kbps, uint32_t gop, void *pUser)
{
    PodEncoder *pEncoder = (PodEncoder *)pUser;
    (void)stream_id;

    if (pEncoder == NULL)
    {
        return -1;
    }

    /* 默认绑定器：把自适应控制结果直接写入编码器参数。 */
    if (PodEncoder_setBitrateKbps(pEncoder, bitrate_kbps) != 0)
    {
        return -2;
    }

    if (PodEncoder_setGop(pEncoder, gop) != 0)
    {
        return -3;
    }

    return 0;
}
