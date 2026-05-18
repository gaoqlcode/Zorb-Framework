# 吊舱系统通信协议草案（TCP 控制 + UDP 视频）

## 1. 总体约定
- 传输分离：
  - TCP：控制命令、参数查询、状态回执。
  - UDP：视频数据。
- 字节序：小端。
- 时间单位：微秒（us）。
- 字符串编码：UTF-8。

## 2. TCP 控制协议

### 2.1 报文封装
当前实现采用文本行协议（便于快速联调），每条命令以 `\n` 结尾。

请求格式（推荐）：
- `REQ <request_id> <CMD> [args...]`

兼容格式（历史）：
- `<CMD> [args...]`

应答格式：
- `ACK <request_id> <code> <message>`
- 若请求未带 `REQ` 前缀，则 `request_id` 返回 `0`。

说明：
- `request_id`：客户端生成的无符号整型，用于重试与幂等追踪。
- `code`：0 表示成功，非 0 为错误码。
- `message`：简短结果文本，成功时可承载查询值。

后续如需二进制高性能协议，可升级到 TLV/二进制头格式。

### 2.2 命令示例
- 心跳：
  - `REQ 1001 PING`
  - `ACK 1001 0 PONG`
- 状态查询：
  - `REQ 1005 STATUS`
  - `ACK 1005 0 running=1 plugins=3 client=online`
- 插件列表：
  - `REQ 1006 LIST`
  - `ACK 1006 0 VIS_0:1,NIR_0:2,TIR_0:3`
- 设置参数：
  - `REQ 1002 SET VIS_0 exposure 1200`
  - `ACK 1002 0 OK`
- 查询参数：
  - `REQ 1003 GET VIS_0 exposure`
  - `ACK 1003 0 1200`
- 云台控制：
  - `REQ 1004 GIMBAL MOVE yaw=2.0 pitch=-1.0`
  - `ACK 1004 0 OK`

### 2.3 应答规范
- 通用应答字段：
  - code: int (0=成功，非0失败)
  - message: string
  - request_id: uint32
- 失败应明确错误分类：
  - 100x 请求格式/参数错误
  - 200x 目标对象不可用
  - 300x 执行失败

当前代码中已使用的错误码：
- `1001`: empty
- `1002`: bad_req
- `1003`: unknown_cmd
- `1004`: line_too_long
- `1101`: bad_set
- `1102`: bad_get
- `1103`: bad_gimbal
- `2001`: no_plugin
- `2002`: no_gimbal_hook
- `3001`: set_fail
- `3002`: get_fail
- `3003`: gimbal_fail

## 3. UDP 视频协议

### 3.1 UDP 包头
- magic: uint16 (0x5650)
- version: uint8
- stream_id: uint8
- frame_id: uint32
- packet_id: uint16
- packet_count: uint16
- ts_us: uint64
- flags: uint8
- payload_len: uint16

flags 建议位定义：
- bit0: key_frame
- bit1: sync_mark
- bit2: frame_start
- bit3: frame_end

### 3.2 分片与重组
- 单帧按 MTU 分片，packet_count 表示总分片数。
- 客户端在重组窗口内收齐后解码；超时丢弃并统计丢包率。
- 服务端支持丢包回调钩子，便于上层触发降码率或告警策略。
- 推荐与 `pod_adaptive_rate` 联动，实现自动降码率/升码率闭环。

### 3.3 多流规划
- stream_id 建议：
  - 1: VIS
  - 2: NIR
  - 3: TIR
- 可选策略：每路独立 UDP 端口，简化重组和 QoS 管理。

## 4. 心跳与保活

### 4.1 TCP 心跳
- 客户端每 1s 发送 PING。
- 服务端返回 PONG 并带系统状态摘要。
- 连续 N 次超时则重连。

### 4.2 视频链路健康上报
- TCP 状态消息周期上报：
  - 每路 fps
  - 编码延迟
  - 发送码率
  - 丢包率估计

## 5. 安全与鲁棒性
- 命令白名单与参数范围校验。
- request_id 去重，避免重复执行。
- 控制通道建议预留鉴权字段（token 或签名）。
- 关键命令（如固件升级/重启）采用二次确认。

## 6. 与 MCU 控制接口映射
- Linux 视觉层收到控制命令后，按映射转发到 MCU 控制总线：
  - GIMBAL_MOVE -> MCU_CMD_GIMBAL_RATE
  - GIMBAL_HOME -> MCU_CMD_GIMBAL_HOME
  - CAMERA_SET(机械部分) -> MCU_CMD_LENS/FOCUS
- MCU 返回执行状态，再透传回 TCP 客户端。
