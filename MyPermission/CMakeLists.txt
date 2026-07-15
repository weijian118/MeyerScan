cmake_minimum_required(VERSION 3.15)

project(MeyerScan_Permission VERSION 0.1.1 LANGUAGES CXX)



include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core)



# Permission 是非界面模块。当前内部使用 Qt JSON 读取 permission_rules.json；

# 公共接口继续使用 const char* / bool，便于未来替换内部实现而不影响调用方。

meyer_add_qt_core_library(MeyerScan_Permission MEYERSCAN_PERMISSION_EXPORTS

    src/PermissionImpl.cpp

    src/Version.rc

)



target_include_directories(MeyerScan_Permission

    PUBLIC include

    PRIVATE src

)



option(MEYER_BUILD_PERMISSIONTEST "Build PermissionTest.exe" ON)

if(MEYER_BUILD_PERMISSIONTEST)

    add_executable(PermissionTest test/main.cpp)

    meyer_configure_module(PermissionTest PermissionTest)

    meyer_use_dynamic_crt(PermissionTest)

    target_include_directories(PermissionTest PRIVATE include)

    target_link_libraries(PermissionTest PRIVATE MeyerScan_Permission Qt5::Core)



    add_custom_command(TARGET PermissionTest POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:PermissionTest>/Qt5Core.dll"

    )

endif()
