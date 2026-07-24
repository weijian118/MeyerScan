cmake_minimum_required(VERSION 3.15)

project(MeyerScan_CaptureImagePipeline VERSION 0.1.0 LANGUAGES CXX RC)

include(CTest)
include(../cmake/MeyerScanCommon.cmake)

set(MEYER_CAPTURE_IMAGE_PIPELINE_SOURCES
    include/CaptureImagePipeline.h
    ../Common/include/MeyerCaptureTypes.h
    src/api/CaptureImagePipelineApi.cpp
    src/core/CaptureImagePipelineSession.h
    src/core/CaptureImagePipelineSession.cpp
    src/support/ModuleInfo.h
    src/Version.rc
)

add_library(MeyerScan_CaptureImagePipeline SHARED
    ${MEYER_CAPTURE_IMAGE_PIPELINE_SOURCES})
add_library(MeyerScan::CaptureImagePipeline ALIAS MeyerScan_CaptureImagePipeline)

meyer_configure_module(MeyerScan_CaptureImagePipeline
                       MeyerScan_CaptureImagePipeline)
meyer_use_static_crt(MeyerScan_CaptureImagePipeline)
target_compile_definitions(MeyerScan_CaptureImagePipeline PRIVATE
    MEYERSCAN_CAPTURE_IMAGE_PIPELINE_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)
target_include_directories(MeyerScan_CaptureImagePipeline
    PUBLIC include ../Common/include
    PRIVATE src
)
meyer_set_module_output(MeyerScan_CaptureImagePipeline)
if(MSVC)
    target_compile_options(MeyerScan_CaptureImagePipeline PRIVATE /W4)
endif()

option(MEYER_BUILD_CAPTURE_IMAGE_PIPELINE_TEST
       "Build CaptureImagePipelineTest.exe" ON)
if(MEYER_BUILD_CAPTURE_IMAGE_PIPELINE_TEST)
    add_executable(CaptureImagePipelineTest test/main.cpp)
    meyer_configure_module(CaptureImagePipelineTest
                           MeyerScan_CaptureImagePipelineTest)
    meyer_use_static_crt(CaptureImagePipelineTest)
    target_include_directories(CaptureImagePipelineTest PRIVATE
        include ../Common/include)
    target_link_libraries(CaptureImagePipelineTest PRIVATE
        MeyerScan_CaptureImagePipeline)
    meyer_set_module_output(CaptureImagePipelineTest)
    if(MSVC)
        target_compile_options(CaptureImagePipelineTest PRIVATE /W4)
    endif()
    if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_test(NAME MeyerScan.CaptureImagePipeline.Smoke
                 COMMAND CaptureImagePipelineTest --smoke)
        set_tests_properties(MeyerScan.CaptureImagePipeline.Smoke PROPERTIES
            TIMEOUT 30 LABELS "MeyerScan;Capture;Pipeline")
    endif()
endif()
