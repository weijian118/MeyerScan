cmake_minimum_required(VERSION 3.4)
project(MeyerScan_CaseUI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_PREFIX_PATH "C:/Qt/Qt5.6.3/5.6.3/msvc2015_64")
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)

add_library(MeyerScan_CaseUI SHARED
    src/CaseUIImpl.cpp
    src/Version.rc
)

target_include_directories(MeyerScan_CaseUI
    PUBLIC include
    PRIVATE
        src
        ../MyDatabase/include
        ../MyLogger/include
)

target_compile_definitions(MeyerScan_CaseUI PRIVATE
    MEYERSCAN_CASEUI_EXPORTS
    MEYER_MODULE_NAME="MeyerScan_CaseUI"
)

target_link_directories(MeyerScan_CaseUI PRIVATE
    ../MyDatabase/bin/$<CONFIG>
)

target_link_libraries(MeyerScan_CaseUI PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Sql
    MeyerScan_Database
)

add_executable(CaseUITest test/main.cpp)
target_include_directories(CaseUITest PRIVATE include)
target_link_libraries(CaseUITest PRIVATE MeyerScan_CaseUI Qt5::Core Qt5::Gui Qt5::Widgets)

