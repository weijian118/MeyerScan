#include "UIResources.h"

#include <QMutex>
#include <QMutexLocker>
#include <QResource>

#include <Windows.h>

namespace {
namespace ModuleInfo {
// 模块名和 DLL 文件说明保持一致，便于日志、版本清单和现场文件对应。
const char* Name = "MeyerScan_UIResources";

// 代码版本必须与 Version.rc 的 FILEVERSION/PRODUCTVERSION 同步修改。
const char* Version = "MeyerScan_UIResources v0.1.4 (2026-07-16)";
}

// Version.rc 中 RCDATA 资源的固定编号。
// 编号一旦发布就不要随意修改，否则旧版本代码会找不到内嵌资源包。
const int kUiResourcePayloadId = 101;

// 注册状态由本 DLL 自己维护。
// 多个 UI DLL 可能同时调用初始化接口，因此必须通过互斥锁保护。
QMutex g_resourceMutex;
bool g_resourceInitialized = false;
const unsigned char* g_resourceData = nullptr;

// 根据当前函数地址取得本 DLL 的 HMODULE。
// 不能用进程 EXE 句柄，否则 FindResource 会错误地去 MeyerScan.exe 中查找 RCDATA。
HMODULE CurrentModuleHandle() {
    HMODULE module = nullptr;
    // GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 表示第二个参数是模块内地址，不是 DLL 名称。
    // UNCHANGED_REFCOUNT 避免这里只为查询句柄而额外增加 DLL 引用计数。
    const BOOL ok = GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&MeyerScanInitializeUiResources),
        &module);
    return ok ? module : nullptr;
}
}

// 注册 DLL 内嵌的 Qt 二进制资源。
extern "C" MEYERSCAN_UIRESOURCES_API bool MeyerScanInitializeUiResources() {
    // QMutexLocker 使用 RAII：函数无论从哪个 return 离开都会自动解锁。
    QMutexLocker locker(&g_resourceMutex);
    if (g_resourceInitialized) {
        // 初始化接口允许重复调用，便于多个独立 UI 模块都在创建页面前做防御性检查。
        return true;
    }

    const HMODULE module = CurrentModuleHandle();
    if (!module) {
        return false;
    }

    // FindResource 只定位资源描述；LoadResource/LockResource 才取得只读内存地址。
    const HRSRC resourceInfo = FindResourceW(
        module,
        MAKEINTRESOURCEW(kUiResourcePayloadId),
        // 10 是 Win32 的 RT_RCDATA 数值。
        // 显式使用宽字符 MAKEINTRESOURCEW，避免 CMake/手写 vcxproj 的 Unicode 宏差异改变参数类型。
        MAKEINTRESOURCEW(10));
    if (!resourceInfo) {
        return false;
    }

    const HGLOBAL resourceHandle = LoadResource(module, resourceInfo);
    if (!resourceHandle) {
        return false;
    }

    const void* rawData = LockResource(resourceHandle);
    const DWORD resourceSize = SizeofResource(module, resourceInfo);
    if (!rawData || resourceSize == 0) {
        return false;
    }

    // QResource::registerResource(const uchar*) 直接读取 rcc -binary 生成的数据结构。
    // RCDATA 内存由 Windows 映射的 DLL 持有，只要 DLL 不卸载，指针就始终有效。
    const unsigned char* resourceData = static_cast<const unsigned char*>(rawData);
    if (!QResource::registerResource(resourceData)) {
        return false;
    }

    g_resourceData = resourceData;
    g_resourceInitialized = true;
    return true;
}

// 查询资源注册状态。
extern "C" MEYERSCAN_UIRESOURCES_API bool MeyerScanUiResourcesInitialized() {
    QMutexLocker locker(&g_resourceMutex);
    return g_resourceInitialized;
}

// 注销 Qt 资源。
extern "C" MEYERSCAN_UIRESOURCES_API void MeyerScanShutdownUiResources() {
    QMutexLocker locker(&g_resourceMutex);
    if (!g_resourceInitialized || !g_resourceData) {
        return;
    }

    // 注销后清空状态，测试宿主可以再次调用 Initialize 验证重复注册生命周期。
    QResource::unregisterResource(g_resourceData);
    g_resourceData = nullptr;
    g_resourceInitialized = false;
}

// 返回代码版本。
extern "C" MEYERSCAN_UIRESOURCES_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
