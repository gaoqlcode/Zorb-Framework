# RK3588 平台裁剪清单

## 目标
面向 RK3588/Linux 运行环境，剥离 MCU 专用移植代码，保留可复用通用层与吊舱业务层。

## 保留模块
- `inc/core/` 与 `src/core/` 下通用容器与基础库：`zf_assert`、`zf_list`、`zf_buffer` 等。
- `inc/pod/` 与 `src/pod/` 下吊舱模块：`pod_*`。
- `inc/core/zf_malloc.h`、`inc/core/zf_debug.h`（Linux 通用实现）。
- `inc/platform/zf_critical.h`、`src/platform/zf_critical_stub.c`（Linux 兼容占位实现）。

## 删除模块
- `ports/zf_critical.h`
- `ports/zf_critical.c`
- `ports/zf_debug.h`
- `ports/zf_malloc.h`
- `doc/board_ports.md`

删除原因：以上文件属于 Cortex-M 专用移植层或历史 ports 目录冗余封装，RK3588/Linux 主线下不使用。

## 构建建议
- Linux 目标默认不编译 `ports/` 目录。
- 若需保留历史 MCU 分支，请在独立分支维护，不与 RK3588 主线混编。
- Linux 构建入口：`CMakeLists.txt`，使用说明见 `doc/linux_build.md`。
- 最小可运行示例：`src/demo/pod_demo_main.c`（目标名 `zorb_pod_demo`）。

## 当前状态
- `ports/` 目录已清空，不再承载主线功能代码。

## 说明
- 当前 `zf_task` 相关接口在 Linux 下由 `zf_critical_stub` 提供兼容实现，确保代码可编译。
- 若业务完全不使用 `zf_task`，可在后续继续移除任务系统相关文件以进一步瘦身。
