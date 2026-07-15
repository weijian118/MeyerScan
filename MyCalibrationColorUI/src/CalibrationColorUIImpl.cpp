#include "CalibrationColorUIImpl.h"

#include "MeyerQtModuleUtils.h"

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
const char* Version = "MeyerScan_CalibrationColorUI v0.1.1 (2026-07-15)";
}
}

// 返回颜色校准模块单例。
CalibrationColorUIImpl& CalibrationColorUIImpl::Instance() {
    // 颜色校准模块保存日志和页面弱引用，单例能保证 MainExe 多次获取时状态一致。
    static CalibrationColorUIImpl instance;
    return instance;
}

// 初始化颜色校准模块。
// 当前只准备路径和日志；真实算法/设备资源后续也应在这里按需初始化。
bool CalibrationColorUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 保存为 QByteArray，避免调用方传入临时 UTF-8 字节数组后本模块持有悬空指针。
    // 这是跨 DLL const char* 参数最常见的安全处理方式：入口处立即复制。
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
    // objectName 方便样式表和自动化测试准确定位颜色校准根节点。
    root->setObjectName("MeyerScanCalibrationColorUIRoot");
    // 预留给后续色卡预览/采集步骤/结果区域的最小尺寸。
    root->setMinimumSize(980, 620);
    // 颜色校准界面的视觉样式从 qss 文件加载，源码只保留结构和 objectName。
    MeyerQtModule::ApplyModuleQss(root, "MyCalibrationColorUI", "calibration_color.qss", m_logger);

    // 使用 Qt Layout 而非固定坐标，后续接入色卡预览或结果面板时更容易维护。
    auto* layout = new QVBoxLayout(root);
    // 边距/间距集中定义，避免后续每个子控件都写绝对位置。
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(16);

    // 源码 UI 文案使用英文并包 tr()，中文和其他语言由 qm 文件提供。
    auto* title = new QLabel(tr("Color Calibration"), root);
    QFont titleFont = title->font();
    // 保留系统字体族，只调字号和粗细，对中英文混排更稳。
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* intro = new QLabel(tr("Prepare the color target and follow the color capture sequence."), root);
    // 长翻译自动换行，避免按语言 if/else 改控件宽度。
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* content = new QFrame(root);
    // QFrame 作为内容容器，未来色卡预览和结果确认都在这个区域内扩展。
    content->setObjectName("CalibrationColorContent");

    // 内容区后续用于放置色卡预览、采集步骤、计算状态和结果确认。
    auto* contentLayout = new QVBoxLayout(content);
    // 给内容区内部留白，避免占位/预览控件贴边。
    contentLayout->setContentsMargins(18, 18, 18, 18);
    contentLayout->setSpacing(12);

    auto* step = new QLabel(tr("Color correction placeholder"), content);
    step->setObjectName("CalibrationColorStepPlaceholder");
    // 占位区固定最小高度，保证后续替换成色卡/预览画面时整体布局稳定。
    // setAlignment 让占位文字保持居中，后续替换成预览控件时可删除该 QLabel。
    step->setAlignment(Qt::AlignCenter);
    step->setMinimumHeight(280);
    contentLayout->addWidget(step, 1);

    auto* buttonRow = new QHBoxLayout();
    // stretch 占据左侧剩余空间，使 Start/Cancel 靠右排列。
    buttonRow->addStretch();

    // 当前按钮只作为流程占位，不连接真实算法；后续 Start/Cancel 应在本模块内部完成。
    auto* startButton = new QPushButton(tr("Start"), content);
    auto* cancelButton = new QPushButton(tr("Cancel"), content);
    // 只设置最小宽度，多语言文本更长时按钮可自然变宽。
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
    // 未来如果本模块自己打开算法句柄或设备句柄，应在这里释放那些明确属于本模块的资源。
    m_root = nullptr;
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
}

// 写结构化日志。
// 日志失败不应影响校准界面创建，所以没有 logger 时静默返回。
void CalibrationColorUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // MeyerQtModule::WriteQtLog 会自动补充 MEYER_MODULE_NAME，
    // 并把 QString 在跨 DLL 前转换成 UTF-8 const char*。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// 导出颜色校准 UI 模块实例。
extern "C" MEYERSCAN_CALIBRATIONCOLORUI_API ICalibrationColorUI* GetCalibrationColorUI() {
    // C ABI 工厂函数用于稳定动态加载，内部实现类不暴露给调用方。
    return &CalibrationColorUIImpl::Instance();
}

// 统一版本导出函数。
// 版本清单读取颜色校准 UI 代码版本时，不加载校准页面、算法 DLL 或设备资源。
extern "C" MEYERSCAN_CALIBRATIONCOLORUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}

// 返回颜色校准界面公共接口 ABI 版本。
extern "C" __declspec(dllexport) int GetMeyerModuleApiVersion() {
    return 1;
}
