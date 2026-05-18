#if __has_include("pod_tcp_control.h")
#include "pod_tcp_control.h"
#else
#include "../../../inc/pod/control/pod_tcp_control.h"
#endif

#if __has_include("pod_camera_plugin.h")
#include "pod_camera_plugin.h"
#else
#include "../../../inc/pod/plugin/pod_camera_plugin.h"
#endif

#include <string.h>

#if defined(__linux__)
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if __has_include("zf_assert.h")
#include "zf_assert.h"
#else
#include "../../../inc/core/zf_assert.h"
#endif

#if __has_include("zf_malloc.h")
#include "zf_malloc.h"
#else
#include "../../../inc/core/zf_malloc.h"
#endif

#define POD_CTRL_BUF_SIZE 1024

typedef struct _PodReqCtx
{
    /* REQ/ACK 协议上下文：兼容“旧命令”与“REQ id cmd ...”两种输入。 */
    uint32_t RequestId;
    uint8_t HasRequestId;
    const char *CmdToken;
} PodReqCtx;

typedef struct _PodTcpControlServer
{
    PodTcpControlConfig Config;
    uint8_t IsStarted;

#if defined(__linux__)
    int ListenFd;
    int ClientFd;
    char RxBuf[POD_CTRL_BUF_SIZE];
    uint32_t RxLen;
#endif
} PodTcpControlServer;

#if defined(__linux__)
static void CloseFdSafe(int *pFd)
{
    if (pFd != NULL && *pFd >= 0)
    {
        close(*pFd);
        *pFd = -1;
    }
}

static void SendText(int fd, const char *text)
{
    if (fd >= 0 && text != NULL)
    {
        (void)send(fd, text, strlen(text), 0);
    }
}

static void SendAck(int fd, uint8_t has_request_id, uint32_t request_id,
    int32_t code, const char *message)
{
    char out[320];

    if (message == NULL)
    {
        message = "";
    }

    /* 统一 ACK 输出格式，便于地面端统一解析。 */
    if (has_request_id)
    {
        (void)snprintf(out, sizeof(out), "ACK %lu %ld %s\n",
            (unsigned long)request_id, (long)code, message);
    }
    else
    {
        (void)snprintf(out, sizeof(out), "ACK 0 %ld %s\n",
            (long)code, message);
    }

    SendText(fd, out);
}

static PodReqCtx ParseReqPrefix(char **psaveptr, char *first_token)
{
    PodReqCtx ctx;

    ctx.RequestId = 0u;
    ctx.HasRequestId = 0u;
    ctx.CmdToken = first_token;

    /*
     * 支持格式：
     * 1) REQ <id> <CMD> ...
     * 2) <CMD> ...（兼容旧协议）
     */
    if (first_token != NULL && strcmp(first_token, "REQ") == 0)
    {
        char *id_token = strtok_r(NULL, " \t\r\n", psaveptr);
        char *cmd_token = strtok_r(NULL, " \t\r\n", psaveptr);

        if (id_token != NULL && cmd_token != NULL)
        {
            ctx.RequestId = (uint32_t)strtoul(id_token, NULL, 10);
            ctx.HasRequestId = 1u;
            ctx.CmdToken = cmd_token;
        }
    }

    return ctx;
}

static PodCameraPlugin *FindPlugin(PodTcpControlServer *pServer,
    const char *name)
{
    /* 控制面通过名字路由到具体插件。 */
    if (pServer->Config.pPluginManager == NULL || name == NULL)
    {
        return NULL;
    }

    return PodPluginManager_findByName(pServer->Config.pPluginManager, name);
}

static void HandleStatusCmd(PodTcpControlServer *pServer, const PodReqCtx *pReq)
{
    char msg[128];
    uint32_t plugin_count = 0u;

    if (pServer->Config.pPluginManager != NULL)
    {
        plugin_count = PodPluginManager_getCount(pServer->Config.pPluginManager);
    }

    /* STATUS 用于给上位机快速确认系统是否在线。 */
    (void)snprintf(msg, sizeof(msg), "running=%u plugins=%lu client=%s",
        (unsigned int)pServer->IsStarted,
        (unsigned long)plugin_count,
        (pServer->ClientFd >= 0) ? "online" : "offline");

    SendAck(pServer->ClientFd, pReq->HasRequestId, pReq->RequestId, 0, msg);
}

static void HandleListCmd(PodTcpControlServer *pServer, const PodReqCtx *pReq)
{
    char msg[512];
    uint32_t i;
    uint32_t count;

    if (pServer->Config.pPluginManager == NULL)
    {
        SendAck(pServer->ClientFd, pReq->HasRequestId, pReq->RequestId,
            2001, "no_plugin_manager");
        return;
    }

    msg[0] = '\0';
    count = PodPluginManager_getCount(pServer->Config.pPluginManager);

    /* 把当前注册的插件列表拼成一行文本返回。 */
    for (i = 0u; i < count; i++)
    {
        PodCameraPlugin *pPlugin = PodPluginManager_getAtIndex(
            pServer->Config.pPluginManager, i);
        char item[96];

        if (pPlugin == NULL || pPlugin->name == NULL)
        {
            continue;
        }

        (void)snprintf(item, sizeof(item), "%s%s:%u",
            (msg[0] == '\0') ? "" : ",",
            pPlugin->name,
            (unsigned int)pPlugin->stream_id);

        if (strlen(msg) + strlen(item) + 1u >= sizeof(msg))
        {
            break;
        }

        strcat(msg, item);
    }

    if (msg[0] == '\0')
    {
        strcpy(msg, "empty");
    }

    SendAck(pServer->ClientFd, pReq->HasRequestId, pReq->RequestId, 0, msg);
}

