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

typedef struct _PodEncoder
{
    PodEncoderConfig Config;
    uint8_t IsOpened;
} PodEncoder;

bool PodEncoder_create(PodEncoder **ppEncoder, const PodEncoderConfig *pConfig)
{
    PodEncoder *pEncoder;

    ZF_ASSERT(ppEncoder != (PodEncoder **)0)
    ZF_ASSERT(pConfig != (PodEncoderConfig *)0)

    pEncoder = (PodEncoder *)ZF_MALLOC(sizeof(PodEncoder));
    if (pEncoder == NULL)
    {
        *ppEncoder = NULL;
        return false;
    }

    pEncoder->Config = *pConfig;
    pEncoder->IsOpened = 0u;

    *ppEncoder = pEncoder;
    return true;
}

void PodEncoder_dispose(PodEncoder *pEncoder)
{
    if (pEncoder == NULL)
    {
        return;
    }

    ZF_FREE(pEncoder);
}

int32_t PodEncoder_open(PodEncoder *pEncoder)
{
    ZF_ASSERT(pEncoder != (PodEncoder *)0)
    pEncoder->IsOpened = 1u;
    return 0;
}

int32_t PodEncoder_close(PodEncoder *pEncoder)
{
    ZF_ASSERT(pEncoder != (PodEncoder *)0)
    pEncoder->IsOpened = 0u;
    return 0;
}

int32_t PodEncoder_setBitrateKbps(PodEncoder *pEncoder, uint32_t bitrate_kbps)
{
    ZF_ASSERT(pEncoder != (PodEncoder *)0)

    if (bitrate_kbps == 0u)
    {
        return -1;
    }

    pEncoder->Config.BitrateKbps = bitrate_kbps;
    return 0;
}

int32_t PodEncoder_setGop(PodEncoder *pEncoder, uint32_t gop)
{
    ZF_ASSERT(pEncoder != (PodEncoder *)0)

    if (gop == 0u)
    {
        return -1;
    }

    pEncoder->Config.Gop = gop;
    return 0;
}

uint32_t PodEncoder_getBitrateKbps(PodEncoder *pEncoder)
{
    ZF_ASSERT(pEncoder != (PodEncoder *)0)
    return pEncoder->Config.BitrateKbps;
}

uint32_t PodEncoder_getGop(PodEncoder *pEncoder)
{
    ZF_ASSERT(pEncoder != (PodEncoder *)0)
    return pEncoder->Config.Gop;
}

int32_t PodEncoder_encode(PodEncoder *pEncoder, const PodFrameBlock *pIn,
    PodFrameBlock *pOut, uint8_t *pIsKeyFrame)
{
    ZF_ASSERT(pEncoder != (PodEncoder *)0)
    ZF_ASSERT(pIn != (PodFrameBlock *)0)
    ZF_ASSERT(pOut != (PodFrameBlock *)0)
    ZF_ASSERT(pIsKeyFrame != (uint8_t *)0)

    if (!pEncoder->IsOpened)
    {
        return -1;
    }

    if (pOut->Capacity < pIn->Length)
    {
        return -2;
    }

    if (pIn->Length > 0u)
    {
        ZF_MEMCPY(pOut->pData, pIn->pData, pIn->Length);
    }

    pOut->Length = pIn->Length;
    pOut->TimestampUs = pIn->TimestampUs;
    pOut->FrameId = pIn->FrameId;
    pOut->StreamId = pIn->StreamId;

    /* 占位策略：每 GOP 产生一帧关键帧，便于联调与验证流程 */
    if (pEncoder->Config.Gop > 0u
        && (pOut->FrameId % pEncoder->Config.Gop) == 0u)
    {
        *pIsKeyFrame = 1u;
    }
    else
    {
        *pIsKeyFrame = 0u;
    }

    return 0;
}
