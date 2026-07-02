cmake_minimum_required(VERSION 3.15)
project(MeyerScan_CalibrationColorUI VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_CalibrationColorUI MEYERSCAN_CALIBRATIONCOLORUI_EXPORTS
    src/CalibrationColorUIImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_CalibrationColorUI
    PUBLIC include
    PRIVATE src
)
meyer_link_sibling_module(MeyerScan_CalibrationColorUI MyLogger MeyerScan_Logger)
