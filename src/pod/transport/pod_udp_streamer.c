#if __has_include("pod_udp_streamer.h")
#include "pod_udp_streamer.h"
#else
#include "../../../inc/pod/transport/pod_udp_streamer.h"
#endif

#include <string.h>

#if defined(__linux__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
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

#define POD_STREAM_VIS_ID 1u
#define POD_STREAM_NIR_ID 2u
#define POD_STREAM_TIR_ID 3u
#define POD_UDP_HEADER_MAGIC 0x5650u

typedef struct _PodUdpHeader
{
    /* 固定头：接收端按该头完成分片重组和帧边界识别。 */
    uint16_t Magic;
    uint8_t Version;
    uint8_t StreamId;
    uint32_t FrameId;
    uint16_t PacketId;
    uint16_t PacketCount;
    uint64_t TimestampUs;
    uint8_t Flags;
    uint16_t PayloadLen;
} PodUdpHeader;

typedef struct _PodUdpStreamer
{
    uint16_t Mtu;
    uint8_t IsStarted;
    /* 全局发送丢包统计（发送失败/本地组包失败）。 */
    uint32_t PacketDropCount;
    PodUdpDropHook DropHook;
    void *DropUser;

#if defined(__linux__)
    int SockVis;
    int SockNir;
    int SockTir;
    struct sockaddr_in AddrVis;
    struct sockaddr_in AddrNir;
    struct sockaddr_in AddrTir;
#endif
} PodUdpStreamer;

static void NotifyDrop(PodUdpStreamer *pStreamer, uint8_t stream_id,
    uint32_t frame_id)
{
    /* 丢包事件同时上报给外部 hook，供自适应码率使用。 */
    pStreamer->PacketDropCount++;
    if (pStreamer->DropHook != NULL)
    {
        pStreamer->DropHook(stream_id, frame_id, pStreamer->PacketDropCount,
            pStreamer->DropUser);
    }
}

static uint16_t GetMaxPayload(uint16_t mtu)
{
    /* MTU 里还要留出自定义协议头空间。 */
    if (mtu <= (uint16_t)sizeof(PodUdpHeader))
    {
        return 0u;
    }

    return (uint16_t)(mtu - (uint16_t)sizeof(PodUdpHeader));
}

#if defined(__linux__)
static int GetSockByStream(PodUdpStreamer *p, uint8_t stream_id,
    struct sockaddr_in **ppAddr)
{
    /* 不同视频流对应不同 UDP 端口。 */
    if (stream_id == POD_STREAM_VIS_ID)
    {
        *ppAddr = &p->AddrVis;
        return p->SockVis;
    }

    if (stream_id == POD_STREAM_NIR_ID)
    {
        *ppAddr = &p->AddrNir;
        return p->SockNir;
    }

    *ppAddr = &p->AddrTir;
    return p->SockTir;
}

static void CloseSockSafe(int *pFd)
{
    if (pFd != NULL && *pFd >= 0)
    {
        close(*pFd);
        *pFd = -1;
    }
}
#endif

bool PodUdpStreamer_create(PodUdpStreamer **ppStreamer, uint16_t mtu)
{
    PodUdpStreamer *p;

    ZF_ASSERT(ppStreamer != (PodUdpStreamer **)0)

    /* MTU 太小时即使不含业务数据，也无法容纳自定义头。 */
    if (mtu < 256u)
    {
        *ppStreamer = NULL;
        return false;
    }

    p = (PodUdpStreamer *)ZF_MALLOC(sizeof(PodUdpStreamer));
    if (p == NULL)
    {
        *ppStreamer = NULL;
        return false;
    }

    memset(p, 0, sizeof(PodUdpStreamer));
    p->Mtu = mtu;
    p->DropHook = NULL;
    p->DropUser = NULL;

#if defined(__linux__)
    p->SockVis = -1;
    p->SockNir = -1;
    p->SockTir = -1;
#endif

    *ppStreamer = p;
    return true;
}

