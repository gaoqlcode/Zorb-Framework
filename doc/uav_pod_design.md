# 基于 Zorb-Framework 的无人机吊舱系统设计（多相机插件化）

## 1. 目标与约束

### 1.1 业务目标
- 支持多个相机并行接入：可见光、近红外、热红外。
- 相机接口类型：USB 与以太网。
- 多路视频需要时间同步保存。
- 视频通过 UDP 发送到地面客户端实时显示。
- 客户端通过 TCP 下发控制命令：云台控制、相机参数设置。
- 每个相机必须插件化，支持独立开发、替换与扩展。
- 项目整体模块化，便于维护和迭代。

### 1.2 技术约束（基于当前仓库）
- 当前 Zorb-Framework 是轻量级嵌入式内核框架，核心能力为：时间、列表、环形缓冲、状态机、事件、定时器、任务。
- ports 目录当前适配 Cortex-M3/M4。
- 框架不包含现成的视频采集、编码、RTSP、USB UVC 或网络相机协议栈。

结论：建议采用“双处理器/双进程分层架构”。
- 实时控制层（MCU）：继续基于 Zorb-Framework，负责云台电机、姿态闭环、故障保护、控制命令执行。
- 视觉处理层（Linux SBC 或 x86 工控机）：负责多相机采集、编码、时间同步、落盘、UDP 视频发送与 TCP 参数服务。

## 2. 总体分层架构

### 2.1 层次划分
- Layer A：设备驱动层
  - USB 相机驱动适配（UVC/V4L2）
  - 网口相机驱动适配（RTSP/GigE/厂商 SDK）
- Layer B：相机插件层
  - 每个相机型号一个插件
  - 统一插件接口（open/start/stop/get_frame/set_param）
- Layer C：数据与同步层
  - 全局时钟服务（单调时钟 + 可选 PTP）
  - 帧同步器（多路帧按时间窗对齐）
  - 同步录像器（同一同步组写入统一索引）
- Layer D：传输与控制层
  - UDP 视频发送器（多路流）
  - TCP 控制服务（命令-响应协议）
- Layer E：平台服务层
  - 配置中心、日志、健康监控、故障恢复
- Layer F：MCU 实时控制层（Zorb）
  - 云台控制任务
  - 安全状态机
  - 命令执行与回执

### 2.2 关键设计原则
- 数据面与控制面分离：UDP 只承载视频，TCP 只承载控制与回执。
- 插件接口稳定：业务逻辑不依赖具体相机 SDK。
- 时间先行：所有帧必须带统一时间戳与帧序号。
- 失效隔离：单个相机插件故障不拖垮全局。
- 可追溯：同步录像必须包含元数据索引。

## 3. 模块化目录建议

建议在仓库内新增一个应用域目录（不改动现有核心库）：

- app/pod_core/
  - include/
    - pod_camera_plugin.h
    - pod_plugin_manager.h
    - pod_sync.h
    - pod_recorder.h
    - pod_stream_udp.h
    - pod_control_tcp.h
    - pod_messages.h
  - src/
    - pod_plugin_manager.c
    - pod_sync.c
    - pod_recorder.c
    - pod_stream_udp.c
    - pod_control_tcp.c
- app/pod_plugins/
  - visible_usb/
  - nir_usb/
  - tir_usb/
  - visible_eth/
  - nir_eth/
  - tir_eth/
- app/pod_mcu/
  - 基于 zf_task/zf_event/zf_fsm 的云台控制应用

## 4. 相机插件接口规范

每个插件实现相同接口：
- Init(config)
- Open()
- StartStream()
- StopStream()
- GetFrame(timeout_ms)
- SetParam(key, value)
- GetParam(key)
- GetHealth()
- Close()

统一帧结构建议：
- stream_id：流标识（如 VIS/NIR/TIR）
- frame_id：帧序号
- ts_mono_us：单调时钟时间戳（微秒）
- ts_sync_us：同步时钟时间戳（微秒，可选）
- format：原始格式或编码格式
- width/height
- payload/data_len

## 5. 同步保存策略

### 5.1 时间同步
- 单机优先：使用单调时钟作为基础时间。
- 多设备可选：引入 PTP（IEEE 1588）或外部 PPS 脉冲。

### 5.2 帧对齐策略
- 以固定时间窗进行多路匹配，例如 33ms（30fps）或 40ms（25fps）。
- 主相机（可见光）作为锚点，在窗口内匹配近红外与热红外最近帧。
- 超窗策略：
  - 方案 A：补空帧标记（推荐，便于后处理）
  - 方案 B：丢弃该组（实时性优先）

### 5.3 同步录像格式
- 数据文件：按流分文件滚动保存（避免大文件损坏风险）。
- 索引文件：记录同步组信息。
  - group_id
  - 各路 frame_id 与时间戳
  - 文件偏移
  - 有效性标记

## 6. UDP 视频发送设计

