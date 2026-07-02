cmake_minimum_required(VERSION 3.15)
project(MeyerScan_CaseOrderService VERSION 0.2.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core)

# CaseOrderService 是数据库领域服务，不是 UI 模块。
# 当前骨架内部用 Qt JSON 处理字段映射；后续去 Qt 时应优先替换 .cpp 内部实现，
# 对外保持 UTF-8 JSON / 调用方缓冲区接口稳定。
meyer_add_qt_core_library(MeyerScan_CaseOrderService MEYERSCAN_CASEORDERSERVICE_EXPORTS
    src/CaseOrderServiceImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_CaseOrderService
    PUBLIC include
    PRIVATE src
)
meyer_link_sibling_module(MeyerScan_CaseOrderService MyDatabase MeyerScan_Database)
meyer_link_sibling_module(MeyerScan_CaseOrderService MyLogger MeyerScan_Logger)
