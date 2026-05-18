cmake_minimum_required(VERSION 3.16)
project(ZorbFrameworkLinux C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_POD_CORE "Build RK3588 pod core static library" ON)
option(BUILD_ZF_LEGACY_CORE "Build legacy zf core static library" OFF)
option(BUILD_POD_DEMO "Build minimal pod demo executable" ON)

option(BUILD_POD_PLUGIN "Build pod plugin domain" ON)
option(BUILD_POD_DATA "Build pod data domain" ON)
option(BUILD_POD_RUNTIME "Build pod runtime domain" ON)
option(BUILD_POD_CODEC "Build pod codec domain" ON)
option(BUILD_POD_TRANSPORT "Build pod transport domain" ON)
option(BUILD_POD_CONTROL "Build pod control domain" ON)
option(BUILD_PLATFORM_LAYER "Build platform layer" ON)
option(BUILD_CORE_LAYER "Build core layer" ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/core
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/pod
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/pod/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/pod/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/pod/codec
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/pod/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/pod/control
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/pod/data
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/platform
)

if(BUILD_POD_CORE)
    if(BUILD_POD_RUNTIME AND NOT BUILD_POD_PLUGIN)
        message(FATAL_ERROR "BUILD_POD_RUNTIME=ON requires BUILD_POD_PLUGIN=ON")
    endif()

    if(BUILD_POD_RUNTIME AND NOT BUILD_POD_DATA)
        message(FATAL_ERROR "BUILD_POD_RUNTIME=ON requires BUILD_POD_DATA=ON")
    endif()

    if(BUILD_POD_RUNTIME AND NOT BUILD_PLATFORM_LAYER)
        message(FATAL_ERROR "BUILD_POD_RUNTIME=ON requires BUILD_PLATFORM_LAYER=ON")
    endif()

    if(BUILD_POD_CODEC AND NOT BUILD_PLATFORM_LAYER)
        message(FATAL_ERROR "BUILD_POD_CODEC=ON requires BUILD_PLATFORM_LAYER=ON")
    endif()

    if(BUILD_POD_DEMO)
        if(NOT BUILD_POD_PLUGIN OR NOT BUILD_POD_DATA OR NOT BUILD_POD_RUNTIME
            OR NOT BUILD_POD_CODEC OR NOT BUILD_PLATFORM_LAYER OR NOT BUILD_CORE_LAYER)
            message(FATAL_ERROR
                "BUILD_POD_DEMO=ON requires plugin/data/runtime/codec/platform/core layers")
        endif()
    endif()

    set(POD_CORE_SOURCES)

    if(BUILD_POD_PLUGIN)
        set(POD_PLUGIN_SOURCES
            src/pod/plugin/pod_plugin_manager.c
            src/pod/plugin/pod_camera_plugin_template.c
        )
        list(APPEND POD_CORE_SOURCES ${POD_PLUGIN_SOURCES})
    endif()

    if(BUILD_POD_DATA)
        set(POD_DATA_SOURCES
            src/pod/data/pod_perf_queue.c
            src/pod/data/pod_frame_pool.c
        )
        list(APPEND POD_CORE_SOURCES ${POD_DATA_SOURCES})
    endif()

    if(BUILD_POD_RUNTIME)
        set(POD_RUNTIME_SOURCES
            src/pod/runtime/pod_profile.c
            src/pod/runtime/pod_runtime.c
        )
        list(APPEND POD_CORE_SOURCES ${POD_RUNTIME_SOURCES})
    endif()

    if(BUILD_POD_CODEC)
        set(POD_CODEC_SOURCES
            src/pod/codec/pod_encoder.c
            src/pod/codec/pod_adaptive_rate.c
        )
        list(APPEND POD_CORE_SOURCES ${POD_CODEC_SOURCES})
    endif()

    if(BUILD_POD_TRANSPORT)
        set(POD_TRANSPORT_SOURCES
            src/pod/transport/pod_udp_streamer.c
        )
        list(APPEND POD_CORE_SOURCES ${POD_TRANSPORT_SOURCES})
    endif()

    if(BUILD_POD_CONTROL)
        set(POD_CONTROL_SOURCES
            src/pod/control/pod_tcp_control.c
        )
        list(APPEND POD_CORE_SOURCES ${POD_CONTROL_SOURCES})
    endif()

    if(BUILD_PLATFORM_LAYER)
        set(PLATFORM_SOURCES
            src/platform/pod_rk3588_platform.c
            src/platform/zf_critical_stub.c
        )
        list(APPEND POD_CORE_SOURCES ${PLATFORM_SOURCES})
    endif()

    if(BUILD_CORE_LAYER)
        set(CORE_SOURCES
            src/core/zf_assert.c
            src/core/zf_list.c
            src/core/zf_buffer.c
            src/core/zf_event.c
            src/core/zf_fsm.c
            src/core/zf_timer.c
            src/core/zf_task_schedule.c
            src/core/zf_task.c
            src/core/zf_time.c
        )
        list(APPEND POD_CORE_SOURCES ${CORE_SOURCES})
    endif()

    if(NOT POD_CORE_SOURCES)
        message(FATAL_ERROR "No sources selected for zorb_pod_core. Enable at least one layer/domain option.")
    endif()

    add_library(zorb_pod_core STATIC ${POD_CORE_SOURCES})

    if(UNIX AND NOT APPLE)
        target_link_libraries(zorb_pod_core pthread)
    endif()

    if(BUILD_POD_DEMO)
        add_executable(zorb_pod_demo src/demo/pod_demo_main.c)
        target_link_libraries(zorb_pod_demo zorb_pod_core)
    endif()
endif()

if(BUILD_ZF_LEGACY_CORE)
    set(ZF_LEGACY_SOURCES
        src/core/zf_assert.c
        src/core/zf_buffer.c
        src/core/zf_event.c
        src/core/zf_fsm.c
        src/core/zf_list.c
        src/core/zf_task.c
        src/core/zf_task_schedule.c
        src/core/zf_time.c
        src/core/zf_timer.c
        src/platform/zf_critical_stub.c
    )

    add_library(zf_legacy_core STATIC ${ZF_LEGACY_SOURCES})
endif()