### 6.1 建议流程
- 各相机插件输出原始帧。
- 编码器模块（H.264/H.265）编码。
- 按路独立 UDP 通道发送，或单通道多路复用（推荐前者，易隔离）。

### 6.2 实践建议
- 裸 UDP 容易丢包，建议自定义轻量头：
  - magic
  - stream_id
  - frame_id
  - pkt_id/pkt_count
  - ts_us
  - payload_len
- 客户端做重组与超时丢弃。
- 网络较差时支持码率动态调整（由 TCP 控制命令下发）。

## 7. TCP 控制协议设计

### 7.1 控制对象
- 云台：俯仰/方位/变倍联动、归中、模式切换。
- 相机：曝光、增益、伪彩、NUC、快门、帧率、分辨率。
- 系统：开始/停止录像、状态查询、插件重启。

### 7.2 命令执行链
- 客户端 -> TCP 服务 -> 控制路由 -> 目标模块（相机插件或 MCU）-> ACK/结果。
- 所有控制命令必须带 request_id，便于超时重试和幂等处理。

### 7.3 MCU 协同
- 推荐 Linux 视觉层与 MCU 之间使用串口/CAN/Ethernet IPC。
- Zorb 侧采用：
  - Task：控制任务、通信任务、状态上报任务
  - Event：参数变更事件、故障事件
  - FSM：安全状态机（上电自检/待机/跟踪/故障）

## 8. 关键状态机

建议至少实现以下状态：
- INIT：启动与设备发现
- READY：设备在线，待命
- STREAMING：视频发送中
- RECORDING：同步录像中
- DEGRADED：部分相机掉线，降级运行
- FAULT：关键故障，限制动作并告警

状态转换由事件触发：
- 相机上下线
- 编码器错误
- 网络拥塞
- 存储异常
- 温度/电压告警

## 9. 容错与可靠性

- 插件看门狗：超过阈值无帧则自动重启插件。
- 环形缓冲背压：发送慢于采集时，按策略丢弃旧帧或降码率。
- 存储保护：磁盘容量阈值、坏块/写失败告警。
- 关键线程隔离：采集、编码、发送、控制分离。
- 断链重连：网口相机与客户端都要支持自动重连。

## 10. 分阶段实施建议

### 阶段 1：骨架搭建
- 定义插件接口与注册机制。
- 打通 1 路可见光：采集 -> 编码 -> UDP -> 客户端显示。
- 打通 TCP 控制链路。

### 阶段 2：多相机并行
- 接入近红外、热红外插件。
- 建立同步器与索引化录像。
- 实现基本故障恢复。

### 阶段 3：工程化
- 完善状态机、指标监控、日志追踪。
- 压测网络抖动、掉包、设备掉线场景。
- 完成发布配置模板与回归测试清单。

## 11. 与 Zorb 的结合建议（重点）

Zorb 最适合承担 MCU 侧实时任务与控制状态机，不建议承载 USB/网口视频采集主链路。
建议边界：
- Zorb（MCU）：云台闭环控制、安全互锁、低延迟执行。
- Linux 视觉层：多相机采集、同步、编码、传输、录像。

这样可以同时满足“实时控制”和“多媒体吞吐”两类完全不同的系统需求。

## 12. RK3588 通用化与高性能改造（新增）

你的平台是 RK3588，建议将本框架演进为“产品线母版”，所有吊舱项目按配置复用，而不是每个项目重写。

### 12.1 统一工程分层（项目可复制）
- `core/`：协议、插件接口、通用任务编排、日志、配置。
- `platform/rk3588/`：仅放 RK 相关实现（MPP、RGA、DMA-BUF、V4L2 细节）。
- `plugins/camera/*`：各型号相机插件。
- `plugins/gimbal/*`：各云台协议插件。
- `profiles/*.yaml`：项目差异配置（相机数量、分辨率、码率、端口）。

这样新项目只需新增 profile 与少量插件，不改核心。

### 12.2 数据通道高性能策略
- 采集到编码全链路采用预分配内存池，避免运行时频繁 malloc/free。
- 线程间传输采用 SPSC 无锁队列（已新增 `pod_perf_queue`）。
- 帧对象统一使用 `pod_frame_pool` 复用，减少碎片和抖动。
- 索引回绕使用 2 的幂容量 + mask，替代取模，降低热点开销。

### 12.3 RK3588 专项优化建议
- 优先走硬件编码（MPP），避免 CPU 纯软编。
- 使用 DMA-BUF 零拷贝在 V4L2/RGA/编码器间传递图像。
- 多路流做 CPU 亲和性绑定：采集、编码、网络、控制线程分核。
- 网络发送采用批量发送和分流端口，降低单队列拥塞。
- 录像路径使用顺序写 + 周期 fsync，避免同步写放大。

### 12.4 通用化配置中心
- 所有项目差异通过配置表达：
  - 相机拓扑（VIS/NIR/TIR 数量与映射）
  - 编码参数（codec/gop/bitrate/fps）
  - UDP/TCP 端口
  - 同步窗口与丢帧策略
