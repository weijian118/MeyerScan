#pragma once

#include <QWidget>

#ifdef MEYERSCAN_ORDERSCANWORKSPACESHELL_EXPORTS
#  define MEYERSCAN_ORDERSCANWORKSPACESHELL_API __declspec(dllexport)
#else
#  define MEYERSCAN_ORDERSCANWORKSPACESHELL_API __declspec(dllimport)
#endif

// 建单到扫描工作区的步骤枚举。
// 壳子模块用这些稳定 ID 在“建单、扫描、处理、发送”之间切换页面。
enum OrderScanWorkspaceStep {
    WorkspaceStepOrderCreate = 1,
    WorkspaceStepScan = 2,
    WorkspaceStepProcess = 3,
    WorkspaceStepSend = 4,
};

// IOrderScanWorkspaceShell 是建单模块和扫描重建模块之间的统一工作区壳子。
// 设计目的:
//   - 统一建单、扫描、处理、发送几个页面的外观和切换体验。
//   - 让各步骤页面以 QWidget 形式挂入，不让步骤模块互相直接依赖。
//   - 后续扫描重建接入时，MainExe 可以先释放案例管理等非活动模块资源，再进入本壳子。
class MEYERSCAN_ORDERSCANWORKSPACESHELL_API IOrderScanWorkspaceShell {
public:
    // 虚析构函数用于保持跨 DLL 接口的多态安全。
    virtual ~IOrderScanWorkspaceShell() = default;

    // 初始化壳子模块。
    // appDirUtf8 是 MeyerScan.exe 所在目录，logDirUtf8 是统一 logs 目录。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 创建工作区根界面。
    // 创建后可以通过 AttachStepWidget 替换每一步的占位界面。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 切换当前步骤。
    // step 必须是 OrderScanWorkspaceStep 中定义的值。
    virtual void SetStep(int step) = 0;

    // 挂载某个步骤的真实页面。
    // widget 的父子关系会由壳子/Qt 容器接管。
    virtual void AttachStepWidget(int step, QWidget* widget) = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭模块并清理缓存。
    virtual void Shutdown() = 0;
};

// C ABI 工厂函数，便于后续按插件方式动态加载。
extern "C" MEYERSCAN_ORDERSCANWORKSPACESHELL_API IOrderScanWorkspaceShell* GetOrderScanWorkspaceShell();
