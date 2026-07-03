cmake_minimum_required(VERSION 3.15)
project(MeyerScan_Database VERSION 1.3.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)

add_library(MeyerScan_Database SHARED
    src/DatabaseImpl.cpp
    src/Version.rc
)

meyer_configure_module(MeyerScan_Database MeyerScan_Database)
meyer_use_static_crt(MeyerScan_Database)

target_compile_definitions(MeyerScan_Database PRIVATE MEYERSCAN_DATABASE_EXPORTS)
target_include_directories(MeyerScan_Database
    PUBLIC include
    PRIVATE src
)
meyer_copy_sqlite_runtime(MeyerScan_Database)

option(MEYER_BUILD_DATABASE_TEST "Build DatabaseTest.exe" ON)
if(MEYER_BUILD_DATABASE_TEST)
    add_executable(DatabaseTest test/main.cpp)
    meyer_configure_module(DatabaseTest DatabaseTest)
    meyer_use_static_crt(DatabaseTest)
    target_include_directories(DatabaseTest PRIVATE include)
    target_link_libraries(DatabaseTest PRIVATE MeyerScan_Database)
    meyer_copy_sqlite_runtime(DatabaseTest)
endif()
