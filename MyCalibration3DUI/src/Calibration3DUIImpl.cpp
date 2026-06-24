#include "Calibration3DUIImpl.h"

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
    static Calibration3DUIImpl instance;
    return instance;
}

// 初始化三维校准模块。
// 当前骨架只缓存安装目录和日志目录；后续接入算法/设备库时也应放在此处完成。
bool Calibration3DUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 保存为 QByteArray，保证 const char* 生命周期不依赖调用方传入的临时对象。
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
    root->setObjectName("MeyerScanCalibration3DUIRoot");
    root->setMinimumSize(980, 620);

    // 全部使用 Qt Layout，不使用绝对坐标，后续真实控件增删时更容易维护。
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(16);

    // UI 源文案保持英文并使用 tr()，中文由 qm 文件提供。
    auto* title = new QLabel(tr("3D Calibration"), root);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* intro = new QLabel(tr("Prepare the calibration board and follow the capture sequence."), root);
    intro->setWordWrap(true);
    intro->setStyleSheet("QLabel{color:#52616f;}");
    layout->addWidget(intro);

    auto* content = new QFrame(root);
    content->setObjectName("Calibration3DContent");
    content->setStyleSheet("QFrame#Calibration3DContent{border:1px solid #cfd8dc;border-radius:4px;background:#ffffff;}");

    // 内容区先保留为一个稳定容器，后续真实采集预览、步骤提示和结果视图都放在这里扩展。
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 18, 18, 18);
    contentLayout->setSpacing(12);

    auto* step = new QLabel(tr("Capture sequence placeholder"), content);
    // 占位区固定最小高度，后续替换成相机预览/采集示意时页面不会突然变形。
    step->setAlignment(Qt::AlignCenter);
    step->setMinimumHeight(280);
    step->setStyleSheet("QLabel{border:1px dashed #b0bec5;color:#607080;background:#f8fafb;}");
    contentLayout->addWidget(step, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();

    // 当前按钮只占位，不连接算法流程；真实 Start/Cancel 后续应在本模块内部接入。
    auto* startButton = new QPushButton(tr("Start"), content);
    auto* cancelButton = new QPushButton(tr("Cancel"), content);
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
    m_root = nullptr;
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
}

// 写结构化日志。
// 没有日志模块或日志尚未初始化时直接返回，保证校准 UI 不因日志失败影响主流程。
void Calibration3DUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        return;
    }

    // Logger 公共 ABI 使用 UTF-8 const char*，这里在调用前转换。
    const QByteArray bytes = content.toUtf8();
    // 标定页当前没有真实操作员上下文，传空字符串让 Logger 省略 Op 字段。
    m_logger->Write(level, ModuleInfo::Name, operation, "", "", "", bytes.constData());
}

// 导出三维校准 UI 模块实例。
extern "C" MEYERSCAN_CALIBRATION3DUI_API ICalibration3DUI* GetCalibration3DUI() {
    return &Calibration3DUIImpl::Instance();
}
