cmake_minimum_required(VERSION 3.15)
project(MeyerScan_UIComponents VERSION 0.2.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_UIComponents MEYERSCAN_UICOMPONENTS_EXPORTS
    src/UIComponentsImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_UIComponents
    PUBLIC include
    PRIVATE src
)
