#pragma once

#ifdef MEYERSCAN_UIRESOURCES_EXPORTS
#define MEYERSCAN_UIRESOURCES_API __declspec(dllexport)
#else
#define MEYERSCAN_UIRESOURCES_API __declspec(dllimport)
#endif

// 注册 DLL 内嵌的 Qt 二进制资源。
//
// 调用要求：
//   1. QApplication/QCoreApplication 创建后调用；
//   2. 可以重复调用，模块内部会保证只注册一次；
//   3. 成功后资源统一使用 :/MeyerScan/Modules/<模块名>/... 访问。
extern "C" MEYERSCAN_UIRESOURCES_API bool MeyerScanInitializeUiResources();

// 查询资源包是否已经成功注册。
// 该接口只返回当前进程状态，不触发加载或注册动作。
extern "C" MEYERSCAN_UIRESOURCES_API bool MeyerScanUiResourcesInitialized();

// 注销当前 DLL 注册的 Qt 资源。
// 正式程序通常让资源保持到进程退出；该接口主要供测试宿主验证完整生命周期。
extern "C" MEYERSCAN_UIRESOURCES_API void MeyerScanShutdownUiResources();

// 返回资源模块代码版本，供 versionList 同时记录文件版本和代码版本。
extern "C" MEYERSCAN_UIRESOURCES_API const char* GetMeyerModuleVersion();
