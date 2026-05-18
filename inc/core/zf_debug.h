#ifndef __ZF_DEBUG_H__
#define __ZF_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>

/* 日志等级 */
#define LOG_D 0
#define LOG_W 1
#define LOG_E 2

/* 调试输出开关 */
#define _ZF_DEBUG
#define ZF_DEBUG_ON true

#ifdef _ZF_DEBUG
#if ZF_DEBUG_ON
#define ZF_DEBUG(rank, ...) do                            \
{                                                         \
    char code[10] = "[rank=0]";                          \
    code[6] = (char)('0' + (char)(rank));                \
    if (code[6] != '0')                                   \
    {                                                     \
        printf("\r\n\r\n%s", code);                    \
    }                                                     \
    printf(__VA_ARGS__);                                  \
    if (code[6] != '0')                                   \
    {                                                     \
        printf("%s\r\n\r\n", code);                    \
    }                                                     \
} while (0)
#else
#define ZF_DEBUG(rank, ...)
#endif
#endif

#define ZF_DEBUG_D(...) ZF_DEBUG(LOG_D, __VA_ARGS__)
#define ZF_DEBUG_W(...) ZF_DEBUG(LOG_W, __VA_ARGS__)
#define ZF_DEBUG_E(...) ZF_DEBUG(LOG_E, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __ZF_DEBUG_H__ */

/******************************** 文件结束 ********************************/
