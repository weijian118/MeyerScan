cmake_minimum_required(VERSION 3.15)
project(MeyerScan_HomeUI VERSION 0.2.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_HomeUI MEYERSCAN_HOMEUI_EXPORTS
    src/HomeUIImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_HomeUI
    PUBLIC include
    PRIVATE
        src
        ../MyLogger/include
        ../MyUIComponents/include
)
meyer_link_sibling_module(MeyerScan_HomeUI MyLogger MeyerScan_Logger)
meyer_link_sibling_module(MeyerScan_HomeUI MyUIComponents MeyerScan_UIComponents)

option(MEYER_BUILD_HOMEUI_TEST "Build HomeUITest.exe" ON)
if(MEYER_BUILD_HOMEUI_TEST)
    add_executable(HomeUITest test/main.cpp)
    meyer_configure_module(HomeUITest MeyerScan_HomeUITest)
    meyer_use_dynamic_crt(HomeUITest)
    target_include_directories(HomeUITest PRIVATE include)
    target_link_libraries(HomeUITest PRIVATE MeyerScan_HomeUI Qt5::Core Qt5::Gui Qt5::Widgets)
    set_target_properties(HomeUITest PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
