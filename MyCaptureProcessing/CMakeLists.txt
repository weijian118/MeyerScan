cmake_minimum_required(VERSION 3.15)

project(MeyerScan_CaptureProcessing VERSION 0.1.0 LANGUAGES CXX RC)

include(CTest)
include(../cmake/MeyerScanCommon.cmake)

set(MEYER_CAPTURE_PROCESSING_SOURCES
    include/CaptureProcessing.h
    ../Common/include/MeyerCaptureTypes.h
    src/api/CaptureProcessingApi.cpp
    src/core/CaptureGroupAssembler.cpp
    src/core/CaptureGroupAssembler.h
    src/core/CaptureSlowProcessor.cpp
    src/core/CaptureSlowProcessor.h
    src/crypto/LegacyImageDecryptor.cpp
    src/crypto/LegacyImageDecryptor.h
    src/support/ModuleInfo.h
    src/Version.rc
)

add_library(MeyerScan_CaptureProcessing SHARED ${MEYER_CAPTURE_PROCESSING_SOURCES})
add_library(MeyerScan::CaptureProcessing ALIAS MeyerScan_CaptureProcessing)

meyer_configure_module(MeyerScan_CaptureProcessing MeyerScan_CaptureProcessing)
meyer_use_static_crt(MeyerScan_CaptureProcessing)
target_compile_definitions(MeyerScan_CaptureProcessing PRIVATE
    MEYERSCAN_CAPTURE_PROCESSING_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)
target_include_directories(MeyerScan_CaptureProcessing
    PUBLIC include ../Common/include
    PRIVATE src
)
meyer_set_module_output(MeyerScan_CaptureProcessing)

if(MSVC)
    target_compile_options(MeyerScan_CaptureProcessing PRIVATE /W4)
endif()

option(MEYER_BUILD_CAPTUREPROCESSING_TEST "Build CaptureProcessingTest.exe" ON)
if(MEYER_BUILD_CAPTUREPROCESSING_TEST)
    add_executable(CaptureProcessingTest test/main.cpp)
    meyer_configure_module(CaptureProcessingTest MeyerScan_CaptureProcessingTest)
    meyer_use_static_crt(CaptureProcessingTest)
    target_include_directories(CaptureProcessingTest PRIVATE include ../Common/include)
    target_link_libraries(CaptureProcessingTest PRIVATE MeyerScan_CaptureProcessing)
    meyer_set_module_output(CaptureProcessingTest)
    if(MSVC)
        target_compile_options(CaptureProcessingTest PRIVATE /W4)
    endif()
    if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_test(NAME MeyerScan.CaptureProcessing.Smoke
                 COMMAND CaptureProcessingTest --smoke)
        set_tests_properties(MeyerScan.CaptureProcessing.Smoke PROPERTIES
            TIMEOUT 30 LABELS "MeyerScan;Capture;Processing")
    endif()
endif()
