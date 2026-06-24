#include "CalibrationColorUIImpl.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_CalibrationColorUI";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_CalibrationColorUI v0.1.0 (2026-06-24)";
}
}

// 返回颜色校准模块单例。
CalibrationColorUIImpl& CalibrationColorUIImpl::Instance() {
    static CalibrationColorUIImpl instance;
    return instance;
}

// 初始化颜色校准模块。
// 当前只准备路径和日志；真实算法/设备资源后续也应在这里按需初始化。
bool CalibrationColorUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 保存为 QByteArray，避免调用方传入临时 UTF-8 字节数组后本模块持有悬空指针。
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // 每个模块缓存一份日志接口指针，减少重复获取单例和空指针判断分散。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }
    WriteLog(LogLevel::Info, "Init", "CalibrationColorUI initialized");
    return true;
}

// 创建颜色校准主界面。
// 当前使用占位界面先保证模块可加载、可显示、可释放。
QWidget* CalibrationColorUIImpl::CreateWidget(QWidget* parent) {
    // root 挂到 parent 下，由调用方或 Qt 父子树负责销毁。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanCalibrationColorUIRoot");
    root->setMinimumSize(980, 620);

    // 使用 Qt Layout 而非固定坐标，后续接入色卡预览或结果面板时更容易维护。
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(16);

    // 源码 UI 文案使用英文并包 tr()，中文和其他语言由 qm 文件提供。
    auto* title = new QLabel(tr("Color Calibration"), root);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* intro = new QLabel(tr("Prepare the color target and follow the color capture sequence."), root);
    intro->setWordWrap(true);
    intro->setStyleSheet("QLabel{color:#52616f;}");
    layout->addWidget(intro);

    auto* content = new QFrame(root);
    content->setObjectName("CalibrationColorContent");
    content->setStyleSheet("QFrame#CalibrationColorContent{border:1px solid #cfd8dc;border-radius:4px;background:#ffffff;}");

    // 内容区后续用于放置色卡预览、采集步骤、计算状态和结果确认。
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 18, 18, 18);
    contentLayout->setSpacing(12);

    auto* step = new QLabel(tr("Color correction placeholder"), content);
    // 占位区固定最小高度，保证后续替换成色卡/预览画面时整体布局稳定。
    step->setAlignment(Qt::AlignCenter);
    step->setMinimumHeight(280);
    step->setStyleSheet("QLabel{border:1px dashed #b0bec5;color:#607080;background:#f8fafb;}");
    contentLayout->addWidget(step, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();

    // 当前按钮只作为流程占位，不连接真实算法；后续 Start/Cancel 应在本模块内部完成。
    auto* startButton = new QPushButton(tr("Start"), content);
    auto* cancelButton = new QPushButton(tr("Cancel"), content);
    startButton->setMinimumWidth(120);
    cancelButton->setMinimumWidth(120);
    buttonRow->addWidget(startButton);
    buttonRow->addWidget(cancelButton);
    contentLayout->addLayout(buttonRow);

    layout->addWidget(content, 1);

    // 保存弱引用便于 Shutdown 清空状态，不代表本模块拥有 root 的删除权。
    m_root = root;
    WriteLog(LogLevel::Info, "CreateWidget", "CalibrationColorUI widget created");
    return root;
}

// 返回模块版本字符串。
const char* CalibrationColorUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 关闭颜色校准模块。
// 不主动 delete m_root，避免和 Qt 父对象析构产生重复释放。
void CalibrationColorUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "CalibrationColorUI shutdown");
    if (m_logger) {
        // 退出前刷新日志，便于现场看到颜色校准模块最后状态。
        m_logger->Flush();
    }

    // root 通常已挂在上层页面容器下，不能在这里直接 delete，避免双重释放。
    m_root = nullptr;
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
}

// 写结构化日志。
// 日志失败不应影响校准界面创建，所以没有 logger 时静默返回。
void CalibrationColorUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        return;
    }

    // Logger 跨 DLL ABI 使用 UTF-8 const char*，Qt 字符串只在模块内部使用。
    const QByteArray bytes = content.toUtf8();
    // 颜色标定页当前没有真实操作员上下文，传空字符串让 Logger 省略 Op 字段。
    m_logger->Write(level, ModuleInfo::Name, operation, "", "", "", bytes.constData());
}

// 导出颜色校准 UI 模块实例。
extern "C" MEYERSCAN_CALIBRATIONCOLORUI_API ICalibrationColorUI* GetCalibrationColorUI() {
    return &CalibrationColorUIImpl::Instance();
}
