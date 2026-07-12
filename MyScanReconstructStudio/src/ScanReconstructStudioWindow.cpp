#include "ScanReconstructStudioWindow.h"
#include "MeyerQtModuleUtils.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 该名称写入结构化日志的模块字段。
const char* Name = "ScanReconstructStudio";

// GetMeyerModuleVersion() 返回的代码版本，必须与 Version.rc 保持同步。
const char* Version = "ScanReconstructStudio v0.1.3 (2026-07-12)";
}

// 从子 DLL 解析出来的 C ABI 工厂函数类型。
typedef IScanWorkflowUI* (*GetScanWorkflowUIFunc)();
typedef IDataProcessUI* (*GetDataProcessUIFunc)();
}

// 构造扫描重建壳窗口。
ScanReconstructStudioWindow::ScanReconstructStudioWindow(const QString& appDir,
                                                         const QString& logDir,
                                                         const QByteArray& contextJson,
                                                         QWidget* parent)
    : QMainWindow(parent),
      m_appDir(appDir),
      m_logDir(logDir),
      m_contextJson(contextJson),
      m_scanLibrary(QDir(appDir).filePath("MeyerScan_ScanWorkflowUI.dll")),
      m_processLibrary(QDir(appDir).filePath("MeyerScan_DataProcessUI.dll")) {
    // 可见文本使用英文源文案，后续通过 qm 翻译成中文/英文等语言。
    setWindowTitle(tr("Scan Reconstruct Studio"));
    // 独立 EXE 和嵌入 DLL 都使用无边框窗口，避免出现 Qt 原生标题栏。
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setMinimumSize(1280, 760);

    // 扫描/处理页面内部后续会持有 VTK/OpenGL 资源。
    // 退出阶段不主动卸载 DLL，可以降低 Qt 对象、OpenGL 上下文、DLL 静态对象析构顺序互相踩踏的风险。
    m_scanLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_processLibrary.setLoadHints(QLibrary::PreventUnloadHint);
}

// 析构前释放当前活动页面资源。
ScanReconstructStudioWindow::~ScanReconstructStudioWindow() {
    // 顺序很重要：先让子模块释放 VTK/OpenGL，再清模块指针。
    ReleaseCurrentStepResources();
    // 析构阶段不会再返回主事件循环，主动处理已经安全排队的 DeferredDelete，
    // 确保 QVTK 原生窗口在子模块 Shutdown 和进程退出前完成销毁。
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    UnloadModules();
}

// 初始化日志、子模块和默认页面。
bool ScanReconstructStudioWindow::Initialize() {
    // 日志必须最早初始化，这样后续 DLL 加载失败也能落盘。
    m_logger = GetLogger();
    if (m_logger) {
        const QByteArray logDirBytes = m_logDir.toUtf8();
        if (!m_logger->Init(logDirBytes.constData(), LogLevel::Info)) {
            // 壳子允许无日志降级，但不保留半初始化接口。
            m_logger = nullptr;
        }
    }
    WriteLog(LogLevel::Info, "Initialize", "ScanReconstructStudio initializing");

    // 先加载两个子 DLL，页面 widget 仍在切换时按需创建。
    if (!LoadScanModule()) {
        return false;
    }
    if (!LoadDataProcessModule()) {
        return false;
    }

    // 创建壳子 UI，并进入扫描阶段；页面创建失败必须向上传播，不能显示空壳后仍返回成功。
    BuildShellUi();
    if (!SwitchToStep(StepScan)) {
        WriteLog(LogLevel::Error, "Initialize", "Initial scan page create failed");
        return false;
    }
    WriteLog(LogLevel::Info, "Initialize", "ScanReconstructStudio initialized");
    return true;
}

