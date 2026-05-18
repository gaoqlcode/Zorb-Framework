#if __has_include("pod_frame_pool.h")
#include "pod_frame_pool.h"
#else
#include "../../../inc/pod/data/pod_frame_pool.h"
#endif

#if __has_include("pod_perf_queue.h")
#include "pod_perf_queue.h"
#else
#include "../../../inc/pod/data/pod_perf_queue.h"
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

typedef struct _PodFramePool
{
    PodFrameBlock *pBlocks;
    uint8_t *pRaw;
    uint32_t BlockCount;
    uint32_t BlockSize;
    PodSpscQueue *pFreeQueue;
    PodSpscQueue *pReadyQueue;
} PodFramePool;

static bool FramePool_fillFreeQueue(PodFramePool *pPool)
{
    uint32_t i;

    for (i = 0; i < pPool->BlockCount; i++)
    {
        if (!PodSpscQueue_push(pPool->pFreeQueue, (void *)&pPool->pBlocks[i]))
        {
            return false;
        }
    }

    return true;
}

bool PodFramePool_create(PodFramePool **ppPool, uint32_t block_count_pow2,
    uint32_t block_size)
{
    PodFramePool *pPool;
    uint32_t i;

    ZF_ASSERT(ppPool != (PodFramePool **)0)

    if (block_size == 0u || block_count_pow2 < 2u
        || (block_count_pow2 & (block_count_pow2 - 1u)) != 0u)
    {
        *ppPool = NULL;
        return false;
    }

    pPool = (PodFramePool *)ZF_MALLOC(sizeof(PodFramePool));
    if (pPool == NULL)
    {
        *ppPool = NULL;
        return false;
    }

    pPool->pBlocks = (PodFrameBlock *)ZF_MALLOC(sizeof(PodFrameBlock)
        * block_count_pow2);
    pPool->pRaw = (uint8_t *)ZF_MALLOC(block_count_pow2 * block_size);

    if (pPool->pBlocks == NULL || pPool->pRaw == NULL)
    {
        if (pPool->pBlocks != NULL)
        {
            ZF_FREE(pPool->pBlocks);
        }
        if (pPool->pRaw != NULL)
        {
            ZF_FREE(pPool->pRaw);
        }
        ZF_FREE(pPool);
        *ppPool = NULL;
        return false;
    }

    pPool->BlockCount = block_count_pow2;
    pPool->BlockSize = block_size;
    pPool->pFreeQueue = NULL;
    pPool->pReadyQueue = NULL;

    if (!PodSpscQueue_create(&pPool->pFreeQueue, block_count_pow2)
        || !PodSpscQueue_create(&pPool->pReadyQueue, block_count_pow2))
    {
        if (pPool->pFreeQueue != NULL)
        {
            PodSpscQueue_dispose(pPool->pFreeQueue);
        }
        if (pPool->pReadyQueue != NULL)
        {
            PodSpscQueue_dispose(pPool->pReadyQueue);
        }
        ZF_FREE(pPool->pBlocks);
        ZF_FREE(pPool->pRaw);
        ZF_FREE(pPool);
        *ppPool = NULL;
        return false;
    }

    for (i = 0; i < block_count_pow2; i++)
    {
        pPool->pBlocks[i].pData = pPool->pRaw + i * block_size;
        pPool->pBlocks[i].Capacity = block_size;
        pPool->pBlocks[i].Length = 0u;
        pPool->pBlocks[i].TimestampUs = 0u;
        pPool->pBlocks[i].FrameId = 0u;
        pPool->pBlocks[i].StreamId = 0u;
    }

    if (!FramePool_fillFreeQueue(pPool))
    {
        PodSpscQueue_dispose(pPool->pFreeQueue);
        PodSpscQueue_dispose(pPool->pReadyQueue);
        ZF_FREE(pPool->pBlocks);
        ZF_FREE(pPool->pRaw);
        ZF_FREE(pPool);
        *ppPool = NULL;
        return false;
    }

    *ppPool = pPool;
    return true;
}

void PodFramePool_dispose(PodFramePool *pPool)
{
    if (pPool == NULL)
    {
        return;
    }

    if (pPool->pFreeQueue != NULL)
    {
        PodSpscQueue_dispose(pPool->pFreeQueue);
    }

    if (pPool->pReadyQueue != NULL)
    {
        PodSpscQueue_dispose(pPool->pReadyQueue);
    }

    if (pPool->pBlocks != NULL)
    {
        ZF_FREE(pPool->pBlocks);
    }

    if (pPool->pRaw != NULL)
    {
        ZF_FREE(pPool->pRaw);
    }

    ZF_FREE(pPool);
}

PodFrameBlock *PodFramePool_acquireFree(PodFramePool *pPool)
{
    void *pItem;

    ZF_ASSERT(pPool != (PodFramePool *)0)

    if (PodSpscQueue_pop(pPool->pFreeQueue, &pItem))
    {
        PodFrameBlock *pBlock = (PodFrameBlock *)pItem;
        pBlock->Length = 0u;
        return pBlock;
    }

    return NULL;
}

bool PodFramePool_releaseFree(PodFramePool *pPool, PodFrameBlock *pBlock)
{
    ZF_ASSERT(pPool != (PodFramePool *)0)
    ZF_ASSERT(pBlock != (PodFrameBlock *)0)

    pBlock->Length = 0u;
    return PodSpscQueue_push(pPool->pFreeQueue, (void *)pBlock);
}

bool PodFramePool_pushReady(PodFramePool *pPool, PodFrameBlock *pBlock)
{
    ZF_ASSERT(pPool != (PodFramePool *)0)
    ZF_ASSERT(pBlock != (PodFrameBlock *)0)

    if (pBlock->Length > pBlock->Capacity)
    {
        pBlock->Length = pBlock->Capacity;
    }

    return PodSpscQueue_push(pPool->pReadyQueue, (void *)pBlock);
}

PodFrameBlock *PodFramePool_popReady(PodFramePool *pPool)
{
    void *pItem;

    ZF_ASSERT(pPool != (PodFramePool *)0)

    if (PodSpscQueue_pop(pPool->pReadyQueue, &pItem))
    {
        return (PodFrameBlock *)pItem;
    }

    return NULL;
}

uint32_t PodFramePool_freeCount(PodFramePool *pPool)
{
    ZF_ASSERT(pPool != (PodFramePool *)0)
    return PodSpscQueue_count(pPool->pFreeQueue);
}

uint32_t PodFramePool_readyCount(PodFramePool *pPool)
{
    ZF_ASSERT(pPool != (PodFramePool *)0)
    return PodSpscQueue_count(pPool->pReadyQueue);
}

uint32_t PodFramePool_dropCount(PodFramePool *pPool)
{
    ZF_ASSERT(pPool != (PodFramePool *)0)
    return PodSpscQueue_dropCount(pPool->pReadyQueue);
}
