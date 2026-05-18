#ifndef __POD_CAMERA_PLUGIN_H__
#define __POD_CAMERA_PLUGIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 吊舱系统中的视频流类型 */
typedef enum _PodStreamType
{
    POD_STREAM_VIS = 1,
    POD_STREAM_NIR = 2,
    POD_STREAM_TIR = 3
} PodStreamType;

/* 常见帧格式 */
typedef enum _PodFrameFormat
{
    POD_FMT_RAW = 0,
    POD_FMT_H264 = 1,
    POD_FMT_H265 = 2,
    POD_FMT_MJPEG = 3
} PodFrameFormat;

/* 帧元数据：供同步、录像、推流模块统一使用 */
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

/* 统一相机插件接口表 */
typedef struct _PodCameraPluginOps
{
    int32_t (*Init)(void *ctx, const char *cfg_text);
    int32_t (*Open)(void *ctx);
    int32_t (*StartStream)(void *ctx);
    int32_t (*StopStream)(void *ctx);
    int32_t (*GetFrame)(void *ctx, PodFrameMeta *meta,
        const uint8_t **ppPayload, uint32_t *pPayloadLen, uint32_t timeout_ms);
    int32_t (*SetParam)(void *ctx, const char *key, const char *value);
    int32_t (*GetParam)(void *ctx, const char *key, char *value_buf,
        uint32_t value_buf_size);
    int32_t (*GetHealth)(void *ctx, uint32_t *pHealthCode);
    int32_t (*Close)(void *ctx);
} PodCameraPluginOps;

/* 单个相机插件描述 */
typedef struct _PodCameraPlugin
{
    const char *name;
    uint8_t stream_id;
    PodCameraPluginOps ops;
    void *ctx;
} PodCameraPlugin;

#ifdef __cplusplus
}
#endif

#endif /* __POD_CAMERA_PLUGIN_H__ */
