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



option(MEYER_BUILD_VERSIONMANAGERTEST "Build VersionManagerTest.exe" ON)

if(MEYER_BUILD_VERSIONMANAGERTEST)

    add_executable(VersionManagerTest test/main.cpp)

    meyer_configure_module(VersionManagerTest VersionManagerTest)

    meyer_use_dynamic_crt(VersionManagerTest)

    target_include_directories(VersionManagerTest PRIVATE include)

    target_link_libraries(VersionManagerTest PRIVATE MeyerScan_VersionManager Qt5::Core)



    add_custom_command(TARGET VersionManagerTest POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:VersionManagerTest>/Qt5Core.dll"

    )

endif()
