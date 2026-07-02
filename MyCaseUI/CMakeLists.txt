cmake_minimum_required(VERSION 3.15)
project(MeyerScan_CaseUI VERSION 0.2.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)

meyer_add_qt_widgets_library(MeyerScan_CaseUI MEYERSCAN_CASEUI_EXPORTS
    src/CaseUIImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_CaseUI
    PUBLIC include
    PRIVATE
        src
        ../MyDatabase/include
        ../MyLogger/include
        ../MyUIComponents/include
        ../MyRuntimeDataCenter/include
)
target_link_libraries(MeyerScan_CaseUI PRIVATE Qt5::Sql)
meyer_link_sibling_module(MeyerScan_CaseUI MyDatabase MeyerScan_Database)
meyer_link_sibling_module(MeyerScan_CaseUI MyLogger MeyerScan_Logger)
meyer_link_sibling_module(MeyerScan_CaseUI MyUIComponents MeyerScan_UIComponents)
meyer_link_sibling_module(MeyerScan_CaseUI MyRuntimeDataCenter MeyerScan_RuntimeDataCenter)

option(MEYER_BUILD_CASEUI_TEST "Build CaseUITest.exe" ON)
if(MEYER_BUILD_CASEUI_TEST)
    add_executable(CaseUITest test/main.cpp)
    meyer_configure_module(CaseUITest MeyerScan_CaseUITest)
    meyer_use_dynamic_crt(CaseUITest)
    target_include_directories(CaseUITest PRIVATE include ../MyDatabase/include)
    target_link_libraries(CaseUITest PRIVATE
        MeyerScan_CaseUI
        MeyerScan_Database
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Sql
    )
    set_target_properties(CaseUITest PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
