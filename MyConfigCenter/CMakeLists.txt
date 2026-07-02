cmake_minimum_required(VERSION 3.15)
project(MeyerScan_ConfigCenter VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core)

# ConfigCenter 是非界面模块。当前骨架内部用 Qt JSON/文件能力，公共 ABI 不暴露 Qt 类型。
# 后续若切换为纯 C++ JSON/文件实现，应保持 include/ConfigCenter.h 接口稳定。
meyer_add_qt_core_library(MeyerScan_ConfigCenter MEYERSCAN_CONFIGCENTER_EXPORTS
    src/ConfigCenterImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_ConfigCenter
    PUBLIC include
    PRIVATE src
)
