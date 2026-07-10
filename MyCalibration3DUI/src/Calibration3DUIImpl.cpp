#include "Calibration3DUIImpl.h"

#include "MeyerQtModuleUtils.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_Calibration3DUI";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_Calibration3DUI v0.1.0 (2026-06-24)";
}
}

// 返回三维校准模块单例。
// C++11 保证局部静态变量初始化线程安全。
Calibration3DUIImpl& Calibration3DUIImpl::Instance() {
    // 校准模块内部会缓存日志和根页面弱引用，使用单例保证同进程只维护一份状态。
    static Calibration3DUIImpl instance;
    return instance;
}

// 初始化三维校准模块。
// 当前骨架只缓存安装目录和日志目录；后续接入算法/设备库时也应放在此处完成。
bool Calibration3DUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 保存为 QByteArray，保证 const char* 生命周期不依赖调用方传入的临时对象。
    // 这在 MainExe 使用 QDir::fromNativeSeparators(...).toUtf8().constData() 调用时尤其重要。
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // 缓存日志接口，降低后续调用复杂度，也符合“每个模块一份日志变量”的约定。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }
    WriteLog(LogLevel::Info, "Init", "Calibration3DUI initialized");
    return true;
}

// 创建三维校准主界面。
// 当前是占位实现，用于先跑通模块加载、界面嵌入、日志输出和资源释放流程。
QWidget* Calibration3DUIImpl::CreateWidget(QWidget* parent) {
    // root 挂到 parent 下，由调用方页面容器管理销毁。
    auto* root = new QWidget(parent);
    // objectName 用于样式表和自动化测试定位，避免只能靠控件类型查找。
    root->setObjectName("MeyerScanCalibration3DUIRoot");
    // 最小尺寸先保护未来采集预览区域，不让窗口过小导致控件挤压。
    root->setMinimumSize(980, 620);
    // 校准界面的视觉样式从 qss 文件加载，源码只保留结构和 objectName。
    MeyerQtModule::ApplyModuleQss(root, "MyCalibration3DUI", "calibration_3d.qss", m_logger);

    // 全部使用 Qt Layout，不使用绝对坐标，后续真实控件增删时更容易维护。
    auto* layout = new QVBoxLayout(root);
    // 边距和间距由本模块统一控制，避免子控件各自写固定位置。
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(16);

    // UI 源文案保持英文并使用 tr()，中文由 qm 文件提供。
    auto* title = new QLabel(tr("3D Calibration"), root);
    QFont titleFont = title->font();
    // 复用系统字体，只调整字号和粗细，避免多语言字体回退出问题。
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* intro = new QLabel(tr("Prepare the calibration board and follow the capture sequence."), root);
    // WordWrap 允许长英文或其它语言翻译自动换行，不需要写语言 if/else 调宽度。
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* content = new QFrame(root);
    // QFrame 用作内容容器，后续可以把采集预览、步骤状态和结果页都放进这里。
    content->setObjectName("Calibration3DContent");

    // 内容区先保留为一个稳定容器，后续真实采集预览、步骤提示和结果视图都放在这里扩展。
    auto* contentLayout = new QVBoxLayout(content);
    // contentLayout 的 parent 是 content，Qt 会自动把 layout 绑定到该 frame。
    contentLayout->setContentsMargins(18, 18, 18, 18);
    contentLayout->setSpacing(12);

    auto* step = new QLabel(tr("Capture sequence placeholder"), content);
    step->setObjectName("Calibration3DStepPlaceholder");
    // 占位区固定最小高度，后续替换成相机预览/采集示意时页面不会突然变形。
    step->setAlignment(Qt::AlignCenter);
    step->setMinimumHeight(280);
    contentLayout->addWidget(step, 1);

    auto* buttonRow = new QHBoxLayout();
    // addStretch 把按钮推到右侧，符合设置/向导页常见操作区布局。
    buttonRow->addStretch();

    // 当前按钮只占位，不连接算法流程；真实 Start/Cancel 后续应在本模块内部接入。
    auto* startButton = new QPushButton(tr("Start"), content);
    auto* cancelButton = new QPushButton(tr("Cancel"), content);
    // 宽度只设最小值，给多语言翻译留出自然扩展空间。
    startButton->setMinimumWidth(120);
    cancelButton->setMinimumWidth(120);
    buttonRow->addWidget(startButton);
    buttonRow->addWidget(cancelButton);
    contentLayout->addLayout(buttonRow);

    layout->addWidget(content, 1);

    // 保存弱引用，便于 Shutdown 清空状态；不代表本模块拥有 root 的删除权。
    m_root = root;
    WriteLog(LogLevel::Info, "CreateWidget", "Calibration3DUI widget created");
    return root;
}

// 返回模块版本字符串。
const char* Calibration3DUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 关闭三维校准模块。
// Shutdown 不主动 delete m_root，因为该 QWidget 通常已经挂到 MainExe 或壳子模块的父对象树中。
void Calibration3DUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Calibration3DUI shutdown");
    if (m_logger) {
        // 退出前尽力刷新日志，便于看到校准模块最后一次状态。
        m_logger->Flush();
    }

    // 只清空弱引用和缓存路径。root 的真实释放由 Qt 父子关系或调用方 deleteLater 负责。
    // 未来接入算法/设备句柄时，应在这里释放本模块明确拥有的资源。
    m_root = nullptr;
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
}

// 写结构化日志。
// 没有日志模块或日志尚未初始化时直接返回，保证校准 UI 不因日志失败影响主流程。
void Calibration3DUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // MeyerQtModule::WriteQtLog 会自动补充 MEYER_MODULE_NAME，
    // 并把 QString 在跨 DLL 前转换成 UTF-8 const char*。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// 导出三维校准 UI 模块实例。
extern "C" MEYERSCAN_CALIBRATION3DUI_API ICalibration3DUI* GetCalibration3DUI() {
    // 对外只暴露接口指针，后续内部页面和算法接入不会影响调用方头文件。
    return &Calibration3DUIImpl::Instance();
}

// 统一版本导出函数。
// 版本清单读取三维校准 UI 代码版本时，不加载校准页面、算法 DLL 或设备资源。
extern "C" MEYERSCAN_CALIBRATION3DUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
