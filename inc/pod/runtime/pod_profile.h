#ifndef __POD_PROFILE_H__
#define __POD_PROFILE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

typedef struct _PodProfile
{
    /* 相机启用开关。用于裁剪具体机型需要打开的流。 */
    uint8_t EnableVis;
    uint8_t EnableNir;
    uint8_t EnableTir;
    /* 逻辑上希望存在的相机数量。 */
    uint8_t CameraCount;

    /* 控制与推流端口。 */
    uint16_t TcpControlPort;
    uint16_t UdpVisPort;
    uint16_t UdpNirPort;
    uint16_t UdpTirPort;

    /* 视频规格。多数采集、编码、传输参数都从这里继承。 */
    uint16_t Width;
    uint16_t Height;
    uint16_t Fps;

    /* 编码与同步参数。 */
    uint32_t BitrateKbps;
    uint32_t Gop;
    uint32_t SyncWindowUs;

    /* 帧池配置。决定运行时缓存容量与单帧最大容纳尺寸。 */
    uint32_t FramePoolBlockCount;
    uint32_t FramePoolBlockSize;

    /* 线程绑核规划。用于平台层做 CPU affinity。 */
    uint8_t CaptureCpu;
    uint8_t EncodeCpu;
    uint8_t NetworkCpu;
    uint8_t ControlCpu;
} PodProfile;

/* 填充默认配置，适合 demo 或最小运行环境。 */
void PodProfile_setDefaults(PodProfile *pProfile);
/* 解析 key=value 文本配置。未知字段会被忽略。 */
bool PodProfile_parseText(PodProfile *pProfile, const char *text);
/* 从文件加载配置。通常先 setDefaults，再 load 覆盖。 */
bool PodProfile_loadFromFile(PodProfile *pProfile, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* __POD_PROFILE_H__ */
