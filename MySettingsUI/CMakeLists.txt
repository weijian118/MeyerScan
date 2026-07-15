cmake_minimum_required(VERSION 3.15)
project(MeyerScan_SettingsUI VERSION 0.2.3 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_SettingsUI MEYERSCAN_SETTINGSUI_EXPORTS
    src/SettingsUIImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_SettingsUI
    PUBLIC include
    PRIVATE
        src
        ../MyCalibration3DUI/include
        ../MyCalibrationColorUI/include
        ../MyLogger/include
)
meyer_link_sibling_module(MeyerScan_SettingsUI MyLogger MeyerScan_Logger)
meyer_copy_module_resources(MeyerScan_SettingsUI MySettingsUI)

option(MEYER_BUILD_SETTINGSUI_TEST "Build SettingsUITest.exe" ON)
if(MEYER_BUILD_SETTINGSUI_TEST)
    add_executable(SettingsUITest test/main.cpp)
    meyer_configure_module(SettingsUITest MeyerScan_SettingsUITest)
    meyer_use_dynamic_crt(SettingsUITest)
    target_include_directories(SettingsUITest PRIVATE include)
    target_link_libraries(SettingsUITest PRIVATE
        MeyerScan_SettingsUI
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
    )
    set_target_properties(SettingsUITest PROPERTIES WIN32_EXECUTABLE TRUE)
    meyer_copy_module_resources(SettingsUITest MySettingsUI)
endif()
