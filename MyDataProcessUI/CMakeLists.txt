cmake_minimum_required(VERSION 3.15)

project(MeyerScan_DataProcessUI VERSION 0.2.4 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
include(../cmake/MeyerScanScanThirdParty.cmake)

add_library(MeyerScan_DataProcessUI SHARED
    src/DataProcessUIImpl.cpp
    src/Version.rc
)

meyer_configure_module(MeyerScan_DataProcessUI MeyerScan_DataProcessUI)
meyer_use_dynamic_crt(MeyerScan_DataProcessUI)
target_compile_definitions(MeyerScan_DataProcessUI PRIVATE MEYERSCAN_DATAPROCESSUI_EXPORTS)
target_include_directories(MeyerScan_DataProcessUI BEFORE PRIVATE
    "${MEYER_SCAN_VTK_HEADERS_ROOT}/include/vtk-8.0"
)
target_include_directories(MeyerScan_DataProcessUI
    PUBLIC include
    PRIVATE
        src
        "${MEYER_SCAN_VTK_HEADERS_ROOT}/include/vtk-8.0"
)
target_link_libraries(MeyerScan_DataProcessUI PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    ${VTK_LIBRARIES}
    ${OpenCV_LIBS}
)
meyer_link_sibling_module(MeyerScan_DataProcessUI MyLogger MeyerScan_Logger)
meyer_copy_module_resources(MeyerScan_DataProcessUI MyDataProcessUI)

option(MEYER_BUILD_DATAPROCESSUITEST "Build DataProcessUITest.exe" ON)
if(MEYER_BUILD_DATAPROCESSUITEST)
    add_executable(DataProcessUITest test/main.cpp)
    meyer_configure_module(DataProcessUITest DataProcessUITest)
    meyer_use_dynamic_crt(DataProcessUITest)
    target_include_directories(DataProcessUITest PRIVATE include)
    target_link_libraries(DataProcessUITest PRIVATE
        MeyerScan_DataProcessUI
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        ${VTK_LIBRARIES}
        ${OpenCV_LIBS}
    )
    meyer_link_sibling_module(DataProcessUITest MyLogger MeyerScan_Logger)
    meyer_copy_module_resources(DataProcessUITest MyDataProcessUI)
endif()
