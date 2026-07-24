# =============================================================================
# MeyerScan_CaptureService.dll
#
# 纯 C++ 采集编排模块。它不静态链接 DeviceCmd、CaptureProcessing、
# CaptureImagePipeline 和 AutoExposure，而是在运行时从自身目录动态加载四套
# DLL，保证正式程序和测试程序使用同一套 C ABI，也方便后续替换下层实现。
# =============================================================================
cmake_minimum_required(VERSION 3.15)

project(MeyerScan_CaptureService VERSION 0.2.0 LANGUAGES CXX RC)

include(CTest)
include(../cmake/MeyerScanCommon.cmake)

set(MEYER_CAPTURE_SERVICE_SOURCES
    include/CaptureService.h
    ../Common/include/MeyerCaptureTypes.h
    ../MyDeviceCmd/include/DeviceCmd.h
    ../MyCaptureProcessing/include/CaptureProcessing.h
    ../MyCaptureImagePipeline/include/CaptureImagePipeline.h
    ../MyAutoExposure/include/AutoExposure.h
    src/api/CaptureServiceApi.cpp
    src/core/CaptureServiceContext.h
    src/core/CaptureServiceContext.cpp
    src/core/CaptureServiceCapture.cpp
    src/core/CaptureServicePostProcess.cpp
    src/core/CaptureServiceResults.cpp
    src/loader/DeviceCmdLibrary.h
    src/loader/DeviceCmdLibrary.cpp
    src/loader/CaptureProcessingLibrary.h
    src/loader/CaptureProcessingLibrary.cpp
    src/loader/CaptureImagePipelineLibrary.h
    src/loader/CaptureImagePipelineLibrary.cpp
    src/loader/AutoExposureLibrary.h
    src/loader/AutoExposureLibrary.cpp
    src/support/ModuleInfo.h
    src/support/ModuleLogger.h
    src/support/ModuleLogger.cpp
    src/support/PathUtils.h
    src/support/PathUtils.cpp
    src/Version.rc
)

add_library(MeyerScan_CaptureService SHARED ${MEYER_CAPTURE_SERVICE_SOURCES})
add_library(MeyerScan::CaptureService ALIAS MeyerScan_CaptureService)

meyer_configure_module(MeyerScan_CaptureService MeyerScan_CaptureService)
meyer_use_static_crt(MeyerScan_CaptureService)
target_compile_definitions(MeyerScan_CaptureService PRIVATE
    MEYERSCAN_CAPTURE_SERVICE_EXPORTS
    MEYER_DEVICE_CMD_HIDE_GENERIC_EXPORTS
    MEYER_CAPTURE_PROCESSING_HIDE_GENERIC_EXPORTS
    MEYER_CAPTURE_IMAGE_PIPELINE_HIDE_GENERIC_EXPORTS
    MEYER_AUTO_EXPOSURE_HIDE_GENERIC_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)
target_include_directories(MeyerScan_CaptureService
    PUBLIC include ../Common/include
    PRIVATE
        src
        ../MyDeviceCmd/include
        ../MyCaptureProcessing/include
        ../MyCaptureImagePipeline/include
        ../MyAutoExposure/include
)
target_link_libraries(MeyerScan_CaptureService PRIVATE user32)
meyer_set_module_output(MeyerScan_CaptureService)

if(MSVC)
    target_compile_options(MeyerScan_CaptureService PRIVATE /W4)
endif()

