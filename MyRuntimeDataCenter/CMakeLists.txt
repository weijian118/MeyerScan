cmake_minimum_required(VERSION 3.15)
project(MeyerScan_RuntimeDataCenter VERSION 0.1.1 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core)

# RuntimeDataCenter 是非界面读模型/缓存模块。
# 当前内部使用 Qt JSON/容器包装 domain 快照；公共 ABI 保持 UTF-8 JSON 和调用方缓冲区。
add_library(MeyerScan_RuntimeDataCenter SHARED
    src/RuntimeDataCenterImpl.cpp
    src/Version.rc
)

meyer_configure_module(MeyerScan_RuntimeDataCenter MeyerScan_RuntimeDataCenter)
meyer_use_dynamic_crt(MeyerScan_RuntimeDataCenter)
target_compile_definitions(MeyerScan_RuntimeDataCenter PRIVATE MEYERSCAN_RUNTIMEDATACENTER_EXPORTS)
target_include_directories(MeyerScan_RuntimeDataCenter
    PUBLIC include
    PRIVATE src
)
target_link_libraries(MeyerScan_RuntimeDataCenter PRIVATE Qt5::Core)
meyer_link_sibling_module(MeyerScan_RuntimeDataCenter MyDatabaseQtAdapter MeyerScan_DatabaseQtAdapter)
meyer_link_sibling_module(MeyerScan_RuntimeDataCenter MyLogger MeyerScan_Logger)
meyer_copy_sqlite_runtime(MeyerScan_RuntimeDataCenter)

option(MEYER_BUILD_RUNTIMEDATACENTER_TEST "Build RuntimeDataCenterTest.exe" ON)
if(MEYER_BUILD_RUNTIMEDATACENTER_TEST)
    add_executable(RuntimeDataCenterTest test/main.cpp)
    meyer_configure_module(RuntimeDataCenterTest RuntimeDataCenterTest)
    meyer_use_dynamic_crt(RuntimeDataCenterTest)
    target_include_directories(RuntimeDataCenterTest PRIVATE include)
    target_link_libraries(RuntimeDataCenterTest PRIVATE
        MeyerScan_RuntimeDataCenter
        MeyerScan_DatabaseQtAdapter
        MeyerScan_Logger
        Qt5::Core
    )
    meyer_copy_sqlite_runtime(RuntimeDataCenterTest)
endif()
