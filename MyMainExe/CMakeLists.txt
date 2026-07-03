cmake_minimum_required(VERSION 3.15)
project(MeyerScan_MainExe VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)

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
    ../MyConfigCenter/include
    ../MyPermission/include
    ../MyUIComponents/include
    ../MyRuntimeDataCenter/include
    ../MyDatabaseQtAdapter/include
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

meyer_link_sibling_module(MeyerScan MyHomeUI MeyerScan_HomeUI)
meyer_link_sibling_module(MeyerScan MyCaseUI MeyerScan_CaseUI)
meyer_link_sibling_module(MeyerScan MySettingsUI MeyerScan_SettingsUI)
meyer_link_sibling_module(MeyerScan MyConfigCenter MeyerScan_ConfigCenter)
meyer_link_sibling_module(MeyerScan MyPermission MeyerScan_Permission)
meyer_link_sibling_module(MeyerScan MyUIComponents MeyerScan_UIComponents)
meyer_link_sibling_module(MeyerScan MyRuntimeDataCenter MeyerScan_RuntimeDataCenter)
meyer_link_sibling_module(MeyerScan MyDatabaseQtAdapter MeyerScan_DatabaseQtAdapter)
meyer_link_sibling_module(MeyerScan MyLogger MeyerScan_Logger)
meyer_copy_sqlite_runtime(MeyerScan)
