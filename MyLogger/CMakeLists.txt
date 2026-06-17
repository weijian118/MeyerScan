# =============================================================================
# CMakeLists.txt — MeyerScan_Logger.dll
#
# 用途:
#   构建日志基础设施 DLL (MeyerScan_Logger.dll) 和
#   可选的测试可执行文件 (LoggerTest.exe)。
#
# 构建要求:
#   - CMake 3.4+     (VS2015 时代；presets 需要 3.15+，但是可选的)
#   - MSVC 14.0      (Visual Studio 2015 工具链) — C++14
#   - Windows SDK    (任意版本；我们使用 kernel32 + shell32)
#
# 无 Qt 依赖。无 VTK/PCL/OpenCV 依赖。
# 这是有意设计为整个 MeyerScan 项目中最轻量的模块。
#
# 使用方法 (VS Code + Ninja — 推荐日常开发):
#   cd F:\MeyerScan\MyLogger
#   mkdir build && cd build
#   cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
#   cmake --build .
#   .\bin\LoggerTest.exe
#
# 使用方法 (Visual Studio 2015 — 用于调试/生产构建):
#   cd F:\MeyerScan\MyLogger
#   mkdir build && cd build
#   cmake -G "Visual Studio 14 2015 Win64" ..
#   start MeyerScan_Logger.sln
#
# 使用方法 (CMake Presets — 需要 CMake 3.15+):
#   cmake --preset ninja-debug
#   cmake --build --preset ninja-debug
# =============================================================================
cmake_minimum_required(VERSION 3.4)
project(MeyerScan_Logger
    VERSION     1.0.0
    DESCRIPTION "MeyerScan structured logging infrastructure DLL"
    LANGUAGES   CXX
)

# ---------------------------------------------------------------------------
# C++ 标准: C++14
# ---------------------------------------------------------------------------
# C++14 是 VS2015 (MSVC 14.0) 完整支持的最高标准。
# 使用的主要 C++14 特性:
#   - std::make_unique
#   - std::chrono::seconds（实际上是 C++11）
#   - 泛型 lambda（auto 参数）
#   - 二进制字面量 (0b...) — 未使用，但可用
#
# 有意避免 C++17 特性（不使用 std::optional、不使用结构化绑定、
# 不使用内联变量）以保持 VS2015 兼容性。
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 不使用 MSVC 扩展（无 /clr、无 /ZW……）

# ---------------------------------------------------------------------------
# DLL 目标:  MeyerScan_Logger.dll
# ---------------------------------------------------------------------------
set(MODULE_NAME MeyerScan_Logger)

add_library(${MODULE_NAME} SHARED
    # 源文件，按数据流顺序排列:
    src/LogFormat.cpp       # 步骤 1: 原始字段 → 格式化的 UTF-8 行
    src/LogBuffer.cpp       # 步骤 2: 线程安全的行累积器
    src/LogRotation.cpp     # 步骤 3: 基于日期/大小的文件轮转逻辑
    src/LogWriter.cpp       # 步骤 4: 文件 I/O + 跨进程命名互斥量
    src/LoggerImpl.cpp      # 步骤 5: 单例 + 流水线编排
    src/Version.rc          # Windows 版本资源
)

# ---- 包含路径 ---------------------------------------------------------------
# PUBLIC:  消费者看到 include/ → 它们可以 #include "Logger.h"
# PRIVATE: 内部 .cpp 文件看到 src/ → 它们可以 #include "LogBuffer.h" 等
target_include_directories(${MODULE_NAME}
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---- 编译定义 ---------------------------------------------------------------
# MEYER_LOGGER_EXPORTS 在 Logger.h 中激活 __declspec(dllexport)。
# 这必须是 PRIVATE — 只有 DLL 本身应导出符号。
target_compile_definitions(${MODULE_NAME}
    PRIVATE MEYER_LOGGER_EXPORTS
)

# ---- 链接库 -----------------------------------------------------------------
# shell32.lib 提供 SHCreateDirectoryExW（在 LogWriter::Open 中使用）。
# kernel32.lib 由 MSVC 隐式链接；在此列出以明确表示。
target_link_libraries(${MODULE_NAME}
    PRIVATE
        shell32
)

# ---- 输出命名 ---------------------------------------------------------------
# PREFIX ""       → MeyerScan_Logger.dll（而非 libMeyerScan_Logger.dll）
# SUFFIX ".dll"   → 显式指定（Windows 上的默认值，但显式更清晰）
# OUTPUT_NAME     → 确保 .lib 导入库也正确命名
set_target_properties(${MODULE_NAME} PROPERTIES
    PREFIX      ""
    SUFFIX      ".dll"
    OUTPUT_NAME "${MODULE_NAME}"
)

# ---- MSVC 运行时: 静态链接 (/MT) -------------------------------------------
# /MT (MultiThreaded) 将 MSVC 运行时静态链接到 DLL 中。
# /MD (MultiThreadedDLL) 将在运行时需要 msvcr140.dll。
#
# 我们为 Logger.dll 选择 /MT，因为:
#   a) Logger 是第一个加载的模块 — 我们不希望在应用程序的 PATH
#      被设置之前依赖 msvcr140.dll 可被发现。
#   b) Logger.dll 很小（/MT 约 200 KB vs /MD 约 100 KB）—
#      额外的大小微不足道。
#   c) Logger 的公开 ABI 只使用 const char* / enum / 纯虚接口，
#      可被 /MD 的 Qt 模块通过导入库或运行时动态加载安全调用。
#
# 项目规则: 无 Qt 依赖的基础模块可使用 /MT；有 Qt 依赖的模块使用 /MD。
if(MSVC)
    set_property(TARGET ${MODULE_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------------------
# 测试可执行文件:  LoggerTest.exe（可选）
# ---------------------------------------------------------------------------
# 这是一个纯 C++ 控制台应用程序，测试所有 Logger 功能:
#   - 初始化
#   - 全部五个日志级别
#   - 级别过滤
#   - 多线程压力测试（10 线程 × 500 条消息）
#   - 立即 Error/Fatal 刷新
#   - 优雅关闭
#
# 该测试也作为构建验证测试 — 如果它能编译和链接，
# 说明 DLL 结构正确。
option(BUILD_TESTS "Build the Logger test executable" ON)

if(BUILD_TESTS)
    add_executable(LoggerTest
        test/main.cpp
    )

    # 测试链接 MeyerScan_Logger.dll（及其导入库）。
    target_link_libraries(LoggerTest
        PRIVATE ${MODULE_NAME}
    )

    # 为测试中使用的日志宏定义模块名称。
    target_compile_definitions(LoggerTest
        PRIVATE MEYER_MODULE_NAME="LoggerTest"
    )

    # 将测试 .exe 放在 .dll 旁边，以便在运行时能找到它，
    # 无需修改 %PATH%。
    set_target_properties(LoggerTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()
