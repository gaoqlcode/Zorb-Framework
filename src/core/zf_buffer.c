/**
  *****************************************************************************
  * @file    zf_buffer.c
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
 * @brief   环形缓冲区实现
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
    *    Modification:建立文件
  *
  *****************************************************************************
  */

#include "zf_buffer.h"
#include "zf_assert.h"
#include "zf_malloc.h"
#include "zf_debug.h"

/******************************************************************************
 * 功能  创建环形缓冲区
 * 说明  Head 指向读位置，Trail 指向写位置，Count 记录当前有效字节数。
******************************************************************************/
bool RB_create(RingBuffer **ppRb, uint32_t size)
{
    RingBuffer *pRb;
    uint8_t *pBuf;
    
    ZF_ASSERT(ppRb != (RingBuffer **)0)
    
    pRb = (RingBuffer *)ZF_MALLOC(sizeof(RingBuffer));
    if (pRb == NULL)
    {
        ZF_DEBUG(LOG_E, "malloc ringbuffer space error\r\n");
        return false;
    }
    
    pRb->Head = (uint32_t)0;
    pRb->Trail = (uint32_t)0;
    pRb->Count = 0;
    
    /* size > 0 时由内部申请缓冲区；否则留给外部后续挂接。 */
    if (size > 0)
    {
        pBuf = (void *)ZF_MALLOC(size);
        if (pBuf == NULL)
        {
            ZF_DEBUG(LOG_E, "malloc ringbuffer buffer space error\r\n");
            return false;
        }
        
        pRb->pBuf = pBuf;
        pRb->Size = size;
        pRb->IsExternBuffer = false;
    }
    else
    {
        pRb->pBuf = NULL;
        pRb->Size = 0;
        pRb->IsExternBuffer = true;
    }
    
    /* 绑定方法表。 */
    pRb->IsFull = RB_isFull;
    pRb->IsEmpty = RB_isEmpty;
    pRb->SaveByte = RB_saveByte;
    pRb->SaveRange = RB_saveRange;
    pRb->GetByte = RB_getByte;
    pRb->GetCount = RB_getCount;
    pRb->ReadBytes = RB_readBytes;
    pRb->DropBytes = RB_dropBytes;
    pRb->Clear = RB_clear;
    pRb->Dispose = RB_dispose;
    
    /* ��� */
    *ppRb = pRb;
    
    return true;
}

/******************************************************************************
 * 功能  判断缓冲区是否已满
******************************************************************************/
bool RB_isFull(RingBuffer * const pRb)
{
    bool res = false;
    
    ZF_ASSERT(pRb != (RingBuffer *)0)
    
    if (pRb->Size == pRb->Count)
    {
        res = true;
    }
    
    return res;
}

/******************************************************************************
 * 功能  判断缓冲区是否为空
******************************************************************************/
bool RB_isEmpty(RingBuffer * const pRb)
{
    bool res = false;
    
    ZF_ASSERT(pRb != (RingBuffer *)0)
    
    if (pRb->Count == 0)
    {
        res = true;
    }
    
    return res;
}

/******************************************************************************
 * 功能  写入一个字节
 * 说明  Trail 写完后回绕，Count 增长到 Size 即视为满。
******************************************************************************/
bool RB_saveByte(RingBuffer * const pRb, uint8_t byte)
{
    bool res = false;
    
    ZF_ASSERT(pRb != (RingBuffer *)0)
    ZF_ASSERT(pRb->pBuf != (uint8_t *)0)
    
    if (!RB_isFull(pRb))
    {
        pRb->pBuf[pRb->Trail++] = byte;
        pRb->Trail %= pRb->Size;
        pRb->Count++;
        
        res = true;
    }
    
    return res;
}

