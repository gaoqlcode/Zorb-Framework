/**
  *****************************************************************************
  * @file    zf_event.c
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
 * @brief   事件与事件处理器实现
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
    *    Modification:建立文件
  *
  *****************************************************************************
  */

#include "zf_event.h"
#include "zf_assert.h"
#include "zf_debug.h"
#include "zf_malloc.h"
#include "zf_critical.h"

/******************************************************************************
 * 功能  创建事件
 * 说明  创建后默认没有参数、没有处理函数，调用方再补充内容。
******************************************************************************/
bool Event_create(Event **ppEvent)
{
    Event *pEvent;
    
    ZF_ASSERT(ppEvent != (Event **)0)
    
    /* 为事件对象分配内存。 */
    pEvent = ZF_MALLOC(sizeof(Event));
    if (pEvent == NULL)
    {
        ZF_DEBUG(LOG_E, "malloc event space error\r\n");
        return false;
    }
    
    /* 初始化默认成员。 */
    pEvent->Priority = EVENT_LOWEST_PRIORITY;
    pEvent->EventProcess = NULL;
    pEvent->pArgList = NULL;
    
    /* 绑定事件对象的方法表。 */
    pEvent->AddArg = Event_addArg;
    pEvent->Dispose = Event_Dispose;
    
    /* ��� */
    *ppEvent = pEvent;
    
    return true;
}

/******************************************************************************
 * 功能  给事件追加参数
 * 说明  这里做深拷贝，因此调用方传入的临时变量在函数返回后也安全。
******************************************************************************/
bool Event_addArg(Event * const pEvent, void *pArg, uint32_t size)
{
    ListNode *pNode;
    void *pData;
    
    ZF_ASSERT(pEvent != (Event *)0)
    ZF_ASSERT(pArg != (void *)0)
    ZF_ASSERT(size > 0)
    
    /* 首次追加参数时再创建参数链表。 */
    if (pEvent->pArgList == NULL)
    {
        List_create(&pEvent->pArgList);
        
        if (pEvent->pArgList == NULL)
        {
            ZF_DEBUG(LOG_E, "malloc event arg list space error\r\n");
            
            return false;
        }
    }
    
    /* 为本次参数创建一个独立节点。 */
    if (!List_mallocNode(&pNode, &pData, size))
    {
        ZF_DEBUG(LOG_E, "malloc event arg node space error\r\n");
        
        return false;
    }
    
    /* 深拷贝参数内容。 */
    ZF_MEMCPY(pData, pArg, size);
    
    /* 把新参数追加到事件参数链表尾部。 */
    if (!pEvent->pArgList->Add(pEvent->pArgList, pNode))
    {
        ZF_DEBUG(LOG_E, "add event arg node into event arg list error\r\n");
        
        return false;
    }
    
    return true;
}

/******************************************************************************
 * 功能  销毁事件
 * 说明  事件参数链表也会一并释放。
******************************************************************************/
bool Event_Dispose(Event * const pEvent)
{
    ZF_ASSERT(pEvent != (Event *)0)
    
    /* 释放事件内部参数链表。 */
    if (pEvent->pArgList != NULL)
    {
        pEvent->pArgList->Dispose(pEvent->pArgList);
    }
    
    ZF_FREE(pEvent);
    
    return true;
}

/******************************************************************************
 * 功能  创建事件处理器
 * 说明  处理器只管理队列，不直接决定在哪个线程或任务上下文执行。
******************************************************************************/
bool EventHandler_create(EventHandler **ppEventHandler)
{
    EventHandler *pEventHandler;
    
    ZF_ASSERT(ppEventHandler != (EventHandler **)0)
    
    /* 为事件处理器对象分配内存。 */
    pEventHandler = ZF_MALLOC(sizeof(EventHandler));
    if (pEventHandler == NULL)
    {
        ZF_DEBUG(LOG_E, "malloc event handler space error\r\n");
        return false;
    }
    
    /* 初始化为空队列且默认允许运行。 */
    pEventHandler->pEventList = NULL;
    pEventHandler->IsRunning = true;
    
    /* 绑定事件处理器的方法表。 */
    pEventHandler->GetEventCount = EventHandler_getEventCount;
    pEventHandler->Add = EventHandler_add;
    pEventHandler->Delete = EventHandler_delete;
    pEventHandler->Clear = EventHandler_clear;
    pEventHandler->Dispose = EventHandler_dispose;
    pEventHandler->Execute = EventHandler_execute;
    
    /* ��� */
    *ppEventHandler = pEventHandler;
    
    return true;
}

