#ifndef __POD_FRAME_POOL_H__
#define __POD_FRAME_POOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

typedef struct _PodFrameBlock
{
    /* 指向真实帧数据缓冲区。 */
    uint8_t *pData;
    /* 当前块最多能容纳多少字节。 */
    uint32_t Capacity;
    /* 当前已经写入的有效字节数。 */
    uint32_t Length;
    /* 单调时钟时间戳，通常由采集侧填写。 */
    uint64_t TimestampUs;
    /* 帧序号，用于判断乱序、丢帧和关键帧节奏。 */
    uint32_t FrameId;
    /* 所属视频流编号。 */
    uint8_t StreamId;
} PodFrameBlock;

typedef struct _PodFramePool PodFramePool;

/*
 * 创建一个帧池：
 * - free 队列保存“可写”的空闲块。
 * - ready 队列保存“已写好、等待消费”的块。
 * - block_count_pow2 必须是 2 的幂，便于底层环形队列高效取模。
 * - block_size 建议按 cache line 或 DMA 对齐要求设置。
 */
bool PodFramePool_create(PodFramePool **ppPool, uint32_t block_count_pow2,
    uint32_t block_size);
void PodFramePool_dispose(PodFramePool *pPool);

/* 从 free 队列取一个空闲帧块，通常由采集线程调用。 */
PodFrameBlock *PodFramePool_acquireFree(PodFramePool *pPool);
/* 将消费完的帧块归还 free 队列。 */
bool PodFramePool_releaseFree(PodFramePool *pPool, PodFrameBlock *pBlock);

/* 将填充完成的帧块压入 ready 队列。 */
bool PodFramePool_pushReady(PodFramePool *pPool, PodFrameBlock *pBlock);
/* 从 ready 队列中取出下一帧。 */
PodFrameBlock *PodFramePool_popReady(PodFramePool *pPool);

/* 下面三个统计接口便于观察池容量是否合理。 */
uint32_t PodFramePool_freeCount(PodFramePool *pPool);
uint32_t PodFramePool_readyCount(PodFramePool *pPool);
uint32_t PodFramePool_dropCount(PodFramePool *pPool);

#ifdef __cplusplus
}
#endif

#endif /* __POD_FRAME_POOL_H__ */
