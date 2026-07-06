cmake_minimum_required(VERSION 3.15)

project(MeyerScan_ExternalLaunchAdapter VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core)

meyer_add_qt_core_library(MeyerScan_ExternalLaunchAdapter MEYERSCAN_EXTERNALLAUNCHADAPTER_EXPORTS
    src/ExternalLaunchAdapterImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_ExternalLaunchAdapter
    PUBLIC include
    PRIVATE src
)

meyer_link_sibling_module(MeyerScan_ExternalLaunchAdapter MyLogger MeyerScan_Logger)

add_custom_command(TARGET MeyerScan_ExternalLaunchAdapter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/test/external_order_sample.json" "$<TARGET_FILE_DIR:MeyerScan_ExternalLaunchAdapter>/external_order_sample.json"
)

option(MEYER_BUILD_EXTERNALLAUNCHADAPTERTEST "Build ExternalLaunchAdapterTest.exe" ON)
if(MEYER_BUILD_EXTERNALLAUNCHADAPTERTEST)
    add_executable(ExternalLaunchAdapterTest test/main.cpp)
    meyer_configure_module(ExternalLaunchAdapterTest ExternalLaunchAdapterTest)
    meyer_use_dynamic_crt(ExternalLaunchAdapterTest)
    target_include_directories(ExternalLaunchAdapterTest PRIVATE include)
    target_link_libraries(ExternalLaunchAdapterTest PRIVATE MeyerScan_ExternalLaunchAdapter Qt5::Core)
    meyer_link_sibling_module(ExternalLaunchAdapterTest MyLogger MeyerScan_Logger)

    add_custom_command(TARGET ExternalLaunchAdapterTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:ExternalLaunchAdapterTest>/Qt5Core.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/test/external_order_sample.json" "$<TARGET_FILE_DIR:ExternalLaunchAdapterTest>/external_order_sample.json"
    )
endif()
