#pragma once

#include "DataProcessUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QVector>

#include "Logger.h"

class QLabel;
class QPushButton;
class DataProcessViewerWidget;
class vtkRenderer;

// 数据处理阶段 UI 的内部实现。
// QWidget 由宿主父子树持有，单例只缓存弱引用和当前 renderer 生命周期。
class DataProcessUIImpl : public IDataProcessUI {
    Q_DECLARE_TR_FUNCTIONS(DataProcessUI)

public:
    // 返回 DLL 内唯一实例。
    static DataProcessUIImpl& Instance();

    // 复制路径并缓存统一 Logger。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;
    // 创建页面结构和 QVTK 占位场景，激活由宿主显式调用。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;
    // 校验上下文并刷新与 Scan 页一致的流程按钮。
    bool SetSessionContextJson(const char* contextJsonUtf8) override;
    // 保存纯 C 动作回调。
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;
    // 刷新当前处理部位的可见状态。
    void Activate() override;
    // 释放 QVTK/OpenGL 重资源和控件弱引用。
    void DeactivateAndRelease() override;
    // 返回代码版本。
    const char* GetModuleVersion() const override;
    // 完整清理模块状态。
    void Shutdown() override;

private:
    // 单例禁止复制，防止 VTK 裸指针和 QWidget 弱引用被浅复制。
    DataProcessUIImpl() = default;
    ~DataProcessUIImpl() = default;
    DataProcessUIImpl(const DataProcessUIImpl&) = delete;
    DataProcessUIImpl& operator=(const DataProcessUIImpl&) = delete;

    // scanProcess.steps 在处理页中的私有视图模型，不跨 DLL 暴露。
    struct ProcessStepInfo {
        // 部位类别。
        QString part;
        // 稳定步骤编码。
        QString code;
        // 当前语言显示文字。
        QString label;
        // 是否为数据交换节点。
        bool exchange = false;
        // 是否允许用户进入该步骤。
        bool enabled = true;
    };

    // 创建顶部模型/流程按钮宿主。
    QWidget* CreateModelModeBar(QWidget* parent);
    // 从当前上下文重建流程按钮。
    void RefreshScanProcessButtons();
    // 解析流程；缺失时返回默认四步。
    QVector<ProcessStepInfo> ResolveScanProcessSteps() const;
    // 切换当前处理部位并上报动作。
    void SelectProcessStep(int index, const QString& reason);
    // 刷新按钮 checked/QSS 状态。
    void RefreshScanProcessButtonStates();
    // 更新 Process 专属提示和占位数据。
    void UpdateDisplayedProcessData();
    // 创建右侧处理工具入口，不执行真实算法。
    QWidget* CreateProcessingToolBar(QWidget* parent);
    // 创建 QVTKWidget 显示区域。
    QWidget* CreateViewerArea(QWidget* parent);
    // 创建底部状态/前后步骤栏；不包含扫描 Start/Pause。
    QWidget* CreateBottomStatusBar(QWidget* parent);
    // 创建 Process 专属左下提示区。
    QWidget* CreateHintPanel(QWidget* parent);
    // 初始化 renderer 与 render window 关系。
    void BuildPlaceholderScene();
    // 按当前步骤重建轻量占位 actor。
    void RebuildStepPlaceholderScene();
    // 写日志后上报稳定 actionId。
    void EmitAction(int actionId, const QString& operation);
    // 通过公共 Qt 适配层写 Logger。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    // 自有 UTF-8 缓冲区和缓存日志入口。
    QByteArray m_appDir;
    QByteArray m_logDir;
    ILogger* m_logger = nullptr;

    // QWidget 指针均不拥有对象；m_renderer 由 New/Delete 成对管理。
    QWidget* m_root = nullptr;
    DataProcessViewerWidget* m_vtkWidget = nullptr;
    vtkRenderer* m_renderer = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QWidget* m_modelModeBar = nullptr;
    QVector<QPushButton*> m_processButtons;
    QVector<ProcessStepInfo> m_processSteps;
    int m_currentStepIndex = -1;
    QByteArray m_contextJson;
    // 宿主拥有回调及上下文，Shutdown 后清空。
    void (*m_actionCallback)(void* context, int actionId) = nullptr;
    void* m_actionContext = nullptr;
};