/******************************************************************************
 * 功能  获取事件数
******************************************************************************/
uint32_t EventHandler_getEventCount(EventHandler * const pEventHandler)
{
    ZF_ASSERT(pEventHandler != (EventHandler *)0)
    
    if (pEventHandler->pEventList == NULL)
    {
        return 0;
    }
    else
    {
        return pEventHandler->pEventList->Count;
    }
}

/******************************************************************************
 * 功能  添加事件
 * 说明  事件按优先级插入，优先级越高越靠前。
******************************************************************************/
bool EventHandler_add(EventHandler * const pEventHandler, Event *pEvent)
{
    /* 保存并恢复中断上下文的局部变量。 */
    ZF_SR_VAL();
    
    ListNode *pNode;
    uint32_t i;
    uint32_t index; /* 新事件应插入的位置。 */
    
    ZF_ASSERT(pEventHandler != (EventHandler *)0)
    ZF_ASSERT(pEvent != (Event *)0)
    
    /* 首次投递事件时再初始化事件链表。 */
    if (pEventHandler->pEventList == NULL)
    {
        List_create(&pEventHandler->pEventList);
        
        if (pEventHandler->pEventList == NULL)
        {
            ZF_DEBUG(LOG_E, "malloc event list space error\r\n");
            
            return false;
        }
    }
    
    /* 为事件本身创建一个链表节点。 */
    if (!List_mallocNode(&pNode, NULL, 0))
    {
        ZF_DEBUG(LOG_E, "malloc event node space error\r\n");
        
        return false;
    }
    
    pNode->pData = (void *)pEvent;
    pNode->Size = sizeof(Event);
    
    /* 找到应插入的位置，保证链表按优先级升序排列。 */
    index = pEventHandler->pEventList->Count;
    
    for (i = 0; i < pEventHandler->pEventList->Count; i++)
    {
        if (pEvent->Priority < ((Event *)pEventHandler->pEventList
            ->GetElementDataAt(pEventHandler->pEventList, i))->Priority)
        {
            index = i;
            break;
        }
    }
    
    /* 进入临界区，安全地修改事件队列。 */
    ZF_CRITICAL_ENTER();
    
    /* ���ӵ��¼��б� */
    if (!pEventHandler->pEventList
        ->AddElementAt(pEventHandler->pEventList, index, pNode))
    {
        ZF_DEBUG(LOG_E, "add event node into event list error\r\n");
        
        /* 出错时先退出临界区。 */
        ZF_CRITICAL_EXIT();
        
        return false;
    }
    
    /* 退出临界区。 */
    ZF_CRITICAL_EXIT();
    
    return true;
}

/******************************************************************************
 * 功能  删除事件
 * 说明  从队列移除后，还会释放事件本身。
******************************************************************************/
bool EventHandler_delete(EventHandler * const pEventHandler, Event *pEvent)
{
    /* 保存并恢复中断上下文的局部变量。 */
    ZF_SR_VAL();
    
    ListNode *pNode;
    
    ZF_ASSERT(pEventHandler != (EventHandler *)0)
    ZF_ASSERT(pEvent != (Event *)0)
    
    if (pEventHandler->pEventList == NULL ||
        pEventHandler->pEventList->Count == 0)
    {
        return false;
    }
    
    /* 进入临界区，安全地删除目标事件。 */
    ZF_CRITICAL_ENTER();
    
    /* �Ƴ��¼� */
    while(pEventHandler->pEventList
        ->GetElementByData(pEventHandler->pEventList, pEvent, &pNode))
    {
        if (pNode == NULL)
        {
            break;
        }
        
        if (!pEventHandler->pEventList->Delete(pEventHandler->pEventList, pNode))
        {
            ZF_DEBUG(LOG_E, "delete event node from list error\r\n");
            
            break;
        }
    }
    
    /* 退出临界区。 */
    ZF_CRITICAL_EXIT();
    
    pEvent->Dispose(pEvent);
    
    return true;
}

