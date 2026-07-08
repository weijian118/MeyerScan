cmake_minimum_required(VERSION 3.15)

project(MeyerScan_OrderCreateUI VERSION 0.4.0 LANGUAGES CXX)

include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

meyer_add_qt_widgets_library(MeyerScan_OrderCreateUI MEYERSCAN_ORDERCREATEUI_EXPORTS
    src/OrderCreateUIImpl.cpp
    src/ToothTreatmentPlanWidget.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_OrderCreateUI
    PUBLIC include
    PRIVATE src ../MyUIComponents/include
)

meyer_link_sibling_module(MeyerScan_OrderCreateUI MyLogger MeyerScan_Logger)
if(TARGET MeyerScan_UIComponents)
    add_dependencies(MeyerScan_OrderCreateUI MeyerScan_UIComponents)

    add_custom_command(TARGET MeyerScan_OrderCreateUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:MeyerScan_UIComponents>"
                "$<TARGET_FILE_DIR:MeyerScan_OrderCreateUI>/MeyerScan_UIComponents.dll"
    )
endif()

set(MEYER_ORDERCREATE_TREATMENT_ASSET_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon/createModule/sacanPlan")

if(EXISTS "${MEYER_ORDERCREATE_TREATMENT_ASSET_DIR}")
    add_custom_command(TARGET MeyerScan_OrderCreateUI POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -Dsource_dir="${MEYER_ORDERCREATE_TREATMENT_ASSET_DIR}"
                -Dtarget_dir="$<TARGET_FILE_DIR:MeyerScan_OrderCreateUI>/Resources/Modules/MyOrderCreateUI/icon/createModule/sacanPlan"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/copy_treatment_assets.cmake"
    )
else()
    message(WARNING "OrderCreateUI treatment plan assets not found: ${MEYER_ORDERCREATE_TREATMENT_ASSET_DIR}")
endif()

option(MEYER_BUILD_ORDERCREATEUITEST "Build OrderCreateUITest.exe" ON)
if(MEYER_BUILD_ORDERCREATEUITEST)
    add_executable(OrderCreateUITest test/main.cpp)
    meyer_configure_module(OrderCreateUITest OrderCreateUITest)
    meyer_use_dynamic_crt(OrderCreateUITest)
    target_include_directories(OrderCreateUITest PRIVATE include)
    target_link_libraries(OrderCreateUITest PRIVATE MeyerScan_OrderCreateUI Qt5::Core Qt5::Gui Qt5::Widgets)
    meyer_link_sibling_module(OrderCreateUITest MyLogger MeyerScan_Logger)
    if(TARGET MeyerScan_UIComponents)
        add_dependencies(OrderCreateUITest MeyerScan_UIComponents)
    endif()

    set(order_create_test_post_build_commands
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:OrderCreateUITest>/Qt5Core.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Gui.dll" "$<TARGET_FILE_DIR:OrderCreateUITest>/Qt5Gui.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Widgets.dll" "$<TARGET_FILE_DIR:OrderCreateUITest>/Qt5Widgets.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:OrderCreateUITest>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/plugins/platforms/qwindows.dll" "$<TARGET_FILE_DIR:OrderCreateUITest>/platforms/qwindows.dll"
    )
    if(TARGET MeyerScan_UIComponents)
        list(APPEND order_create_test_post_build_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:MeyerScan_UIComponents>" "$<TARGET_FILE_DIR:OrderCreateUITest>/MeyerScan_UIComponents.dll"
        )
    endif()
    add_custom_command(TARGET OrderCreateUITest POST_BUILD
        ${order_create_test_post_build_commands}
    )
endif()
