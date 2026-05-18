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

static bool IsPowerOfTwo(uint32_t v)
{
    return (v > 1u) && ((v & (v - 1u)) == 0u);
}

static void PodMemoryBarrier(void)
{
#if defined(__GNUC__) || defined(__clang__)
    __sync_synchronize();
#else
    /* 未知编译器下的保守兜底：保持为空实现 */
#endif
}

bool PodSpscQueue_create(PodSpscQueue **ppQueue, uint32_t capacity_pow2)
{
    PodSpscQueue *pQueue;

    ZF_ASSERT(ppQueue != (PodSpscQueue **)0)

    if (!IsPowerOfTwo(capacity_pow2))
    {
        *ppQueue = NULL;
        return false;
    }

    pQueue = (PodSpscQueue *)ZF_MALLOC(sizeof(PodSpscQueue));
    if (pQueue == NULL)
    {
        *ppQueue = NULL;
        return false;
    }

    pQueue->pSlots = (void **)ZF_MALLOC(sizeof(void *) * capacity_pow2);
    if (pQueue->pSlots == NULL)
    {
        ZF_FREE(pQueue);
        *ppQueue = NULL;
        return false;
    }

    pQueue->Capacity = capacity_pow2;
    pQueue->Mask = capacity_pow2 - 1u;
    pQueue->ReadIndex = 0u;
    pQueue->WriteIndex = 0u;
    pQueue->IsExternSlots = false;
    pQueue->DropCount = 0u;

    *ppQueue = pQueue;
    return true;
}

bool PodSpscQueue_init(PodSpscQueue *pQueue, void **pExternSlots,
    uint32_t capacity_pow2)
{
    ZF_ASSERT(pQueue != (PodSpscQueue *)0)

    if (!IsPowerOfTwo(capacity_pow2) || pExternSlots == NULL)
    {
        return false;
    }

    pQueue->pSlots = pExternSlots;
    pQueue->Capacity = capacity_pow2;
    pQueue->Mask = capacity_pow2 - 1u;
    pQueue->ReadIndex = 0u;
    pQueue->WriteIndex = 0u;
    pQueue->IsExternSlots = true;
    pQueue->DropCount = 0u;

    return true;
}

void PodSpscQueue_dispose(PodSpscQueue *pQueue)
{
    if (pQueue == NULL)
    {
        return;
    }

    if (!pQueue->IsExternSlots && pQueue->pSlots != NULL)
    {
        ZF_FREE(pQueue->pSlots);
    }

    ZF_FREE(pQueue);
}

bool PodSpscQueue_push(PodSpscQueue *pQueue, void *pItem)
{
    uint32_t w;
    uint32_t r;

    ZF_ASSERT(pQueue != (PodSpscQueue *)0)

    w = pQueue->WriteIndex;
    r = pQueue->ReadIndex;

    if ((w - r) >= pQueue->Capacity)
    {
        pQueue->DropCount++;
        return false;
    }

    pQueue->pSlots[w & pQueue->Mask] = pItem;
    PodMemoryBarrier();
    pQueue->WriteIndex = w + 1u;

    return true;
}

bool PodSpscQueue_pop(PodSpscQueue *pQueue, void **ppItem)
{
    uint32_t r;
    uint32_t w;

    ZF_ASSERT(pQueue != (PodSpscQueue *)0)
    ZF_ASSERT(ppItem != (void **)0)

    r = pQueue->ReadIndex;
    w = pQueue->WriteIndex;

    if (w == r)
    {
        return false;
    }

    *ppItem = pQueue->pSlots[r & pQueue->Mask];
    PodMemoryBarrier();
    pQueue->ReadIndex = r + 1u;

    return true;
}

uint32_t PodSpscQueue_count(PodSpscQueue *pQueue)
{
    ZF_ASSERT(pQueue != (PodSpscQueue *)0)
    return pQueue->WriteIndex - pQueue->ReadIndex;
}

uint32_t PodSpscQueue_dropCount(PodSpscQueue *pQueue)
{
    ZF_ASSERT(pQueue != (PodSpscQueue *)0)
    return pQueue->DropCount;
}

bool PodSpscQueue_isFull(PodSpscQueue *pQueue)
{
    ZF_ASSERT(pQueue != (PodSpscQueue *)0)
    return PodSpscQueue_count(pQueue) >= pQueue->Capacity;
}

bool PodSpscQueue_isEmpty(PodSpscQueue *pQueue)
{
    ZF_ASSERT(pQueue != (PodSpscQueue *)0)
    return PodSpscQueue_count(pQueue) == 0u;
}
