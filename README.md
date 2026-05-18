# Zorb Framework

Zorb Framework 是一个面向嵌入式场景的轻量级 C 框架，目标是把常见基础能力和吊舱视频业务骨架拆分成可复用模块，减少重复造轮子。

当前仓库主线已聚焦 Linux/RK3588 场景，并提供最小可运行 demo 作为学习与联调入口。

## 项目目标

1. 以 core 层提供通用基础能力：时间、缓冲区、链表、状态机、事件、定时器、任务。
2. 以 pod 层提供吊舱业务能力：插件管理、运行时、帧池、编码、传输、控制、自适应码率。
3. 支持按子域裁剪构建，满足“只保留必要能力”的工程化需求。
4. 通过统一注释和阅读文档降低上手成本。

## 目录结构

1. inc/core + src/core：通用基础模块。
2. inc/platform + src/platform：平台适配层。
3. inc/pod + src/pod：吊舱业务域（plugin/runtime/data/codec/transport/control）。
4. src/demo：最小可运行示例。
5. doc：构建说明、目录规范、阅读导图等文档。

详细目录规范见 [doc/folder_conventions.md](doc/folder_conventions.md)。

## 快速开始

### 环境要求

1. CMake >= 3.16
2. GCC/Clang（C99）
3. Linux pthread（Linux 默认可用）

### 默认构建

在仓库根目录执行：

```bash
cmake -S . -B build
cmake --build build -j
```

生成产物：

1. build/libzorb_pod_core.a
2. build/zorb_pod_demo

### 运行 demo

```bash
./build/zorb_pod_demo
```

当前 demo 是“功能覆盖型样例”，输出会按模块分段。典型输出片段类似：

```text
========== demo: core modules ==========
[core/rb] first byte=1 remain=3
[core/list] count=1 first=10
[core/spsc] pop=alpha
[core/event] exec_count=1
[core/fsm] enter=2 exit=1 work=1
[core/timer] timer_fire_count=1 systick=3

========== demo: pod modules ==========
[pod/plugin] count=3
[pod/plugin] exposure=180
[pod/pipeline] stream=1 encoded len=... key=...
[pod/pipeline] stream=2 encoded len=... key=...
[pod/pipeline] stream=3 encoded len=... key=...
[pod/pipeline] encoded stream count=3
[pod/adapt] bitrate=... kbps gop=... drops=...
```

### 综合 demo 输出解读

你可以把输出和代码一一对照，建议顺序如下：

1. core 分段日志：先看 core 基础能力是否正常。
2. pod 分段日志：再看业务链路是否跑通。
3. 若某一段缺失，优先定位该段对应模块，而不是全局排查。

关键日志与模块映射：

1. `[core/rb]` 对应 ring buffer：见 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c) 里的 `DemoRunCoreModules`。
2. `[core/list]` 对应链表容器：见 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c)。
3. `[core/spsc]` 对应无锁单生产者单消费者队列：见 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c) 与 [src/pod/data/pod_perf_queue.c](src/pod/data/pod_perf_queue.c)。
4. `[core/event]` 对应事件派发：见 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c) 与 [src/core/zf_event.c](src/core/zf_event.c)。
5. `[core/fsm]` 对应状态迁移：见 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c) 与 [src/core/zf_fsm.c](src/core/zf_fsm.c)。
6. `[core/timer]` 对应定时器与系统 tick：见 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c)、[src/core/zf_timer.c](src/core/zf_timer.c)、[src/core/zf_time.c](src/core/zf_time.c)。
7. `[pod/plugin]` 对应插件注册、查找、参数读写：见 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c)、[src/pod/plugin/pod_plugin_manager.c](src/pod/plugin/pod_plugin_manager.c)。
8. `[pod/pipeline]` 对应 runtime 拉帧、编码、UDP 发送链路：见 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c)、[src/pod/runtime/pod_runtime.c](src/pod/runtime/pod_runtime.c)、[src/pod/codec/pod_encoder.c](src/pod/codec/pod_encoder.c)、[src/pod/transport/pod_udp_streamer.c](src/pod/transport/pod_udp_streamer.c)。
9. `[pod/adapt]` 对应丢包反馈和自适应码率：见 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c) 与 [src/pod/codec/pod_adaptive_rate.c](src/pod/codec/pod_adaptive_rate.c)。

