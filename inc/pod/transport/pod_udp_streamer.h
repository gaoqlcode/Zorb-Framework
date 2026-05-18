#ifndef __POD_UDP_STREAMER_H__
#define __POD_UDP_STREAMER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

#include "../data/pod_frame_pool.h"

typedef struct _PodUdpStreamer PodUdpStreamer;

typedef void (*PodUdpDropHook)(uint8_t stream_id, uint32_t frame_id,
    uint32_t drop_count, void *pUser);

typedef struct _PodUdpTarget
{
    /* 目标客户端地址与各路端口 */
    const char *Ip;
    uint16_t VisPort;
    uint16_t NirPort;
    uint16_t TirPort;
} PodUdpTarget;

bool PodUdpStreamer_create(PodUdpStreamer **ppStreamer, uint16_t mtu);
void PodUdpStreamer_dispose(PodUdpStreamer *pStreamer);

bool PodUdpStreamer_setTarget(PodUdpStreamer *pStreamer,
    const PodUdpTarget *pTarget);
int32_t PodUdpStreamer_start(PodUdpStreamer *pStreamer);
int32_t PodUdpStreamer_stop(PodUdpStreamer *pStreamer);

int32_t PodUdpStreamer_sendFrame(PodUdpStreamer *pStreamer,
    const PodFrameBlock *pBlock);

/* 设置丢包回调，用于上层触发降码率或告警 */
void PodUdpStreamer_setDropHook(PodUdpStreamer *pStreamer,
    PodUdpDropHook hook, void *pUser);

/* 获取发送阶段累计丢包计数 */
uint32_t PodUdpStreamer_packetDropCount(PodUdpStreamer *pStreamer);

#ifdef __cplusplus
}
#endif

#endif /* __POD_UDP_STREAMER_H__ */
