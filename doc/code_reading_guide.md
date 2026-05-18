# Zorb Framework 代码阅读导图

这份文档的目标不是替代源码，而是帮你决定：

1. 先看哪一层
2. 每一层在解决什么问题
3. 读代码时应该重点盯哪些结构体和函数

如果你已经看到项目里补充过的详细注释，这份导图可以作为总索引使用。

配套文档：

1. [demo 启动到退出调用链分析](doc/demo_call_chain_analysis.md)

## 一、先建立整体图景

这个工程可以粗略看成两层：

1. core 层
   提供通用嵌入式基础能力，例如断言、时间、链表、事件、定时器、任务。
2. pod 层
   在 core 层之上实现吊舱视频业务，包括插件、运行时、帧池、编码、传输、控制、自适应码率。

如果你先看业务，再回头看底层，会更容易理解“这些通用能力为什么存在”。

## 二、推荐阅读顺序

### 第 1 步：看最小主流程

先看 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c)。

这一文件最值得回答的问题有三个：

1. 运行时是怎么创建的
2. 编码器和自适应码率是怎么挂上的
3. 资源销毁为什么要按逆序做

如果你能顺着 main 把 create、start、poll、stop、dispose 走一遍，整个工程的“业务骨架”就清楚了。

### 第 2 步：看运行时主干

看 [inc/pod/runtime/pod_runtime.h](inc/pod/runtime/pod_runtime.h) 和 [src/pod/runtime/pod_runtime.c](src/pod/runtime/pod_runtime.c)。

这里要重点理解：

1. PodRuntime 不是采集设备本身，而是“把插件产出的帧整理成系统内可消费数据”的中间层
2. PodRuntime_start 的顺序为什么是 Init -> Open -> StartStream
3. PodRuntime_pollOnce 为什么采用“失败即跳过”的实时策略

建议重点盯这几个符号：

1. PodRuntime
2. PodRuntime_create
3. PodRuntime_start
4. PodRuntime_pollOnce
5. PodRuntime_releaseBlock

### 第 3 步：看帧数据怎么流动

看 [inc/pod/data/pod_frame_pool.h](inc/pod/data/pod_frame_pool.h)、[src/pod/data/pod_frame_pool.c](src/pod/data/pod_frame_pool.c)、[inc/pod/data/pod_perf_queue.h](inc/pod/data/pod_perf_queue.h)、[src/pod/data/pod_perf_queue.c](src/pod/data/pod_perf_queue.c)。

这一组文件要回答的是：

1. 为什么要把 free 队列和 ready 队列分开
2. 为什么 SPSC 队列要求容量必须是 2 的幂
3. 为什么帧块复用比频繁 malloc/free 更适合实时视频链路

建议盯住这些结构体：

1. PodFrameBlock
2. PodFramePool
3. PodSpscQueue

### 第 4 步：看插件抽象

看 [inc/pod/plugin/pod_camera_plugin.h](inc/pod/plugin/pod_camera_plugin.h) 和 [src/pod/plugin/pod_plugin_manager.c](src/pod/plugin/pod_plugin_manager.c)。

这里最重要的不是实现复杂度，而是“边界定义”：

1. 插件必须提供什么能力
2. 插件管理器如何保证 name 和 stream_id 唯一
3. 业务层为什么要通过接口表而不是直接依赖具体设备代码

如果你未来要接新的相机来源，这一组文件就是第一入口。

### 第 5 步：看编码与网络反馈闭环

看 [inc/pod/codec/pod_encoder.h](inc/pod/codec/pod_encoder.h)、[src/pod/codec/pod_encoder.c](src/pod/codec/pod_encoder.c)、[inc/pod/codec/pod_adaptive_rate.h](inc/pod/codec/pod_adaptive_rate.h)、[src/pod/codec/pod_adaptive_rate.c](src/pod/codec/pod_adaptive_rate.c)。

这里要理解：

1. 当前编码器实现为什么是“流程骨架”而不是完整硬编驱动
2. 自适应码率为什么只负责决策，不直接写编码器内部状态
3. UDP 丢包是怎么转成“降低码率”的

建议看这条调用链：

1. PodAdaptiveRate_udpDropHook
2. PodAdaptiveRate_tick
3. PodAdaptiveRate_applyToEncoder
4. PodEncoder_setBitrateKbps

### 第 6 步：看传输层

看 [inc/pod/transport/pod_udp_streamer.h](inc/pod/transport/pod_udp_streamer.h) 和 [src/pod/transport/pod_udp_streamer.c](src/pod/transport/pod_udp_streamer.c)。

这里的重点是：

1. 一帧数据如何被拆成多个 UDP 分片
2. 发送失败为什么只统计、不重传
3. 丢包统计为什么要回调给上层策略模块

### 第 7 步：看控制面