常见现象说明：

1. `encoded len` 和 `bitrate` 不是固定值，这是正常现象。
2. TCP 控制服务默认会启动，但没有外部客户端连接时不会打印控制命令。
3. 若你在同机重复运行 demo，端口被占用时可能导致控制服务或 UDP 目标初始化失败，请先释放端口再重试。

## 构建开关说明

核心开关定义在 [CMakeLists.txt](CMakeLists.txt)：

1. BUILD_POD_CORE：构建主线静态库 zorb_pod_core。
2. BUILD_ZF_LEGACY_CORE：构建 legacy core 静态库。
3. BUILD_POD_DEMO：构建最小 demo 可执行程序。

pod 子域开关：

1. BUILD_POD_PLUGIN
2. BUILD_POD_DATA
3. BUILD_POD_RUNTIME
4. BUILD_POD_CODEC
5. BUILD_POD_TRANSPORT
6. BUILD_POD_CONTROL
7. BUILD_PLATFORM_LAYER
8. BUILD_CORE_LAYER

依赖约束（由 CMake 直接检查）：

1. BUILD_POD_RUNTIME=ON 需要 plugin/data/platform 同时 ON。
2. BUILD_POD_CODEC=ON 需要 platform ON。
3. BUILD_POD_DEMO=ON 需要 plugin/data/runtime/codec/platform/core 同时 ON。

更完整的 Linux 构建说明见 [doc/linux_build.md](doc/linux_build.md)。

## 模块速览

### core 层

1. zf_assert：断言与错误停机入口。
2. zf_debug：统一日志输出宏。
3. zf_malloc：内存抽象与平台映射。
4. zf_buffer：环形缓冲区。
5. zf_list：单向链表容器。
6. zf_fsm：有限状态机。
7. zf_event：事件与处理器。
8. zf_timer：软件定时器。
9. zf_task + zf_task_schedule：任务与调度。
10. zf_time：系统 tick 与时间基准。

### pod 层

1. plugin：相机插件抽象与插件管理。
2. data：SPSC 队列与帧池。
3. runtime：运行时主流程和 profile。
4. codec：编码器骨架与自适应码率控制。
5. transport：UDP 分片发送。
6. control：TCP 文本协议控制。

## 学习入口（推荐顺序）

1. [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c)
2. [inc/pod/runtime/pod_runtime.h](inc/pod/runtime/pod_runtime.h) 与 [src/pod/runtime/pod_runtime.c](src/pod/runtime/pod_runtime.c)
3. [inc/pod/data/pod_frame_pool.h](inc/pod/data/pod_frame_pool.h) 与 [src/pod/data/pod_frame_pool.c](src/pod/data/pod_frame_pool.c)
4. [inc/pod/plugin/pod_plugin_manager.h](inc/pod/plugin/pod_plugin_manager.h) 与 [src/pod/plugin/pod_plugin_manager.c](src/pod/plugin/pod_plugin_manager.c)
5. [inc/pod/codec/pod_encoder.h](inc/pod/codec/pod_encoder.h) 与 [inc/pod/codec/pod_adaptive_rate.h](inc/pod/codec/pod_adaptive_rate.h)
6. [inc/core/zf_task.h](inc/core/zf_task.h) 与 [src/core/zf_task.c](src/core/zf_task.c)

## 学习文档

1. [代码阅读导图](doc/code_reading_guide.md)
2. [demo 启动到退出调用链分析](doc/demo_call_chain_analysis.md)
3. [相机插件开发快速上手](doc/camera_plugin_quickstart.md)
4. [Linux 构建说明](doc/linux_build.md)
5. [目录与分层规范](doc/folder_conventions.md)

## 历史测试文章

1. [环形缓冲区测试例子](https://www.cnblogs.com/54zorb/p/9278680.html)
2. [列表测试例子](https://www.cnblogs.com/54zorb/p/9279805.html)
3. [状态机测试例子](https://www.cnblogs.com/54zorb/p/9285805.html)
4. [事件测试例子](https://www.cnblogs.com/54zorb/p/9325298.html)
5. [定时器测试例子](https://www.cnblogs.com/54zorb/p/9325510.html)
6. [任务测试例子](https://www.cnblogs.com/54zorb/p/9337754.html)