/******************************************************************************
 * 功能  清空事件列表
 * 说明  会逐个释放事件对象，然后再清空链表节点。
******************************************************************************/
bool EventHandler_clear(EventHandler * const pEventHandler)
{
    /* 保存并恢复中断上下文的局部变量。 */
    ZF_SR_VAL();
    
    /* ���ؽ�� */
    bool res = true;
    
    uint32_t i;
    Event *pEvent;
    
    ZF_ASSERT(pEventHandler != (EventHandler *)0)
    
    if (pEventHandler->pEventList == NULL ||
        pEventHandler->pEventList->Count == 0)
    {
        return true;
    }
    
    /* 进入临界区，避免清空过程中被其他上下文并发修改。 */
    ZF_CRITICAL_ENTER();
    
    for (i = 0; i < pEventHandler->pEventList->Count; i++)
    {
        /* 先释放事件对象本身。 */
        pEvent = (Event *)pEventHandler->pEventList
            ->GetElementDataAt(pEventHandler->pEventList, i);
        
        pEvent->Dispose(pEvent);
    }
    
    res = pEventHandler->pEventList->Clear(pEventHandler->pEventList);
    
    /* 退出临界区。 */
    ZF_CRITICAL_EXIT();
    
    return res;
}

/******************************************************************************
 * 功能  销毁事件处理器
 * 说明  先清空待处理事件，再释放事件链表容器和处理器自身。
******************************************************************************/
bool EventHandler_dispose(EventHandler * const pEventHandler)
{
    /* 保存并恢复中断上下文的局部变量。 */
    ZF_SR_VAL();
    
    ZF_ASSERT(pEventHandler != (EventHandler *)0)
    
    /* 临界区内处理收尾，避免队列被并发访问。 */
    ZF_CRITICAL_ENTER();
    
    /* 清空所有待处理事件。 */
    EventHandler_clear(pEventHandler);
    
    pEventHandler->pEventList->Dispose(pEventHandler->pEventList);
    
    pEventHandler->pEventList = NULL;
    
    /* 退出临界区。 */
    ZF_CRITICAL_EXIT();
    
    ZF_FREE(pEventHandler);
    
    return true;
}

/******************************************************************************
 * 功能  执行一个事件
 * 说明  每次只取队头一个事件执行，执行完立即删除。
******************************************************************************/
void EventHandler_execute(EventHandler * const pEventHandler)
{
     /* 保存并恢复中断上下文的局部变量。 */
    ZF_SR_VAL();
    
    Event *pEvent;
    
    ZF_ASSERT(pEventHandler != (EventHandler *)0)
    
    /* 处理器被暂停时不执行任何事件。 */
    if (!pEventHandler->IsRunning)
    {
        return;
    }
    
    /* 没有待处理事件时直接返回。 */
    if (pEventHandler->pEventList == NULL 
        || pEventHandler->pEventList->Count == 0)
    {
        return;
    }
    
    /* 临界区里只做取队头动作，避免执行用户回调时长期关中断。 */
    ZF_CRITICAL_ENTER();
    
    pEvent = (Event *)pEventHandler->pEventList
        ->GetElementDataAt(pEventHandler->pEventList, 0);
    
    /* 退出临界区，再执行真实回调。 */
    ZF_CRITICAL_EXIT();
    
    if (pEvent == NULL || pEvent->EventProcess == NULL)
    {
        ZF_DEBUG(LOG_E, "event handler execute error:event is null\r\n");
        while(1);
    }
    
    /* 先执行事件逻辑，再把该事件从队列中删除。 */
    pEvent->EventProcess(pEvent->pArgList);
    
    EventHandler_delete(pEventHandler, pEvent);
}

/******************************** 文件结束 ********************************/