// 执行无交互烟测路径。
bool ScanReconstructStudioWindow::RunSmoke() {
    // 复用真实初始化路径，确保动态加载和页面创建都被覆盖。
    if (!Initialize()) {
        return false;
    }

    // 前后切换一次，验证页面释放和重建链路。
    if (!SwitchToStep(StepDataProcess)) {
        return false;
    }
    if (!m_pages.contains(StepDataProcess)) {
        WriteLog(LogLevel::Error, "RunSmoke", "Data process page was not created");
        return false;
    }
    // 切换函数完整返回后再处理旧页面的 deleteLater，避免在释放函数内部重入 QWidget 析构。
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    if (!SwitchToStep(StepScan)) {
        return false;
    }
    if (!m_pages.contains(StepScan)) {
        WriteLog(LogLevel::Error, "RunSmoke", "Scan page was not recreated");
        return false;
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    WriteLog(LogLevel::Info, "RunSmoke", "ScanReconstructStudio smoke passed");
    return true;
}

// 创建壳子界面框架。
void ScanReconstructStudioWindow::BuildShellUi() {
    auto* central = new QWidget(this);
    central->setObjectName("ScanReconstructStudioRoot");
    MeyerQtModule::ApplyModuleQss(central,
                                  "MyScanReconstructStudio",
                                  "scan_reconstruct_studio.qss",
                                  m_logger);

    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 顶部栏属于壳子，因为扫描和数据处理两个页面共享同一套流程外框。
    auto* topBar = new QFrame(central);
    topBar->setProperty("bar", true);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(18, 12, 18, 12);
    topLayout->setSpacing(20);
    topLayout->addWidget(CreateTopStepBar(topBar), 1);
    topLayout->addWidget(CreateWindowToolBar(topBar), 0);
    layout->addWidget(topBar, 0);

    // stack 只保留当前重页面；切换时旧页面会移除并释放。
    m_stack = new QStackedWidget(central);
    m_stack->setObjectName("ScanReconstructStudioStack");
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);
}

// 创建顶部阶段导航行。
QWidget* ScanReconstructStudioWindow::CreateTopStepBar(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* backButton = new QPushButton(tr("Back"), widget);
    auto* scanButton = new QPushButton(tr("Scan"), widget);
    auto* processButton = new QPushButton(tr("Data Processing"), widget);
    m_stepLabel = new QLabel(tr("Scan"), widget);

    QObject::connect(scanButton, &QPushButton::clicked, [this]() {
        // SwitchToStep 内部记录失败原因；按钮回调不在 UI 层弹业务错误框。
        SwitchToStep(StepScan);
    });
    QObject::connect(processButton, &QPushButton::clicked, [this]() {
        SwitchToStep(StepDataProcess);
    });
    QObject::connect(backButton, &QPushButton::clicked, [this]() {
        close();
    });

    layout->addWidget(backButton, 0);
    layout->addStretch(1);
    layout->addWidget(scanButton, 0);
    layout->addWidget(processButton, 0);
    layout->addStretch(1);
    layout->addWidget(m_stepLabel, 0);
    return widget;
}

// 创建右上角壳子工具按钮行。
QWidget* ScanReconstructStudioWindow::CreateWindowToolBar(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    const QStringList tools = {
        tr("Upload"),
        tr("Crop"),
        tr("Settings"),
        tr("Folder"),
        tr("Home"),
    };

    for (const QString& text : tools) {
        auto* button = new QPushButton(text, widget);
        button->setMinimumWidth(68);
        QObject::connect(button, &QPushButton::clicked, [this, text]() {
            WriteLog(LogLevel::Info, "TopToolClicked", QString("Tool clicked: %1").arg(text));
        });
        layout->addWidget(button);
    }
    return widget;
}

// 切换扫描/数据处理阶段，并释放上一个阶段的重资源。
bool ScanReconstructStudioWindow::SwitchToStep(StudioStep step) {
    // 请求的页面已经处于活动状态时，只刷新标题，不重复创建页面。
    if (m_currentStep == step && m_pages.contains(step)) {
        if (m_stepLabel) {
            m_stepLabel->setText(step == StepScan ? tr("Scan") : tr("Data Processing"));
        }
        return true;
    }

    // 创建新页面前先释放上一个 QVTK/OpenGL 页面，避免显存累积。
    ReleaseCurrentStepResources();
    m_currentStep = step;

    QWidget* page = nullptr;
    if (step == StepScan) {
        page = CreateScanPage();
    } else {
        page = CreateDataProcessPage();
    }

    if (!page || !m_stack) {
        WriteLog(LogLevel::Error, "SwitchToStep", "Failed to create step page");
        return false;
    }

    // ReleaseCurrentStepResources 后 stack 只保留当前重页面，避免 Scan/Process 同时占用显存。
    m_stack->addWidget(page);
    m_stack->setCurrentWidget(page);
    m_pages.insert(step, page);

    if (m_stepLabel) {
        m_stepLabel->setText(step == StepScan ? tr("Scan") : tr("Data Processing"));
    }

    WriteLog(LogLevel::Info, "SwitchToStep",
             QString("Current step: %1").arg(step == StepScan ? "scan" : "data-process"));
    return true;
}

