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
 * 很适合采集线程写、编码线程读这种一进一出的流水线。
 */
typedef struct _PodSpscQueue
{
    /* 指针槽数组，队列里存放的是指针而不是对象拷贝。 */
    void **pSlots;
    uint32_t Capacity;
    uint32_t Mask;
    /* 读写索引分别由消费者/生产者单独推进。 */
    volatile uint32_t ReadIndex;
    volatile uint32_t WriteIndex;
    bool IsExternSlots;
    /* push 失败时累计丢弃次数。 */
    volatile uint32_t DropCount;
} PodSpscQueue;

/* 内部分配槽数组并创建队列。 */
bool PodSpscQueue_create(PodSpscQueue **ppQueue, uint32_t capacity_pow2);
/* 使用外部给定槽数组初始化队列。 */
bool PodSpscQueue_init(PodSpscQueue *pQueue, void **pExternSlots,
    uint32_t capacity_pow2);
void PodSpscQueue_dispose(PodSpscQueue *pQueue);

/* 单生产者写入一个指针。 */
bool PodSpscQueue_push(PodSpscQueue *pQueue, void *pItem);
/* 单消费者弹出一个指针。 */
bool PodSpscQueue_pop(PodSpscQueue *pQueue, void **ppItem);

uint32_t PodSpscQueue_count(PodSpscQueue *pQueue);
uint32_t PodSpscQueue_dropCount(PodSpscQueue *pQueue);
bool PodSpscQueue_isFull(PodSpscQueue *pQueue);
bool PodSpscQueue_isEmpty(PodSpscQueue *pQueue);

#ifdef __cplusplus
}
#endif

#endif /* __POD_PERF_QUEUE_H__ */
