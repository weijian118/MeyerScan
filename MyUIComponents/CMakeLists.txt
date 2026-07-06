cmake_minimum_required(VERSION 3.15)

project(MeyerScan_UIComponents VERSION 0.4.0 LANGUAGES CXX)



include(../cmake/MeyerScanCommon.cmake)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)



meyer_add_qt_widgets_library(MeyerScan_UIComponents MEYERSCAN_UICOMPONENTS_EXPORTS

    src/UIComponentsImpl.cpp

    src/Version.rc

)



target_include_directories(MeyerScan_UIComponents

    PUBLIC include

    PRIVATE src

)



option(MEYER_BUILD_UICOMPONENTSTEST "Build UIComponentsTest.exe" ON)

if(MEYER_BUILD_UICOMPONENTSTEST)

    add_executable(UIComponentsTest test/main.cpp)

    meyer_configure_module(UIComponentsTest UIComponentsTest)

    meyer_use_dynamic_crt(UIComponentsTest)

    target_include_directories(UIComponentsTest PRIVATE include)

    target_link_libraries(UIComponentsTest PRIVATE MeyerScan_UIComponents Qt5::Core Qt5::Gui Qt5::Widgets)



    add_custom_command(TARGET UIComponentsTest POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Core.dll" "$<TARGET_FILE_DIR:UIComponentsTest>/Qt5Core.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Gui.dll" "$<TARGET_FILE_DIR:UIComponentsTest>/Qt5Gui.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/bin/Qt5Widgets.dll" "$<TARGET_FILE_DIR:UIComponentsTest>/Qt5Widgets.dll"

        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:UIComponentsTest>/platforms"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MEYER_QT_ROOT}/plugins/platforms/qwindows.dll" "$<TARGET_FILE_DIR:UIComponentsTest>/platforms/qwindows.dll"

    )

endif()