// 动态加载扫描 UI DLL。
bool ScanReconstructStudioWindow::LoadScanModule() {
    if (!m_scanLibrary.load()) {
        WriteLog(LogLevel::Error,
                 "LoadScanModule",
                 QString("Load scan workflow DLL failed: %1").arg(m_scanLibrary.errorString()));
        return false;
    }
    WriteLog(LogLevel::Info, "LoadScanModule", "Scan workflow DLL loaded");

    GetScanWorkflowUIFunc getter =
        reinterpret_cast<GetScanWorkflowUIFunc>(m_scanLibrary.resolve("GetScanWorkflowUI"));
    if (!getter) {
        WriteLog(LogLevel::Error, "LoadScanModule", "GetScanWorkflowUI not found");
        return false;
    }

    m_scanModule = getter();
    if (!m_scanModule) {
        WriteLog(LogLevel::Error, "LoadScanModule", "Scan module instance is null");
        return false;
    }

    // QByteArray 局部变量保证两个 constData 指针在整个跨 DLL 调用期间有效。
    const QByteArray appDirBytes = m_appDir.toUtf8();
    const QByteArray logDirBytes = m_logDir.toUtf8();
    const bool initOk = m_scanModule->Init(appDirBytes.constData(), logDirBytes.constData());
    if (!initOk) {
        WriteLog(LogLevel::Error, "LoadScanModule", "Scan module Init returned false");
        return false;
    }
    if (!m_scanModule->SetSessionContextJson(m_contextJson.constData())) {
        WriteLog(LogLevel::Error, "LoadScanModule", "Scan module rejected session context");
        return false;
    }
    m_scanModule->SetActionCallback(&ScanReconstructStudioWindow::OnChildAction, this);
    WriteLog(LogLevel::Info, "LoadScanModule", m_scanModule->GetModuleVersion());
    return true;
}

// 动态加载数据处理 UI DLL。
bool ScanReconstructStudioWindow::LoadDataProcessModule() {
    if (!m_processLibrary.load()) {
        WriteLog(LogLevel::Error,
                 "LoadDataProcessModule",
                 QString("Load data process DLL failed: %1").arg(m_processLibrary.errorString()));
        return false;
    }
    WriteLog(LogLevel::Info, "LoadDataProcessModule", "Data process DLL loaded");

    GetDataProcessUIFunc getter =
        reinterpret_cast<GetDataProcessUIFunc>(m_processLibrary.resolve("GetDataProcessUI"));
    if (!getter) {
        WriteLog(LogLevel::Error, "LoadDataProcessModule", "GetDataProcessUI not found");
        return false;
    }

    m_processModule = getter();
    if (!m_processModule) {
        WriteLog(LogLevel::Error, "LoadDataProcessModule", "Data process module instance is null");
        return false;
    }

    // 显式 UTF-8 缓冲区避免把临时 toUtf8().constData() 指针误保存到子模块。
    const QByteArray appDirBytes = m_appDir.toUtf8();
    const QByteArray logDirBytes = m_logDir.toUtf8();
    const bool initOk = m_processModule->Init(appDirBytes.constData(), logDirBytes.constData());
    if (!initOk) {
        WriteLog(LogLevel::Error, "LoadDataProcessModule", "Data process module Init returned false");
        return false;
    }
    if (!m_processModule->SetSessionContextJson(m_contextJson.constData())) {
        WriteLog(LogLevel::Error, "LoadDataProcessModule", "Data process module rejected session context");
        return false;
    }
    m_processModule->SetActionCallback(&ScanReconstructStudioWindow::OnChildAction, this);
    WriteLog(LogLevel::Info, "LoadDataProcessModule", m_processModule->GetModuleVersion());
    return true;
}

