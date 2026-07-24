// =============================================================================
// 文件: PathUtils.cpp
// 作用: 实现 CaptureService 的模块目录定位和路径转换。
// =============================================================================
#include "PathUtils.h"

#include <windows.h>

#include <vector>

namespace meyer
{
    namespace captureservice
    {
        // 通过函数地址反查 DLL 模块句柄，不读取进程 current directory。
        bool GetOwnDirectory(std::wstring& directory)
        {
            HMODULE module = nullptr;
            const BOOL found = ::GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&GetOwnDirectory),
                &module);
            if (!found || module == nullptr)
            {
                return false;
            }

            std::vector<wchar_t> path(32768U, L'\0');
            const DWORD length = ::GetModuleFileNameW(
                module, &path[0], static_cast<DWORD>(path.size()));
            if (length == 0U || length >= path.size())
            {
                return false;
            }

            directory.assign(&path[0], length);
            const std::wstring::size_type slash = directory.find_last_of(L"\\/");
            if (slash == std::wstring::npos)
            {
                directory.clear();
                return false;
            }
            directory.resize(slash);
            return true;
        }

        // MultiByteToWideChar 明确指定 UTF-8，避免系统活动代码页造成路径乱码。
        bool Utf8ToWide(const char* text, std::wstring& result)
        {
            result.clear();
            if (text == nullptr || text[0] == '\0')
            {
                return false;
            }
            const int length = ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
            if (length <= 0)
            {
                return false;
            }
            std::vector<wchar_t> buffer(static_cast<std::size_t>(length), L'\0');
            if (::MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                    &buffer[0], length) <= 0)
            {
                return false;
            }
            result.assign(&buffer[0], static_cast<std::size_t>(length - 1));
            return true;
        }

        // WideCharToMultiByte 同样固定使用 UTF-8，保证 C ABI 文本可跨模块复制。
        bool WideToUtf8(const std::wstring& text, std::string& result)
        {
            result.clear();
            if (text.empty())
            {
                return false;
            }
            const int length = ::WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, text.c_str(), -1,
                nullptr, 0, nullptr, nullptr);
            if (length <= 0)
            {
                return false;
            }
            std::vector<char> buffer(static_cast<std::size_t>(length), '\0');
            if (::WideCharToMultiByte(
                    CP_UTF8, WC_ERR_INVALID_CHARS, text.c_str(), -1,
                    &buffer[0], length, nullptr, nullptr) <= 0)
            {
                return false;
            }
            result.assign(&buffer[0], static_cast<std::size_t>(length - 1));
            return true;
        }

        // 绝对路径只接受盘符路径或 UNC 路径，防止 DLL 搜索路径被外部环境劫持。
        bool IsAbsoluteWindowsPath(const std::wstring& path)
        {
            const bool drivePath = path.size() >= 3U &&
                ((path[0] >= L'A' && path[0] <= L'Z') ||
                 (path[0] >= L'a' && path[0] <= L'z')) &&
                path[1] == L':' &&
                (path[2] == L'\\' || path[2] == L'/');
            const bool uncPath = path.size() >= 3U &&
                path[0] == L'\\' && path[1] == L'\\';
            return drivePath || uncPath;
        }

        // 统一把服务自身目录作为模块搜索根，避免第三方从任意目录启动时加载旧库。
        std::string SiblingModulePathUtf8(const char* fileName)
        {
            std::wstring directory;
            std::string result;
            if (fileName == nullptr || !GetOwnDirectory(directory))
            {
                return result;
            }
            directory += L"\\";
            std::wstring wideName;
            if (!Utf8ToWide(fileName, wideName))
            {
                return result;
            }
            directory += wideName;
            WideToUtf8(directory, result);
            return result;
        }
    }
}
