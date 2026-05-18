#ifndef __POD_PROFILE_H__
#define __POD_PROFILE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

typedef struct _PodProfile
{
    /* 相机启用开关 */
    uint8_t EnableVis;
    uint8_t EnableNir;
    uint8_t EnableTir;
    uint8_t CameraCount;

    /* 控制与推流端口 */
    uint16_t TcpControlPort;
    uint16_t UdpVisPort;
    uint16_t UdpNirPort;
    uint16_t UdpTirPort;

    /* 视频规格 */
    uint16_t Width;
    uint16_t Height;
    uint16_t Fps;

    /* 编码与同步参数 */
    uint32_t BitrateKbps;
    uint32_t Gop;
    uint32_t SyncWindowUs;

    /* 帧池配置 */
    uint32_t FramePoolBlockCount;
    uint32_t FramePoolBlockSize;

    /* 线程绑核规划 */
    uint8_t CaptureCpu;
    uint8_t EncodeCpu;
    uint8_t NetworkCpu;
    uint8_t ControlCpu;
} PodProfile;

/* 填充默认配置 */
void PodProfile_setDefaults(PodProfile *pProfile);
/* 解析 key=value 文本配置 */
bool PodProfile_parseText(PodProfile *pProfile, const char *text);
/* 从文件加载配置 */
bool PodProfile_loadFromFile(PodProfile *pProfile, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* __POD_PROFILE_H__ */
