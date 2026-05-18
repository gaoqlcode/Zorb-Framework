# Pod 模块索引（按子域）

## 1. plugin
- 作用：相机插件接口与插件生命周期管理。
- 头文件：
  - inc/pod/plugin/pod_camera_plugin.h
  - inc/pod/plugin/pod_plugin_manager.h
- 源文件：
  - src/pod/plugin/pod_plugin_manager.c
- 关键入口：
  - PodPluginManager_create
  - PodPluginManager_register
  - PodPluginManager_startAll / PodPluginManager_stopAll

## 2. runtime
- 作用：运行时主流程与配置加载。
- 头文件：
  - inc/pod/runtime/pod_profile.h
  - inc/pod/runtime/pod_runtime.h
- 源文件：
  - src/pod/runtime/pod_profile.c
  - src/pod/runtime/pod_runtime.c
- 关键入口：
  - PodProfile_loadFromFile
  - PodRuntime_create
  - PodRuntime_pollOnce

## 3. codec
- 作用：编码抽象与自适应码率控制。
- 头文件：
  - inc/pod/codec/pod_encoder.h
  - inc/pod/codec/pod_adaptive_rate.h
- 源文件：
  - src/pod/codec/pod_encoder.c
  - src/pod/codec/pod_adaptive_rate.c
- 关键入口：
  - PodEncoder_encode
  - PodEncoder_setBitrateKbps
  - PodAdaptiveRate_tick
  - PodAdaptiveRate_applyToEncoder

## 4. transport
- 作用：UDP 视频推流与分片发送。
- 头文件：
  - inc/pod/transport/pod_udp_streamer.h
- 源文件：
  - src/pod/transport/pod_udp_streamer.c
- 关键入口：
  - PodUdpStreamer_start
  - PodUdpStreamer_sendFrame
  - PodUdpStreamer_setDropHook

## 5. control
- 作用：TCP 控制服务与命令协议处理。
- 头文件：
  - inc/pod/control/pod_tcp_control.h
- 源文件：
  - src/pod/control/pod_tcp_control.c
- 关键入口：
  - PodTcpControlServer_start
  - PodTcpControlServer_poll

## 6. data
- 作用：数据通道基础设施（队列与帧池）。
- 头文件：
  - inc/pod/data/pod_perf_queue.h
  - inc/pod/data/pod_frame_pool.h
- 源文件：
  - src/pod/data/pod_perf_queue.c
  - src/pod/data/pod_frame_pool.c
- 关键入口：
  - PodSpscQueue_push / PodSpscQueue_pop
  - PodFramePool_acquire / PodFramePool_commit

## 7. 关联层
- platform：inc/platform + src/platform（RK3588/Linux 相关实现）。
- core：inc/core + src/core（通用基础能力）。
- demo：src/demo/pod_demo_main.c（最小可运行样例）。

## 8. 新增模块放置检查清单
- 是否放入正确子域（plugin/runtime/codec/transport/control/data）。
- 头源命名是否一致（pod_xxx.h / pod_xxx.c）。
- CMakeLists.txt 是否加入对应 domain 源文件组。
- 是否在文档中补充模块职责和关键入口。

## 9. 学习建议（源码阅读顺序）
- 第一步：先看 `plugin`。
  - 目标：理解“相机能力如何被抽象成统一接口”。
  - 建议入口：`PodCameraPluginOps`、`PodPluginManager_register`。
- 第二步：再看 `runtime`。
  - 目标：理解“采集帧如何进入统一缓冲通道”。
  - 建议入口：`PodRuntime_start`、`PodRuntime_pollOnce`。
- 第三步：看 `data`。
  - 目标：理解“为什么用预分配帧池 + SPSC 队列”。
  - 建议入口：`PodFramePool_acquireFree`、`PodFramePool_pushReady`。
- 第四步：看 `transport` 与 `control`。
  - 目标：理解“视频如何发出去、命令如何进来”。
  - 建议入口：`PodUdpStreamer_sendFrame`、`PodTcpControlServer_pollOnce`。
- 第五步：看 `codec`。
  - 目标：理解“网络状态如何反向调节编码参数”。
  - 建议入口：`PodAdaptiveRate_tick`、`PodAdaptiveRate_applyToEncoder`。

## 10. 关键时序（最小闭环）
1. `PodPluginManager_startAll` 启动各相机流。
2. `PodRuntime_pollOnce` 拉取插件帧并写入 ready 队列。
3. 上层取出帧后调用 `PodUdpStreamer_sendFrame` 发 UDP。
4. 发送失败通过 drop hook 上报给 `PodAdaptiveRate_udpDropHook`。
5. 周期调用 `PodAdaptiveRate_tick`，并通过 apply hook 更新编码器参数。
