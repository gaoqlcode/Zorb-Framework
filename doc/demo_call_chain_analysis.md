# zorb_pod_demo 启动到退出调用链分析

本文聚焦 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c) 的执行过程，按时间顺序拆成多个阶段。

阅读方式建议：

1. 先看“成功主路径”
2. 再看“失败回滚路径”
3. 最后对照“关键模块职责表”回到实现细节

## 1. 成功主路径（按阶段）

### 阶段 A：入口与基础对象声明

入口在 [src/demo/pod_demo_main.c](src/demo/pod_demo_main.c#L33)。

此阶段只做三件事：

1. 声明四个核心对象指针：插件管理器、运行时、编码器、自适应码率控制器
2. 声明三份配置结构：运行时初始化、编码器配置、自适应配置
3. 保持所有指针初值为 NULL，便于失败分支安全清理

### 阶段 B：加载默认配置

调用 [PodProfile_setDefaults](src/demo/pod_demo_main.c#L53)。

这一步把运行时 profile 填成可运行默认值，后续编码器配置直接复用该 profile 字段。

### 阶段 C：创建插件管理器

调用 [PodPluginManager_create](src/demo/pod_demo_main.c#L55)，实现位于 [src/pod/plugin/pod_plugin_manager.c](src/pod/plugin/pod_plugin_manager.c#L58)。

结果：

1. 成功则得到插件容器对象
2. 失败则 main 直接返回 1

### 阶段 D：创建运行时

先注入依赖：`init.pPluginManager = pMgr`，然后调用 [PodRuntime_create](src/demo/pod_demo_main.c#L64)，实现位于 [src/pod/runtime/pod_runtime.c](src/pod/runtime/pod_runtime.c#L96)。

运行时创建时会做两件关键动作：

1. 复制 profile 快照
2. 将 CPU 规划下发到平台层

失败回滚：释放插件管理器并返回 2。

### 阶段 E：创建并打开编码器

先组装 `enc_cfg`，再执行：

1. [PodEncoder_create](src/demo/pod_demo_main.c#L83) -> [src/pod/codec/pod_encoder.c](src/pod/codec/pod_encoder.c#L27)
2. [PodEncoder_open](src/demo/pod_demo_main.c#L83) -> [src/pod/codec/pod_encoder.c](src/pod/codec/pod_encoder.c#L59)

失败回滚：释放运行时、插件管理器并返回 5。

### 阶段 F：创建自适应码率控制器

先组装 `ar_cfg`，再调用 [PodAdaptiveRate_create](src/demo/pod_demo_main.c#L105)，实现位于 [src/pod/codec/pod_adaptive_rate.c](src/pod/codec/pod_adaptive_rate.c#L73)。

成功后再绑定下发钩子：

1. [PodAdaptiveRate_setApplyHook](src/demo/pod_demo_main.c#L122)
2. 使用默认桥接器 [PodAdaptiveRate_applyToEncoder](src/pod/codec/pod_adaptive_rate.c#L228)

这意味着：策略层只算“要调到多少码率/GOP”，真正修改编码器由 hook 完成。

失败回滚：关闭并释放编码器，释放运行时和插件管理器，返回 6。

### 阶段 G：演示一次网络反馈闭环

示例主动触发：

1. [PodAdaptiveRate_udpDropHook](src/demo/pod_demo_main.c#L128)
2. [PodAdaptiveRate_tick](src/demo/pod_demo_main.c#L129)

这一步不是必须流程，而是为了演示“丢包统计 -> 决策评估”的闭环入口。

### 阶段 H：启动运行时（拉起插件生命周期）

调用 [PodRuntime_start](src/demo/pod_demo_main.c#L129)，其内部顺序在 [src/pod/runtime/pod_runtime.c](src/pod/runtime/pod_runtime.c#L141)：

1. PodPluginManager_initAll
2. PodPluginManager_openAll
3. PodPluginManager_startAll

对应实现位置：

1. [src/pod/plugin/pod_plugin_manager.c#L205](src/pod/plugin/pod_plugin_manager.c#L205)
2. [src/pod/plugin/pod_plugin_manager.c#L228](src/pod/plugin/pod_plugin_manager.c#L228)
3. [src/pod/plugin/pod_plugin_manager.c#L251](src/pod/plugin/pod_plugin_manager.c#L251)

失败回滚：释放运行时和插件管理器，返回 3。

### 阶段 I：轮询一次采集

调用 [PodRuntime_pollOnce](src/demo/pod_demo_main.c#L138)，实现位于 [src/pod/runtime/pod_runtime.c](src/pod/runtime/pod_runtime.c#L182)。

本轮关键动作：

1. 逐插件尝试拉帧
2. 把帧写入对应流的 frame pool ready 队列
3. 如果池满或队列满，记录 drop 计数

### 阶段 J：停止运行时

调用 [PodRuntime_stop](src/demo/pod_demo_main.c#L140)，其内部顺序在 [src/pod/runtime/pod_runtime.c](src/pod/runtime/pod_runtime.c#L166)：

1. PodPluginManager_stopAll
2. PodPluginManager_closeAll

对应实现位置：

1. [src/pod/plugin/pod_plugin_manager.c#L274](src/pod/plugin/pod_plugin_manager.c#L274)
2. [src/pod/plugin/pod_plugin_manager.c#L297](src/pod/plugin/pod_plugin_manager.c#L297)

失败回滚：仍然执行后续释放并返回 4。

### 阶段 K：按逆序释放资源并退出

最终顺序：

1. [PodAdaptiveRate_dispose](src/demo/pod_demo_main.c#L152)
2. PodEncoder_close + [PodEncoder_dispose](src/demo/pod_demo_main.c#L154)
3. [PodRuntime_dispose](src/demo/pod_demo_main.c#L157)
4. [PodPluginManager_dispose](src/demo/pod_demo_main.c#L158)

成功打印 `zorb_pod_demo ok` 并返回 0。

## 2. 失败回滚路径（按返回码）

main 中当前定义的返回码语义：

1. `1`：插件管理器创建失败
2. `2`：运行时创建失败
3. `3`：运行时启动失败
4. `4`：运行时停止失败
5. `5`：编码器创建或打开失败
6. `6`：自适应控制器创建失败

回滚策略总体遵循“已创建对象逆序释放”的原则，这一点对后续你自己扩展 demo 非常重要。

## 3. 关键模块职责表（对照学习）

在这个 demo 里，各模块扮演的角色：

1. `PodPluginManager`
   负责统一管理设备插件并批量执行生命周期。
2. `PodRuntime`
   负责采集轮询和帧入队，是业务流水线主干。
3. `PodEncoder`
   负责编码参数容器与编码骨架接口。
4. `PodAdaptiveRate`
   负责网络反馈驱动的码率决策。
5. `PodFramePool/PodSpscQueue`
   负责高频帧块复用与无锁队列流转。

## 4. 一条可复用的阅读口诀

阅读这种系统入口时，可以一直套用下面四步：

1. 先看 create 链，理解对象关系
2. 再看 start/poll 链，理解运行期数据流
3. 再看 stop/dispose 链，理解资源边界
4. 最后看所有失败分支，理解可靠性策略

按这个方式，你可以把同样的方法迁移到其他入口函数，不只限于当前 demo。