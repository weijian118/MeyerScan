# =============================================================================
# MeyerScan root CMake aggregate project
#
# Purpose:
#   1. Let VSCode / CMake Tools open and build all active modules from F:/MeyerScan.
#   2. Keep VS2015 .sln/.vcxproj as the main debugging entry while CMake remains
#      the portable build entry.
#   3. Require each new top-level module to provide its own CMakeLists.txt.
# =============================================================================
cmake_minimum_required(VERSION 3.15)

project(MeyerScan_AllModules
    VERSION 0.1.0
    DESCRIPTION "MeyerScan modular refactor aggregate project"
    LANGUAGES CXX
)

include(cmake/MeyerScanCommon.cmake)
include(CTest)

# Infrastructure modules.
add_subdirectory(MyLogger)
add_subdirectory(MyDatabase)
add_subdirectory(MyDatabaseQtAdapter)
add_subdirectory(MyDeviceTransport)
add_subdirectory(MyDeviceCmd)
add_subdirectory(MyCaptureProcessing)
add_subdirectory(MyCaptureImagePipeline)
add_subdirectory(MyAutoExposure)
add_subdirectory(MyCaptureService)
add_subdirectory(MyConfigCenter)
add_subdirectory(MyPermission)
add_subdirectory(MyUIResources)
add_subdirectory(MyUIComponents)

# Business services and runtime read models.
add_subdirectory(MyCaseOrderService)
add_subdirectory(MyScanSchemaService)
add_subdirectory(MyRuntimeDataCenter)

# Settings、calibration、order workspace、scan workflow 和历史版本模块。
add_subdirectory(MyCalibration3DUI)
add_subdirectory(MyCalibrationColorUI)
add_subdirectory(MyOrderCreateUI)
add_subdirectory(MyOrderScanWorkspaceShell)
add_subdirectory(MyExternalLaunchAdapter)
add_subdirectory(MyVersionManager)
add_subdirectory(MyScanWorkflowUI)
add_subdirectory(MyDataProcessUI)
add_subdirectory(MySendUI)
add_subdirectory(MyScanReconstructStudio)

# UI layer modules.
add_subdirectory(MyHomeUI)
add_subdirectory(MyCaseUI)
add_subdirectory(MySettingsUI)

# Legacy schema/reference directory.
add_subdirectory(MyCaseManager)

# Login host and main executable are built last so all runtime DLL targets exist.
add_subdirectory(MyMainExe/MyLogin)
add_subdirectory(MyMainExe)

