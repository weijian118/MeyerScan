#pragma once

#include <QWidget>

#ifdef MEYERSCAN_CALIBRATION3DUI_EXPORTS
#  define MEYERSCAN_CALIBRATION3DUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_CALIBRATION3DUI_API __declspec(dllimport)
#endif

// ICalibration3DUI 是三维校准 UI 模块的公共接口。
// 模块边界:
//   - 本 DLL 可以使用 Qt 创建界面，也可以在内部调用后续算法/设备 DLL。
//   - 对外只暴露“初始化、创建界面、版本、关闭”四类能力。
//   - MainExe 或工作区壳子只负责加载和嵌入该 QWidget，不直接参与校准计算细节。
class MEYERSCAN_CALIBRATION3DUI_API ICalibration3DUI {
public:
    // 虚析构函数保证通过接口指针释放派生类时 ABI 行为正确。
    virtual ~ICalibration3DUI() = default;

    // 初始化模块。
    // appDirUtf8 必须是 MeyerScan.exe 所在目录；logDirUtf8 是统一 logs 目录。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 创建三维校准主界面。
    // 调用方取得 QWidget* 后负责把它挂到自己的页面容器中。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 返回模块版本字符串，用于 MainExe 启动时生成版本清单。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭模块并释放缓存状态。
    // 注意 QWidget 的销毁仍由 Qt 父子关系或调用方负责。
    virtual void Shutdown() = 0;
};

// C ABI 工厂函数。
// 使用 extern "C" 是为了后续可以通过 LoadLibrary/GetProcAddress 动态加载模块。
extern "C" MEYERSCAN_CALIBRATION3DUI_API ICalibration3DUI* GetCalibration3DUI();
