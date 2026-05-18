/**
  *****************************************************************************
  * @file    zf_fsm.c
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
 * @brief   有限状态机实现
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
    *    Modification:建立文件
  *
  *****************************************************************************
  */

#include "zf_fsm.h"
#include "zf_assert.h"
#include "zf_debug.h"
#include "zf_malloc.h"

/******************************************************************************
 * 功能  创建状态机
 * 说明  创建后只是一个空壳，还需要设置初始状态并显式 Run。
******************************************************************************/
bool Fsm_create(Fsm ** ppFsm)
{
    Fsm *pFsm;
    
    ZF_ASSERT(ppFsm != (Fsm **)0)
    
    /* 为状态机对象分配内存。 */
    pFsm = ZF_MALLOC(sizeof(Fsm));
    if (pFsm == NULL)
    {
        ZF_DEBUG(LOG_E, "malloc fsm space error\r\n");
        return false;
    }
    
    /* 初始化为单节点状态机，没有父子关系。 */
    pFsm->Level = 1;
    pFsm->ChildList = NULL;
    pFsm->Owner = NULL;
    pFsm->OwnerTriggerState = NULL;
    pFsm->CurrentState = NULL;
    pFsm->IsRunning = false;
    
    /* 绑定状态机的方法表。 */
    pFsm->SetInitialState = Fsm_setInitialState;
    pFsm->Run = Fsm_run;
    pFsm->RunAll = Fsm_runAll;
    pFsm->Stop = Fsm_stop;
    pFsm->StopAll = Fsm_stopAll;
    pFsm->Dispose = Fsm_dispose;
    pFsm->DisposeAll = Fsm_disposeAll;
    pFsm->AddChild = Fsm_addChild;
    pFsm->RemoveChild = Fsm_removeChild;
    pFsm->Dispatch = Fsm_dispatch;
    pFsm->Transfer = Fsm_transfer;
    pFsm->TransferWithEvent = Fsm_transferWithEvent;
    
    /* 返回新建状态机对象。 */
    *ppFsm = pFsm;
    
    return true;
}

/******************************************************************************
 * 功能  设置初始状态
******************************************************************************/
void Fsm_setInitialState(Fsm * const pFsm, IFsmState initialState)
{
    ZF_ASSERT(pFsm != (Fsm *)0)
    ZF_ASSERT(initialState != (IFsmState)0)
    
    pFsm->CurrentState = initialState;
}

/******************************************************************************
 * 功能  启动当前状态机
 * 说明  这里只把 IsRunning 置位，不会主动发送 ENTER 信号。
******************************************************************************/
bool Fsm_run(Fsm * const pFsm)
{
    /* 返回结果。 */
    bool res = false;
    
    ZF_ASSERT(pFsm != (Fsm *)0)
    
    if (!pFsm->IsRunning)
    {
        pFsm->IsRunning = true;
        
        res = true;
    }
    
    return res;
}

/******************************************************************************
 * 功能  递归启动当前状态机及其子状态机
******************************************************************************/
bool Fsm_runAll(Fsm * const pFsm)
{
    ZF_ASSERT(pFsm != (Fsm *)0)
    
    Fsm_run(pFsm);
    
    if (pFsm->ChildList != NULL && pFsm->ChildList->Count > 0)
    {
        uint32_t i;
        Fsm * pChildFsm;
        
        for (i = 0; i < pFsm->ChildList->Count; i++)
        {
            pChildFsm = (Fsm *)pFsm->ChildList
                ->GetElementDataAt(pFsm->ChildList, i);
            
            if (pChildFsm != NULL)
            {
                Fsm_runAll(pChildFsm);
            }
        }
    }
    
    return true;
}

/******************************************************************************
 * 功能  停止当前状态机
******************************************************************************/
bool Fsm_stop(Fsm * const pFsm)
{
    ZF_ASSERT(pFsm != (Fsm *)0)
    
    pFsm->IsRunning = false;
    
    return true;
}

/******************************************************************************
 * 功能  递归停止当前状态机及其子状态机
******************************************************************************/
bool Fsm_stopAll(Fsm * const pFsm)
{
    ZF_ASSERT(pFsm != (Fsm *)0)
    
    Fsm_stop(pFsm);
    
    if (pFsm->ChildList != NULL && pFsm->ChildList->Count > 0)
    {
        uint32_t i;
        Fsm * pChildFsm;
        
        for (i = 0; i < pFsm->ChildList->Count; i++)
        {
            pChildFsm = (Fsm *)pFsm->ChildList
                ->GetElementDataAt(pFsm->ChildList, i);
            
            if (pChildFsm != NULL)
            {
                Fsm_stopAll(pChildFsm);
            }
        }
    }
    
    return true;
}

/******************************************************************************
 * 功能  释放当前状态机
 * 说明  只释放自己和子链表容器，不递归释放子状态机对象。
******************************************************************************/
bool Fsm_dispose(Fsm * const pFsm)
{
    ZF_ASSERT(pFsm != (Fsm *)0)
    
    if (pFsm->ChildList != NULL)
    {
        pFsm->ChildList->Dispose(pFsm->ChildList);
    }
    
    ZF_FREE(pFsm);
    
    return true;
}

