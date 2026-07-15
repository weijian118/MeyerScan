cmake_minimum_required(VERSION 3.15)

project(MeyerScan_ScanSchemaService VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core)

# 扫描方案服务是非界面业务模块；Qt Core 仅作为内部 JSON 实现细节。
meyer_add_qt_core_library(MeyerScan_ScanSchemaService MEYERSCAN_SCANSCHEMASERVICE_EXPORTS
    src/ScanSchemaServiceImpl.cpp
    src/Version.rc
)
target_include_directories(MeyerScan_ScanSchemaService PUBLIC include PRIVATE src)
meyer_link_sibling_module(MeyerScan_ScanSchemaService MyLogger MeyerScan_Logger)
if(TARGET MeyerScan_Logger)
    add_custom_command(TARGET MeyerScan_ScanSchemaService POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:MeyerScan_Logger>"
                "$<TARGET_FILE_DIR:MeyerScan_ScanSchemaService>/MeyerScan_Logger.dll"
    )
endif()

option(MEYER_BUILD_SCANSCHEMASERVICETEST "Build ScanSchemaServiceTest.exe" ON)
if(MEYER_BUILD_SCANSCHEMASERVICETEST)
    add_executable(ScanSchemaServiceTest test/main.cpp)
    meyer_configure_module(ScanSchemaServiceTest ScanSchemaServiceTest)
    meyer_use_dynamic_crt(ScanSchemaServiceTest)
    target_include_directories(ScanSchemaServiceTest PRIVATE include)
    target_link_libraries(ScanSchemaServiceTest PRIVATE MeyerScan_ScanSchemaService Qt5::Core)
    meyer_link_sibling_module(ScanSchemaServiceTest MyLogger MeyerScan_Logger)
    if(TARGET MeyerScan_Logger)
        add_custom_command(TARGET ScanSchemaServiceTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:MeyerScan_Logger>"
                    "$<TARGET_FILE_DIR:ScanSchemaServiceTest>/MeyerScan_Logger.dll"
        )
    endif()
    add_custom_command(TARGET ScanSchemaServiceTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MEYER_QT_ROOT}/bin/Qt5Core.dll"
                "$<TARGET_FILE_DIR:ScanSchemaServiceTest>/Qt5Core.dll"
    )
endif()
