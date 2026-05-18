#ifndef __POD_PERF_QUEUE_H__
#define __POD_PERF_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

/*
 * 无锁单生产者单消费者指针队列。
 * 容量必须是 2 的幂，便于用掩码快速回绕索引。
 */
typedef struct _PodSpscQueue
{
    void **pSlots;
    uint32_t Capacity;
    uint32_t Mask;
    volatile uint32_t ReadIndex;
    volatile uint32_t WriteIndex;
    bool IsExternSlots;
    volatile uint32_t DropCount;
} PodSpscQueue;

bool PodSpscQueue_create(PodSpscQueue **ppQueue, uint32_t capacity_pow2);
bool PodSpscQueue_init(PodSpscQueue *pQueue, void **pExternSlots,
    uint32_t capacity_pow2);
void PodSpscQueue_dispose(PodSpscQueue *pQueue);

bool PodSpscQueue_push(PodSpscQueue *pQueue, void *pItem);
bool PodSpscQueue_pop(PodSpscQueue *pQueue, void **ppItem);

uint32_t PodSpscQueue_count(PodSpscQueue *pQueue);
uint32_t PodSpscQueue_dropCount(PodSpscQueue *pQueue);
bool PodSpscQueue_isFull(PodSpscQueue *pQueue);
bool PodSpscQueue_isEmpty(PodSpscQueue *pQueue);

#ifdef __cplusplus
}
#endif

#endif /* __POD_PERF_QUEUE_H__ */
