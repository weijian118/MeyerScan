cmake_minimum_required(VERSION 3.15)

project(ScanReconstructStudio VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(ScanReconstructStudio WIN32
    src/ScanReconstructStudioWindow.cpp
    test/main.cpp
    src/Version.rc
)

meyer_configure_module(ScanReconstructStudio ScanReconstructStudio)
meyer_use_dynamic_crt(ScanReconstructStudio)

target_include_directories(ScanReconstructStudio PRIVATE
    include
    ../MyScanWorkflowUI/include
    ../MyDataProcessUI/include
    ../MyLogger/include
)

target_link_libraries(ScanReconstructStudio PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)
meyer_link_sibling_module(ScanReconstructStudio MyLogger MeyerScan_Logger)
set_target_properties(ScanReconstructStudio PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")

foreach(_scan_runtime_target
        MeyerScan_ScanWorkflowUI
        MeyerScan_DataProcessUI
        MeyerScan_Logger)
    if(TARGET ${_scan_runtime_target})
        add_dependencies(ScanReconstructStudio ${_scan_runtime_target})
        add_custom_command(TARGET ScanReconstructStudio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${_scan_runtime_target}>"
                    "$<TARGET_FILE_DIR:ScanReconstructStudio>"
        )
    endif()
endforeach()
