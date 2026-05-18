#ifndef __POD_CAMERA_PLUGIN_H__
#define __POD_CAMERA_PLUGIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 吊舱系统中的视频流类型。stream_id 在整个项目里都是关键路由键。 */
typedef enum _PodStreamType
{
    POD_STREAM_VIS = 1,
    POD_STREAM_NIR = 2,
    POD_STREAM_TIR = 3
} PodStreamType;

/* 常见帧格式。采集侧和编码侧都通过它表达负载类型。 */
typedef enum _PodFrameFormat
{
    POD_FMT_RAW = 0,
    POD_FMT_H264 = 1,
    POD_FMT_H265 = 2,
    POD_FMT_MJPEG = 3
} PodFrameFormat;

/*
 * 帧元数据：
 * - frame_id 用于识别连续性。
 * - ts_mono_us 用于单机时序分析。
 * - ts_sync_us 用于多路同步或外部时钟对齐。
 */
typedef struct _PodFrameMeta
{
    uint32_t frame_id;
    uint64_t ts_mono_us;
    uint64_t ts_sync_us;
    uint16_t width;
    uint16_t height;
    uint8_t stream_id;
    uint8_t format;
} PodFrameMeta;

/*
 * 统一相机插件接口表。
 * 学习时可以把它理解成“平台适配层和上层运行时之间的契约”。
 */
typedef struct _PodCameraPluginOps
{
    /* 解析配置文本。 */
    int32_t (*Init)(void *ctx, const char *cfg_text);
    /* 打开设备或准备资源。 */
    int32_t (*Open)(void *ctx);
    /* 启动采集流。 */
    int32_t (*StartStream)(void *ctx);
    /* 停止采集流。 */
    int32_t (*StopStream)(void *ctx);
    /* 拉取一帧数据。timeout_ms 决定本次等待上限。 */
    int32_t (*GetFrame)(void *ctx, PodFrameMeta *meta,
        const uint8_t **ppPayload, uint32_t *pPayloadLen, uint32_t timeout_ms);
    /* 写参数。 */
    int32_t (*SetParam)(void *ctx, const char *key, const char *value);
    /* 读参数。 */
    int32_t (*GetParam)(void *ctx, const char *key, char *value_buf,
        uint32_t value_buf_size);
    /* 健康检查。 */
    int32_t (*GetHealth)(void *ctx, uint32_t *pHealthCode);
    /* 关闭设备并释放资源。 */
    int32_t (*Close)(void *ctx);
} PodCameraPluginOps;

/* 单个相机插件描述。 */
typedef struct _PodCameraPlugin
{
    /* 逻辑名称，供控制面按名字查找。 */
    const char *name;
    /* 所属流号，供数据面路由。 */
    uint8_t stream_id;
    PodCameraPluginOps ops;
    /* 插件私有上下文，由具体实现自行定义。 */
    void *ctx;
} PodCameraPlugin;

#ifdef __cplusplus
}
#endif

#endif /* __POD_CAMERA_PLUGIN_H__ */