static void HandleCommand(PodTcpControlServer *pServer, const char *cmd)
{
    char temp[POD_CTRL_BUF_SIZE];
    char *token;
    char *saveptr;
    PodReqCtx req;

    strncpy(temp, cmd, sizeof(temp) - 1u);
    temp[sizeof(temp) - 1u] = '\0';

    /* 对每一行命令做 token 化，再进入分发分支。 */
    token = strtok_r(temp, " \t\r\n", &saveptr);
    if (token == NULL)
    {
        SendAck(pServer->ClientFd, 0u, 0u, 1001, "empty");
        return;
    }

    req = ParseReqPrefix(&saveptr, token);
    token = (char *)req.CmdToken;
    if (token == NULL)
    {
        SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
            1002, "bad_req");
        return;
    }

    /* PING/PONG 是最基本的链路活性探测。 */
    if (strcmp(token, "PING") == 0)
    {
        SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
            0, "PONG");
        return;
    }

    if (strcmp(token, "STATUS") == 0)
    {
        HandleStatusCmd(pServer, &req);
        return;
    }

    if (strcmp(token, "LIST") == 0)
    {
        HandleListCmd(pServer, &req);
        return;
    }

    /* SET 走插件的 SetParam 接口。 */
    if (strcmp(token, "SET") == 0)
    {
        char *plugin_name = strtok_r(NULL, " \t\r\n", &saveptr);
        char *key = strtok_r(NULL, " \t\r\n", &saveptr);
        char *value = strtok_r(NULL, "\r\n", &saveptr);
        PodCameraPlugin *pPlugin;

        if (plugin_name == NULL || key == NULL || value == NULL)
        {
            SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                1101, "bad_set");
            return;
        }

        while (*value == ' ' || *value == '\t')
        {
            value++;
        }

        pPlugin = FindPlugin(pServer, plugin_name);
        if (pPlugin == NULL || pPlugin->ops.SetParam == NULL)
        {
            SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                2001, "no_plugin");
            return;
        }

        /* SET 仅做参数分发，实际生效逻辑由插件自身实现。 */
        if (pPlugin->ops.SetParam(pPlugin->ctx, key, value) == 0)
        {
            SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                0, "OK");
        }
        else
        {
            SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                3001, "set_fail");
        }
        return;
    }

    /* GET 走插件的 GetParam 接口。 */
    if (strcmp(token, "GET") == 0)
    {
        char *plugin_name = strtok_r(NULL, " \t\r\n", &saveptr);
        char *key = strtok_r(NULL, " \t\r\n", &saveptr);
        PodCameraPlugin *pPlugin;

        if (plugin_name == NULL || key == NULL)
        {
            SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                1102, "bad_get");
            return;
        }

        pPlugin = FindPlugin(pServer, plugin_name);
        if (pPlugin == NULL || pPlugin->ops.GetParam == NULL)
        {
            SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                2001, "no_plugin");
            return;
        }

        {
            char value_buf[256];
            if (pPlugin->ops.GetParam(pPlugin->ctx, key, value_buf,
                (uint32_t)sizeof(value_buf)) == 0)
            {
                char out[320];
                if (req.HasRequestId)
                {
                    (void)snprintf(out, sizeof(out), "ACK %lu 0 %s\n",
                        (unsigned long)req.RequestId, value_buf);
                }
                else
                {
                    (void)snprintf(out, sizeof(out), "ACK 0 0 %s\n",
                        value_buf);
                }
                SendText(pServer->ClientFd, out);
            }
            else
            {
                SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                    3002, "get_fail");
            }
        }
        return;
    }

    /* GIMBAL 命令由外部 hook 接管，服务器只负责协议桥接。 */
    if (strcmp(token, "GIMBAL") == 0)
    {
        char *subcmd = strtok_r(NULL, " \t\r\n", &saveptr);
        char *args = strtok_r(NULL, "\r\n", &saveptr);
        if (subcmd == NULL)
        {
            SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                1103, "bad_gimbal");
            return;
        }
        if (args == NULL)
        {
            args = "";
        }

        if (pServer->Config.GimbalHook != NULL)
        {
            int32_t ret = pServer->Config.GimbalHook(subcmd, args,
                pServer->Config.pGimbalUser);
            if (ret == 0)
            {
                SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                    0, "OK");
            }
            else
            {
                SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                    3003, "gimbal_fail");
            }
        }
        else
        {
            SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
                2002, "no_gimbal_hook");
        }
        return;
    }

    SendAck(pServer->ClientFd, req.HasRequestId, req.RequestId,
        1003, "unknown_cmd");
}

