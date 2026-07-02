cmake_minimum_required(VERSION 3.15)
project(MeyerScan_VersionManager VERSION 0.1.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core)

meyer_add_qt_core_library(MeyerScan_VersionManager MEYERSCAN_VERSIONMANAGER_EXPORTS
    src/VersionManagerImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_VersionManager
    PUBLIC include
    PRIVATE src
)
target_link_libraries(MeyerScan_VersionManager PRIVATE version)
