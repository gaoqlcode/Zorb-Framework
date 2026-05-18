/**
  *****************************************************************************
  * @file    zf_includes.h
  * @author  Zorb
  * @version V1.0.0
  * @date    2018-06-28
  * @brief   Zorb Framework 聚合头文件（核心 + 吊舱业务）
  *****************************************************************************
  * @history
  *
  * 1. Date:2018-06-28
  *    Author:Zorb
  *    Modification:创建文件
  *
  *****************************************************************************
  */

#ifndef __ZF_INCLUDES_H__
#define __ZF_INCLUDES_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 这是面向使用者的聚合头文件。
 * 如果你只是想“把框架能力一次性包含进来”，直接包含它最省事；
 * 如果你想学习模块边界，建议继续分别阅读各个头文件。
 */

#if __has_include("zf_debug.h")
#include "zf_debug.h"
#else
#include "zf_debug.h"
#endif
#include "zf_assert.h"
#include "zf_time.h"
#if __has_include("zf_malloc.h")
#include "zf_malloc.h"
#else
#include "zf_malloc.h"
#endif
#include "zf_buffer.h"
#include "zf_list.h"
#include "zf_fsm.h"
#include "zf_event.h"
#include "zf_timer.h"
#include "zf_task.h"

/*
 * 下面这组头文件属于吊舱多相机业务层。
 * 它们建立在 core 层之上，复用了链表、事件、定时器、任务等通用能力。
 */
#include "../pod/plugin/pod_camera_plugin.h"
#include "../pod/plugin/pod_plugin_manager.h"
#include "../pod/data/pod_perf_queue.h"
#include "../pod/data/pod_frame_pool.h"
#include "../pod/runtime/pod_profile.h"
#include "../platform/pod_rk3588_platform.h"
#include "../pod/runtime/pod_runtime.h"
#include "../pod/codec/pod_encoder.h"
#include "../pod/transport/pod_udp_streamer.h"
#include "../pod/control/pod_tcp_control.h"
#include "../pod/codec/pod_adaptive_rate.h"

#ifdef __cplusplus
}
#endif

#endif /* __ZF_INCLUDES_H__ */

/******************************** 文件结束 ********************************/

