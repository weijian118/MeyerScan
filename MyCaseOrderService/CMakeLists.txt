cmake_minimum_required(VERSION 3.15)

project(MeyerScan_CaseOrderService VERSION 0.2.2 LANGUAGES CXX)



include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core)



# CaseOrderService 是数据库领域服务，不是 UI 模块。

# 当前骨架内部用 Qt JSON 处理字段映射；后续去 Qt 时应优先替换 .cpp 内部实现，

# 对外保持 UTF-8 JSON / 调用方缓冲区接口稳定。

meyer_add_qt_core_library(MeyerScan_CaseOrderService MEYERSCAN_CASEORDERSERVICE_EXPORTS

    src/CaseOrderServiceImpl.cpp

    src/Version.rc

)



target_include_directories(MeyerScan_CaseOrderService

    PUBLIC include

    PRIVATE src

)

meyer_link_sibling_module(MeyerScan_CaseOrderService MyDatabaseQtAdapter MeyerScan_DatabaseQtAdapter)

meyer_link_sibling_module(MeyerScan_CaseOrderService MyLogger MeyerScan_Logger)

meyer_copy_sqlite_runtime(MeyerScan_CaseOrderService)

# 把 CaseOrderService 运行所需的自研 DLL 复制到目标目录。
# 根聚合构建可直接使用 CMake target 路径；单模块独立构建则从兄弟模块标准 bin/<Config> 目录取文件。
function(meyer_copy_case_order_runtime target)
    foreach(runtime_target MeyerScan_DatabaseQtAdapter MeyerScan_Database MeyerScan_Logger)
        # CMake target/文件名使用 MeyerScan_*，仓库目录使用 My*；独立构建时需要显式映射。
        if(runtime_target STREQUAL "MeyerScan_DatabaseQtAdapter")
            set(runtime_module_dir "MyDatabaseQtAdapter")
        elseif(runtime_target STREQUAL "MeyerScan_Database")
            set(runtime_module_dir "MyDatabase")
        else()
            set(runtime_module_dir "MyLogger")
        endif()

        if(TARGET ${runtime_target})
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:${runtime_target}>"
                        "$<TARGET_FILE_DIR:${target}>"
            )
        else()
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MEYER_ROOT_DIR}/${runtime_module_dir}/bin/$<CONFIG>/${runtime_target}.dll"
                        "$<TARGET_FILE_DIR:${target}>/${runtime_target}.dll"
            )
        endif()
    endforeach()

    # QtCore 是本模块内部 JSON 实现的运行时依赖，服务 DLL 和测试宿主都需要放在同目录。
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MEYER_QT_ROOT}/bin/Qt5Core.dll"
                "$<TARGET_FILE_DIR:${target}>/Qt5Core.dll"
    )
endfunction()

meyer_copy_case_order_runtime(MeyerScan_CaseOrderService)



option(MEYER_BUILD_CASEORDERSERVICETEST "Build CaseOrderServiceTest.exe" ON)

if(MEYER_BUILD_CASEORDERSERVICETEST)

    add_executable(CaseOrderServiceTest test/main.cpp)

    meyer_configure_module(CaseOrderServiceTest CaseOrderServiceTest)

    meyer_use_dynamic_crt(CaseOrderServiceTest)

    target_include_directories(CaseOrderServiceTest PRIVATE include)

    target_link_libraries(CaseOrderServiceTest PRIVATE MeyerScan_CaseOrderService Qt5::Core)

    meyer_link_sibling_module(CaseOrderServiceTest MyDatabaseQtAdapter MeyerScan_DatabaseQtAdapter)

    meyer_link_sibling_module(CaseOrderServiceTest MyLogger MeyerScan_Logger)

    meyer_copy_sqlite_runtime(CaseOrderServiceTest)
    meyer_copy_case_order_runtime(CaseOrderServiceTest)

endif()
