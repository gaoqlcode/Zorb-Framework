# 目录与功能分层规范（RK3588 Linux 主线）

## 1. 目标
- 让代码“按职责聚合”，避免所有模块平铺在同一目录。
- 新项目复制时，能快速定位要改的层级。
- 便于后续做子域负责人协作（transport/control/runtime/codec）。

## 2. 顶层分层
- `inc/core` + `src/core`：通用基础能力（容器、事件、任务、时间等）。
- `inc/platform` + `src/platform`：平台相关能力（RK3588/Linux 绑定实现）。
- `inc/pod` + `src/pod`：吊舱业务域。
- `src/demo`：最小可运行示例。
- `doc`：设计、协议、构建与规范文档。

## 3. 吊舱域子分层（pod）
- `plugin`：相机插件接口与插件管理。
- `runtime`：运行时组织、配置加载。
- `codec`：编码与码率控制（含自适应）。
- `transport`：UDP 视频发送链路。
- `control`：TCP 控制与命令协议。
- `data`：帧池、队列等数据通道基础设施。

## 4. 文件放置规则
- 新增 `pod_*.h/.c` 必须放入上述六个子域之一。
- 同一模块头源文件保持同名，例如：
  - `inc/pod/transport/pod_udp_streamer.h`
  - `src/pod/transport/pod_udp_streamer.c`
- 平台特有实现只放 `platform`，不直接侵入 `pod` 业务层。

## 5. include 约定
- 业务源码优先使用短 include（依赖 CMake include 目录）。
- fallback include 统一指向新目录，避免编辑器诊断误报。
- 聚合头入口为 `inc/core/zf_includes.h`，对外导出完整能力集合。

## 6. CMake 约定
- `POD_CORE_SOURCES` 按子域分组维护，新增文件时同步更新。
- include 目录需覆盖 `inc/pod/*` 子目录，保证短 include 生效。

## 7. 后续扩展建议
- 当 `pod` 规模继续增长，可在子域下再按“接口/实现/adapter”细分。
- 若接入 MPP，建议放入 `codec/mpp/`，并保留统一 `pod_encoder` 抽象接口。

## 8. 配套索引
- 模块入口与职责清单见 `doc/pod_module_index.md`。

## 9. 注释与编码规范（学习向）
- 核心链路函数应补充“输入/输出/失败处理”说明，便于新人快速理解。
- 注释优先说明“为什么这样设计”，不要只描述代码字面动作。
- 仓库统一使用 UTF-8 保存，避免出现乱码注释。
- 若发现历史乱码（如异常汉字串），应在功能不变前提下优先修复注释文本。
