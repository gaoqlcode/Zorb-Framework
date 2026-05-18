#ifndef __POD_PLUGIN_MANAGER_H__
#define __POD_PLUGIN_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

typedef struct _PodCameraPlugin PodCameraPlugin;

typedef struct _PodPluginManager PodPluginManager;

/* 插件管理器接口 */
bool PodPluginManager_create(PodPluginManager **ppMgr);
void PodPluginManager_dispose(PodPluginManager *pMgr);

bool PodPluginManager_register(PodPluginManager *pMgr, PodCameraPlugin *pPlugin);
PodCameraPlugin *PodPluginManager_findByName(PodPluginManager *pMgr,
    const char *name);
PodCameraPlugin *PodPluginManager_findByStream(PodPluginManager *pMgr,
    uint8_t stream_id);
PodCameraPlugin *PodPluginManager_getAtIndex(PodPluginManager *pMgr,
    uint32_t index);
uint32_t PodPluginManager_getCount(PodPluginManager *pMgr);

int32_t PodPluginManager_initAll(PodPluginManager *pMgr, const char *cfg_text);
int32_t PodPluginManager_openAll(PodPluginManager *pMgr);
int32_t PodPluginManager_startAll(PodPluginManager *pMgr);
int32_t PodPluginManager_stopAll(PodPluginManager *pMgr);
int32_t PodPluginManager_closeAll(PodPluginManager *pMgr);

#ifdef __cplusplus
}
#endif

#endif /* __POD_PLUGIN_MANAGER_H__ */
