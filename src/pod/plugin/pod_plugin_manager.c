#if __has_include("pod_plugin_manager.h")
#include "pod_plugin_manager.h"
#else
#include "../../../inc/pod/plugin/pod_plugin_manager.h"
#endif

#if __has_include("pod_camera_plugin.h")
#include "pod_camera_plugin.h"
#else
#include "../../../inc/pod/plugin/pod_camera_plugin.h"
#endif

#include "string.h"

#if __has_include("zf_assert.h")
#include "zf_assert.h"
#else
#include "../../../inc/core/zf_assert.h"
#endif

#if __has_include("zf_list.h")
#include "zf_list.h"
#else
#include "../../../inc/core/zf_list.h"
#endif

#if __has_include("zf_malloc.h")
#include "zf_malloc.h"
#else
#include "../../../inc/core/zf_malloc.h"
#endif

typedef struct _PodPluginManager
{
    /* 插件列表中存放 PodCameraPlugin*，按注册顺序遍历。 */
    List *pPluginList;
} PodPluginManager;

static PodCameraPlugin *PodPluginManager_getAt(PodPluginManager *pMgr,
    uint32_t index)
{
    PodCameraPlugin **ppPlugin;

    ZF_ASSERT(pMgr != (PodPluginManager *)0)
    ZF_ASSERT(pMgr->pPluginList != (List *)0)

    /* List 存储的是 PodCameraPlugin*，所以这里要先拿到二级指针。 */
    ppPlugin = (PodCameraPlugin **)pMgr->pPluginList
        ->GetElementDataAt(pMgr->pPluginList, index);
    if (ppPlugin == NULL)
    {
        return NULL;
    }

    return *ppPlugin;
}

bool PodPluginManager_create(PodPluginManager **ppMgr)
{
    PodPluginManager *pMgr;

    ZF_ASSERT(ppMgr != (PodPluginManager **)0)

    /* 管理器本身很轻，只持有一个插件链表。 */
    pMgr = (PodPluginManager *)ZF_MALLOC(sizeof(PodPluginManager));
    if (pMgr == NULL)
    {
        return false;
    }

    pMgr->pPluginList = NULL;
    if (!List_create(&pMgr->pPluginList))
    {
        ZF_FREE(pMgr);
        return false;
    }

    *ppMgr = pMgr;
    return true;
}

void PodPluginManager_dispose(PodPluginManager *pMgr)
{
    if (pMgr == NULL)
    {
        return;
    }

    if (pMgr->pPluginList != NULL)
    {
        pMgr->pPluginList->Dispose(pMgr->pPluginList);
        pMgr->pPluginList = NULL;
    }

    ZF_FREE(pMgr);
}

bool PodPluginManager_register(PodPluginManager *pMgr, PodCameraPlugin *pPlugin)
{
    ListNode *pNode;
    PodCameraPlugin **ppData;

    ZF_ASSERT(pMgr != (PodPluginManager *)0)
    ZF_ASSERT(pMgr->pPluginList != (List *)0)
    ZF_ASSERT(pPlugin != (PodCameraPlugin *)0)

    /*
     * 名称与 stream_id 都必须唯一：
     * - 名称重复会让控制路径混乱。
     * - stream_id 重复会让数据路由冲突。
     */
    if (PodPluginManager_findByName(pMgr, pPlugin->name) != NULL)
    {
        return false;
    }

    if (PodPluginManager_findByStream(pMgr, pPlugin->stream_id) != NULL)
    {
        return false;
    }

    if (!List_mallocNode(&pNode, (void **)&ppData, sizeof(PodCameraPlugin *)))
    {
        return false;
    }

    /* 链表节点里只保存插件指针，不复制插件对象本身。 */
    *ppData = pPlugin;

    if (!pMgr->pPluginList->Add(pMgr->pPluginList, pNode))
    {
        List_freeNode(pNode);
        return false;
    }

    return true;
}

PodCameraPlugin *PodPluginManager_findByName(PodPluginManager *pMgr,
    const char *name)
{
    uint32_t i;

    ZF_ASSERT(pMgr != (PodPluginManager *)0)

    if (name == NULL)
    {
        return NULL;
    }

    /* 线性遍历足够简单，适合插件数量通常不大的场景。 */
    for (i = 0; i < pMgr->pPluginList->Count; i++)
    {
        PodCameraPlugin *pPlugin = PodPluginManager_getAt(pMgr, i);
        if (pPlugin != NULL && pPlugin->name != NULL)
        {
            if (strcmp(pPlugin->name, name) == 0)
            {
                return pPlugin;
            }
        }
    }

    return NULL;
}

