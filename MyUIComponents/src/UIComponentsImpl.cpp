#include "UIComponentsImpl.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopWidget>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志和工程识别，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_UIComponents";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_UIComponents v0.1.0 (2026-06-23)";
}

const double kDesignWidth = 1920.0;
const double kDesignHeight = 1080.0;
}

// 返回共享 UI 组件单例。
// 当前模块是无业务状态的控件工厂，单例可避免重复计算全局屏幕缩放。
UIComponentsImpl& UIComponentsImpl::Instance() {
    static UIComponentsImpl instance;
    return instance;
}

// 初始化 UI 辅助缩放系数。
// 主布局仍应优先使用 Qt Layout；这里的缩放只用于按钮高度、边距、进度条等辅助尺寸。
bool UIComponentsImpl::Init(const char* /*appDirUtf8*/) {
    // availableGeometry() 取当前主屏可用区域，不包含任务栏。
    // 这里不是做绝对坐标缩放，只是给控件高度、边距等辅助尺寸一个温和比例。
    const QRect geometry = QApplication::desktop()->availableGeometry();

    // 限制在 0.75~2.0，避免小屏过小不可点，也避免 4K 屏控件无限放大。
    m_scaleX = qMax(0.75, qMin(2.0, geometry.width() / kDesignWidth));
    m_scaleY = qMax(0.75, qMin(2.0, geometry.height() / kDesignHeight));
    return true;
}

// 返回横向辅助缩放系数。
double UIComponentsImpl::ScaleX() const {
    return m_scaleX;
}

// 返回纵向辅助缩放系数。
double UIComponentsImpl::ScaleY() const {
    return m_scaleY;
}

// 创建等待页。
// 等待页只展示状态，不执行数据库检查、登录或版本清单生成等业务动作。
QWidget* UIComponentsImpl::CreateWaitWidget(const char* titleUtf8, const char* messageUtf8, QWidget* parent) {
    // parent 由 MainExe 的页面容器传入，Qt 父子关系会负责销毁等待页。
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanWaitWidget");

    // 等待页使用布局而非固定坐标，适配多语言和不同分辨率。
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(36, 36, 36, 36);
    layout->setSpacing(14);

    // title/message 都由调用方决定，UIComponents 不写业务文案。
    auto* title = CreatePageTitle(titleUtf8, root);
    auto* message = new QLabel(QString::fromUtf8(messageUtf8 ? messageUtf8 : ""), root);
    message->setAlignment(Qt::AlignCenter);
    message->setStyleSheet("color:#607080;font-size:15px;");

    // 不确定总进度时使用忙碌进度条：range(0,0) 是 Qt 的无限进度模式。
    auto* progress = new QProgressBar(root);
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->setFixedHeight(static_cast<int>(qMax(6.0, 8.0 * m_scaleY)));

    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(message);
    layout->addWidget(progress);
    layout->addStretch();
    return root;
}

// 创建主操作按钮。
// 调用方负责传入已经经过 tr() 的文字，并负责连接 clicked 信号。
QPushButton* UIComponentsImpl::CreatePrimaryButton(const char* textUtf8, QWidget* parent) {
    // textUtf8 应该已经是调用方 tr("English source text") 后的结果。
    auto* button = new QPushButton(QString::fromUtf8(textUtf8 ? textUtf8 : ""), parent);

    // 高度使用 ScaleY 辅助，宽度交给布局和 sizePolicy 自适应多语言文本。
    button->setMinimumHeight(static_cast<int>(42 * m_scaleY));
    button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    // 当前骨架先用 QSS 固化基础风格；后续可集中改为统一 qss 文件。
    button->setStyleSheet("QPushButton{background:#007d68;color:white;border:0;border-radius:4px;padding:8px 18px;}"
                          "QPushButton:hover{background:#009176;}"
                          "QPushButton:disabled{background:#d8d8d8;color:#888;}");
    return button;
}

// 创建次操作按钮。
// 仅统一外观和基本尺寸，不内置任何业务行为。
QPushButton* UIComponentsImpl::CreateSecondaryButton(const char* textUtf8, QWidget* parent) {
    auto* button = new QPushButton(QString::fromUtf8(textUtf8 ? textUtf8 : ""), parent);

    // 次按钮略低于主按钮，但仍保持足够点击高度，适合触控和高 DPI。
    button->setMinimumHeight(static_cast<int>(40 * m_scaleY));
    button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    button->setStyleSheet("QPushButton{background:#f6f8fa;color:#23313f;border:1px solid #cfd8dc;border-radius:4px;padding:8px 16px;}"
                          "QPushButton:hover{background:#edf2f5;}"
                          "QPushButton:disabled{color:#999;border-color:#ddd;}");
    return button;
}

// 创建通用输入框。
// placeholder 由调用方翻译后传入，避免 UIComponents 持有业务文案。
QLineEdit* UIComponentsImpl::CreateLineEdit(const char* placeholderUtf8, QWidget* parent) {
    auto* edit = new QLineEdit(parent);

    // placeholder 只作为输入提示；正式错误提示应由业务模块在旁边/弹窗中展示。
    edit->setPlaceholderText(QString::fromUtf8(placeholderUtf8 ? placeholderUtf8 : ""));
    edit->setMinimumHeight(static_cast<int>(36 * m_scaleY));
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    edit->setStyleSheet("QLineEdit{border:1px solid #cfd8dc;border-radius:4px;padding:6px 10px;background:white;color:#23313f;}"
                        "QLineEdit:focus{border-color:#007d68;}");
    return edit;
}

// 创建通用下拉框。
// 下拉项由调用方添加，UIComponents 不知道业务枚举含义。
QComboBox* UIComponentsImpl::CreateComboBox(QWidget* parent) {
    auto* combo = new QComboBox(parent);

    // 下拉框宽度设置为 MinimumExpanding，让翻译较长的选项有空间撑开。
    combo->setMinimumHeight(static_cast<int>(36 * m_scaleY));
    combo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    combo->setStyleSheet("QComboBox{border:1px solid #cfd8dc;border-radius:4px;padding:5px 10px;background:white;color:#23313f;}"
                         "QComboBox:focus{border-color:#007d68;}");
    return combo;
}

// 创建页面标题。
// 标题默认居中和自动换行，适配多语言变长文本。
QLabel* UIComponentsImpl::CreatePageTitle(const char* textUtf8, QWidget* parent) {
    auto* label = new QLabel(QString::fromUtf8(textUtf8 ? textUtf8 : ""), parent);

    // 标题允许换行，防止英文/德文等长翻译在窄屏上被截断。
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 字体大小暂时固定 pointSize，不跟 viewport 线性缩放，避免极端分辨率下文字比例失控。
    QFont font = label->font();
    font.setPointSize(20);
    font.setBold(true);
    label->setFont(font);
    label->setStyleSheet("color:#23313f;");
    return label;
}

// 返回模块版本字符串。
const char* UIComponentsImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 重置缩放状态。
// QWidget 生命周期由调用方管理，本模块不保存已创建控件指针。
void UIComponentsImpl::Shutdown() {
    // 恢复默认比例，便于测试宿主重复 Init/Shutdown 时结果可预测。
    m_scaleX = 1.0;
    m_scaleY = 1.0;
}

// C ABI 导出函数。
// MainExe 和各 UI 测试宿主通过该函数获取共享控件工厂。
extern "C" MEYERSCAN_UICOMPONENTS_API IUIComponents* GetUIComponents() {
    return &UIComponentsImpl::Instance();
}
