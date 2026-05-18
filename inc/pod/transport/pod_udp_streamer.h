#ifndef __POD_UDP_STREAMER_H__
#define __POD_UDP_STREAMER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

#include "../data/pod_frame_pool.h"

typedef struct _PodUdpStreamer PodUdpStreamer;

/*
 * 丢包回调由传输层上报给策略层。
 * 常见用途是触发自适应码率或记录告警。
 */
typedef void (*PodUdpDropHook)(uint8_t stream_id, uint32_t frame_id,
    uint32_t drop_count, void *pUser);

typedef struct _PodUdpTarget
{
    /* 目标客户端地址与各路端口。三路流共用一个 IP，不同流分不同 UDP 端口。 */
    const char *Ip;
    uint16_t VisPort;
    uint16_t NirPort;
    uint16_t TirPort;
} PodUdpTarget;

/* 创建发送器。mtu 决定单个 UDP 包允许承载的最大总字节数。 */
bool PodUdpStreamer_create(PodUdpStreamer **ppStreamer, uint16_t mtu);
void PodUdpStreamer_dispose(PodUdpStreamer *pStreamer);

/* 设置发送目标地址。 */
bool PodUdpStreamer_setTarget(PodUdpStreamer *pStreamer,
    const PodUdpTarget *pTarget);
/* 启动底层 socket。 */
int32_t PodUdpStreamer_start(PodUdpStreamer *pStreamer);
/* 关闭 socket 并停止发送。 */
int32_t PodUdpStreamer_stop(PodUdpStreamer *pStreamer);

/*
 * 发送一整帧。
 * 内部会按 MTU 自动切分为多个 UDP 包，并给每个包补上重组头。
 */
int32_t PodUdpStreamer_sendFrame(PodUdpStreamer *pStreamer,
    const PodFrameBlock *pBlock);

/* 设置丢包回调，用于上层触发降码率或告警。 */
void PodUdpStreamer_setDropHook(PodUdpStreamer *pStreamer,
    PodUdpDropHook hook, void *pUser);

/* 获取发送阶段累计丢包计数。 */
uint32_t PodUdpStreamer_packetDropCount(PodUdpStreamer *pStreamer);

#ifdef __cplusplus
}
#endif

#endif /* __POD_UDP_STREAMER_H__ */
