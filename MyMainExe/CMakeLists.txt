cmake_minimum_required(VERSION 3.15)
project(MeyerScan_MainExe VERSION 0.1.7 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
set(MEYER_SCAN_THIRD_PARTY_RUNTIME_ONLY ON)
include(../cmake/MeyerScanScanThirdParty.cmake)
unset(MEYER_SCAN_THIRD_PARTY_RUNTIME_ONLY)

set(MEYER_LOGIN_ROOT "D:/wj/My-wj/MyLogin" CACHE PATH
    "External MeyerLoginWidget SDK root")

add_executable(MeyerScan WIN32
    src/MainWindow.cpp
    test/main.cpp
    src/Version.rc
)

meyer_configure_module(MeyerScan MeyerScan_MainExe)
meyer_use_dynamic_crt(MeyerScan)
set_target_properties(MeyerScan PROPERTIES OUTPUT_NAME "MeyerScan")

target_include_directories(MeyerScan PRIVATE
    include
    "${MEYER_LOGIN_ROOT}"
    ../MyHomeUI/include
    ../MyCaseUI/include
    ../MySettingsUI/include
    ../MyOrderCreateUI/include
    ../MyOrderScanWorkspaceShell/include
    ../MyScanWorkflowUI/include
    ../MyDataProcessUI/include
    ../MySendUI/include
    ../MyExternalLaunchAdapter/include
    ../MyConfigCenter/include
    ../MyPermission/include
    ../MyUIComponents/include
    ../MyRuntimeDataCenter/include
    ../MyDatabaseQtAdapter/include
    ../MyDatabase/include
    ../MyCaseOrderService/include
    ../MyCalibration3DUI/include
    ../MyCalibrationColorUI/include
    ../MyLogger/include
)

target_link_libraries(MeyerScan PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    version
    "${MEYER_LOGIN_ROOT}/lib/MeyerLoginWidget.lib"
)

foreach(_meyer_runtime_target
        MeyerScan_HomeUI
        MeyerScan_CaseUI
        MeyerScan_SettingsUI
        MeyerScan_OrderCreateUI
        MeyerScan_OrderScanWorkspaceShell
        MeyerScan_ExternalLaunchAdapter
        MeyerScan_ConfigCenter
        MeyerScan_Permission
        MeyerScan_UIResources
        MeyerScan_UIComponents
        MeyerScan_RuntimeDataCenter
        MeyerScan_DatabaseQtAdapter
        MeyerScan_Database
        MeyerScan_DeviceTransport
        MeyerScan_CaseOrderService
        MeyerScan_Calibration3DUI
        MeyerScan_CalibrationColorUI
        MeyerScan_ScanWorkflowUI
        MeyerScan_DataProcessUI
        MeyerScan_SendUI
        MeyerScan_ScanReconstructStudio
        ScanReconstructStudio
        MeyerScan_Logger)
    if(TARGET ${_meyer_runtime_target})
        # MainExe 运行时通过 QLibrary 动态加载这些 DLL/EXE。
        # 这里仅保证运行文件被构建并复制到 MeyerScan.exe 同级目录，不要求 MainExe 静态链接导入库。
        add_dependencies(MeyerScan ${_meyer_runtime_target})
        add_custom_command(TARGET MeyerScan POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${_meyer_runtime_target}>"
                    "$<TARGET_FILE_DIR:MeyerScan>"
        )
    endif()
endforeach()

meyer_copy_sqlite_runtime(MeyerScan)

file(GLOB MEYER_SCAN_VTK_RUNTIME_DLLS "${MEYER_SCAN_VTK_ROOT}/bin/*.dll")
file(GLOB MEYER_SCAN_OPENCV_RUNTIME_DLLS "${MEYER_SCAN_OPENCV_ROOT}/build/x64/vc14/bin/opencv_world330.dll")

add_custom_command(TARGET MeyerScan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MeyerScan>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_ROOT_DIR}/MyDatabase/config/db_config.json" "$<TARGET_FILE_DIR:MeyerScan>/config/db_config.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/config/version_modules.json" "$<TARGET_FILE_DIR:MeyerScan>/config/version_modules.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/config/version_modules.md" "$<TARGET_FILE_DIR:MeyerScan>/config/version_modules.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_ROOT_DIR}/MyConfigCenter/config/runtime_config.json" "$<TARGET_FILE_DIR:MeyerScan>/config/runtime_config.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_ROOT_DIR}/MyConfigCenter/config/runtime_config.md" "$<TARGET_FILE_DIR:MeyerScan>/config/runtime_config.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_ROOT_DIR}/MyPermission/config/permission_rules.json" "$<TARGET_FILE_DIR:MeyerScan>/config/permission_rules.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_ROOT_DIR}/MyPermission/config/permission_rules.md" "$<TARGET_FILE_DIR:MeyerScan>/config/permission_rules.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_ROOT_DIR}/MyExternalLaunchAdapter/test/external_order_sample.json" "$<TARGET_FILE_DIR:MeyerScan>/external_order_sample.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MEYER_SCAN_VTK_RUNTIME_DLLS} "$<TARGET_FILE_DIR:MeyerScan>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MEYER_SCAN_OPENCV_RUNTIME_DLLS} "$<TARGET_FILE_DIR:MeyerScan>"
)
