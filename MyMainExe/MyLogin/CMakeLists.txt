cmake_minimum_required(VERSION 3.15)
project(MeyerScan_MyLogin VERSION 0.1.0 LANGUAGES CXX)

include(../../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)

set(MEYER_LOGIN_ROOT "D:/wj/My-wj/MyLogin" CACHE PATH
    "External MeyerLoginWidget SDK root")

add_executable(MeyerLoginTest
    src/LoginHost.cpp
    test/main.cpp
    src/Version.rc
)

meyer_configure_module(MeyerLoginTest MeyerScan_LoginTest)
meyer_use_dynamic_crt(MeyerLoginTest)

target_include_directories(MeyerLoginTest PRIVATE
    include
    "${MEYER_LOGIN_ROOT}"
)
target_link_libraries(MeyerLoginTest PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    "${MEYER_LOGIN_ROOT}/lib/MeyerLoginWidget.lib"
)
