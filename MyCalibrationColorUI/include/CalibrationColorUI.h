#pragma once

#include <QWidget>

#ifdef MEYERSCAN_CALIBRATIONCOLORUI_EXPORTS
#  define MEYERSCAN_CALIBRATIONCOLORUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_CALIBRATIONCOLORUI_API __declspec(dllimport)
#endif

// ICalibrationColorUI 是颜色校准 UI 模块的公共接口。
// 模块边界:
//   - 本 DLL 可以使用 Qt，并在内部接入颜色校准算法和设备相关 DLL。
//   - 对外只暴露界面创建和生命周期，不让 MainExe 了解颜色校准内部步骤。
//   - 后续若颜色校准流程变化，只需要改本模块和算法适配层。
class MEYERSCAN_CALIBRATIONCOLORUI_API ICalibrationColorUI {
public:
    // 虚析构函数用于保持接口类的正确多态析构语义。
    virtual ~ICalibrationColorUI() = default;

    // 初始化颜色校准模块。
    // appDirUtf8 必须来自 QApplication::applicationDirPath() 或 MainExe 显式传入。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 创建颜色校准 QWidget。
    // 返回对象一般由调用方页面容器接管父子关系。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭模块，清理缓存指针和路径。
    virtual void Shutdown() = 0;
};

// C ABI 导出函数，便于 MainExe 后续用动态加载方式获取模块接口。
extern "C" MEYERSCAN_CALIBRATIONCOLORUI_API ICalibrationColorUI* GetCalibrationColorUI();
