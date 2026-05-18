# Linux 构建说明（RK3588）

## 目标
- 仅构建 Linux/RK3588 主线代码。
- 不包含任何 MCU 专用移植代码。

## 前置依赖
- CMake >= 3.16
- GCC/Clang（支持 C99）
- pthread（Linux 默认可用）

说明：
- 使用 `CMakePresets.json` 需要 CMake >= 3.20。
- 若环境只有 CMake 3.16，可继续使用下方手动 `-D` 参数方式。

## 构建命令
在仓库根目录执行：

```bash
cmake -S . -B build -DBUILD_POD_CORE=ON -DBUILD_ZF_LEGACY_CORE=OFF
cmake --build build -j
```

## 一键预设构建（推荐）

仓库已提供 3 套预设（见 `CMakePresets.json`）：
- `full`：完整功能（所有子域 + demo）。
- `stream_only`：仅流媒体链路（data + codec + transport + platform + core）。
- `control_only`：仅控制链路（control + platform + core）。

注意：
- 上述 presets 为 Linux 专用，生成器固定为 `Unix Makefiles`。
- Windows 环境请不要使用这些 presets（按你的要求已移除 Windows preset）。

使用方式：

```bash
cmake --preset full
cmake --build --preset full
```

```bash
cmake --preset stream_only
cmake --build --preset stream_only
```

```bash
cmake --preset control_only
cmake --build --preset control_only
```

## 子域裁剪开关（新增）

可在 CMake 配置阶段按子域开关裁剪：

- `BUILD_POD_PLUGIN`：插件域
- `BUILD_POD_DATA`：数据通道域
- `BUILD_POD_RUNTIME`：运行时域
- `BUILD_POD_CODEC`：编码域
- `BUILD_POD_TRANSPORT`：UDP 传输域
- `BUILD_POD_CONTROL`：TCP 控制域
- `BUILD_PLATFORM_LAYER`：平台层
- `BUILD_CORE_LAYER`：基础层

依赖约束：
- `BUILD_POD_RUNTIME=ON` 需要 `BUILD_POD_PLUGIN=ON`、`BUILD_POD_DATA=ON`、`BUILD_PLATFORM_LAYER=ON`。
- `BUILD_POD_CODEC=ON` 需要 `BUILD_PLATFORM_LAYER=ON`。
- `BUILD_POD_DEMO=ON` 需要 plugin/data/runtime/codec/platform/core 全部开启。

裁剪示例：仅保留“控制 + 传输 + 基础能力”（不构建 demo）

```bash
cmake -S . -B build \
	-DBUILD_POD_CORE=ON \
	-DBUILD_POD_DEMO=OFF \
	-DBUILD_POD_PLUGIN=OFF \
	-DBUILD_POD_DATA=OFF \
	-DBUILD_POD_RUNTIME=OFF \
	-DBUILD_POD_CODEC=OFF \
	-DBUILD_POD_TRANSPORT=ON \
	-DBUILD_POD_CONTROL=ON \
	-DBUILD_PLATFORM_LAYER=ON \
	-DBUILD_CORE_LAYER=ON
cmake --build build -j
```

产物：
- `build/libzorb_pod_core.a`
- `build/zorb_pod_demo`（当 `BUILD_POD_DEMO=ON`）

## 运行最小示例

```bash
./build/zorb_pod_demo
```

正常输出示例：

```text
zorb_pod_demo ok
```

## 说明
- 当前构建目标为静态库，便于你在多个吊舱项目中复用。
- 若需要构建可执行程序，可在业务仓增加 `main.c` 并链接该静态库。

## 与裁剪清单关系
- 裁剪标准见：`doc/rk3588_pruning.md`
- 本构建脚本默认按该清单执行，不依赖 `ports/` 目录。
