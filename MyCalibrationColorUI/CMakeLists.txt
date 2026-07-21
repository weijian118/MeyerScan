cmake_minimum_required(VERSION 3.15)

project(MeyerScan_CalibrationColorUI VERSION 0.7.0 LANGUAGES CXX)



include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)



meyer_add_qt_widgets_library(MeyerScan_CalibrationColorUI MEYERSCAN_CALIBRATIONCOLORUI_EXPORTS

    src/CalibrationColorUIImpl.cpp

    src/Version.rc

)



target_include_directories(MeyerScan_CalibrationColorUI

    PUBLIC include

    PRIVATE
        src
        ../MyUIComponents/include

)

meyer_link_sibling_module(MeyerScan_CalibrationColorUI MyLogger MeyerScan_Logger)
meyer_copy_module_resources(MeyerScan_CalibrationColorUI MyCalibrationColorUI)

# UIComponents 通过 QLibrary 动态获取，不静态链接导入库；根工程构建时仍建立
# 构建/复制依赖，保证测试目录和正式运行目录能找到共享按钮组件 DLL。
if(TARGET MeyerScan_UIComponents)
    add_dependencies(MeyerScan_CalibrationColorUI MeyerScan_UIComponents)
    add_custom_command(TARGET MeyerScan_CalibrationColorUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:MeyerScan_UIComponents>"
                "$<TARGET_FILE_DIR:MeyerScan_CalibrationColorUI>"
    )
endif()



option(MEYER_BUILD_CALIBRATIONCOLORUITEST "Build CalibrationColorUITest.exe" ON)

if(MEYER_BUILD_CALIBRATIONCOLORUITEST)

    add_executable(CalibrationColorUITest test/main.cpp)

    meyer_configure_module(CalibrationColorUITest CalibrationColorUITest)

    meyer_use_dynamic_crt(CalibrationColorUITest)

    target_include_directories(CalibrationColorUITest PRIVATE include)

    target_link_libraries(CalibrationColorUITest PRIVATE MeyerScan_CalibrationColorUI Qt5::Core Qt5::Gui Qt5::Widgets)

    # 测试宿主默认用于人工查看界面，使用 Windows 子系统避免双击时额外弹出控制台。
    # CTest 会显式传入 --smoke，因此仍可通过进程退出码执行自动化回归。
    set_target_properties(CalibrationColorUITest PROPERTIES WIN32_EXECUTABLE TRUE)

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
