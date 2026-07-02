# =============================================================================
# MeyerScan 根目录 CMake 聚合工程
#
# 用途:
#   1. 让 VSCode / CMake Tools 可以从 F:\MeyerScan 根目录一次看到所有模块。
#   2. 保持 VS2015 现有 .sln/.vcxproj 不变，CMake 作为并行构建入口。
#   3. 约束新增模块必须同步提供 CMakeLists.txt，避免只能在单台电脑编译。
#
# 注意:
#   - VS2015 正式调试仍可继续打开 MeyerScan_AllModules.sln。
#   - CMake 默认使用 Qt 5.6.3 MSVC2015 x64，可通过 -DMEYER_QT_ROOT=... 改路径。
#   - 外部既有登录模块 MeyerLoginWidget.dll/lib 不纳入源码构建，只在 MyMainExe 中链接。
# =============================================================================
cmake_minimum_required(VERSION 3.15)

project(MeyerScan_AllModules
    VERSION 0.1.0
    DESCRIPTION "MeyerScan modular refactor aggregate project"
    LANGUAGES CXX
)

include(cmake/MeyerScanCommon.cmake)

# 基础设施层。Logger 不依赖 Qt，必须最先可构建。
add_subdirectory(MyLogger)
add_subdirectory(MyDatabase)
add_subdirectory(MyConfigCenter)
add_subdirectory(MyPermission)
add_subdirectory(MyUIComponents)

# 业务服务与运行时读模型。
add_subdirectory(MyCaseOrderService)
add_subdirectory(MyRuntimeDataCenter)

# 设置、校准、工作台壳和历史版本清单骨架。
add_subdirectory(MyCalibration3DUI)
add_subdirectory(MyCalibrationColorUI)
add_subdirectory(MyOrderScanWorkspaceShell)
add_subdirectory(MyVersionManager)

# UI 层。
add_subdirectory(MyHomeUI)
add_subdirectory(MyCaseUI)
add_subdirectory(MySettingsUI)

# 旧 schema 参考目录也提供 CMakeLists，保证每个一级模块目录都有工程说明。
add_subdirectory(MyCaseManager)

# 登录测试宿主和主程序壳放在最后，确保前面的 DLL target 已经存在。
add_subdirectory(MyMainExe/MyLogin)
add_subdirectory(MyMainExe)
