#ifndef __POD_PLUGIN_MANAGER_H__
#define __POD_PLUGIN_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

typedef struct _PodCameraPlugin PodCameraPlugin;

typedef struct _PodPluginManager PodPluginManager;

/*
 * 插件管理器负责三类事情：
 * 1. 保存所有已注册的相机插件实例。
 * 2. 提供按名称、按 stream_id、按索引的查找能力。
 * 3. 统一执行 Init/Open/Start/Stop/Close 生命周期。
 */
bool PodPluginManager_create(PodPluginManager **ppMgr);
void PodPluginManager_dispose(PodPluginManager *pMgr);

/* 注册插件时要求 name 与 stream_id 都唯一。 */
bool PodPluginManager_register(PodPluginManager *pMgr, PodCameraPlugin *pPlugin);
/* 按名字查找插件，常用于配置或控制路径。 */
PodCameraPlugin *PodPluginManager_findByName(PodPluginManager *pMgr,
    const char *name);
/* 按 stream_id 查找插件，常用于数据路径。 */
PodCameraPlugin *PodPluginManager_findByStream(PodPluginManager *pMgr,
    uint8_t stream_id);
/* 按注册顺序获取插件，便于顺序遍历。 */
PodCameraPlugin *PodPluginManager_getAtIndex(PodPluginManager *pMgr,
    uint32_t index);
uint32_t PodPluginManager_getCount(PodPluginManager *pMgr);

/* 批量生命周期调用，遇到第一个错误立即返回。 */
int32_t PodPluginManager_initAll(PodPluginManager *pMgr, const char *cfg_text);
int32_t PodPluginManager_openAll(PodPluginManager *pMgr);
int32_t PodPluginManager_startAll(PodPluginManager *pMgr);
int32_t PodPluginManager_stopAll(PodPluginManager *pMgr);
int32_t PodPluginManager_closeAll(PodPluginManager *pMgr);

#ifdef __cplusplus
}
#endif

#endif /* __POD_PLUGIN_MANAGER_H__ */
