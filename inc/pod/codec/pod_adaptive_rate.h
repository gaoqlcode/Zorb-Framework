#ifndef __POD_ADAPTIVE_RATE_H__
#define __POD_ADAPTIVE_RATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

typedef struct _PodAdaptiveRate PodAdaptiveRate;

typedef struct _PodAdaptiveRateConfig
{
    uint8_t StreamId;
    uint32_t InitBitrateKbps;
    uint32_t MinBitrateKbps;
    uint32_t MaxBitrateKbps;
    uint32_t WindowMs;
    uint32_t CooldownMs;
    uint32_t DropThreshold;
    uint8_t StepDownPercent;
    uint8_t StepUpPercent;
} PodAdaptiveRateConfig;

/* 返回 0 表示应用成功 */
typedef int32_t (*PodAdaptiveRateApplyHook)(uint8_t stream_id,
    uint32_t bitrate_kbps, uint32_t gop, void *pUser);

bool PodAdaptiveRate_create(PodAdaptiveRate **ppCtrl,
    const PodAdaptiveRateConfig *pConfig);
void PodAdaptiveRate_dispose(PodAdaptiveRate *pCtrl);

void PodAdaptiveRate_setApplyHook(PodAdaptiveRate *pCtrl,
    PodAdaptiveRateApplyHook hook, void *pUser);

/* UDP 丢包回调桥接函数，可直接传给 PodUdpStreamer_setDropHook */
void PodAdaptiveRate_udpDropHook(uint8_t stream_id, uint32_t frame_id,
    uint32_t drop_count, void *pUser);

/* 周期调用（如每 100ms），触发窗口评估与码率调整 */
void PodAdaptiveRate_tick(PodAdaptiveRate *pCtrl);

uint32_t PodAdaptiveRate_getCurrentBitrateKbps(PodAdaptiveRate *pCtrl);

/*
 * 默认编码器应用钩子：
 * - pUser 传入 PodEncoder*。
 * - 返回 0 表示更新成功。
 */
int32_t PodAdaptiveRate_applyToEncoder(uint8_t stream_id,
    uint32_t bitrate_kbps, uint32_t gop, void *pUser);

#ifdef __cplusplus
}
#endif

#endif /* __POD_ADAPTIVE_RATE_H__ */
