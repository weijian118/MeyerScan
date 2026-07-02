cmake_minimum_required(VERSION 3.15)
project(MeyerScan_Database VERSION 1.2.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Sql)

# 说明:
#   Database 当前内部仍使用 QtSql 连接 MySQL/SQLite，这是既有可运行链路。
#   评审后的新规则要求非界面模块“能不用 Qt 就不用 Qt”，因此本模块后续应把
#   QtSql 收敛在 .cpp 私有实现中，公共头文件继续保持 POD / UTF-8 / 调用方缓冲区。
add_library(MeyerScan_Database SHARED
    src/DatabaseImpl.cpp
    src/Version.rc
)

meyer_configure_module(MeyerScan_Database MeyerScan_Database)
meyer_use_dynamic_crt(MeyerScan_Database)

target_compile_definitions(MeyerScan_Database PRIVATE MEYERSCAN_DATABASE_EXPORTS)
target_include_directories(MeyerScan_Database
    PUBLIC include
    PRIVATE src
)
target_link_libraries(MeyerScan_Database PRIVATE Qt5::Core Qt5::Sql)

option(MEYER_BUILD_DATABASE_TEST "Build DatabaseTest.exe" ON)
if(MEYER_BUILD_DATABASE_TEST)
    add_executable(DatabaseTest test/main.cpp)
    meyer_configure_module(DatabaseTest DatabaseTest)
    meyer_use_dynamic_crt(DatabaseTest)
    target_include_directories(DatabaseTest PRIVATE include)
    target_link_libraries(DatabaseTest PRIVATE MeyerScan_Database Qt5::Core Qt5::Sql)
endif()