/******************************************************************************
 * 功能  递归释放当前状态机及子状态机
******************************************************************************/
bool Fsm_disposeAll(Fsm * const pFsm)
{
    ZF_ASSERT(pFsm != (Fsm *)0)
    
    if (pFsm->ChildList != NULL && pFsm->ChildList->Count > 0)
    {
        uint32_t i;
        Fsm * pChildFsm;
        
        for (i = 0; i < pFsm->ChildList->Count; i++)
        {
            pChildFsm = (Fsm *)pFsm->ChildList
                ->GetElementDataAt(pFsm->ChildList, i);
            
            if (pChildFsm != NULL)
            {
                Fsm_disposeAll(pChildFsm);
            }
        }
    }
    
    Fsm_dispose(pFsm);
    
    return true;
}

/******************************************************************************
 * 功能  添加子状态机
 * 说明  添加后会建立 Owner 关系，并把层级加一。
******************************************************************************/
bool Fsm_addChild(Fsm * const pFsm, Fsm * const pChildFsm)
{
    /* ���ؽ�� */
    bool res = false;
    
    List *pList;
    ListNode *pNode;
    
    ZF_ASSERT(pFsm != (Fsm *)0)
    ZF_ASSERT(pChildFsm != (Fsm *)0)
    
    if (pFsm->ChildList == NULL)
    {
        List_create(&pList);
        pFsm->ChildList = pList;
    }
    
    pList = pFsm->ChildList;
    
    if (pList != NULL)
    {
        List_mallocNode(&pNode, NULL, 0);
        if (pNode != NULL)
        {
            pNode->pData = (void *)pChildFsm;
            pNode->Size = sizeof(Fsm);
            
            pList->Add(pList, pNode);
            
            pChildFsm->Owner = pFsm;
            pChildFsm->Level = pFsm->Level + 1;
            
            res = true;
        }
    }
    
    return res;
}

/******************************************************************************
 * 功能  移除子状态机
 * 说明  这里只从父状态机链表中摘掉，不释放子状态机对象。
******************************************************************************/
bool Fsm_removeChild(Fsm * const pFsm, Fsm * const pChildFsm)
{
    /* ���ؽ�� */
    bool res = false;
    
    ZF_ASSERT(pFsm != (Fsm *)0)
    ZF_ASSERT(pChildFsm != (Fsm *)0)
    
    if (pFsm->ChildList != NULL && pFsm->ChildList->Count > 0)
    {
        ListNode *pNode;
        
        /* 从父状态机子列表中移除目标子状态机。 */
        while(pFsm->ChildList
            ->GetElementByData(pFsm->ChildList, pChildFsm, &pNode))
        {
            if (pNode == NULL)
            {
                break;
            }
            
            if (!pFsm->ChildList->Delete(pFsm->ChildList, pNode))
            {
                ZF_DEBUG(LOG_E, "delete fsm node from list error\r\n");
                
                break;
            }
            
            res = true;
        }
    }
    
    return res;
}

/******************************************************************************
 * 功能  分发信号
 * 说明  先递归分发给子状态机，再由当前状态机决定是否处理该信号。
 *       子状态机是否触发，还受 OwnerTriggerState 限制。
******************************************************************************/
bool Fsm_dispatch(Fsm * const pFsm, FsmSignal const signal)
{
    /* 返回结果。 */
    bool res = false;
    
    ZF_ASSERT(pFsm != (Fsm *)0)
    
    if (pFsm->IsRunning)
    {
        if (pFsm->ChildList != NULL && pFsm->ChildList->Count > 0)
        {
            uint32_t i;
            Fsm * pChildFsm;
            
            for (i = 0; i < pFsm->ChildList->Count; i++)
            {
                pChildFsm = (Fsm *)pFsm->ChildList
                    ->GetElementDataAt(pFsm->ChildList, i);
                
                if (pChildFsm != NULL)
                {
                    Fsm_dispatch(pChildFsm, signal);
                }
            }
        }
        
        if (pFsm->CurrentState != NULL)
        {
                        /*
                         * 下面三种情况允许当前状态执行：
                         * 1. 这是根状态机。
                         * 2. 没有限定父状态触发条件。
                         * 3. 父状态机当前正处于约定触发状态。
                         */
            if (pFsm->Owner == NULL || pFsm->OwnerTriggerState == NULL
                || pFsm->OwnerTriggerState == pFsm->Owner->CurrentState)
            {
                pFsm->CurrentState(pFsm, signal);
                
                res = true;
            }
        }
    }
    
    return res;
}

/******************************************************************************
 * 功能  直接状态转移
 * 说明  这里只更新 CurrentState，不自动触发 EXIT/ENTER。
******************************************************************************/
void Fsm_transfer(Fsm * const pFsm, IFsmState nextState)
{
    ZF_ASSERT(pFsm != (Fsm *)0)
    ZF_ASSERT(nextState != (IFsmState)0)
    
    pFsm->CurrentState = (IFsmState)nextState;
}

/******************************************************************************
 * 功能  带事件的状态转移
 * 说明  先给旧状态发 EXIT，再切换 CurrentState，最后给新状态发 ENTER。
******************************************************************************/
void Fsm_transferWithEvent(Fsm * const pFsm, IFsmState nextState)
{
    ZF_ASSERT(pFsm != (Fsm *)0)
    ZF_ASSERT(nextState != (IFsmState)0)
    
    Fsm_dispatch(pFsm, FSM_EXIT_SIG);
    
    Fsm_transfer(pFsm, nextState);
    
    Fsm_dispatch(pFsm, FSM_ENTER_SIG);
}

/******************************** 文件结束 ********************************/

