#if __has_include("pod_profile.h")
#include "pod_profile.h"
#else
#include "../../../inc/pod/runtime/pod_profile.h"
#endif

#include <stdio.h>
#include <string.h>

#if __has_include("zf_assert.h")
#include "zf_assert.h"
#else
#include "../../../inc/core/zf_assert.h"
#endif

#if __has_include("zf_malloc.h")
#include "zf_malloc.h"
#else
#include "../../../inc/core/zf_malloc.h"
#endif

static void TrimRight(char *s)
{
    /* 去除行尾空白，便于统一解析 */
    int32_t i;

    if (s == NULL)
    {
        return;
    }

    i = (int32_t)strlen(s) - 1;
    while (i >= 0 && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r'
        || s[i] == '\n'))
    {
        s[i] = '\0';
        i--;
    }
}

static char *TrimLeft(char *s)
{
    /* 跳过行首空白 */
    if (s == NULL)
    {
        return NULL;
    }

    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
    {
        s++;
    }

    return s;
}

static uint32_t ParseU32(const char *value)
{
    uint32_t v = 0;

    while (value != NULL && *value >= '0' && *value <= '9')
    {
        v = v * 10u + (uint32_t)(*value - '0');
        value++;
    }

    return v;
}

static bool ApplyKeyValue(PodProfile *p, const char *key, const char *value)
{
    /* 配置项分发：未知 key 返回 false，不中断整体解析 */
    if (strcmp(key, "enable_vis") == 0)
    {
        p->EnableVis = (uint8_t)ParseU32(value);
    }
    else if (strcmp(key, "enable_nir") == 0)
    {
        p->EnableNir = (uint8_t)ParseU32(value);
    }
    else if (strcmp(key, "enable_tir") == 0)
    {
        p->EnableTir = (uint8_t)ParseU32(value);
    }
    else if (strcmp(key, "camera_count") == 0)
    {
        p->CameraCount = (uint8_t)ParseU32(value);
    }
    else if (strcmp(key, "tcp_control_port") == 0)
    {
        p->TcpControlPort = (uint16_t)ParseU32(value);
    }
    else if (strcmp(key, "udp_vis_port") == 0)
    {
        p->UdpVisPort = (uint16_t)ParseU32(value);
    }
    else if (strcmp(key, "udp_nir_port") == 0)
    {
        p->UdpNirPort = (uint16_t)ParseU32(value);
    }
    else if (strcmp(key, "udp_tir_port") == 0)
    {
        p->UdpTirPort = (uint16_t)ParseU32(value);
    }
    else if (strcmp(key, "width") == 0)
    {
        p->Width = (uint16_t)ParseU32(value);
    }
    else if (strcmp(key, "height") == 0)
    {
        p->Height = (uint16_t)ParseU32(value);
    }
    else if (strcmp(key, "fps") == 0)
    {
        p->Fps = (uint16_t)ParseU32(value);
    }
    else if (strcmp(key, "bitrate_kbps") == 0)
    {
        p->BitrateKbps = ParseU32(value);
    }
    else if (strcmp(key, "gop") == 0)
    {
        p->Gop = ParseU32(value);
    }
    else if (strcmp(key, "sync_window_us") == 0)
    {
        p->SyncWindowUs = ParseU32(value);
    }
    else if (strcmp(key, "frame_pool_block_count") == 0)
    {
        p->FramePoolBlockCount = ParseU32(value);
    }
    else if (strcmp(key, "frame_pool_block_size") == 0)
    {
        p->FramePoolBlockSize = ParseU32(value);
    }
    else if (strcmp(key, "capture_cpu") == 0)
    {
        p->CaptureCpu = (uint8_t)ParseU32(value);
    }
    else if (strcmp(key, "encode_cpu") == 0)
    {
        p->EncodeCpu = (uint8_t)ParseU32(value);
    }
    else if (strcmp(key, "network_cpu") == 0)
    {
        p->NetworkCpu = (uint8_t)ParseU32(value);
    }
    else if (strcmp(key, "control_cpu") == 0)
    {
        p->ControlCpu = (uint8_t)ParseU32(value);
    }
    else
    {
        return false;
    }

    return true;
}

