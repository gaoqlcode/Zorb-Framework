#ifndef __POD_TCP_CONTROL_H__
#define __POD_TCP_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

#include "../plugin/pod_plugin_manager.h"

typedef int32_t (*PodGimbalControlHook)(const char *cmd, const char *args,
    void *pUser);

typedef struct _PodTcpControlServer PodTcpControlServer;

typedef struct _PodTcpControlConfig
{
    uint16_t ListenPort;
    PodPluginManager *pPluginManager;
    PodGimbalControlHook GimbalHook;
    void *pGimbalUser;
} PodTcpControlConfig;

bool PodTcpControlServer_create(PodTcpControlServer **ppServer,
    const PodTcpControlConfig *pConfig);
void PodTcpControlServer_dispose(PodTcpControlServer *pServer);

int32_t PodTcpControlServer_start(PodTcpControlServer *pServer);
int32_t PodTcpControlServer_stop(PodTcpControlServer *pServer);

/*
 * 非阻塞轮询一次。
 * 无致命错误时返回 0。
 */
int32_t PodTcpControlServer_pollOnce(PodTcpControlServer *pServer);

#ifdef __cplusplus
}
#endif

#endif /* __POD_TCP_CONTROL_H__ */