// 通过子 DLL 创建扫描页面。
QWidget* ScanReconstructStudioWindow::CreateScanPage() {
    if (!m_scanModule) {
        WriteLog(LogLevel::Error, "CreateScanPage", "Scan module is null");
        return nullptr;
    }
    QWidget* page = m_scanModule->CreateWidget(m_stack);
    if (!page) {
        WriteLog(LogLevel::Error, "CreateScanPage", "Scan widget create returned null");
        return nullptr;
    }
    m_scanModule->Activate();
    return page;
}

// 通过子 DLL 创建数据处理页面。
QWidget* ScanReconstructStudioWindow::CreateDataProcessPage() {
    if (!m_processModule) {
        WriteLog(LogLevel::Error, "CreateDataProcessPage", "Data process module is null");
        return nullptr;
    }
    QWidget* page = m_processModule->CreateWidget(m_stack);
    if (!page) {
        WriteLog(LogLevel::Error, "CreateDataProcessPage", "Data process widget create returned null");
        return nullptr;
    }
    m_processModule->Activate();
    return page;
}

// 释放当前活动页面及其重资源。
void ScanReconstructStudioWindow::ReleaseCurrentStepResources() {
    QWidget* page = m_pages.value(m_currentStep, nullptr);
    if (!page) {
        return;
    }

    WriteLog(LogLevel::Info,
             "ReleaseCurrentStepResources",
             QString("Release step resources: %1").arg(m_currentStep));

    // 删除 widget 前先通知子模块释放 QVTK/OpenGL。
    if (m_currentStep == StepScan && m_scanModule) {
        m_scanModule->DeactivateAndRelease();
    }
    if (m_currentStep == StepDataProcess && m_processModule) {
        m_processModule->DeactivateAndRelease();
    }

    if (m_stack) {
        m_stack->removeWidget(page);
    }
    m_pages.remove(m_currentStep);
    page->deleteLater();
}

// 关闭子模块并清理 DLL 句柄。
void ScanReconstructStudioWindow::UnloadModules() {
    if (m_scanModule) {
        m_scanModule->Shutdown();
        m_scanModule = nullptr;
    }
    if (m_processModule) {
        m_processModule->Shutdown();
        m_processModule = nullptr;
    }

    // 当前扫描/处理页面包含 QVTK/OpenGL 等重资源。
    // 为降低退出阶段 DLL 卸载顺序造成崩溃的风险，先不主动 unload 这些 DLL，
    // 让操作系统在进程退出时统一回收。后续若要做插件热卸载，需要单独设计资源屏障。

    if (m_logger) {
        m_logger->Flush();
        m_logger = nullptr;
    }
}

// 传给子模块的静态回调入口。
void ScanReconstructStudioWindow::OnChildAction(void* context, int actionId) {
    auto* self = static_cast<ScanReconstructStudioWindow*>(context);
    if (self) {
        self->HandleChildAction(actionId);
    }
}

// 处理子模块动作。
void ScanReconstructStudioWindow::HandleChildAction(int actionId) {
    WriteLog(LogLevel::Info, "HandleChildAction",
             QString("Child action: %1, current step: %2").arg(actionId).arg(m_currentStep));

    // 初版只接通阶段流转：扫描完成进入数据处理。
    if (m_currentStep == StepScan &&
        (actionId == ScanWorkflowActionComplete || actionId == ScanWorkflowActionNext)) {
        // SwitchToStep 会记录目标页创建失败，壳子继续保留可诊断状态。
        SwitchToStep(StepDataProcess);
        return;
    }

    // 数据处理可通过稳定 actionId 回到扫描。
    if (m_currentStep == StepDataProcess && actionId == DataProcessActionPrevious) {
        SwitchToStep(StepScan);
        return;
    }
}

// 写结构化日志。
void ScanReconstructStudioWindow::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // ScanReconstructStudio 既可编译为 EXE，也可编译为 DLL。
    // 统一走公共 Qt 日志工具后，两种形态都会用 MEYER_MODULE_NAME 自动写入模块名。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// 运行时版本清单读取使用的统一版本导出函数。
#ifndef MEYERSCAN_SCANRECONSTRUCTSTUDIO_NO_WINDOW_VERSION_EXPORT
extern "C" __declspec(dllexport) const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
#endif
