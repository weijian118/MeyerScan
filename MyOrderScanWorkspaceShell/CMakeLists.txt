cmake_minimum_required(VERSION 3.15)

project(MeyerScan_OrderScanWorkspaceShell VERSION 0.1.3 LANGUAGES CXX)



include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)



meyer_add_qt_widgets_library(MeyerScan_OrderScanWorkspaceShell MEYERSCAN_ORDERSCANWORKSPACESHELL_EXPORTS

    src/OrderScanWorkspaceShellImpl.cpp

    src/Version.rc

)



target_include_directories(MeyerScan_OrderScanWorkspaceShell

    PUBLIC include

    PRIVATE src

)

meyer_link_sibling_module(MeyerScan_OrderScanWorkspaceShell MyLogger MeyerScan_Logger)
meyer_copy_module_resources(MeyerScan_OrderScanWorkspaceShell MyOrderScanWorkspaceShell)



option(MEYER_BUILD_ORDERSCANWORKSPACESHELLTEST "Build OrderScanWorkspaceShellTest.exe" ON)

if(MEYER_BUILD_ORDERSCANWORKSPACESHELLTEST)

    add_executable(OrderScanWorkspaceShellTest test/main.cpp)

    meyer_configure_module(OrderScanWorkspaceShellTest OrderScanWorkspaceShellTest)

    meyer_use_dynamic_crt(OrderScanWorkspaceShellTest)

    target_include_directories(OrderScanWorkspaceShellTest PRIVATE include)

    target_link_libraries(OrderScanWorkspaceShellTest PRIVATE MeyerScan_OrderScanWorkspaceShell Qt5::Core Qt5::Gui Qt5::Widgets)

    meyer_link_sibling_module(OrderScanWorkspaceShellTest MyLogger MeyerScan_Logger)
    meyer_copy_module_resources(OrderScanWorkspaceShellTest MyOrderScanWorkspaceShell)



    add_custom_command(TARGET OrderScanWorkspaceShellTest POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:OrderScanWorkspaceShellTest>/Qt5Core.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Gui.dll" "$<TARGET_FILE_DIR:OrderScanWorkspaceShellTest>/Qt5Gui.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Widgets.dll" "$<TARGET_FILE_DIR:OrderScanWorkspaceShellTest>/Qt5Widgets.dll"

        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:OrderScanWorkspaceShellTest>/platforms"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/plugins/platforms/qwindows.dll" "$<TARGET_FILE_DIR:OrderScanWorkspaceShellTest>/platforms/qwindows.dll"

    )

endif()
