cmake_minimum_required(VERSION 3.15)

project(MeyerScan_DatabaseQtAdapter VERSION 0.1.0 LANGUAGES CXX)



include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core)



add_library(MeyerScan_DatabaseQtAdapter SHARED

    src/DatabaseQtAdapter.cpp

    src/Version.rc

)



meyer_configure_module(MeyerScan_DatabaseQtAdapter MeyerScan_DatabaseQtAdapter)

meyer_use_dynamic_crt(MeyerScan_DatabaseQtAdapter)



target_compile_definitions(MeyerScan_DatabaseQtAdapter PRIVATE MEYERSCAN_DATABASEQTADAPTER_EXPORTS)

target_include_directories(MeyerScan_DatabaseQtAdapter

    PUBLIC include

    PRIVATE src

)

target_link_libraries(MeyerScan_DatabaseQtAdapter PRIVATE Qt5::Core)

meyer_link_sibling_module(MeyerScan_DatabaseQtAdapter MyDatabase MeyerScan_Database)

meyer_link_sibling_module(MeyerScan_DatabaseQtAdapter MyLogger MeyerScan_Logger)

meyer_copy_sqlite_runtime(MeyerScan_DatabaseQtAdapter)



option(MEYER_BUILD_DATABASEQTADAPTERTEST "Build DatabaseQtAdapterTest.exe" ON)

if(MEYER_BUILD_DATABASEQTADAPTERTEST)

    add_executable(DatabaseQtAdapterTest test/main.cpp)

    meyer_configure_module(DatabaseQtAdapterTest DatabaseQtAdapterTest)

    meyer_use_dynamic_crt(DatabaseQtAdapterTest)

    target_include_directories(DatabaseQtAdapterTest PRIVATE include)

    target_link_libraries(DatabaseQtAdapterTest PRIVATE MeyerScan_DatabaseQtAdapter Qt5::Core)

    meyer_link_sibling_module(DatabaseQtAdapterTest MyLogger MeyerScan_Logger)

    meyer_copy_sqlite_runtime(DatabaseQtAdapterTest)



    add_custom_command(TARGET DatabaseQtAdapterTest POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:DatabaseQtAdapterTest>/Qt5Core.dll"

    )

endif()