void PodUdpStreamer_setDropHook(PodUdpStreamer *pStreamer,
    PodUdpDropHook hook, void *pUser)
{
    ZF_ASSERT(pStreamer != (PodUdpStreamer *)0)

    pStreamer->DropHook = hook;
    pStreamer->DropUser = pUser;
}

void PodUdpStreamer_dispose(PodUdpStreamer *pStreamer)
{
    if (pStreamer == NULL)
    {
        return;
    }

    /* 销毁前统一 stop，确保 socket 被关闭。 */
    (void)PodUdpStreamer_stop(pStreamer);
    ZF_FREE(pStreamer);
}

bool PodUdpStreamer_setTarget(PodUdpStreamer *pStreamer,
    const PodUdpTarget *pTarget)
{
    ZF_ASSERT(pStreamer != (PodUdpStreamer *)0)
    ZF_ASSERT(pTarget != (PodUdpTarget *)0)

#if defined(__linux__)
    if (pTarget->Ip == NULL)
    {
        return false;
    }

    /* 三路流虽然是不同端口，但发往同一地面端 IP。 */
    memset(&pStreamer->AddrVis, 0, sizeof(struct sockaddr_in));
    memset(&pStreamer->AddrNir, 0, sizeof(struct sockaddr_in));
    memset(&pStreamer->AddrTir, 0, sizeof(struct sockaddr_in));

    pStreamer->AddrVis.sin_family = AF_INET;
    pStreamer->AddrVis.sin_port = htons(pTarget->VisPort);
    pStreamer->AddrVis.sin_addr.s_addr = inet_addr(pTarget->Ip);

    pStreamer->AddrNir.sin_family = AF_INET;
    pStreamer->AddrNir.sin_port = htons(pTarget->NirPort);
    pStreamer->AddrNir.sin_addr.s_addr = inet_addr(pTarget->Ip);

    pStreamer->AddrTir.sin_family = AF_INET;
    pStreamer->AddrTir.sin_port = htons(pTarget->TirPort);
    pStreamer->AddrTir.sin_addr.s_addr = inet_addr(pTarget->Ip);

    return true;
#else
    (void)pTarget;
    return false;
#endif
}