void PodProfile_setDefaults(PodProfile *pProfile)
{
    ZF_ASSERT(pProfile != (PodProfile *)0)

    pProfile->EnableVis = 1u;
    pProfile->EnableNir = 1u;
    pProfile->EnableTir = 1u;
    pProfile->CameraCount = 3u;

    pProfile->TcpControlPort = 19000u;
    pProfile->UdpVisPort = 20001u;
    pProfile->UdpNirPort = 20002u;
    pProfile->UdpTirPort = 20003u;

    pProfile->Width = 1920u;
    pProfile->Height = 1080u;
    pProfile->Fps = 30u;

    pProfile->BitrateKbps = 4096u;
    pProfile->Gop = 30u;
    pProfile->SyncWindowUs = 30000u;

    pProfile->FramePoolBlockCount = 64u;
    pProfile->FramePoolBlockSize = 1024u * 1024u;

    pProfile->CaptureCpu = 4u;
    pProfile->EncodeCpu = 5u;
    pProfile->NetworkCpu = 6u;
    pProfile->ControlCpu = 2u;
}

bool PodProfile_parseText(PodProfile *pProfile, const char *text)
{
    char line[256];
    uint32_t i = 0u;
    uint32_t k = 0u;

    ZF_ASSERT(pProfile != (PodProfile *)0)

    if (text == NULL)
    {
        return false;
    }

    while (text[i] != '\0')
    {
        if (text[i] != '\n' && k < (uint32_t)(sizeof(line) - 1u))
        {
            line[k++] = text[i++];
            continue;
        }

        line[k] = '\0';
        k = 0u;
        if (text[i] == '\n')
        {
            i++;
        }

        {
            char *pLine = TrimLeft(line);
            char *pEq = NULL;

            TrimRight(pLine);
            if (*pLine == '\0' || *pLine == '#')
            {
                continue;
            }

            pEq = strchr(pLine, '=');
            if (pEq != NULL)
            {
                char *key;
                char *value;
                *pEq = '\0';
                key = TrimLeft(pLine);
                value = TrimLeft(pEq + 1);
                TrimRight(key);
                TrimRight(value);
                ApplyKeyValue(pProfile, key, value);
            }
        }
    }

    if (k > 0u)
    {
        line[k] = '\0';
        {
            char *pLine = TrimLeft(line);
            char *pEq = NULL;

            TrimRight(pLine);
            if (*pLine != '\0' && *pLine != '#')
            {
                pEq = strchr(pLine, '=');
                if (pEq != NULL)
                {
                    char *key;
                    char *value;
                    *pEq = '\0';
                    key = TrimLeft(pLine);
                    value = TrimLeft(pEq + 1);
                    TrimRight(key);
                    TrimRight(value);
                    ApplyKeyValue(pProfile, key, value);
                }
            }
        }
    }

    return true;
}

bool PodProfile_loadFromFile(PodProfile *pProfile, const char *path)
{
    FILE *fp;
    long len;
    char *buf;
    bool ok;

    ZF_ASSERT(pProfile != (PodProfile *)0)

    if (path == NULL)
    {
        return false;
    }

    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return false;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return false;
    }

    len = ftell(fp);
    if (len < 0)
    {
        fclose(fp);
        return false;
    }

    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        fclose(fp);
        return false;
    }

    buf = (char *)ZF_MALLOC((size_t)len + 1u);
    if (buf == NULL)
    {
        fclose(fp);
        return false;
    }

    if (fread(buf, 1, (size_t)len, fp) != (size_t)len)
    {
        ZF_FREE(buf);
        fclose(fp);
        return false;
    }

    buf[len] = '\0';
    fclose(fp);

    ok = PodProfile_parseText(pProfile, buf);
    ZF_FREE(buf);
    return ok;
}
