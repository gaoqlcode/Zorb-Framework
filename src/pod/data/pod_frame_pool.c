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
    /* 连续的块描述符数组。 */
    PodFrameBlock *pBlocks;
    /* 实际承载帧数据的原始大块内存。 */
    uint8_t *pRaw;
    uint32_t BlockCount;
    uint32_t BlockSize;
    /* free 队列给生产者用，ready 队列给消费者用。 */
    PodSpscQueue *pFreeQueue;
    PodSpscQueue *pReadyQueue;
} PodFramePool;

static bool FramePool_fillFreeQueue(PodFramePool *pPool)
{
    uint32_t i;

    /* 初始化时把全部帧块都放进 free 队列，代表“全部可写”。 */
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

    /* 参数检查里最关键的是 block_count 必须为 2 的幂。 */
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

    /*
     * 这里采用“两段式内存”：
     * 1. pBlocks 保存元数据。
     * 2. pRaw 保存真实字节流。
     * 这样既方便按块管理，也能保持数据区域连续。
     */
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

    /*
     * free/ready 都是单生产者单消费者队列。
     * 这种结构很适合“采集线程写、编码/发送线程读”的流水线。
     */
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

    /* 为每个块建立元数据与 pRaw 中实际数据区的映射。 */
    for (i = 0; i < block_count_pow2; i++)
    {
        pPool->pBlocks[i].pData = pPool->pRaw + i * block_size;
        pPool->pBlocks[i].Capacity = block_size;
        pPool->pBlocks[i].Length = 0u;
        pPool->pBlocks[i].TimestampUs = 0u;
        pPool->pBlocks[i].FrameId = 0u;
        pPool->pBlocks[i].StreamId = 0u;
    }

    /* 创建完成后，全部块都应处于空闲可写状态。 */
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
        /* 每次重新分配给生产者前，先清掉旧长度。 */
        pBlock->Length = 0u;
        return pBlock;
    }

    return NULL;
}

bool PodFramePool_releaseFree(PodFramePool *pPool, PodFrameBlock *pBlock)
{
    ZF_ASSERT(pPool != (PodFramePool *)0)
    ZF_ASSERT(pBlock != (PodFrameBlock *)0)

    /* 回收到 free 队列时只清空长度，其他元数据由下次写入者覆盖。 */
    pBlock->Length = 0u;
    return PodSpscQueue_push(pPool->pFreeQueue, (void *)pBlock);
}

bool PodFramePool_pushReady(PodFramePool *pPool, PodFrameBlock *pBlock)
{
    ZF_ASSERT(pPool != (PodFramePool *)0)
    ZF_ASSERT(pBlock != (PodFrameBlock *)0)

    if (pBlock->Length > pBlock->Capacity)
    {
        /* 冗余保险：避免越界长度被继续向下游传播。 */
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
