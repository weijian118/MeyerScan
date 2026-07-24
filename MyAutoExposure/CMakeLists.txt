cmake_minimum_required(VERSION 3.15)

project(MeyerScan_AutoExposure VERSION 0.1.0 LANGUAGES CXX RC)

include(CTest)
include(../cmake/MeyerScanCommon.cmake)

set(MEYER_AUTO_EXPOSURE_SOURCES
    include/AutoExposure.h
    ../Common/include/MeyerCaptureTypes.h
    src/api/AutoExposureApi.cpp
    src/support/ModuleInfo.h
    src/Version.rc
)

add_library(MeyerScan_AutoExposure SHARED ${MEYER_AUTO_EXPOSURE_SOURCES})
add_library(MeyerScan::AutoExposure ALIAS MeyerScan_AutoExposure)

meyer_configure_module(MeyerScan_AutoExposure MeyerScan_AutoExposure)
meyer_use_static_crt(MeyerScan_AutoExposure)
target_compile_definitions(MeyerScan_AutoExposure PRIVATE
    MEYERSCAN_AUTO_EXPOSURE_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)
target_include_directories(MeyerScan_AutoExposure
    PUBLIC include ../Common/include
    PRIVATE src
)
meyer_set_module_output(MeyerScan_AutoExposure)

if(MSVC)
    target_compile_options(MeyerScan_AutoExposure PRIVATE /W4)
endif()

option(MEYER_BUILD_AUTOEXPOSURE_TEST "Build AutoExposureTest.exe" ON)
if(MEYER_BUILD_AUTOEXPOSURE_TEST)
    add_executable(AutoExposureTest test/main.cpp)
    meyer_configure_module(AutoExposureTest MeyerScan_AutoExposureTest)
    meyer_use_static_crt(AutoExposureTest)
    target_include_directories(AutoExposureTest PRIVATE include ../Common/include)
    target_link_libraries(AutoExposureTest PRIVATE MeyerScan_AutoExposure)
    meyer_set_module_output(AutoExposureTest)
    if(MSVC)
        target_compile_options(AutoExposureTest PRIVATE /W4)
    endif()
    if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_test(NAME MeyerScan.AutoExposure.Smoke
                 COMMAND AutoExposureTest --smoke)
        set_tests_properties(MeyerScan.AutoExposure.Smoke PROPERTIES
            TIMEOUT 30 LABELS "MeyerScan;Capture;Exposure")
    endif()
endif()
