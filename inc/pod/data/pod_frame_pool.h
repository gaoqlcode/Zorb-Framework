#ifndef __POD_FRAME_POOL_H__
#define __POD_FRAME_POOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

typedef struct _PodFrameBlock
{
    uint8_t *pData;
    uint32_t Capacity;
    uint32_t Length;
    uint64_t TimestampUs;
    uint32_t FrameId;
    uint8_t StreamId;
} PodFrameBlock;

typedef struct _PodFramePool PodFramePool;

/*
 * block_count_pow2 必须是 2 的幂。
 * block_size 建议按 cache line 或 DMA 对齐要求设置。
 */
bool PodFramePool_create(PodFramePool **ppPool, uint32_t block_count_pow2,
    uint32_t block_size);
void PodFramePool_dispose(PodFramePool *pPool);

PodFrameBlock *PodFramePool_acquireFree(PodFramePool *pPool);
bool PodFramePool_releaseFree(PodFramePool *pPool, PodFrameBlock *pBlock);

bool PodFramePool_pushReady(PodFramePool *pPool, PodFrameBlock *pBlock);
PodFrameBlock *PodFramePool_popReady(PodFramePool *pPool);

uint32_t PodFramePool_freeCount(PodFramePool *pPool);
uint32_t PodFramePool_readyCount(PodFramePool *pPool);
uint32_t PodFramePool_dropCount(PodFramePool *pPool);

#ifdef __cplusplus
}
#endif

#endif /* __POD_FRAME_POOL_H__ */