/******************************************************************************
 * 功能  批量写入字节
 * 说明  写到满为止，返回实际写入数量。
******************************************************************************/
uint32_t RB_saveRange(RingBuffer * const pRb, uint8_t *pArray, uint32_t n)
{
    uint32_t res = 0;
    
    if (pArray == 0)
    {
        return res;
    }
    
    ZF_ASSERT(pRb != (RingBuffer *)0)
    ZF_ASSERT(pRb->pBuf != (uint8_t *)0)
    ZF_ASSERT(pArray != (uint8_t *)0)
    
    uint32_t i;
    
    for (i = 0; i < n; i++)
    {
        pRb->pBuf[pRb->Trail++] = *(pArray + i);
        pRb->Trail %= pRb->Size;
        pRb->Count++;
        
        res++;
        
        if (pRb->Count >= pRb->Size)
        {
            break;
        }
    }
    
    return res;
}

/******************************************************************************
 * 功能  取出一个字节
 * 说明  Head 每取出一个字节就前进一次并回绕。
******************************************************************************/
bool RB_getByte(RingBuffer * const pRb, uint8_t *pByte)
{
    bool res = false;
    
    ZF_ASSERT(pRb != (RingBuffer *)0)
    ZF_ASSERT(pRb->pBuf != (uint8_t *)0)
    ZF_ASSERT(pByte != (uint8_t *)0)
    
    if (!RB_isEmpty(pRb))
    {
        *pByte = pRb->pBuf[pRb->Head++];
        pRb->Head %= pRb->Size;
        pRb->Count--;
        
        res = true;
    }
    
    return res;
}

/******************************************************************************
 * 功能  获取当前有效字节数
 * 说明  Count 始终表示缓冲区里还能被读取的字节总数。
******************************************************************************/
uint32_t RB_getCount(RingBuffer * const pRb)
{
    ZF_ASSERT(pRb != (RingBuffer *)0)
    
    return pRb->Count;
}

/******************************************************************************
 * 功能  读取多个字节但不消费
 * 说明  这是“窥视”接口，读完后 Head 和 Count 都不会变化。
******************************************************************************/
uint32_t RB_readBytes(RingBuffer * const pRb, uint8_t *pArray, uint32_t n)
{
    uint32_t len;
    uint32_t i, index;
    
    ZF_ASSERT(pRb != (RingBuffer *)0)
    ZF_ASSERT(pRb->pBuf != (uint8_t *)0)
    ZF_ASSERT(pArray != (uint8_t *)0)
    
    if (!RB_isEmpty(pRb))
    {
        len = RB_getCount(pRb);
        
        if (len > n)
        {
            len = n;
        }
        
        for(i = 0; i < len; i++) 
        {
            index = (pRb->Head + i) % pRb->Size;
            *(pArray + i) = pRb->pBuf[index];
        }
        
        return len;
    }
    
    return 0;
}

/******************************************************************************
 * 功能  丢弃多个字节
 * 说明  适合协议解析时快速跳过已消费内容。
******************************************************************************/
uint32_t RB_dropBytes(RingBuffer * const pRb, uint32_t n)
{
    uint32_t len;
    
    ZF_ASSERT(pRb != (RingBuffer *)0)
    ZF_ASSERT(pRb->pBuf != (uint8_t *)0)
    
    if (!RB_isEmpty(pRb))
    {
        len = RB_getCount(pRb);
        
        if (len > n)
        {
            len = n;
        }
        
        pRb->Head += len;
        pRb->Head %= pRb->Size;
        pRb->Count -= len;
        
        return len;
    }
    
    return 0;
}

/******************************************************************************
 * 功能  清空缓冲区
******************************************************************************/
bool RB_clear(RingBuffer * const pRb)
{
    bool res = false;
    
    ZF_ASSERT(pRb != (RingBuffer *)0)
    
    pRb->Head = (uint32_t)0;
    pRb->Trail = (uint32_t)0;
    pRb->Count = 0;
    
    res = true;
    
    return res;
}

/******************************************************************************
 * 功能  销毁缓冲区
 * 说明  只有内部申请的 pBuf 才会在这里释放。
******************************************************************************/
bool RB_dispose(RingBuffer * const pRb)
{
    ZF_ASSERT(pRb != (RingBuffer *)0)
    
    /* 只有内部申请的缓冲区才由这里释放。 */
    if (!pRb->IsExternBuffer)
    {
        ZF_FREE(pRb->pBuf);
    }
    
    ZF_FREE(pRb);
    
    return true;
}

/******************************** 文件结束 ********************************/

