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
meyer_copy_module_resources(MeyerScan_CalibrationColorUI MyCalibrationColorUI)



option(MEYER_BUILD_CALIBRATIONCOLORUITEST "Build CalibrationColorUITest.exe" ON)

if(MEYER_BUILD_CALIBRATIONCOLORUITEST)

    add_executable(CalibrationColorUITest test/main.cpp)

    meyer_configure_module(CalibrationColorUITest CalibrationColorUITest)

    meyer_use_dynamic_crt(CalibrationColorUITest)

    target_include_directories(CalibrationColorUITest PRIVATE include)

    target_link_libraries(CalibrationColorUITest PRIVATE MeyerScan_CalibrationColorUI Qt5::Core Qt5::Gui Qt5::Widgets)

    meyer_link_sibling_module(CalibrationColorUITest MyLogger MeyerScan_Logger)
    meyer_copy_module_resources(CalibrationColorUITest MyCalibrationColorUI)



    add_custom_command(TARGET CalibrationColorUITest POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:CalibrationColorUITest>/Qt5Core.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Gui.dll" "$<TARGET_FILE_DIR:CalibrationColorUITest>/Qt5Gui.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Widgets.dll" "$<TARGET_FILE_DIR:CalibrationColorUITest>/Qt5Widgets.dll"

        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CalibrationColorUITest>/platforms"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/plugins/platforms/qwindows.dll" "$<TARGET_FILE_DIR:CalibrationColorUITest>/platforms/qwindows.dll"

    )

endif()
