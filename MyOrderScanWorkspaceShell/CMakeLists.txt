cmake_minimum_required(VERSION 3.15)
project(MeyerScan_OrderScanWorkspaceShell VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_OrderScanWorkspaceShell MEYERSCAN_ORDERSCANWORKSPACESHELL_EXPORTS
    src/OrderScanWorkspaceShellImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_OrderScanWorkspaceShell
    PUBLIC include
    PRIVATE src
)
meyer_link_sibling_module(MeyerScan_OrderScanWorkspaceShell MyLogger MeyerScan_Logger)
