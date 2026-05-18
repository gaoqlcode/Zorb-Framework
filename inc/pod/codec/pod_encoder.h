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
    uint8_t StreamId;
    uint8_t Codec;
    uint16_t Width;
    uint16_t Height;
    uint16_t Fps;
    uint32_t BitrateKbps;
    uint32_t Gop;
} PodEncoderConfig;

typedef struct _PodEncoder PodEncoder;

bool PodEncoder_create(PodEncoder **ppEncoder, const PodEncoderConfig *pConfig);
void PodEncoder_dispose(PodEncoder *pEncoder);

int32_t PodEncoder_open(PodEncoder *pEncoder);
int32_t PodEncoder_close(PodEncoder *pEncoder);
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
