cmake_minimum_required(VERSION 3.15)
project(MeyerScan_Permission VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core)

# Permission 是非界面模块。当前内部使用 Qt JSON 读取 permission_rules.json；
# 公共接口继续使用 const char* / bool，便于未来替换内部实现而不影响调用方。
meyer_add_qt_core_library(MeyerScan_Permission MEYERSCAN_PERMISSION_EXPORTS
    src/PermissionImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_Permission
    PUBLIC include
    PRIVATE src
)