看 [inc/pod/control/pod_tcp_control.h](inc/pod/control/pod_tcp_control.h) 和 [src/pod/control/pod_tcp_control.c](src/pod/control/pod_tcp_control.c)。

这一组文件的阅读目标是：

1. 理解控制协议是如何解析的
2. LIST/GET/SET/GIMBAL 分别路由到哪里
3. 为什么控制服务只做协议桥接，而不直接承载设备逻辑

### 第 8 步：回到底层 core 层

当你已经知道业务层怎么跑，再回来看 core 会更有感觉。

推荐顺序：

1. [src/core/zf_list.c](src/core/zf_list.c)
2. [src/core/zf_buffer.c](src/core/zf_buffer.c)
3. [src/core/zf_event.c](src/core/zf_event.c)
4. [src/core/zf_timer.c](src/core/zf_timer.c)
5. [src/core/zf_time.c](src/core/zf_time.c)
6. [src/core/zf_task_schedule.c](src/core/zf_task_schedule.c)
7. [src/core/zf_task.c](src/core/zf_task.c)
8. [src/core/zf_fsm.c](src/core/zf_fsm.c)

## 三、各模块一句话理解

### core 层

1. zf_assert
   发生严重内部错误时立即停机，方便尽早暴露问题。
2. zf_debug
   用最简单的 printf 提供统一日志入口。
3. zf_malloc
   给整个工程提供统一内存抽象，便于后续替换分配器。
4. zf_list
   最基础的容器，很多其他模块都依赖它组织对象。
5. zf_buffer
   用于处理连续字节流的环形缓冲区。
6. zf_event
   把离散工作项封装成事件，并交给事件处理器按优先级执行。
7. zf_timer
   按系统 tick 驱动的软定时器，可以直接回调，也可以转事件。
8. zf_time
   提供全局 tick、毫秒时间和延时能力，是 timer/task 的时间基准。
9. zf_task_schedule
   只负责一件事：找出当前最该运行的任务。
10. zf_task
   提供任务对象、延时、调度和空闲任务机制。
11. zf_fsm
   提供有限状态机和父子状态机层次结构。

### pod 层

1. pod_camera_plugin
   定义设备插件和上层运行时之间的契约。
2. pod_plugin_manager
   管理插件集合，并统一执行生命周期。
3. pod_perf_queue
   提供轻量 SPSC 无锁队列，适合高频流水线。
4. pod_frame_pool
   负责帧块复用和 ready/free 队列管理。
5. pod_profile
   保存系统配置，是运行时和编码器的重要参数来源。
6. pod_runtime
   负责把插件产帧转换成系统可消费的 ready 帧。
7. pod_encoder
   当前是编码流程骨架和参数容器。
8. pod_udp_streamer
   把帧切片后通过 UDP 发给地面端。
9. pod_tcp_control
   提供文本协议控制入口。
10. pod_adaptive_rate
   根据丢包反馈动态调整编码参数。

## 四、读代码时建议重点盯住的结构体

如果你不想一开始就把全部函数都看完，先抓住下面这些结构体，会效率更高：

1. Task
2. Event
3. EventHandler
4. Timer
5. RingBuffer
6. List
7. PodFrameBlock
8. PodFramePool
9. PodSpscQueue
10. PodCameraPlugin
11. PodPluginManager
12. PodRuntime
13. PodEncoder
14. PodAdaptiveRate
15. PodTcpControlServer
16. PodUdpStreamer

## 五、推荐的学习方法

### 方法一：顺着调用链读

适合第一次看项目。

从 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c) 出发，遇到函数就跳进去看，优先看 create/start/poll/stop/dispose 这类主流程函数。

### 方法二：围绕一个结构体读

适合已经知道整体流程，但想吃透局部模块。

例如你想理解任务系统，就只围绕 Task 结构体，把所有对它字段的读写看一遍。

### 方法三：围绕一个问题读

适合准备二次开发时使用。

可以按下面的问题选入口：

1. 我想接新相机：从 pod_camera_plugin 和 pod_plugin_manager 开始
2. 我想看视频帧怎么流动：从 pod_runtime 和 pod_frame_pool 开始
3. 我想看网络不好时怎么降码率：从 pod_udp_streamer 和 pod_adaptive_rate 开始
4. 我想看底层调度：从 zf_time、zf_timer、zf_task_schedule、zf_task 开始

## 六、最后建议

这个工程最适合的阅读方式不是“从第一行读到最后一行”，而是：

1. 先看 demo 和 runtime，建立业务主线
2. 再看 frame pool / plugin / encoder / transport，理解数据链路
3. 最后回头看 core，理解这些通用能力怎么支撑上层业务

如果你后面继续让我补注释，我建议下一步就不再只是加散点注释，而是可以继续做两件更有学习价值的事：

1. 给每个核心模块补一份“典型时序说明”
2. 给 demo 补一份“从启动到退出”的调用链分析文档