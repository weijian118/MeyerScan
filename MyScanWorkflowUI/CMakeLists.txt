cmake_minimum_required(VERSION 3.15)

project(MeyerScan_ScanWorkflowUI VERSION 0.2.1 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
include(../cmake/MeyerScanScanThirdParty.cmake)

add_library(MeyerScan_ScanWorkflowUI SHARED
    src/ScanWorkflowUIImpl.cpp
    src/Version.rc
)

meyer_configure_module(MeyerScan_ScanWorkflowUI MeyerScan_ScanWorkflowUI)
meyer_use_dynamic_crt(MeyerScan_ScanWorkflowUI)
target_compile_definitions(MeyerScan_ScanWorkflowUI PRIVATE MEYERSCAN_SCANWORKFLOWUI_EXPORTS)
target_include_directories(MeyerScan_ScanWorkflowUI BEFORE PRIVATE
    "${MEYER_SCAN_VTK_HEADERS_ROOT}/include/vtk-8.0"
)
target_include_directories(MeyerScan_ScanWorkflowUI
    PUBLIC include
    PRIVATE
        src
        "${MEYER_SCAN_VTK_HEADERS_ROOT}/include/vtk-8.0"
)
target_link_libraries(MeyerScan_ScanWorkflowUI PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    ${VTK_LIBRARIES}
    ${OpenCV_LIBS}
)
meyer_link_sibling_module(MeyerScan_ScanWorkflowUI MyLogger MeyerScan_Logger)
meyer_copy_module_resources(MeyerScan_ScanWorkflowUI MyScanWorkflowUI)

option(MEYER_BUILD_SCANWORKFLOWUITEST "Build ScanWorkflowUITest.exe" ON)
if(MEYER_BUILD_SCANWORKFLOWUITEST)
    add_executable(ScanWorkflowUITest test/main.cpp)
    meyer_configure_module(ScanWorkflowUITest ScanWorkflowUITest)
    meyer_use_dynamic_crt(ScanWorkflowUITest)
    target_include_directories(ScanWorkflowUITest PRIVATE include)
    target_link_libraries(ScanWorkflowUITest PRIVATE
        MeyerScan_ScanWorkflowUI
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        ${VTK_LIBRARIES}
        ${OpenCV_LIBS}
    )
    meyer_link_sibling_module(ScanWorkflowUITest MyLogger MeyerScan_Logger)
    meyer_copy_module_resources(ScanWorkflowUITest MyScanWorkflowUI)
endif()
