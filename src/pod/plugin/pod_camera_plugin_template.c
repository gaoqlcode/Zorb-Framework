#if __has_include("pod_camera_plugin_template.h")
#include "pod_camera_plugin_template.h"
#else
#include "../../../inc/pod/plugin/pod_camera_plugin_template.h"
#endif

#if __has_include("zf_assert.h")
#include "zf_assert.h"
#else
#include "../../../inc/core/zf_assert.h"
#endif

#include <string.h>

static int32_t PodTpl_Init(void *pOpaqueCtx, const char *pCfgText)
{
    PodSdkCameraCtx *pCtx = (PodSdkCameraCtx *)pOpaqueCtx;

    ZF_ASSERT(pCtx != NULL)

    if (pCtx->Hooks.SdkInit != NULL)
    {
        int32_t ret = pCtx->Hooks.SdkInit(pCtx->pUser, pCfgText);
        if (ret != 0)
        {
            pCtx->LastError = (uint32_t)(-ret);
            return ret;
        }
    }

    pCtx->IsInited = 1u;
    return 0;
}

static int32_t PodTpl_Open(void *pOpaqueCtx)
{
    PodSdkCameraCtx *pCtx = (PodSdkCameraCtx *)pOpaqueCtx;

    ZF_ASSERT(pCtx != NULL)

    if (pCtx->IsInited == 0u)
    {
        return -1;
    }

    if (pCtx->Hooks.SdkOpen != NULL)
    {
        int32_t ret = pCtx->Hooks.SdkOpen(pCtx->pUser);
        if (ret != 0)
        {
            pCtx->LastError = (uint32_t)(-ret);
            return ret;
        }
    }

    pCtx->IsOpened = 1u;
    return 0;
}

static int32_t PodTpl_StartStream(void *pOpaqueCtx)
{
    PodSdkCameraCtx *pCtx = (PodSdkCameraCtx *)pOpaqueCtx;

    ZF_ASSERT(pCtx != NULL)

    if (pCtx->IsOpened == 0u)
    {
        return -1;
    }

    if (pCtx->Hooks.SdkStart != NULL)
    {
        int32_t ret = pCtx->Hooks.SdkStart(pCtx->pUser);
        if (ret != 0)
        {
            pCtx->LastError = (uint32_t)(-ret);
            return ret;
        }
    }

    pCtx->IsStreaming = 1u;
    return 0;
}

static int32_t PodTpl_StopStream(void *pOpaqueCtx)
{
    PodSdkCameraCtx *pCtx = (PodSdkCameraCtx *)pOpaqueCtx;

    ZF_ASSERT(pCtx != NULL)

    if (pCtx->Hooks.SdkStop != NULL)
    {
        int32_t ret = pCtx->Hooks.SdkStop(pCtx->pUser);
        if (ret != 0)
        {
            pCtx->LastError = (uint32_t)(-ret);
            return ret;
        }
    }

    pCtx->IsStreaming = 0u;
    return 0;
}

static int32_t PodTpl_GetFrame(void *pOpaqueCtx, PodFrameMeta *pMeta,
    const uint8_t **ppPayload, uint32_t *pPayloadLen, uint32_t timeoutMs)
{
    PodSdkCameraCtx *pCtx = (PodSdkCameraCtx *)pOpaqueCtx;

    ZF_ASSERT(pCtx != NULL)

    if (pCtx->IsStreaming == 0u)
    {
        return -1;
    }

    if (pCtx->Hooks.SdkGetFrame == NULL)
    {
        return -2;
    }

    return pCtx->Hooks.SdkGetFrame(pCtx->pUser, pMeta, ppPayload,
        pPayloadLen, timeoutMs);
}

static int32_t PodTpl_SetParam(void *pOpaqueCtx, const char *pKey,
    const char *pValue)
{
    PodSdkCameraCtx *pCtx = (PodSdkCameraCtx *)pOpaqueCtx;

    ZF_ASSERT(pCtx != NULL)

    if (pCtx->Hooks.SdkSetParam == NULL)
    {
        return -2;
    }

    return pCtx->Hooks.SdkSetParam(pCtx->pUser, pKey, pValue);
}

static int32_t PodTpl_GetParam(void *pOpaqueCtx, const char *pKey,
    char *pValueBuf, uint32_t valueBufSize)
{
    PodSdkCameraCtx *pCtx = (PodSdkCameraCtx *)pOpaqueCtx;

    ZF_ASSERT(pCtx != NULL)

    if (pCtx->Hooks.SdkGetParam == NULL)
    {
        return -2;
    }

    return pCtx->Hooks.SdkGetParam(pCtx->pUser, pKey, pValueBuf, valueBufSize);
}

static int32_t PodTpl_GetHealth(void *pOpaqueCtx, uint32_t *pHealthCode)
{
    PodSdkCameraCtx *pCtx = (PodSdkCameraCtx *)pOpaqueCtx;

    ZF_ASSERT(pCtx != NULL)

    if (pHealthCode == NULL)
    {
        return -1;
    }

    if (pCtx->Hooks.SdkGetHealth != NULL)
    {
        return pCtx->Hooks.SdkGetHealth(pCtx->pUser, pHealthCode);
    }

    *pHealthCode = (pCtx->LastError == 0u) ? 0u : 1u;
    return 0;
}

static int32_t PodTpl_Close(void *pOpaqueCtx)
{
    PodSdkCameraCtx *pCtx = (PodSdkCameraCtx *)pOpaqueCtx;

    ZF_ASSERT(pCtx != NULL)

    if (pCtx->Hooks.SdkClose != NULL)
    {
        int32_t ret = pCtx->Hooks.SdkClose(pCtx->pUser);
        if (ret != 0)
        {
            pCtx->LastError = (uint32_t)(-ret);
            return ret;
        }
    }

    pCtx->IsOpened = 0u;
    pCtx->IsStreaming = 0u;
    return 0;
}

bool PodSdkCameraPlugin_setup(PodCameraPlugin *pPlugin,
    PodSdkCameraCtx *pCtx,
    const char *pName,
    uint8_t streamId,
    const PodSdkCameraHooks *pHooks,
    void *pUser)
{
    ZF_ASSERT(pPlugin != NULL)
    ZF_ASSERT(pCtx != NULL)
    ZF_ASSERT(pName != NULL)
    ZF_ASSERT(pHooks != NULL)

    memset(pCtx, 0, sizeof(*pCtx));
    pCtx->pName = pName;
    pCtx->StreamId = streamId;
    pCtx->pUser = pUser;
    pCtx->Hooks = *pHooks;

    memset(pPlugin, 0, sizeof(*pPlugin));
    pPlugin->name = pName;
    pPlugin->stream_id = streamId;
    pPlugin->ctx = pCtx;

    pPlugin->ops.Init = PodTpl_Init;
    pPlugin->ops.Open = PodTpl_Open;
    pPlugin->ops.StartStream = PodTpl_StartStream;
    pPlugin->ops.StopStream = PodTpl_StopStream;
    pPlugin->ops.GetFrame = PodTpl_GetFrame;
    pPlugin->ops.SetParam = PodTpl_SetParam;
    pPlugin->ops.GetParam = PodTpl_GetParam;
    pPlugin->ops.GetHealth = PodTpl_GetHealth;
    pPlugin->ops.Close = PodTpl_Close;

    return true;
}
