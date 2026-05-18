# 相机插件开发快速上手（无人机吊舱）

本文给出一条可直接落地的开发路径：

1. 用统一插件接口封装厂商 SDK。
2. 注册到插件管理器。
3. 让 runtime 持续拉帧并进入编码/传输链路。

## 1. 使用模板文件

模板文件：

1. [inc/pod/plugin/pod_camera_plugin_template.h](inc/pod/plugin/pod_camera_plugin_template.h)
2. [src/pod/plugin/pod_camera_plugin_template.c](src/pod/plugin/pod_camera_plugin_template.c)

你需要做的事只有两步：

1. 填写 PodSdkCameraHooks 中的 SDK 回调。
2. 在业务入口里调用 PodSdkCameraPlugin_setup 并注册。

## 2. 最小接入示例

```c
/* 你的 SDK 上下文，建议包含句柄、缓冲区、线程对象等。 */
typedef struct _VendorVisCam
{
    void *Handle;
    uint32_t Width;
    uint32_t Height;
} VendorVisCam;

static int32_t Vendor_Init(void *pUser, const char *pCfg)
{
    (void)pCfg;
    /* TODO: 解析配置并初始化 SDK */
    return 0;
}

static int32_t Vendor_Open(void *pUser)
{
    (void)pUser;
    /* TODO: 打开设备 */
    return 0;
}

static int32_t Vendor_Start(void *pUser)
{
    (void)pUser;
    /* TODO: 启动流 */
    return 0;
}

static int32_t Vendor_GetFrame(void *pUser, PodFrameMeta *pMeta,
    const uint8_t **ppPayload, uint32_t *pLen, uint32_t timeoutMs)
{
    (void)pUser;
    (void)timeoutMs;
    /* TODO: 从 SDK 取帧并填写元数据 */
    pMeta->stream_id = POD_STREAM_VIS;
    pMeta->format = POD_FMT_RAW;
    *ppPayload = NULL;
    *pLen = 0;
    return -1;
}

static int32_t Vendor_Stop(void *pUser)
{
    (void)pUser;
    return 0;
}

static int32_t Vendor_Close(void *pUser)
{
    (void)pUser;
    return 0;
}

/* 入口里装配模板插件。 */
VendorVisCam visCtx;
PodSdkCameraCtx visTplCtx;
PodCameraPlugin visPlugin;
PodSdkCameraHooks hooks = {0};

hooks.SdkInit = Vendor_Init;
hooks.SdkOpen = Vendor_Open;
hooks.SdkStart = Vendor_Start;
hooks.SdkGetFrame = Vendor_GetFrame;
hooks.SdkStop = Vendor_Stop;
hooks.SdkClose = Vendor_Close;

PodSdkCameraPlugin_setup(&visPlugin, &visTplCtx,
    "vis_vendor_cam", POD_STREAM_VIS, &hooks, &visCtx);

PodPluginManager_register(pMgr, &visPlugin);
```

## 3. 建议的线程模型

建议至少分成 3 条线程：

1. 采集线程：只负责 GetFrame，避免被编码/网络阻塞。
2. 编码线程：只负责编码和码率调节。
3. 发送线程：只负责 UDP 发送与丢包统计。

控制面（TCP）建议独立线程，避免控制指令阻塞视频主链路。

## 4. 错误码建议

建议统一约定：

1. `0`：成功。
2. `<0`：失败，按阶段分区（如 -1000 初始化类、-2000 设备类、-3000 帧超时类）。
3. 对于可恢复错误（如超时），GetFrame 返回特定错误码并由上层重试。

## 5. 开发完成后的验证清单

1. 插件生命周期是否全通过：Init/Open/Start/Stop/Close。
2. 长时间运行是否泄漏：句柄、线程、缓冲区。
3. 异常插拔或断流是否能恢复。
4. runtime 是否始终执行 release，避免帧池耗尽。
5. 码率调节在丢包场景下是否稳定收敛。
