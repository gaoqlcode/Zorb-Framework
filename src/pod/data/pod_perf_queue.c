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
    /* 环形队列用掩码回绕时，容量必须是 2 的幂。 */
    return (v > 1u) && ((v & (v - 1u)) == 0u);
}

static void PodMemoryBarrier(void)
{
#if defined(__GNUC__) || defined(__clang__)
    /* 保证槽写入和索引更新对另一侧观察顺序一致。 */
    __sync_synchronize();
#else
    /* 未知编译器下的保守兜底：保持为空实现 */
#endif
}

bool PodSpscQueue_create(PodSpscQueue **ppQueue, uint32_t capacity_pow2)
{
    PodSpscQueue *pQueue;

    ZF_ASSERT(ppQueue != (PodSpscQueue **)0)

    /* 非 2 的幂会破坏 Mask 回绕逻辑。 */
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

    /* 槽数组只保存指针，因此创建和移动成本都很低。 */
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

    /* 允许队列对象和槽数组分离，便于放到静态内存或共享内存里。 */
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

    /* 写索引和读索引之差达到容量时，说明队列已满。 */
    if ((w - r) >= pQueue->Capacity)
    {
        pQueue->DropCount++;
        return false;
    }

    /* 用掩码而不是取模，减少回绕成本。 */
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

    /* 写读索引相等表示当前没有任何可消费元素。 */
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
    /* SPSC 模型下，元素数就是写索引减读索引。 */
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
