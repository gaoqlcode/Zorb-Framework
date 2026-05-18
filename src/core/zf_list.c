/**
  *****************************************************************************
  * @file    zf_list.c
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
 * @brief   简单链表实现
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
    *    Modification:建立文件
  *
  *****************************************************************************
  */

#include "zf_list.h"
#include "zf_assert.h"
#include "zf_debug.h"
#include "zf_malloc.h"

/******************************************************************************
 * 功能  创建链表
 * 说明  这里只创建容器本身，节点需要调用方单独创建后再插入。
******************************************************************************/
bool List_create(List **ppList)
{
    List *pList;
    
    ZF_ASSERT(ppList != (List **)0)
    
    /* 为链表对象分配内存。 */
    pList = ZF_MALLOC(sizeof(List));
    if (pList == NULL)
    {
        ZF_DEBUG(LOG_E, "malloc list space error\r\n");
        return false;
    }
    
    /* 初始化空链表。 */
    pList->pRootNode = NULL;
    pList->Count = 0;
    
    /* 绑定面向对象风格的方法表。 */
    pList->Add = List_add;
    pList->Delete = List_delete;
    pList->Remove = List_remove;
    pList->GetElementAt = List_getElementAt;
    pList->GetElementByData = List_getElementByData;
    pList->GetElementDataAt = List_getElementDataAt;
    pList->GetElementIndex = List_getElementIndex;
    pList->AddElementAt = List_addElementAt;
    pList->DeleteElementAt = List_deleteElementAt;
    pList->RemoveElementAt = List_removeElementAt;
    pList->Clear = List_clear;
    pList->Dispose = List_dispose;
    
    /* 返回新建链表对象。 */
    *ppList = pList;
    
    return true;
}

/******************************************************************************
 * 功能  尾插节点
 * 说明  实际复用了 AddElementAt，把 index 设为当前 Count。
******************************************************************************/
bool List_add(List * const pList, ListNode *pNode)
{
    ZF_ASSERT(pList != (List *)0)
    ZF_ASSERT(pNode != (ListNode *)0)
    
    return List_addElementAt(pList, pList->Count, pNode);
}

/******************************************************************************
 * 功能  删除节点
 * 说明  删除不仅会摘链，还会释放节点及其内部数据。
******************************************************************************/
bool List_delete(List * const pList, ListNode *pNode)
{
    /* 返回结果。 */
    bool res = false;
    
    /* 待删节点的前驱节点。 */
    ListNode *pPreviousNode;
    
    ZF_ASSERT(pList != (List *)0)
    ZF_ASSERT(pNode != (ListNode *)0)
    
    /* 根节点删除是最简单的情况。 */
    if (pNode == pList->pRootNode)
    {
        pList->pRootNode = pNode->Next;
        pList->Count--;
        
        /* 删除时一并释放节点。 */
        List_freeNode(pNode);
        
        res = true;
    }
    /* 非根节点需要先找到它的前驱节点。 */
    else
    {
        pPreviousNode = pList->pRootNode;
        while (pPreviousNode->Next != pNode)
        {
            pPreviousNode = pPreviousNode->Next;
        }
        
        /* 找到目标节点后完成摘链和释放。 */
        if(pPreviousNode != NULL || pPreviousNode->Next == pNode)
        {
            pPreviousNode->Next = pNode->Next;
            pList->Count--;
            
            /* 删除时一并释放节点。 */
            List_freeNode(pNode);
            
            res = true;
        }
    }
    
    return res;
}

/******************************************************************************
 * 功能  移除节点
 * 说明  和 Delete 的区别是这里只摘链，不释放节点本身。
 *       适合节点要转移给其他容器继续使用的场景。
******************************************************************************/
bool List_remove(List * const pList, ListNode *pNode)
{
    /* 返回结果。 */
    bool res = false;
    
    /* 待移除节点的前驱节点。 */
    ListNode *pPreviousNode;
    
    ZF_ASSERT(pList != (List *)0)
    ZF_ASSERT(pNode != (ListNode *)0)
    
    /* 根节点移除是最简单的情况。 */
    if (pNode == pList->pRootNode)
    {
        pList->pRootNode = pNode->Next;
        pList->Count--;
        
        res = true;
    }
    /* 非根节点需要先找到它的前驱节点。 */
    else
    {
        pPreviousNode = pList->pRootNode;
        while (pPreviousNode->Next != pNode)
        {
            pPreviousNode = pPreviousNode->Next;
        }
        
        /* 找到目标节点后完成摘链。 */
        if(pPreviousNode != NULL || pPreviousNode->Next == pNode)
        {
            pPreviousNode->Next = pNode->Next;
            pList->Count--;
            
            res = true;
        }
    }
    
    return res;
}

