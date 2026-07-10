cmake_minimum_required(VERSION 3.15)

project(MeyerScan_SendUI VERSION 0.1.1 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_SendUI MEYERSCAN_SENDUI_EXPORTS
    src/SendUIImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_SendUI
    PUBLIC include
    PRIVATE src ../MyUIComponents/include
)

meyer_link_sibling_module(MeyerScan_SendUI MyLogger MeyerScan_Logger)
meyer_copy_module_resources(MeyerScan_SendUI MySendUI)

if(TARGET MeyerScan_Logger)
    add_custom_command(TARGET MeyerScan_SendUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:MeyerScan_Logger>"
                "$<TARGET_FILE_DIR:MeyerScan_SendUI>/MeyerScan_Logger.dll"
    )
endif()

if(TARGET MeyerScan_UIComponents)
    add_dependencies(MeyerScan_SendUI MeyerScan_UIComponents)
    add_custom_command(TARGET MeyerScan_SendUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:MeyerScan_UIComponents>"
                "$<TARGET_FILE_DIR:MeyerScan_SendUI>/MeyerScan_UIComponents.dll"
    )
endif()

option(MEYER_BUILD_SENDUI_TEST "Build SendUITest.exe" ON)
if(MEYER_BUILD_SENDUI_TEST)
    add_executable(SendUITest test/main.cpp)
    meyer_configure_module(SendUITest SendUITest)
    meyer_use_dynamic_crt(SendUITest)
    target_include_directories(SendUITest PRIVATE include)
    target_link_libraries(SendUITest PRIVATE MeyerScan_SendUI Qt5::Core Qt5::Gui Qt5::Widgets)
    meyer_link_sibling_module(SendUITest MyLogger MeyerScan_Logger)
    if(TARGET MeyerScan_UIComponents)
        add_dependencies(SendUITest MeyerScan_UIComponents)
    endif()

    set(send_ui_test_post_build_commands
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:SendUITest>/Qt5Core.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Gui.dll" "$<TARGET_FILE_DIR:SendUITest>/Qt5Gui.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Widgets.dll" "$<TARGET_FILE_DIR:SendUITest>/Qt5Widgets.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SendUITest>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/plugins/platforms/qwindows.dll" "$<TARGET_FILE_DIR:SendUITest>/platforms/qwindows.dll"
    )
    if(TARGET MeyerScan_UIComponents)
        list(APPEND send_ui_test_post_build_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:MeyerScan_UIComponents>" "$<TARGET_FILE_DIR:SendUITest>/MeyerScan_UIComponents.dll"
        )
    endif()
    if(TARGET MeyerScan_Logger)
        list(APPEND send_ui_test_post_build_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:MeyerScan_Logger>" "$<TARGET_FILE_DIR:SendUITest>/MeyerScan_Logger.dll"
        )
    endif()
    add_custom_command(TARGET SendUITest POST_BUILD
        ${send_ui_test_post_build_commands}
    )
    meyer_copy_module_resources(SendUITest MySendUI)
endif()
