#ifndef __POD_CAMERA_PLUGIN_TEMPLATE_H__
#define __POD_CAMERA_PLUGIN_TEMPLATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#if __has_include("pod_camera_plugin.h")
#include "pod_camera_plugin.h"
#else
#include "pod_camera_plugin.h"
#endif

/*
 * 真实相机插件模板：
 * - PodSdkCameraHooks 由你桥接厂商 SDK。
 * - PodSdkCameraCtx 保存运行状态与插件元信息。
 * - PodSdkCameraPlugin_setup 会把模板回调绑定到 PodCameraPlugin。
 */

typedef struct _PodSdkCameraHooks
{
    int32_t (*SdkInit)(void *pUser, const char *cfgText);
    int32_t (*SdkOpen)(void *pUser);
    int32_t (*SdkStart)(void *pUser);
    int32_t (*SdkStop)(void *pUser);
    int32_t (*SdkGetFrame)(void *pUser, PodFrameMeta *pMeta,
        const uint8_t **ppPayload, uint32_t *pLen, uint32_t timeoutMs);
    int32_t (*SdkSetParam)(void *pUser, const char *pKey, const char *pValue);
    int32_t (*SdkGetParam)(void *pUser, const char *pKey, char *pValueBuf,
        uint32_t valueBufSize);
    int32_t (*SdkGetHealth)(void *pUser, uint32_t *pHealthCode);
    int32_t (*SdkClose)(void *pUser);
} PodSdkCameraHooks;

typedef struct _PodSdkCameraCtx
{
    const char *pName;
    uint8_t StreamId;

    /*
     * pUser 指向你的设备上下文：
     * 例如 SDK 句柄、采集线程、DMA 缓冲区描述等。
     */
    void *pUser;

    PodSdkCameraHooks Hooks;

    uint8_t IsInited;
    uint8_t IsOpened;
    uint8_t IsStreaming;
    uint32_t LastError;
} PodSdkCameraCtx;

bool PodSdkCameraPlugin_setup(PodCameraPlugin *pPlugin,
    PodSdkCameraCtx *pCtx,
    const char *pName,
    uint8_t streamId,
    const PodSdkCameraHooks *pHooks,
    void *pUser);

#ifdef __cplusplus
}
#endif

#endif /* __POD_CAMERA_PLUGIN_TEMPLATE_H__ */
