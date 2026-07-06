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

# Infrastructure modules.
add_subdirectory(MyLogger)
add_subdirectory(MyDatabase)
add_subdirectory(MyDatabaseQtAdapter)
add_subdirectory(MyConfigCenter)
add_subdirectory(MyPermission)
add_subdirectory(MyUIComponents)

# Business services and runtime read models.
add_subdirectory(MyCaseOrderService)
add_subdirectory(MyRuntimeDataCenter)

# Settings, calibration, order workflow, scan workflow, and version skeletons.
add_subdirectory(MyCalibration3DUI)
add_subdirectory(MyCalibrationColorUI)
add_subdirectory(MyOrderCreateUI)
add_subdirectory(MyOrderScanWorkspaceShell)
add_subdirectory(MyExternalLaunchAdapter)
add_subdirectory(MyVersionManager)
add_subdirectory(MyScanWorkflowUI)
add_subdirectory(MyDataProcessUI)
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
