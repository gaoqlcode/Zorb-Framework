#ifndef __ZF_MALLOC_H__
#define __ZF_MALLOC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

/*
 * 通用平台内存接口。
 * 默认映射到标准 C 库，可在后续接入 jemalloc/tcmalloc 或自定义池。
 */
#define _ZF_MALLOC

#ifdef _ZF_MALLOC
#define ZF_MALLOC(size_) malloc(size_)
#define ZF_FREE(ptr_) free(ptr_)
#define ZF_MEMCPY(des_, src_, len_) memcpy(des_, src_, len_)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __ZF_MALLOC_H__ */

/******************************** 文件结束 ********************************/