/******************************************************************************
 * 功能  按索引取节点
 * 说明  单向链表只能从头顺序走到目标位置，因此时间复杂度是 O(n)。
******************************************************************************/
bool List_getElementAt(List * const pList, uint32_t index,
    ListNode **ppNode)
{
    /* 返回结果。 */
    bool res = true;
    
    ListNode *pNode;
    uint32_t i;
    
    ZF_ASSERT(pList != (List *)0)
    ZF_ASSERT(index < pList->Count)
    
    pNode = pList->pRootNode;
    for (i = 0; i < index; i++)
    {
        if (pNode->Next == NULL)
        {
            res = false;
            break;
        }
        pNode = pNode->Next;
    }
    
    if(res == true)
    {
        *ppNode = pNode;
    }
    else
    {
        *ppNode = NULL;
    }
    
    return res;
}

/******************************************************************************
 * 功能  按数据指针查找节点
 * 说明  这里比较的是指针地址，不是数据内容。
******************************************************************************/
bool List_getElementByData(List * const pList, void *pdata, ListNode **ppNode)
{
    ListNode *pNode;
    
    ZF_ASSERT(pList != (List *)0)
    ZF_ASSERT(pdata != (void *)0)
    
    pNode = pList->pRootNode;
    while (pNode != NULL)
    {
        if (pNode->pData == pdata)
        {
            /* 返回命中的节点。 */
            *ppNode = pNode;
            
            return true;
        }
        
        pNode = pNode->Next;
    }
    
    /* 没找到则返回 NULL。 */
    *ppNode = NULL;
    
    return false;
}

/******************************************************************************
 * 功能  按索引取节点数据区指针
 * 说明  这是链表最常用的便捷接口之一。
******************************************************************************/
void *List_getElementDataAt(List * const pList, uint32_t index)
{
    ListNode *pNode;
    
    List_getElementAt(pList, index, &pNode);
    
    if (pNode != NULL)
    {
        return (void*)pNode->pData;
    }
    
    return NULL;
}

/******************************************************************************
 * 功能  获取节点索引
 * 说明  从头遍历直到命中目标节点。
******************************************************************************/
bool List_getElementIndex(List * const pList, ListNode *pNode,
    uint32_t *pIndex)
{
    /* 返回结果。 */
    bool res = true;
    
    uint32_t index = 0;
    
    ListNode *pFindNode;
    
    ZF_ASSERT(pList != (List *)0)
    ZF_ASSERT(pNode != (ListNode *)0)
    
    pFindNode = pList->pRootNode;
    while (pFindNode != pNode && pFindNode != NULL)
    {
        pFindNode = pFindNode->Next;
        index++;
    }
    
    /* 没找到目标节点。 */
    if (pFindNode != pNode)
    {
        res = false;
        index = 0;
    }
    
    /* 输出索引值。 */
    *pIndex = index;
    
    return res;
}

/******************************************************************************
 * 功能  按索引插入节点
 * 说明  如果索引超过当前长度，会自动退化为尾插。
******************************************************************************/
bool List_addElementAt(List * const pList, uint32_t index,
    ListNode *pNode)
{
    /* 返回结果。 */
    bool res = false;
    
    /* 目标插入位置前一个节点。 */
    ListNode *pPreviousNode;
    
    ZF_ASSERT(pList != (List *)0)
    ZF_ASSERT(pNode != (ListNode *)0)
    
    if (index > pList->Count)
    {
        index = pList->Count;
    }
    
    /* 插到头部。 */
    if (index == 0)
    {
        /* 完成头插。 */
        pNode->Next = pList->pRootNode;
        pList->pRootNode = pNode;
        
        /* 节点数加一。 */
        pList->Count++;
        
        res = true;
    }
    else
    {
        /* 找到前驱节点后完成插入。 */
        if (List_getElementAt(pList, index - 1, &pPreviousNode))
        {
            /* 完成中间或尾部插入。 */
            pNode->Next = pPreviousNode->Next;
            pPreviousNode->Next = pNode;
            
            /* 节点数加一。 */
            pList->Count++;
            
            res = true;
        }
    }
    
    return res;
}

