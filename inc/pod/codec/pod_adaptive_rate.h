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
    /* 控制哪一路视频流。 */
    uint8_t StreamId;
    /* 初始码率。 */
    uint32_t InitBitrateKbps;
    /* 允许下探的最小码率。 */
    uint32_t MinBitrateKbps;
    /* 允许恢复到的最大码率。 */
    uint32_t MaxBitrateKbps;
    /* 统计窗口大小，到窗口边界才做一次决策。 */
    uint32_t WindowMs;
    /* 无丢包后至少等待多久才尝试升码率。 */
    uint32_t CooldownMs;
    /* 一个窗口内累计丢包事件达到阈值后触发降码率。 */
    uint32_t DropThreshold;
    /* 降码率百分比。 */
    uint8_t StepDownPercent;
    /* 升码率百分比。 */
    uint8_t StepUpPercent;
} PodAdaptiveRateConfig;

/* 返回 0 表示应用成功；这个钩子把“决策”翻译成“设备参数更新”。 */
typedef int32_t (*PodAdaptiveRateApplyHook)(uint8_t stream_id,
    uint32_t bitrate_kbps, uint32_t gop, void *pUser);

/* 创建控制器，内部会初始化窗口统计状态。 */
bool PodAdaptiveRate_create(PodAdaptiveRate **ppCtrl,
    const PodAdaptiveRateConfig *pConfig);
void PodAdaptiveRate_dispose(PodAdaptiveRate *pCtrl);

/* 绑定参数下发函数，例如把结果应用到编码器。 */
void PodAdaptiveRate_setApplyHook(PodAdaptiveRate *pCtrl,
    PodAdaptiveRateApplyHook hook, void *pUser);

/* UDP 丢包回调桥接函数，可直接传给传输模块的丢包通知接口。 */
void PodAdaptiveRate_udpDropHook(uint8_t stream_id, uint32_t frame_id,
    uint32_t drop_count, void *pUser);

/* 周期调用（如每 100ms），触发窗口评估与码率调整。 */
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
