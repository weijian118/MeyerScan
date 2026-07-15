#pragma once

#include <QWidget>

#ifdef MEYERSCAN_SCANRECONSTRUCTSTUDIO_EXPORTS
#  define MEYERSCAN_SCANRECONSTRUCTSTUDIO_API __declspec(dllexport)
#else
#  define MEYERSCAN_SCANRECONSTRUCTSTUDIO_API __declspec(dllimport)
#endif

// 扫描重建嵌入接口 ABI 版本。
static const int MEYER_SCAN_RECONSTRUCT_STUDIO_API_VERSION = 1;

// 扫描重建工作区的公开嵌入接口。
// 同一套代码既可以编译为 DLL 嵌入 MeyerScan.exe，也可以编译为 ScanReconstructStudio.exe 独立运行。
class MEYERSCAN_SCANRECONSTRUCTSTUDIO_API IScanReconstructStudio {
public:
    virtual ~IScanReconstructStudio() = default;

    // appDirUtf8 是 MeyerScan.exe 或 ScanReconstructStudio.exe 所在目录。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 创建可嵌入的无边框界面；contextJsonUtf8 传订单/会话上下文。
    virtual QWidget* CreateWidget(const char* contextJsonUtf8, QWidget* parent = nullptr) = 0;

    // 执行轻量加载/切换/释放自检，不显示窗口。
    virtual bool RunSmoke(const char* contextJsonUtf8) = 0;

    // 返回模块代码版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 释放本模块持有的活动 UI 资源。
    virtual void Shutdown() = 0;
};

extern "C" MEYERSCAN_SCANRECONSTRUCTSTUDIO_API IScanReconstructStudio* GetScanReconstructStudio();
