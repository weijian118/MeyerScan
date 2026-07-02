cmake_minimum_required(VERSION 3.15)
project(MeyerScan_Calibration3DUI VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_Calibration3DUI MEYERSCAN_CALIBRATION3DUI_EXPORTS
    src/Calibration3DUIImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_Calibration3DUI
    PUBLIC include
    PRIVATE src
)
meyer_link_sibling_module(MeyerScan_Calibration3DUI MyLogger MeyerScan_Logger)
