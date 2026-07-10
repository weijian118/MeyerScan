cmake_minimum_required(VERSION 3.15)
project(MeyerScan_UIResources VERSION 0.1.1 LANGUAGES CXX RC)

include(../cmake/MeyerScanCommon.cmake)
find_package(Qt5 REQUIRED COMPONENTS Core)

find_program(MEYER_QT_RCC_EXECUTABLE
    NAMES rcc rcc.exe
    HINTS "${MEYER_QT_ROOT}/bin"
    REQUIRED
)

set(MEYER_UIRESOURCE_QRC "${CMAKE_CURRENT_SOURCE_DIR}/resources/MeyerScanUiResources.qrc")
set(MEYER_UIRESOURCE_RCC "${CMAKE_CURRENT_SOURCE_DIR}/generated/MeyerScanUiResources.rcc")

# 资源清单每次构建前从各模块 Resources 目录重新生成。
# 这样新增 PNG/QSS 后不需要人工维护数百行 qrc 文件，也不会漏进发布 DLL。
add_custom_target(MeyerScan_UIResourcesPayload ALL
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/GenerateResourceManifest.ps1"
            -RepositoryRoot "${MEYER_ROOT_DIR}"
            -OutputPath "${MEYER_UIRESOURCE_QRC}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/generated"
    COMMAND "${MEYER_QT_RCC_EXECUTABLE}" -binary
            "${MEYER_UIRESOURCE_QRC}"
            -o "${MEYER_UIRESOURCE_RCC}"
    BYPRODUCTS "${MEYER_UIRESOURCE_QRC}" "${MEYER_UIRESOURCE_RCC}"
    COMMENT "Generating MeyerScan embedded Qt UI resource payload"
    VERBATIM
)

meyer_add_qt_core_library(MeyerScan_UIResources MEYERSCAN_UIRESOURCES_EXPORTS
    src/UIResources.cpp
    src/Version.rc
)
add_dependencies(MeyerScan_UIResources MeyerScan_UIResourcesPayload)
target_include_directories(MeyerScan_UIResources PUBLIC include PRIVATE src)

option(MEYER_BUILD_UIRESOURCES_TEST "Build UIResourcesTest.exe" ON)
if(MEYER_BUILD_UIRESOURCES_TEST)
    add_executable(UIResourcesTest test/main.cpp test/Version.rc)
    meyer_configure_module(UIResourcesTest MeyerScan_UIResourcesTest)
    meyer_use_dynamic_crt(UIResourcesTest)
    target_include_directories(UIResourcesTest PRIVATE include)
    target_link_libraries(UIResourcesTest PRIVATE MeyerScan_UIResources Qt5::Core)
endif()