option(MEYER_BUILD_CAPTURESERVICETEST "Build Qt CaptureServiceTest.exe" ON)
if(MEYER_BUILD_CAPTURESERVICETEST)
    # Qt 只服务于测试界面。关闭测试后，CaptureService DLL 可在没有 Qt SDK
    # 的纯 C++ 构建机上独立配置和编译。
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    add_executable(CaptureServiceTest
        test/main.cpp
        test/CaptureServiceTestController.cpp
        test/CaptureServiceTestWindow.cpp
        test/CaptureImageView.cpp
        test/CaptureServiceTestWindow.h
        test/CaptureServiceTestController.h
        test/CaptureImageView.h
        test/Version.rc
    )
    meyer_configure_module(CaptureServiceTest CaptureServiceTest)
    meyer_use_dynamic_crt(CaptureServiceTest)
    target_compile_definitions(CaptureServiceTest PRIVATE
        QT_CORE_LIB QT_GUI_LIB QT_WIDGETS_LIB _CRT_SECURE_NO_WARNINGS
    )
    target_include_directories(CaptureServiceTest PRIVATE
        include ../Common/include
        ../MyDeviceCmd/include
        ../MyCaptureProcessing/include
        ../MyLogger/include
        ../MyUIComponents/include
    )
    target_link_libraries(CaptureServiceTest PRIVATE
        MeyerScan_CaptureService Qt5::Core Qt5::Gui Qt5::Widgets
    )
    set_target_properties(CaptureServiceTest PROPERTIES WIN32_EXECUTABLE TRUE)
    meyer_set_module_output(CaptureServiceTest)
    if(MSVC)
        target_compile_options(CaptureServiceTest PRIVATE /W4)
    endif()

    # 根工程中优先依赖本次构建的兄弟 target，避免测试目录意外拾取旧 DLL；
    # 单模块工程没有这些 target 时，下面的同级 bin/$<CONFIG> 回退复制仍生效。
    foreach(_meyer_capture_runtime_target
            MeyerScan_DeviceCmd
            MeyerScan_CaptureProcessing
            MeyerScan_CaptureImagePipeline
            MeyerScan_AutoExposure
            MeyerScan_DeviceTransport
            MeyerScan_Logger
            MeyerScan_UIComponents)
        if(TARGET ${_meyer_capture_runtime_target})
            add_dependencies(CaptureServiceTest ${_meyer_capture_runtime_target})
            add_custom_command(TARGET CaptureServiceTest POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:${_meyer_capture_runtime_target}>"
                        "$<TARGET_FILE_DIR:CaptureServiceTest>"
            )
        endif()
    endforeach()

    # 测试运行目录必须使用同一批构建出的 DLL；没有根工程 target 时回退到
    # 兄弟模块 bin/Release，便于单独复制 MyCaptureService 到另一台电脑编译。
    add_custom_command(TARGET CaptureServiceTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:MeyerScan_CaptureService>"
                "$<TARGET_FILE_DIR:CaptureServiceTest>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../MyDeviceCmd/bin/$<CONFIG>/MeyerScan_DeviceCmd.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../MyCaptureProcessing/bin/$<CONFIG>/MeyerScan_CaptureProcessing.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../MyCaptureImagePipeline/bin/$<CONFIG>/MeyerScan_CaptureImagePipeline.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../MyAutoExposure/bin/$<CONFIG>/MeyerScan_AutoExposure.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../MyDeviceTransport/bin/$<CONFIG>/MeyerScan_DeviceTransport.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../MyLogger/bin/$<CONFIG>/MeyerScan_Logger.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../MyUIComponents/bin/$<CONFIG>/MeyerScan_UIComponents.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MEYER_QT_ROOT}/bin/Qt5Core.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>/Qt5Core.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MEYER_QT_ROOT}/bin/Qt5Gui.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>/Qt5Gui.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MEYER_QT_ROOT}/bin/Qt5Widgets.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>/Qt5Widgets.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:CaptureServiceTest>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MEYER_QT_ROOT}/plugins/platforms/qwindows.dll"
                "$<TARGET_FILE_DIR:CaptureServiceTest>/platforms/qwindows.dll"
    )
    # 单模块构建时 QSS 从源码目录降级加载；根工程会额外复制资源 DLL。
    if(TARGET MeyerScan_UIResources)
        add_dependencies(CaptureServiceTest MeyerScan_UIResources)
        add_custom_command(TARGET CaptureServiceTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:MeyerScan_UIResources>"
                    "$<TARGET_FILE_DIR:CaptureServiceTest>"
        )
    endif()

    if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_test(NAME MeyerScan.CaptureService.Smoke
                 COMMAND CaptureServiceTest --smoke)
        add_test(NAME MeyerScan.CaptureService.TimeoutOnce
                 COMMAND CaptureServiceTest --smoke-timeout-once)
        add_test(NAME MeyerScan.CaptureService.TimeoutAlways
                 COMMAND CaptureServiceTest --smoke-timeout-always)
        add_test(NAME MeyerScan.CaptureService.Disconnect
                 COMMAND CaptureServiceTest --smoke-disconnect)
        add_test(NAME MeyerScan.CaptureService.PartialPacket
                 COMMAND CaptureServiceTest --smoke-partial)
        set_tests_properties(MeyerScan.CaptureService.Smoke PROPERTIES
            TIMEOUT 30 LABELS "MeyerScan;Capture;Qt;Smoke")
        set_tests_properties(
            MeyerScan.CaptureService.TimeoutOnce
            MeyerScan.CaptureService.TimeoutAlways
            MeyerScan.CaptureService.Disconnect
            MeyerScan.CaptureService.PartialPacket
            PROPERTIES TIMEOUT 30 LABELS "MeyerScan;Capture;Qt;Fault")
    endif()
endif()
