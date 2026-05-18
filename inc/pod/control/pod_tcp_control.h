#ifndef __POD_TCP_CONTROL_H__
#define __POD_TCP_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

#include "../plugin/pod_plugin_manager.h"

/* 云台命令回调。控制服务器只负责解析协议，不直接实现云台动作。 */
typedef int32_t (*PodGimbalControlHook)(const char *cmd, const char *args,
    void *pUser);

typedef struct _PodTcpControlServer PodTcpControlServer;

typedef struct _PodTcpControlConfig
{
    /* 控制端口。 */
    uint16_t ListenPort;
    /* 插件管理器，用于 LIST/GET/SET 等命令分发。 */
    PodPluginManager *pPluginManager;
    /* 云台控制外部回调。 */
    PodGimbalControlHook GimbalHook;
    void *pGimbalUser;
} PodTcpControlConfig;

/* 创建 TCP 控制服务对象。 */
bool PodTcpControlServer_create(PodTcpControlServer **ppServer,
    const PodTcpControlConfig *pConfig);
void PodTcpControlServer_dispose(PodTcpControlServer *pServer);

/* 启动监听 socket。 */
int32_t PodTcpControlServer_start(PodTcpControlServer *pServer);
/* 停止监听并断开客户端。 */
int32_t PodTcpControlServer_stop(PodTcpControlServer *pServer);

/*
 * 非阻塞轮询一次。
 * 负责 accept 客户端、接收文本命令、逐行解析并回 ACK。
 * 无致命错误时返回 0。
 */
int32_t PodTcpControlServer_pollOnce(PodTcpControlServer *pServer);

#ifdef __cplusplus
}
#endif

#endif /* __POD_TCP_CONTROL_H__ */
