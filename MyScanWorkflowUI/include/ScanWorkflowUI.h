#pragma once

#include <QWidget>

#ifdef MEYERSCAN_SCANWORKFLOWUI_EXPORTS
#  define MEYERSCAN_SCANWORKFLOWUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_SCANWORKFLOWUI_API __declspec(dllimport)
#endif

// 扫描阶段向宿主上报的稳定动作编号。
//
// 宿主只依赖整数，不依赖按钮对象、显示文案或 Qt 信号类型。
// 新动作只能追加到末尾，已有数值不能修改，避免不同版本 DLL 解释错位。
enum ScanWorkflowActionId {
    ScanWorkflowActionPrevious = 1,
    ScanWorkflowActionNext = 2,
    ScanWorkflowActionStartPause = 3,
    ScanWorkflowActionComplete = 4,
    ScanWorkflowActionDelete = 5,
    ScanWorkflowActionJawModeChanged = 6,
    ScanWorkflowActionToolChanged = 7,
};

// 扫描阶段 UI 的公开接口。
//
// 本模块持有扫描页面和 QVTK/OpenGL 显示资源，但不直接实现设备通信、扫描算法、
// 病例保存或扫描流程规则。流程按钮只消费宿主传入的 scanProcess.steps。
class MEYERSCAN_SCANWORKFLOWUI_API IScanWorkflowUI {
public:
    // 接口对象由 DLL 内单例持有，调用方不得 delete；虚析构仅保证多态接口定义完整。
    virtual ~IScanWorkflowUI() = default;

    // 初始化应用目录和统一日志目录，参数是调用期间有效的 UTF-8 字符串。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 创建扫描阶段根 QWidget，但不代表页面已经进入活动状态。
    // 宿主挂载完成后应显式调用 Activate，离开前调用 DeactivateAndRelease。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 校验并保存轻量订单/会话 JSON。
    // 非法 JSON 返回 false 且保留上一份有效上下文；未知字段被忽略。
    virtual bool SetSessionContextJson(const char* contextJsonUtf8) = 0;

    // 注册纯 C 动作回调，避免跨 DLL 暴露 QObject、信号槽或 std::function ABI。
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;

    // 页面挂载并成为当前步骤后调用，用于恢复轻量显示状态。
    virtual void Activate() = 0;

    // 离开页面前调用，释放 QVTKWidget、renderer、OpenGL/显存等重资源。
    virtual void DeactivateAndRelease() = 0;

    // 返回供 versionList 记录的静态代码版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭模块并清理上下文、回调和控件弱引用，不跨 DLL 删除宿主根 QWidget。
    virtual void Shutdown() = 0;
};

// C ABI 工厂由 MainExe 或 ScanReconstructStudio.exe 通过 QLibrary::resolve 获取。
extern "C" MEYERSCAN_SCANWORKFLOWUI_API IScanWorkflowUI* GetScanWorkflowUI();
