# =============================================================================
# MeyerScanCommon.cmake
#
# 用途：
#   所有模块共用的 CMake 工程规则集中放在这里，避免每个模块重复维护
#   Qt 路径、输出目录、C++ 标准、MSVC 运行时和兄弟模块链接写法。
#
# 维护原则：
#   - 这里只放工程规则，不放业务判断。
#   - 新增模块优先复用这些函数，不在模块 CMakeLists.txt 中自造路径规则。
# =============================================================================

get_filename_component(MEYER_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(MEYER_QT_ROOT "C:/Qt/Qt5.6.3/5.6.3/msvc2015_64" CACHE PATH
    "Qt 5.6.3 MSVC2015 x64 install root")

set(MEYER_SQLITE_RUNTIME_DLL "${MEYER_ROOT_DIR}/ThirdParty/SQLite/win-x64/sqlite3.dll" CACHE FILEPATH
    "SQLite x64 runtime dll used by pure C++ Database module")

if(MEYER_QT_ROOT)
    list(APPEND CMAKE_PREFIX_PATH "${MEYER_QT_ROOT}")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# 将每个模块的产物输出到自身 bin/<Config>，保持和 VS2015 工程一致。
function(meyer_set_module_output target)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>"
    )
endfunction()

# Qt 模块使用动态 CRT(/MD)，与 Qt 5.6.3 预编译库保持一致。
function(meyer_use_dynamic_crt target)
    if(MSVC)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endfunction()

# 纯 C++ 轻量基础模块可使用静态 CRT(/MT)，Logger 当前采用该策略。
function(meyer_use_static_crt target)
    if(MSVC)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endfunction()

# 统一 DLL/EXE 名称和模块名宏。
function(meyer_configure_module target module_name)
    meyer_set_module_output(${target})
    target_compile_definitions(${target} PRIVATE MEYER_MODULE_NAME="${module_name}")
    target_include_directories(${target} PRIVATE "${MEYER_ROOT_DIR}/Common/include")
endfunction()

# 复制 SQLite x64 运行时到目标输出目录。
# Database 使用 LoadLibraryA("sqlite3.dll") 动态加载运行时，因此 EXE/DLL 所在目录
# 必须能找到与编译平台一致的 sqlite3.dll。当前工程固定 x64，所以不能使用
# SQLiteStudio 历史目录里的 32 位 DLL。
function(meyer_copy_sqlite_runtime target)
    if(EXISTS "${MEYER_SQLITE_RUNTIME_DLL}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MEYER_SQLITE_RUNTIME_DLL}"
                    "$<TARGET_FILE_DIR:${target}>/sqlite3.dll"
        )
    else()
        message(WARNING
            "sqlite3.dll not found: ${MEYER_SQLITE_RUNTIME_DLL}. "
            "Put x64 sqlite3.dll there before running Database/Adapter tests.")
    endif()
endfunction()

# 复制模块 Resources 到运行目录约定位置：
#   <target 输出目录>/Resources/Modules/<module_dir>/
# 每个模块源码目录保留自己的资源，构建/打包时再汇总到 MeyerScan.exe 同级目录。
function(meyer_copy_module_resources target module_dir)
    set(_meyer_resource_source "${MEYER_ROOT_DIR}/${module_dir}/Resources")
    if(EXISTS "${_meyer_resource_source}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${target}>/Resources/Modules/${module_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_meyer_resource_source}"
                    "$<TARGET_FILE_DIR:${target}>/Resources/Modules/${module_dir}"
        )
    endif()
endfunction()

# 链接同仓库兄弟模块。
# 根工程构建时直接链接 target；单模块独立 CMake 构建时回退到兄弟模块
# bin/<Config> 下的导入库。
function(meyer_link_sibling_module consumer module_dir target_name)
    target_include_directories(${consumer} PRIVATE "${MEYER_ROOT_DIR}/${module_dir}/include")
    if(TARGET ${target_name})
        target_link_libraries(${consumer} PRIVATE ${target_name})
    else()
        target_link_libraries(${consumer} PRIVATE
            optimized "${MEYER_ROOT_DIR}/${module_dir}/bin/Release/${target_name}.lib"
            debug     "${MEYER_ROOT_DIR}/${module_dir}/bin/Debug/${target_name}.lib"
        )
    endif()
endfunction()

# 添加常见 Qt Widgets DLL。
function(meyer_add_qt_widgets_library target export_define)
    add_library(${target} SHARED ${ARGN})
    meyer_configure_module(${target} ${target})
    meyer_use_dynamic_crt(${target})
    target_compile_definitions(${target} PRIVATE ${export_define})
    target_link_libraries(${target} PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)
endfunction()

# 添加常见 Qt Core DLL。
function(meyer_add_qt_core_library target export_define)
    add_library(${target} SHARED ${ARGN})
    meyer_configure_module(${target} ${target})
    meyer_use_dynamic_crt(${target})
    target_compile_definitions(${target} PRIVATE ${export_define})
    target_link_libraries(${target} PRIVATE Qt5::Core)
endfunction()
