# =============================================================================
# MeyerScan_DeviceCmd.dll
#
# 纯 C++ 设备命令模块：不依赖 Qt、数据库或 UI；正式后端运行时动态加载
# MeyerScan_DeviceTransport.dll，测试后端显式使用 SimulatorForTest。
# =============================================================================
cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(MeyerScan_DeviceCmd VERSION 0.6.1 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CTest)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "MeyerScan_DeviceCmd currently supports Windows x64 only.")
endif()

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

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MEYER_DEVICE_CMD_SOURCES
    include/DeviceCmd.h
    src/api/DeviceCmdApi.cpp
    src/core/DeviceCommandService.cpp
    src/core/DeviceCommandService.h
    src/model/DeviceModelCatalog.cpp
    src/model/DeviceModelCatalog.h
    src/model/DeviceProductCatalog.cpp
    src/model/DeviceProductCatalog.h
    src/protocol/DeviceCommandCodec.cpp
    src/protocol/DeviceCommandCodec.h
    src/protocol/DeviceProtocolDefs.h
    src/support/ModuleInfo.h
    src/support/ModuleLogger.cpp
    src/support/ModuleLogger.h
    src/transport/DeviceTransportAbi.h
    src/transport/DeviceTransportLibrary.cpp
    src/transport/DeviceTransportLibrary.h
    src/transport/IRawDeviceTransport.h
    src/transport/SimulatedDeviceTransport.cpp
    src/transport/SimulatedDeviceTransport.h
    src/Version.rc
)

add_library(MeyerScan_DeviceCmd SHARED ${MEYER_DEVICE_CMD_SOURCES})
add_library(MeyerScan::DeviceCmd ALIAS MeyerScan_DeviceCmd)

target_compile_definitions(MeyerScan_DeviceCmd PRIVATE
    MEYERSCAN_DEVICE_CMD_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)
target_include_directories(MeyerScan_DeviceCmd
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(MeyerScan_DeviceCmd PRIVATE user32)
meyer_use_static_crt(MeyerScan_DeviceCmd)
meyer_set_module_output(MeyerScan_DeviceCmd)

if(MSVC)
    target_compile_options(MeyerScan_DeviceCmd PRIVATE /W4)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${MEYER_DEVICE_CMD_SOURCES})

option(MEYER_BUILD_DEVICE_CMD_TEST "Build DeviceCmdTest.exe" ON)
if(MEYER_BUILD_DEVICE_CMD_TEST)
    add_executable(DeviceCmdTest test/main.cpp)
    target_include_directories(DeviceCmdTest PRIVATE include)
    target_link_libraries(DeviceCmdTest PRIVATE MeyerScan_DeviceCmd)
    meyer_use_static_crt(DeviceCmdTest)
    meyer_set_module_output(DeviceCmdTest)
    if(MSVC)
        target_compile_options(DeviceCmdTest PRIVATE /W4)
    endif()

    if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_test(NAME MeyerScan.DeviceCmd.Smoke
                 COMMAND DeviceCmdTest --smoke)
        set_tests_properties(MeyerScan.DeviceCmd.Smoke PROPERTIES
            TIMEOUT 30
            LABELS "MeyerScan;Device;Smoke")
    endif()
endif()
