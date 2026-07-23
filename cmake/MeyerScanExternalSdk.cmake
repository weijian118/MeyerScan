# =============================================================================
# MeyerScanExternalSdk.cmake
#
# 作用：集中解析仓库外部 SDK 的位置，并给目标配置头文件、导入库和运行文件。
# 模块 CMakeLists.txt 不再保存开发电脑的绝对路径，整个仓库移动后仍可配置。
# =============================================================================

if(NOT DEFINED MEYER_ROOT_DIR OR MEYER_ROOT_DIR STREQUAL "")
    get_filename_component(MEYER_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

# 显式 cache 值优先；其次允许 CI/其他电脑通过环境变量覆盖；最后使用仓库副本。
if(NOT DEFINED MEYER_LOGIN_SDK_ROOT OR MEYER_LOGIN_SDK_ROOT STREQUAL "")
    if(DEFINED ENV{MEYER_LOGIN_ROOT} AND
       EXISTS "$ENV{MEYER_LOGIN_ROOT}/include/MeyerLoginWidget.h")
        set(_meyer_login_sdk_default "$ENV{MEYER_LOGIN_ROOT}")
    else()
        set(_meyer_login_sdk_default "${MEYER_ROOT_DIR}/External/MyLoginSDK")
    endif()
    set(MEYER_LOGIN_SDK_ROOT "${_meyer_login_sdk_default}" CACHE PATH
        "MeyerLoginWidget SDK root containing include, lib and runtime")
endif()

set(MEYER_LOGIN_INCLUDE_DIR "${MEYER_LOGIN_SDK_ROOT}/include")
set(MEYER_LOGIN_LIBRARY "${MEYER_LOGIN_SDK_ROOT}/lib/MeyerLoginWidget.lib")
set(MEYER_LOGIN_RUNTIME_DIR "${MEYER_LOGIN_SDK_ROOT}/runtime")
set(MEYER_LOGIN_RUNTIME_DLL "${MEYER_LOGIN_RUNTIME_DIR}/MeyerLoginWidget.dll")
set(MEYER_LOGIN_LICENSE_FILE "${MEYER_LOGIN_RUNTIME_DIR}/Resources/license.lic")
file(GLOB MEYER_LOGIN_TRANSLATIONS "${MEYER_LOGIN_RUNTIME_DIR}/translations/*.qm")
file(GLOB MEYER_LOGIN_RUNTIME_DEPENDENCIES
    "${MEYER_LOGIN_RUNTIME_DIR}/dependencies/*.dll")

# 配置阶段尽早报告 SDK 缺失，避免链接或运行时才出现难以定位的错误。
foreach(_meyer_login_required
        "${MEYER_LOGIN_INCLUDE_DIR}/MeyerLoginWidget.h"
        "${MEYER_LOGIN_LIBRARY}"
        "${MEYER_LOGIN_RUNTIME_DLL}"
        "${MEYER_LOGIN_LICENSE_FILE}")
    if(NOT EXISTS "${_meyer_login_required}")
        message(FATAL_ERROR "MeyerLoginWidget SDK file is missing: ${_meyer_login_required}")
    endif()
endforeach()
if(NOT MEYER_LOGIN_TRANSLATIONS)
    message(FATAL_ERROR
        "MeyerLoginWidget translations are missing: ${MEYER_LOGIN_RUNTIME_DIR}/translations")
endif()
if(NOT MEYER_LOGIN_RUNTIME_DEPENDENCIES)
    message(FATAL_ERROR
        "MeyerLoginWidget runtime dependencies are missing: ${MEYER_LOGIN_RUNTIME_DIR}/dependencies")
endif()

# 为登录宿主目标配置编译和运行依赖。qm 仍复制到 EXE 同级目录，匹配既有登录
# SDK 的内部加载约定；许可文件统一复制到 Resources，匹配 MainExe 参数路径。
function(meyer_use_login_sdk target)
    target_include_directories(${target} PRIVATE "${MEYER_LOGIN_INCLUDE_DIR}")
    target_link_libraries(${target} PRIVATE "${MEYER_LOGIN_LIBRARY}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target}>/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MEYER_LOGIN_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MEYER_LOGIN_TRANSLATIONS}
                "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MEYER_LOGIN_RUNTIME_DEPENDENCIES}
                "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MEYER_LOGIN_LICENSE_FILE}"
                "$<TARGET_FILE_DIR:${target}>/Resources/license.lic"
    )
endfunction()
