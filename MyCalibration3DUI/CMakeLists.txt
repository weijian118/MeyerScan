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



option(MEYER_BUILD_CALIBRATION3DUITEST "Build Calibration3DUITest.exe" ON)

if(MEYER_BUILD_CALIBRATION3DUITEST)

    add_executable(Calibration3DUITest test/main.cpp)

    meyer_configure_module(Calibration3DUITest Calibration3DUITest)

    meyer_use_dynamic_crt(Calibration3DUITest)

    target_include_directories(Calibration3DUITest PRIVATE include)

    target_link_libraries(Calibration3DUITest PRIVATE MeyerScan_Calibration3DUI Qt5::Core Qt5::Gui Qt5::Widgets)

    meyer_link_sibling_module(Calibration3DUITest MyLogger MeyerScan_Logger)



    add_custom_command(TARGET Calibration3DUITest POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:Calibration3DUITest>/Qt5Core.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Gui.dll" "$<TARGET_FILE_DIR:Calibration3DUITest>/Qt5Gui.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Widgets.dll" "$<TARGET_FILE_DIR:Calibration3DUITest>/Qt5Widgets.dll"

        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Calibration3DUITest>/platforms"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/plugins/platforms/qwindows.dll" "$<TARGET_FILE_DIR:Calibration3DUITest>/platforms/qwindows.dll"

    )

endif()