PodCameraPlugin *PodPluginManager_findByStream(PodPluginManager *pMgr,
    uint8_t stream_id)
{
    uint32_t i;

    ZF_ASSERT(pMgr != (PodPluginManager *)0)

    for (i = 0; i < pMgr->pPluginList->Count; i++)
    {
        PodCameraPlugin *pPlugin = PodPluginManager_getAt(pMgr, i);
        if (pPlugin != NULL && pPlugin->stream_id == stream_id)
        {
            return pPlugin;
        }
    }

    return NULL;
}

PodCameraPlugin *PodPluginManager_getAtIndex(PodPluginManager *pMgr,
    uint32_t index)
{
    ZF_ASSERT(pMgr != (PodPluginManager *)0)

    if (index >= pMgr->pPluginList->Count)
    {
        return NULL;
    }

    return PodPluginManager_getAt(pMgr, index);
}

uint32_t PodPluginManager_getCount(PodPluginManager *pMgr)
{
    ZF_ASSERT(pMgr != (PodPluginManager *)0)
    return pMgr->pPluginList->Count;
}

int32_t PodPluginManager_initAll(PodPluginManager *pMgr, const char *cfg_text)
{
    uint32_t i;

    ZF_ASSERT(pMgr != (PodPluginManager *)0)

    /* 批量初始化：一旦某个插件失败，立即返回错误码。 */
    for (i = 0; i < pMgr->pPluginList->Count; i++)
    {
        PodCameraPlugin *pPlugin = PodPluginManager_getAt(pMgr, i);
        if (pPlugin != NULL && pPlugin->ops.Init != NULL)
        {
            int32_t ret = pPlugin->ops.Init(pPlugin->ctx, cfg_text);
            if (ret != 0)
            {
                return ret;
            }
        }
    }

    return 0;
}

int32_t PodPluginManager_openAll(PodPluginManager *pMgr)
{
    uint32_t i;

    ZF_ASSERT(pMgr != (PodPluginManager *)0)

    /* open 阶段通常对应设备句柄申请、缓冲区准备等重资源动作。 */
    for (i = 0; i < pMgr->pPluginList->Count; i++)
    {
        PodCameraPlugin *pPlugin = PodPluginManager_getAt(pMgr, i);
        if (pPlugin != NULL && pPlugin->ops.Open != NULL)
        {
            int32_t ret = pPlugin->ops.Open(pPlugin->ctx);
            if (ret != 0)
            {
                return ret;
            }
        }
    }

    return 0;
}

int32_t PodPluginManager_startAll(PodPluginManager *pMgr)
{
    uint32_t i;

    ZF_ASSERT(pMgr != (PodPluginManager *)0)

    /* 启动采集流：由运行时在 start 阶段统一调用。 */
    for (i = 0; i < pMgr->pPluginList->Count; i++)
    {
        PodCameraPlugin *pPlugin = PodPluginManager_getAt(pMgr, i);
        if (pPlugin != NULL && pPlugin->ops.StartStream != NULL)
        {
            int32_t ret = pPlugin->ops.StartStream(pPlugin->ctx);
            if (ret != 0)
            {
                return ret;
            }
        }
    }

    return 0;
}

int32_t PodPluginManager_stopAll(PodPluginManager *pMgr)
{
    uint32_t i;

    ZF_ASSERT(pMgr != (PodPluginManager *)0)

    /* stop 只停数据流，不一定立刻释放底层句柄。 */
    for (i = 0; i < pMgr->pPluginList->Count; i++)
    {
        PodCameraPlugin *pPlugin = PodPluginManager_getAt(pMgr, i);
        if (pPlugin != NULL && pPlugin->ops.StopStream != NULL)
        {
            int32_t ret = pPlugin->ops.StopStream(pPlugin->ctx);
            if (ret != 0)
            {
                return ret;
            }
        }
    }

    return 0;
}

int32_t PodPluginManager_closeAll(PodPluginManager *pMgr)
{
    uint32_t i;

    ZF_ASSERT(pMgr != (PodPluginManager *)0)

    /* close 阶段释放设备与句柄资源，通常是 stop 之后的收尾动作。 */
    for (i = 0; i < pMgr->pPluginList->Count; i++)
    {
        PodCameraPlugin *pPlugin = PodPluginManager_getAt(pMgr, i);
        if (pPlugin != NULL && pPlugin->ops.Close != NULL)
        {
            int32_t ret = pPlugin->ops.Close(pPlugin->ctx);
            if (ret != 0)
            {
                return ret;
            }
        }
    }

    return 0;
}
