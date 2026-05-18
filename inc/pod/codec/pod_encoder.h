#ifndef __POD_ENCODER_H__
#define __POD_ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

#include "../data/pod_frame_pool.h"

typedef enum _PodCodecType
{
    POD_CODEC_BYPASS = 0,
    POD_CODEC_H264 = 1,
    POD_CODEC_H265 = 2
} PodCodecType;

typedef struct _PodEncoderConfig
{
    /* 与哪一路视频流绑定。 */
    uint8_t StreamId;
    /* 编码类型，当前支持旁路、H.264、H.265 枚举表达。 */
    uint8_t Codec;
    /* 输入分辨率。 */
    uint16_t Width;
    uint16_t Height;
    /* 目标帧率。 */
    uint16_t Fps;
    /* 目标码率。 */
    uint32_t BitrateKbps;
    /* GOP 长度，用于关键帧节奏控制。 */
    uint32_t Gop;
} PodEncoderConfig;

typedef struct _PodEncoder PodEncoder;

/* 创建编码器对象，当前实现是轻量配置容器与流程骨架。 */
bool PodEncoder_create(PodEncoder **ppEncoder, const PodEncoderConfig *pConfig);
void PodEncoder_dispose(PodEncoder *pEncoder);

/* open/close 对应真实平台里“申请编码器句柄/释放句柄”的阶段。 */
int32_t PodEncoder_open(PodEncoder *pEncoder);
int32_t PodEncoder_close(PodEncoder *pEncoder);
/* 运行时参数调整接口，供自适应码率策略调用。 */
int32_t PodEncoder_setBitrateKbps(PodEncoder *pEncoder, uint32_t bitrate_kbps);
int32_t PodEncoder_setGop(PodEncoder *pEncoder, uint32_t gop);
uint32_t PodEncoder_getBitrateKbps(PodEncoder *pEncoder);
uint32_t PodEncoder_getGop(PodEncoder *pEncoder);

/*
 * 将输入帧块编码到输出帧块。
 * 当前实现为高性能旁路骨架，便于先联通流程。
 */
int32_t PodEncoder_encode(PodEncoder *pEncoder, const PodFrameBlock *pIn,
    PodFrameBlock *pOut, uint8_t *pIsKeyFrame);

#ifdef __cplusplus
}
#endif

#endif /* __POD_ENCODER_H__ */
