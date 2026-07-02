# =============================================================================
# MeyerScanCommon.cmake
#
# 用途:
#   所有模块共用的 CMake 规则集中在这里，避免每个模块复制一套 Qt 路径、
#   输出目录、C++ 标准、MSVC 运行时和兄弟模块链接写法。
#
# 维护原则:
#   - 这里只放工程规则，不放业务判断。
#   - 新增模块优先复用这些函数，不在模块 CMakeLists 里自造路径规则。
# =============================================================================

get_filename_component(MEYER_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(MEYER_QT_ROOT "C:/Qt/Qt5.6.3/5.6.3/msvc2015_64" CACHE PATH
    "Qt 5.6.3 MSVC2015 x64 install root")

if(MEYER_QT_ROOT)
    list(APPEND CMAKE_PREFIX_PATH "${MEYER_QT_ROOT}")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# 将每个模块的产物继续输出到自身 bin/<Config>，保持和 VS2015 工程一致。
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
endfunction()

# 链接同仓库兄弟模块。
# 根工程构建时直接链接 target；单模块独立 CMake 构建时回退到兄弟模块 bin/<Config> 下的导入库。
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
