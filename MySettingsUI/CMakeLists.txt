cmake_minimum_required(VERSION 3.15)
project(MeyerScan_SettingsUI VERSION 0.9.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_SettingsUI MEYERSCAN_SETTINGSUI_EXPORTS
    src/SettingsUICalibration.cpp
    src/SettingsUIData.cpp
    src/SettingsUIImpl.cpp
    src/SettingsUILayout.cpp
    src/SettingsUIInternal.h
    src/Version.rc
)

target_include_directories(MeyerScan_SettingsUI
    PUBLIC include
    PRIVATE
        src
        ../MyCalibration3DUI/include
        ../MyCalibrationColorUI/include
        ../MyLogger/include
        ../MyUIComponents/include
)
meyer_link_sibling_module(MeyerScan_SettingsUI MyLogger MeyerScan_Logger)
meyer_copy_module_resources(MeyerScan_SettingsUI MySettingsUI)

# SettingsUI 通过 QLibrary 懒加载两个校准 DLL；颜色校准还会动态使用 UIComponents。
# 根工程构建时显式准备这些运行时文件，避免单独运行 SettingsUITest 时误用旧 DLL。
foreach(settings_runtime_target
        MeyerScan_Calibration3DUI
        MeyerScan_CalibrationColorUI
        MeyerScan_UIComponents)
    if(TARGET ${settings_runtime_target})
        add_dependencies(MeyerScan_SettingsUI ${settings_runtime_target})
        add_custom_command(TARGET MeyerScan_SettingsUI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${settings_runtime_target}>"
                    "$<TARGET_FILE_DIR:MeyerScan_SettingsUI>"
        )
    endif()
endforeach()

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
