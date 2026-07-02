cmake_minimum_required(VERSION 3.15)
project(MeyerScan_SettingsUI VERSION 0.2.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)

meyer_add_qt_widgets_library(MeyerScan_SettingsUI MEYERSCAN_SETTINGSUI_EXPORTS
    src/SettingsUIImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_SettingsUI
    PUBLIC include
    PRIVATE
        src
        ../MyRuntimeDataCenter/include
        ../MyCalibration3DUI/include
        ../MyCalibrationColorUI/include
        ../MyDatabase/include
        ../MyLogger/include
)
target_link_libraries(MeyerScan_SettingsUI PRIVATE Qt5::Sql)
meyer_link_sibling_module(MeyerScan_SettingsUI MyLogger MeyerScan_Logger)
meyer_link_sibling_module(MeyerScan_SettingsUI MyRuntimeDataCenter MeyerScan_RuntimeDataCenter)
meyer_link_sibling_module(MeyerScan_SettingsUI MyDatabase MeyerScan_Database)

option(MEYER_BUILD_SETTINGSUI_TEST "Build SettingsUITest.exe" ON)
if(MEYER_BUILD_SETTINGSUI_TEST)
    add_executable(SettingsUITest test/main.cpp)
    meyer_configure_module(SettingsUITest MeyerScan_SettingsUITest)
    meyer_use_dynamic_crt(SettingsUITest)
    target_include_directories(SettingsUITest PRIVATE include ../MyDatabase/include)
    target_link_libraries(SettingsUITest PRIVATE
        MeyerScan_SettingsUI
        MeyerScan_Database
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Sql
    )
    set_target_properties(SettingsUITest PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
