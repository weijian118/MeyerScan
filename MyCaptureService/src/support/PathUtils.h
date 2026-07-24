// =============================================================================
// 文件: PathUtils.h
// 作用: 提供不依赖 current directory 的 DLL 路径和 UTF-8/UTF-16 转换工具。
// =============================================================================
#pragma once

#include <string>

namespace meyer
{
    namespace captureservice
    {
        // 返回当前 CaptureService DLL 所在文件夹。
        bool GetOwnDirectory(std::wstring& directory);
        // 将 UTF-8 文本转换为 Windows 宽字符串。
        bool Utf8ToWide(const char* text, std::wstring& result);
        // 将 Windows 宽字符串转换为 UTF-8 文本。
        bool WideToUtf8(const std::wstring& text, std::string& result);
        // 判断路径是否为 Windows 绝对路径。
        bool IsAbsoluteWindowsPath(const std::wstring& path);
        // 根据服务 DLL 目录补齐同级模块路径。
        std::string SiblingModulePathUtf8(const char* fileName);
    }
}