/******************************************************************************
 * 功能  按索引删除节点
 * 说明  先通过索引找到节点，再复用 List_delete 完成真正删除与释放。
******************************************************************************/
bool List_deleteElementAt(List * const pList, uint32_t index)
{
    /* Ҫɾ���Ľڵ� */
    ListNode *pDeleteNode = NULL;
    
    ZF_ASSERT(pList != (List *)0)
    ZF_ASSERT(index < pList->Count)
    
    if (!List_getElementAt(pList, index, &pDeleteNode))
    {
        return false;
    }
    
    return List_delete(pList, pDeleteNode);
}

/******************************************************************************
 * 功能  按索引移除节点
 * 说明  与 DeleteElementAt 的区别是这里只摘链，不释放节点。
******************************************************************************/
bool List_removeElementAt(List * const pList, uint32_t index)
{
     /* 要移除的节点。 */
    ListNode *pDeleteNode = NULL;
    
    ZF_ASSERT(pList != (List *)0)
    ZF_ASSERT(index < pList->Count)
    
    if (!List_getElementAt(pList, index, &pDeleteNode))
    {
        return false;
    }
    
    return List_remove(pList, pDeleteNode);
}

/******************************************************************************
 * 功能  清空链表
 * 说明  通过不断删除头节点直到 Count 归零。
******************************************************************************/
bool List_clear(List * const pList)
{
    /* 返回结果。 */
    bool res = true;
    
    ZF_ASSERT(pList != (List *)0)
    
    while (pList->Count)
    {
        res &= List_deleteElementAt(pList, 0);
    }
    
    return res;
}

/******************************************************************************
 * 功能  销毁链表
 * 说明  先清空全部节点，再释放链表容器本身。
******************************************************************************/
bool List_dispose(List * const pList)
{
    /* ���ؽ�� */
    bool res = true;
    
    ZF_ASSERT(pList != (List *)0)
    
    res = List_clear(pList);
    
    ZF_FREE(pList);
    
    return res;
}

/******************************************************************************
 * 功能  创建节点
 * 说明  size > 0 时会同时内部申请数据区；size == 0 时表示后续由外部挂接数据。
******************************************************************************/
bool List_mallocNode(ListNode **ppNode, void **ppData,
    uint32_t size)
{
    ListNode *pNode;
    void *pData;
    
    ZF_ASSERT(ppNode != (ListNode **)0)
    
    pNode = (ListNode *)ZF_MALLOC(sizeof(ListNode));
    if (pNode == NULL)
    {
        ZF_DEBUG(LOG_E, "malloc list node space error\r\n");
        return false;
    }
    
    pNode->Next = NULL;
    
    if (size > 0)
    {
        ZF_ASSERT(ppData != (void **)0)
        
        pData = (void *)ZF_MALLOC(size);
        if (pData == NULL)
        {
            ZF_DEBUG(LOG_E, "malloc list node data space error\r\n");
            return false;
        }
        
        pNode->pData = pData;
        pNode->Size = size;
        pNode->IsExternData = false;
        
        /* 返回新申请的数据区地址。 */
        *ppData = pData;
    }
    else
    {
        pNode->pData = NULL;
        pNode->Size = 0;
        pNode->IsExternData = true;
    }
    
    /* 返回新节点地址。 */
    *ppNode = pNode;
    
    return true;
}

/******************************************************************************
 * 功能  释放节点
 * 说明  只有内部申请的数据区才会被一并释放。
******************************************************************************/
bool List_freeNode(ListNode *pNode)
{
    ZF_ASSERT(pNode != (ListNode *)0)
    
    /* 外部挂接的数据不由这里释放。 */
    if (!pNode->IsExternData)
    {
        ZF_FREE(pNode->pData);
    }
    
    ZF_FREE(pNode);
    
    return true;
}

/******************************** 文件结束 ********************************/

