// =============================================================================
// 文件: stdafx.h
// 作用: 兼容初版源码中的历史包含路径。
//
// 说明:
//   新工程不启用预编译头，但保留这个轻量平台头可避免把 Windows API 头重复写入
//   每个实现文件。这里不得继续堆放业务头文件或第三方库头文件。
// =============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif

#include <windows.h>
