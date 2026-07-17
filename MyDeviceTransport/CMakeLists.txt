# =============================================================================
# MeyerScan_DeviceTransport.dll
#
# 该工程只依赖 Windows API 和随模块放置的 Cypress CyAPI SDK，不依赖 Qt、
# Eigen 或 OpenCV。所有路径都由 CMAKE_CURRENT_SOURCE_DIR 推导，因此整个
# MyDeviceTransport 文件夹移动后无需修改绝对路径。
# =============================================================================
cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(MeyerScan_DeviceTransport VERSION 1.2.0 LANGUAGES CXX)

# 独立打开模块时自行启用 CTest；作为根工程子目录时由根 CMake 统一启用和注册。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CTest)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "MeyerScan_DeviceTransport currently supports Windows x64 only.")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 聚合工程提供统一帮助函数；单独复制模块时使用本文件中的等价回退实现。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/MeyerScanCommon.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/MeyerScanCommon.cmake")
else()
    function(meyer_set_module_output target)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>"
        )
    endfunction()

    function(meyer_use_static_crt target)
        if(MSVC)
            set_property(TARGET ${target} PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()
    endfunction()
endif()

set(MEYER_DEVICE_TRANSPORT_SOURCES
    include/DeviceTransport.h
    src/api/DeviceTransportApi.cpp
    src/api/DeviceTypes.h
    src/core/DeviceSession.cpp
    src/core/DeviceSession.h
    src/core/capture/CaptureEngine.cpp
    src/core/capture/CaptureEngine.h
    src/core/capture/CaptureTransportLoop.cpp
    src/core/capture/CaptureTransportLoop.h
    src/core/capture/FramePostProcessor.cpp
    src/core/capture/FramePostProcessor.h
    src/core/capture/FrameSyncAssembler.cpp
    src/core/capture/FrameSyncAssembler.h
    src/model/CaptureFrameConfig.h
    src/model/CapturePacket.h
    src/model/DeviceStatus.h
    src/model/ExposureParams.h
    src/model/ImageFrame.h
    src/model/ImuSample.h
    src/model/TransportConfig.h
    src/processing/imu/IImuProcessor.h
    src/processing/imu/ImuAttitudeProcessor.cpp
    src/processing/imu/ImuAttitudeProcessor.h
    src/processing/imu/ImuPacketDecoder.cpp
    src/processing/imu/ImuPacketDecoder.h
    src/protocol/MeyerProtocolDefs.h
    src/support/ModuleInfo.h
    src/support/ModuleLogger.cpp
    src/support/ModuleLogger.h
    src/transport/ITransport.h
    src/transport/TransportFactory.cpp
    src/transport/TransportFactory.h
    src/transport/cyapi/CyApiTransport.cpp
    src/transport/cyapi/CyApiTransport.h
    src/stdafx.h
    src/Version.rc
)

set(MEYER_CYAPI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/CyAPI/include")
set(MEYER_CYAPI_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/CyAPI/lib/x64/CyAPI.lib")
if(NOT EXISTS "${MEYER_CYAPI_INCLUDE_DIR}/CyAPI.h" OR
   NOT EXISTS "${MEYER_CYAPI_LIBRARY}")
    message(FATAL_ERROR
        "CyAPI SDK is incomplete. Expected include/CyAPI.h and lib/x64/CyAPI.lib "
        "under ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/CyAPI.")
endif()

add_library(MeyerScan_DeviceTransport SHARED ${MEYER_DEVICE_TRANSPORT_SOURCES})
add_library(MeyerScan::DeviceTransport ALIAS MeyerScan_DeviceTransport)

target_compile_definitions(MeyerScan_DeviceTransport PRIVATE
    MEYERSCAN_DEVICE_TRANSPORT_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)
target_include_directories(MeyerScan_DeviceTransport
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${MEYER_CYAPI_INCLUDE_DIR}"
)
target_link_libraries(MeyerScan_DeviceTransport PRIVATE
    "${MEYER_CYAPI_LIBRARY}"
    setupapi
    legacy_stdio_definitions
)

# CyAPI.lib 只提供使用 Release 静态 CRT 的 x64 版本，因此 Debug/Release 都固定 /MT。
# 该模块的公共 C ABI 不跨边界释放内存，不需要向调用方暴露 Debug CRT 对象。
if(MSVC)
    set_property(TARGET MeyerScan_DeviceTransport PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
meyer_set_module_output(MeyerScan_DeviceTransport)

if(MSVC)
    target_compile_options(MeyerScan_DeviceTransport PRIVATE /W4)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${MEYER_DEVICE_TRANSPORT_SOURCES})

option(MEYER_BUILD_DEVICE_TRANSPORT_TEST "Build DeviceTransportTest.exe" ON)
if(MEYER_BUILD_DEVICE_TRANSPORT_TEST)
    add_executable(DeviceTransportTest
        test/main.cpp
        src/processing/imu/ImuAttitudeProcessor.cpp
    )
    target_include_directories(DeviceTransportTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(DeviceTransportTest PRIVATE MeyerScan_DeviceTransport)
    if(MSVC)
        set_property(TARGET DeviceTransportTest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
    meyer_set_module_output(DeviceTransportTest)
    if(MSVC)
        target_compile_options(DeviceTransportTest PRIVATE /W4)
    endif()

    # 聚合工程由根 CMake 把测试复制到隔离运行目录后统一注册；独立构建才在本地注册。
    if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_test(NAME MeyerScan.DeviceTransport.Smoke
                 COMMAND DeviceTransportTest --smoke)
        set_tests_properties(MeyerScan.DeviceTransport.Smoke PROPERTIES
            TIMEOUT 30
            LABELS "MeyerScan;Device;Smoke"
        )
    endif()
endif()