int32_t PodUdpStreamer_start(PodUdpStreamer *pStreamer)
{
    ZF_ASSERT(pStreamer != (PodUdpStreamer *)0)

#if defined(__linux__)
    int flags;

    /* 为三路流各自创建 socket，避免发送路径混淆。 */
    pStreamer->SockVis = socket(AF_INET, SOCK_DGRAM, 0);
    pStreamer->SockNir = socket(AF_INET, SOCK_DGRAM, 0);
    pStreamer->SockTir = socket(AF_INET, SOCK_DGRAM, 0);
    if (pStreamer->SockVis < 0 || pStreamer->SockNir < 0 || pStreamer->SockTir < 0)
    {
        CloseSockSafe(&pStreamer->SockVis);
        CloseSockSafe(&pStreamer->SockNir);
        CloseSockSafe(&pStreamer->SockTir);
        return -1;
    }

    /* 使用非阻塞 socket，发送失败由上层策略决定如何处理。 */
    flags = fcntl(pStreamer->SockVis, F_GETFL, 0);
    (void)fcntl(pStreamer->SockVis, F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(pStreamer->SockNir, F_GETFL, 0);
    (void)fcntl(pStreamer->SockNir, F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(pStreamer->SockTir, F_GETFL, 0);
    (void)fcntl(pStreamer->SockTir, F_SETFL, flags | O_NONBLOCK);

    pStreamer->IsStarted = 1u;
    return 0;
#else
    return -2;
#endif
}

int32_t PodUdpStreamer_stop(PodUdpStreamer *pStreamer)
{
    ZF_ASSERT(pStreamer != (PodUdpStreamer *)0)

#if defined(__linux__)
    CloseSockSafe(&pStreamer->SockVis);
    CloseSockSafe(&pStreamer->SockNir);
    CloseSockSafe(&pStreamer->SockTir);
#endif

    pStreamer->IsStarted = 0u;
    return 0;
}

int32_t PodUdpStreamer_sendFrame(PodUdpStreamer *pStreamer,
    const PodFrameBlock *pBlock)
{
    uint16_t max_payload;
    uint16_t packet_count;
    uint16_t i;

    ZF_ASSERT(pStreamer != (PodUdpStreamer *)0)
    ZF_ASSERT(pBlock != (PodFrameBlock *)0)

    if (!pStreamer->IsStarted)
    {
        return -1;
    }

    max_payload = GetMaxPayload(pStreamer->Mtu);
    if (max_payload == 0u)
    {
        return -2;
    }

    /* 以 MTU 为边界做分片，保证单 UDP 包不过大。 */
    packet_count = (uint16_t)((pBlock->Length + max_payload - 1u) / max_payload);
    if (packet_count == 0u)
    {
        packet_count = 1u;
    }

    for (i = 0; i < packet_count; i++)
    {
        uint8_t packet_buf[1600];
        PodUdpHeader hdr;
        uint16_t offset = (uint16_t)(i * max_payload);
        uint16_t bytes_left;
        uint16_t payload_len;
#if defined(__linux__)
        struct sockaddr_in *pAddr;
        int sock;
        int sent;
#endif

        /* 每个分片只从原始帧里截取本片负责的那一段。 */
        if (offset >= pBlock->Length)
        {
            payload_len = 0u;
        }
        else
        {
            bytes_left = (uint16_t)(pBlock->Length - offset);
            payload_len = bytes_left > max_payload ? max_payload : bytes_left;
        }

        hdr.Magic = POD_UDP_HEADER_MAGIC;
        hdr.Version = 1u;
        hdr.StreamId = pBlock->StreamId;
        hdr.FrameId = pBlock->FrameId;
        hdr.PacketId = i;
        hdr.PacketCount = packet_count;
        hdr.TimestampUs = pBlock->TimestampUs;
        hdr.Flags = 0u;
        if (i == 0u)
        {
            /* 首包标记。 */
            hdr.Flags |= 0x04u;
        }
        if (i == (uint16_t)(packet_count - 1u))
        {
            /* 尾包标记。 */
            hdr.Flags |= 0x08u;
        }
        hdr.PayloadLen = payload_len;

        if (sizeof(PodUdpHeader) + payload_len > sizeof(packet_buf))
        {
            NotifyDrop(pStreamer, pBlock->StreamId, pBlock->FrameId);
            return -3;
        }

        /* 先写协议头，再拼接本片的有效负载。 */
        memcpy(packet_buf, &hdr, sizeof(PodUdpHeader));
        if (payload_len > 0u)
        {
            memcpy(packet_buf + sizeof(PodUdpHeader),
                pBlock->pData + offset, payload_len);
        }

#if defined(__linux__)
        sock = GetSockByStream(pStreamer, pBlock->StreamId, &pAddr);
        sent = (int)sendto(sock, packet_buf, sizeof(PodUdpHeader) + payload_len,
            0, (const struct sockaddr *)pAddr, sizeof(struct sockaddr_in));
        if (sent < 0)
        {
            /* UDP 不重传，这里仅统计并上报给外部策略层。 */
            NotifyDrop(pStreamer, pBlock->StreamId, pBlock->FrameId);
        }
#else
        NotifyDrop(pStreamer, pBlock->StreamId, pBlock->FrameId);
#endif
    }

    return 0;
}

uint32_t PodUdpStreamer_packetDropCount(PodUdpStreamer *pStreamer)
{
    ZF_ASSERT(pStreamer != (PodUdpStreamer *)0)
    return pStreamer->PacketDropCount;
}