- 配置热更新范围应受控：仅允许非危险参数在线更新。

### 12.5 性能预算基线（建议）
- 端到端预览延迟目标：`< 180ms`（机载到客户端显示）。
- 单路 1080p30 编码占用：尽量由硬件编解码承担，CPU 仅管理调度。
- 关键线程调度抖动：`< 5ms`。
- 录像丢帧率目标：`< 0.1%`（正常链路）。

### 12.6 框架级 KPI 监控（必须）
- 每路采集 fps、编码 fps、发送 fps。
- 队列长度、队列丢弃计数。
- 每阶段耗时：采集 -> 编码 -> 发送 -> 显示。
- TCP 命令 RTT 与失败率。
- 存储写入吞吐与剩余空间。

## 13. 已落地的通用性能模块（本仓库）

- `inc/pod/data/pod_perf_queue.h` + `src/pod/data/pod_perf_queue.c`
  - 无锁 SPSC 指针队列，适合高频帧通道。
- `inc/pod/data/pod_frame_pool.h` + `src/pod/data/pod_frame_pool.c`
  - 预分配帧池 + free/ready 双队列，适合零拷贝流水线。

建议后续所有采集插件统一按该模式产出帧，避免每个项目各写一套缓存机制。

## 14. 新增可复用运行时骨架（本次继续）

为支持“多个吊舱项目快速复制”，已新增：

- `inc/pod/runtime/pod_profile.h` + `src/pod/runtime/pod_profile.c`
  - 统一配置结构 `PodProfile`。
  - 支持 `key=value` 文本配置加载。
- `inc/platform/pod_rk3588_platform.h` + `src/platform/pod_rk3588_platform.c`
  - 统一平台能力：线程绑核、线程命名、单调时钟。
- `inc/pod/runtime/pod_runtime.h` + `src/pod/runtime/pod_runtime.c`
  - 最小运行时：插件管理 + 帧池管理 + 单次轮询采集。
- `doc/profiles/rk3588_default.profile`
  - 默认工程模板配置，可直接复制改名用于新项目。

## 15. 多项目复用推荐流程

1. 复制 `rk3588_default.profile` 为项目配置。
2. 新增/替换目标相机插件，实现统一插件接口。
3. 使用 `PodRuntime` 驱动插件采集并接入 UDP/TCP 模块。
4. 仅对项目差异做配置与插件改动，核心通道保持不动。

这样可将“新吊舱项目交付”从重构模式转为组装模式。

## 16. 第三轮新增模块（编码 + UDP + TCP）

已补齐传输控制链路骨架，新增：

- `inc/pod/codec/pod_encoder.h` + `src/pod/codec/pod_encoder.c`
  - 编码抽象层，当前为低开销 bypass 骨架。
  - 后续可在不改业务层的前提下切换到 RK MPP 硬编码实现。
- `inc/pod/transport/pod_udp_streamer.h` + `src/pod/transport/pod_udp_streamer.c`
  - UDP 视频发送器，内置分片头，支持按流端口发送。
  - 为后续丢包统计与码率自适应预留了计数接口。
- `inc/pod/control/pod_tcp_control.h` + `src/pod/control/pod_tcp_control.c`
  - 非阻塞 TCP 控制服务。
  - 支持基础命令：`PING`、`SET`、`GET`、`GIMBAL`。

这三层与前面的 `PodRuntime/PodProfile` 配合后，可直接形成“可复制”的最小吊舱工程母版。

## 17. RK3588 裁剪说明

已新增裁剪清单文档：

- `doc/rk3588_pruning.md`

用于指导团队在 Linux/RK3588 项目中排除 MCU 专用移植代码，并统一保留可复用模块。

## 18. 自适应码率闭环（新增）

已新增模块：

- `inc/pod/codec/pod_adaptive_rate.h` + `src/pod/codec/pod_adaptive_rate.c`

作用：
- 接收 UDP 丢包事件并按时间窗口评估网络状态。
- 在丢包超阈值时下调码率，在稳定窗口逐步回升码率。
- 通过 `ApplyHook` 回调通知编码器层应用新的 `bitrate/gop`。

接入步骤：
1. 创建 `PodAdaptiveRate` 实例并配置窗口参数。
2. 调用 `PodUdpStreamer_setDropHook` 绑定 `PodAdaptiveRate_udpDropHook`。
3. 主循环周期调用 `PodAdaptiveRate_tick`（例如 100ms）。
4. 在 `ApplyHook` 中调用 `PodEncoder_setBitrateKbps` 与 `PodEncoder_setGop`。

推荐默认绑定：
- 直接使用 `PodAdaptiveRate_applyToEncoder` 作为 `ApplyHook`。
- 将 `PodEncoder*` 作为 `pUser` 传入，无需额外胶水代码。
