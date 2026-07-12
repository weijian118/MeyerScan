cmake_minimum_required(VERSION 3.15)
project(MeyerScan_CaseUI VERSION 0.3.2 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_CaseUI MEYERSCAN_CASEUI_EXPORTS
    src/CaseUIImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_CaseUI
    PUBLIC include
    PRIVATE
        src
        ../MyLogger/include
        ../MyUIComponents/include
        ../MyRuntimeDataCenter/include
)
meyer_link_sibling_module(MeyerScan_CaseUI MyLogger MeyerScan_Logger)
meyer_link_sibling_module(MeyerScan_CaseUI MyUIComponents MeyerScan_UIComponents)
meyer_link_sibling_module(MeyerScan_CaseUI MyRuntimeDataCenter MeyerScan_RuntimeDataCenter)
meyer_copy_sqlite_runtime(MeyerScan_CaseUI)
meyer_copy_module_resources(MeyerScan_CaseUI MyCaseUI)

option(MEYER_BUILD_CASEUI_TEST "Build CaseUITest.exe" ON)
if(MEYER_BUILD_CASEUI_TEST)
    add_executable(CaseUITest test/main.cpp)
    meyer_configure_module(CaseUITest MeyerScan_CaseUITest)
    meyer_use_dynamic_crt(CaseUITest)
    target_include_directories(CaseUITest PRIVATE include ../MyDatabaseQtAdapter/include)
    target_link_libraries(CaseUITest PRIVATE
        MeyerScan_CaseUI
        MeyerScan_DatabaseQtAdapter
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
    )
    set_target_properties(CaseUITest PROPERTIES WIN32_EXECUTABLE TRUE)
    meyer_copy_sqlite_runtime(CaseUITest)
    meyer_copy_module_resources(CaseUITest MyCaseUI)
endif()