static void ParseLines(PodTcpControlServer *pServer)
{
    uint32_t start = 0u;
    uint32_t i;

    /* 按换行拆包，支持一次 recv 包含多条命令。 */
    for (i = 0u; i < pServer->RxLen; i++)
    {
        if (pServer->RxBuf[i] == '\n')
        {
            pServer->RxBuf[i] = '\0';
            HandleCommand(pServer, pServer->RxBuf + start);
            start = i + 1u;
        }
    }

    if (start > 0u)
    {
        uint32_t remain = pServer->RxLen - start;
        if (remain > 0u)
        {
            memmove(pServer->RxBuf, pServer->RxBuf + start, remain);
        }
        pServer->RxLen = remain;
    }
}
#endif

bool PodTcpControlServer_create(PodTcpControlServer **ppServer,
    const PodTcpControlConfig *pConfig)
{
    PodTcpControlServer *pServer;

    ZF_ASSERT(ppServer != (PodTcpControlServer **)0)
    ZF_ASSERT(pConfig != (PodTcpControlConfig *)0)

    /* 服务对象本身只保存配置和连接状态。 */
    pServer = (PodTcpControlServer *)ZF_MALLOC(sizeof(PodTcpControlServer));
    if (pServer == NULL)
    {
        *ppServer = NULL;
        return false;
    }

    memset(pServer, 0, sizeof(PodTcpControlServer));
    pServer->Config = *pConfig;

#if defined(__linux__)
    pServer->ListenFd = -1;
    pServer->ClientFd = -1;
#endif

    *ppServer = pServer;
    return true;
}

void PodTcpControlServer_dispose(PodTcpControlServer *pServer)
{
    if (pServer == NULL)
    {
        return;
    }

    (void)PodTcpControlServer_stop(pServer);
    ZF_FREE(pServer);
}

int32_t PodTcpControlServer_start(PodTcpControlServer *pServer)
{
    ZF_ASSERT(pServer != (PodTcpControlServer *)0)

#if defined(__linux__)
    struct sockaddr_in addr;
    int flags;

    /* 建立监听 socket，后续 pollOnce 里再做非阻塞 accept。 */
    pServer->ListenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (pServer->ListenFd < 0)
    {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(pServer->Config.ListenPort);

    if (bind(pServer->ListenFd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        CloseFdSafe(&pServer->ListenFd);
        return -2;
    }

    if (listen(pServer->ListenFd, 1) != 0)
    {
        CloseFdSafe(&pServer->ListenFd);
        return -3;
    }

    flags = fcntl(pServer->ListenFd, F_GETFL, 0);
    (void)fcntl(pServer->ListenFd, F_SETFL, flags | O_NONBLOCK);

    pServer->IsStarted = 1u;
    return 0;
#else
    return -10;
#endif
}

int32_t PodTcpControlServer_stop(PodTcpControlServer *pServer)
{
    ZF_ASSERT(pServer != (PodTcpControlServer *)0)

#if defined(__linux__)
    CloseFdSafe(&pServer->ClientFd);
    CloseFdSafe(&pServer->ListenFd);
    pServer->RxLen = 0u;
#endif

    pServer->IsStarted = 0u;
    return 0;
}

int32_t PodTcpControlServer_pollOnce(PodTcpControlServer *pServer)
{
    ZF_ASSERT(pServer != (PodTcpControlServer *)0)

#if defined(__linux__)
    if (!pServer->IsStarted)
    {
        return -1;
    }

    /* 单客户端模型：没有客户端时先 accept。 */
    if (pServer->ClientFd < 0)
    {
        pServer->ClientFd = accept(pServer->ListenFd, NULL, NULL);
        if (pServer->ClientFd >= 0)
        {
            int flags = fcntl(pServer->ClientFd, F_GETFL, 0);
            (void)fcntl(pServer->ClientFd, F_SETFL, flags | O_NONBLOCK);
            /* 新客户端接入后先回一个欢迎消息，便于上位机确认连接建立。 */
            SendText(pServer->ClientFd, "WELCOME\n");
        }
        return 0;
    }

    if (pServer->RxLen >= POD_CTRL_BUF_SIZE)
    {
        /* 防御异常输入：行过长直接断开，避免缓冲区长期占满。 */
        SendAck(pServer->ClientFd, 0u, 0u, 1004, "line_too_long");
        CloseFdSafe(&pServer->ClientFd);
        pServer->RxLen = 0u;
        return 0;
    }

    if (pServer->RxLen < POD_CTRL_BUF_SIZE)
    {
        int recv_len = recv(pServer->ClientFd,
            pServer->RxBuf + pServer->RxLen,
            POD_CTRL_BUF_SIZE - pServer->RxLen, 0);

        if (recv_len > 0)
        {
            pServer->RxLen += (uint32_t)recv_len;
            ParseLines(pServer);
        }
        else if (recv_len == 0)
        {
            CloseFdSafe(&pServer->ClientFd);
            pServer->RxLen = 0u;
        }
    }

    return 0;
#else
    return -10;
#endif
}
