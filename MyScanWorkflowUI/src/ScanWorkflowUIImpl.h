#pragma once

#include "ScanWorkflowUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QVector>

#include "Logger.h"

class QLabel;
class QPushButton;
class ScanWorkflowViewerWidget;
class vtkRenderer;

// 扫描阶段 UI 的内部实现。
// QWidget 由宿主父子树持有；本单例只缓存一组非 owning 控件指针和 VTK 引用计数对象。
class ScanWorkflowUIImpl : public IScanWorkflowUI {
    Q_DECLARE_TR_FUNCTIONS(ScanWorkflowUI)

public:
    // 返回 DLL 内唯一实例，防止多个扫描页同时争用单例设备/显示状态。
    static ScanWorkflowUIImpl& Instance();

    // 复制路径并缓存 Logger。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;
    // 创建页面结构和 QVTK 占位场景，激活由宿主单独调用。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;
    // 校验上下文并刷新流程按钮。
    bool SetSessionContextJson(const char* contextJsonUtf8) override;
    // 保存纯 C 动作回调。
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;
    // 刷新当前扫描部位的可见数据。
    void Activate() override;
    // 释放 VTK/OpenGL 和控件弱引用。
    void DeactivateAndRelease() override;
    // 返回代码版本。
    const char* GetModuleVersion() const override;
    // 完整清理模块状态。
    void Shutdown() override;

private:
    // 单例禁止外部构造、析构和复制，避免裸 VTK 指针被浅复制。
    ScanWorkflowUIImpl() = default;
    ~ScanWorkflowUIImpl() = default;
    ScanWorkflowUIImpl(const ScanWorkflowUIImpl&) = delete;
    ScanWorkflowUIImpl& operator=(const ScanWorkflowUIImpl&) = delete;

    // 从 scanProcess.steps 解析出的页面私有视图模型，不进入公共 ABI。
    struct ScanProcessStepInfo {
        // 部位类别，例如 maxilla/mandible/exchange/occlusion。
        QString part;
        // 稳定步骤编码，后续用于模型数据索引和日志。
        QString code;
        // 当前语言下的显示文字。
        QString label;
        // 是否为数据交换节点。
        bool exchange = false;
        // false 时按钮显示但禁止进入。
        bool enabled = true;
    };

    // 创建顶部扫描流程按钮宿主。
    QWidget* CreateScanModeBar(QWidget* parent);
    // 从当前上下文重新创建流程按钮。
    void RefreshScanProcessButtons();
    // 解析流程；缺失时返回练习模式默认四步。
    QVector<ScanProcessStepInfo> ResolveScanProcessSteps() const;
    // 切换当前扫描部位并上报动作。
    void SelectScanProcessStep(int index, const QString& reason);
    // 刷新按钮 checked/QSS 状态。
    void RefreshScanProcessButtonStates();
    // 更新提示文字和当前 VTK 占位数据。
    void UpdateDisplayedScanData();
    // 创建右侧扫描工具入口。
    QWidget* CreateRightToolBar(QWidget* parent);
    // 创建扫描页底部 Start/Pause 等控制。
    QWidget* CreateBottomControlBar(QWidget* parent);
    // 创建 Scan 专属左下提示区。
    QWidget* CreateHintPanel(QWidget* parent);
    // 创建 QVTKWidget 显示区域。
    QWidget* CreateViewerArea(QWidget* parent);
    // 初始化 renderer 与 render window 关系。
    void BuildPlaceholderScene();
    // 按当前步骤重建轻量占位 actor。
    void RebuildStepPlaceholderScene();
    // 写日志后上报稳定 actionId。
    void EmitAction(int actionId, const QString& operation);
    // 通过公共 Qt 适配层写 Logger。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    // 自有 UTF-8 缓冲区保证跨 DLL 参数调用结束后仍可使用。
    QByteArray m_appDir;
    QByteArray m_logDir;
    ILogger* m_logger = nullptr;

    // QWidget 指针均为非 owning；m_renderer 由 vtkRenderer::New/Delete 成对管理。
    QWidget* m_root = nullptr;
    ScanWorkflowViewerWidget* m_vtkWidget = nullptr;
    vtkRenderer* m_renderer = nullptr;
    QLabel* m_statusLabel = nullptr;
    QWidget* m_scanModeBar = nullptr;
    QVector<QPushButton*> m_scanProcessButtons;
    QVector<ScanProcessStepInfo> m_scanProcessSteps;
    int m_currentStepIndex = -1;
    QByteArray m_contextJson;
    // 宿主拥有 callback/context，Shutdown 后模块清空它们。
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionContext = nullptr;
};