# =============================================================================
# 统一自动测试注册
#
# 各模块继续拥有并构建自己的轻量测试程序；根工程只把已有 target 注册给
# CTest，使 VSCode/自动化脚本可以复用同一批二进制，不维护第二套测试实现。
# =============================================================================
if(BUILD_TESTING)
    # CMake/VS2015 都把模块产物写回各自 bin/<Config>，旧目录可能残留过期兄弟 DLL。
    # Windows 会优先从 EXE 同级目录加载 DLL，因此只修改 PATH 不能避免加载旧文件。
    # 这里每次构建都重建一个干净的 CTest 运行目录：先复制 MainExe 已收集的完整
    # 运行依赖，再覆盖同批次测试 EXE，确保整套测试使用同一次构建的 DLL。
    set(MEYER_CTEST_RUNTIME_DIR "${CMAKE_BINARY_DIR}/ctest_runtime/$<CONFIG>")
    set(_meyer_ctest_candidate_targets
        MeyerScan
        MeyerLoginTest
        LoggerTest
        DatabaseTest
        DatabaseQtAdapterTest
        MeyerScan_DeviceTransport
        DeviceTransportTest
        MeyerScan_DeviceCmd
        DeviceCmdTest
        MeyerScan_CaptureProcessing
        CaptureProcessingTest
        MeyerScan_CaptureImagePipeline
        CaptureImagePipelineTest
        MeyerScan_AutoExposure
        AutoExposureTest
        MeyerScan_CaptureService
        CaptureServiceTest
        ConfigCenterTest
        PermissionTest
        UIResourcesTest
        UIComponentsTest
        CaseOrderServiceTest
        ScanSchemaServiceTest
        RuntimeDataCenterTest
        VersionManagerTest
        ExternalLaunchAdapterTest
        Calibration3DUITest
        CalibrationColorUITest
        HomeUITest
        CaseUITest
        SettingsUITest
        OrderCreateUITest
        OrderScanWorkspaceShellTest
        ScanWorkflowUITest
        DataProcessUITest
        SendUITest
        ScanReconstructStudio
    )

    # 先准备清空/复制主运行目录的命令，再按实际存在的 target 追加测试 EXE 复制命令。
    set(_meyer_ctest_copy_commands
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${MEYER_CTEST_RUNTIME_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MEYER_CTEST_RUNTIME_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_FILE_DIR:MeyerScan>"
                "${MEYER_CTEST_RUNTIME_DIR}"
    )
    set(_meyer_ctest_runtime_targets MeyerScan)
    foreach(_meyer_test_target IN LISTS _meyer_ctest_candidate_targets)
        if(TARGET ${_meyer_test_target})
            list(APPEND _meyer_ctest_runtime_targets ${_meyer_test_target})
            list(APPEND _meyer_ctest_copy_commands
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:${_meyer_test_target}>"
                        "${MEYER_CTEST_RUNTIME_DIR}/$<TARGET_FILE_NAME:${_meyer_test_target}>"
            )
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _meyer_ctest_runtime_targets)

    add_custom_target(MeyerScan_CTestRuntime ALL
        ${_meyer_ctest_copy_commands}
        DEPENDS ${_meyer_ctest_runtime_targets}
        COMMENT "Preparing isolated MeyerScan CTest runtime"
    )

    # 把一个已构建的测试程序登记成串行回归测试。
    # RUN_SERIAL 避免共享日志、SQLite 测试库互相影响；TIMEOUT 防止 GUI
    # 测试忘记退出事件循环后阻塞整套测试。
    function(meyer_register_aggregate_test test_name target_name)
        if(TARGET ${target_name})
            add_test(NAME ${test_name}
                COMMAND "${MEYER_CTEST_RUNTIME_DIR}/$<TARGET_FILE_NAME:${target_name}>" ${ARGN}
            )
            set_tests_properties(${test_name} PROPERTIES
                WORKING_DIRECTORY "${MEYER_CTEST_RUNTIME_DIR}"
                RUN_SERIAL TRUE
                TIMEOUT 120
                LABELS "MeyerScan"
            )
        endif()
    endfunction()

    # 先跑 MainExe 集成链路，避免后续模块测试生成的隔离配置影响主程序启动配置。
    # 外部 JSON 使用源码内测试数据，测试合同中不写开发者电脑专用绝对路径。
    meyer_register_aggregate_test(MeyerScan.MainSmoke MeyerScan --smoke-main)
    meyer_register_aggregate_test(
        MeyerScan.ExternalOrderSmoke
        MeyerScan
        --smoke-external-order
        --external-order "${CMAKE_SOURCE_DIR}/MyExternalLaunchAdapter/test/external_order_sample.json"
        --external-order-type "ctest_demo"
    )
    meyer_register_aggregate_test(MeyerScan.LoginSDK MeyerLoginTest --smoke)

    # 基础设施、服务、资源和权限模块回归测试。
    meyer_register_aggregate_test(MeyerScan.Logger LoggerTest)
    meyer_register_aggregate_test(MeyerScan.Database DatabaseTest)
    meyer_register_aggregate_test(MeyerScan.DatabaseQtAdapter DatabaseQtAdapterTest)
    meyer_register_aggregate_test(MeyerScan.DeviceTransport DeviceTransportTest --smoke)
    meyer_register_aggregate_test(MeyerScan.DeviceCmd DeviceCmdTest --smoke)
    meyer_register_aggregate_test(MeyerScan.CaptureProcessing CaptureProcessingTest --smoke)
    meyer_register_aggregate_test(MeyerScan.CaptureImagePipeline CaptureImagePipelineTest --smoke)
    meyer_register_aggregate_test(MeyerScan.AutoExposure AutoExposureTest --smoke)
    meyer_register_aggregate_test(MeyerScan.CaptureService CaptureServiceTest --smoke)
    meyer_register_aggregate_test(MeyerScan.CaptureService.TimeoutOnce CaptureServiceTest --smoke-timeout-once)
    meyer_register_aggregate_test(MeyerScan.CaptureService.TimeoutAlways CaptureServiceTest --smoke-timeout-always)
    meyer_register_aggregate_test(MeyerScan.CaptureService.Disconnect CaptureServiceTest --smoke-disconnect)
    meyer_register_aggregate_test(MeyerScan.CaptureService.PartialPacket CaptureServiceTest --smoke-partial)
    meyer_register_aggregate_test(MeyerScan.ConfigCenter ConfigCenterTest)
    meyer_register_aggregate_test(MeyerScan.Permission PermissionTest)
    meyer_register_aggregate_test(MeyerScan.UIResources UIResourcesTest)
    meyer_register_aggregate_test(MeyerScan.UIComponents UIComponentsTest)
    meyer_register_aggregate_test(MeyerScan.CaseOrderService CaseOrderServiceTest)
    meyer_register_aggregate_test(MeyerScan.ScanSchemaService ScanSchemaServiceTest)
    meyer_register_aggregate_test(MeyerScan.RuntimeDataCenter RuntimeDataCenterTest)
    meyer_register_aggregate_test(MeyerScan.VersionManager VersionManagerTest)
    meyer_register_aggregate_test(MeyerScan.ExternalLaunchAdapter ExternalLaunchAdapterTest)

    # CTest 中的 UI 测试必须使用非交互模式；支持 --smoke 的测试显式传入参数，
    # 既允许双击测试 EXE 做人工验收，也能保证自动化事件循环按时退出。
    meyer_register_aggregate_test(MeyerScan.Calibration3DUI Calibration3DUITest)
    meyer_register_aggregate_test(MeyerScan.CalibrationColorUI CalibrationColorUITest --smoke)
    meyer_register_aggregate_test(MeyerScan.HomeUI HomeUITest --smoke)
    meyer_register_aggregate_test(MeyerScan.CaseUI CaseUITest --smoke)
    meyer_register_aggregate_test(MeyerScan.SettingsUI SettingsUITest --smoke)
    meyer_register_aggregate_test(MeyerScan.OrderCreateUI OrderCreateUITest --smoke)
    meyer_register_aggregate_test(MeyerScan.OrderScanWorkspaceShell OrderScanWorkspaceShellTest)
    meyer_register_aggregate_test(MeyerScan.ScanWorkflowUI ScanWorkflowUITest)
    meyer_register_aggregate_test(MeyerScan.DataProcessUI DataProcessUITest)
    meyer_register_aggregate_test(MeyerScan.SendUI SendUITest)
    meyer_register_aggregate_test(MeyerScan.ScanReconstructStudio ScanReconstructStudio --smoke)

    set_tests_properties(
        MeyerScan.MainSmoke
        MeyerScan.ExternalOrderSmoke
        PROPERTIES LABELS "MeyerScan;Integration"
    )
endif()
