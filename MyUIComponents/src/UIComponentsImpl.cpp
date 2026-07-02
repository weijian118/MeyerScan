#include "UIComponentsImpl.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
// 模块名用于日志和工程识别，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_UIComponents";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_UIComponents v0.2.0 (2026-06-26)";
}

const double kDesignWidth = 1920.0;
const double kDesignHeight = 1080.0;
}

// 返回共享 UI 组件单例。
// 当前模块是无业务状态的控件工厂，单例可避免重复计算全局屏幕缩放。
UIComponentsImpl& UIComponentsImpl::Instance() {
    // 共享 UI 组件没有业务状态，使用单例主要是统一保存屏幕缩放系数。
    // 函数内 static 在 C++11 中线程安全，足够轻量。
    static UIComponentsImpl instance;
    return instance;
}

// 初始化 UI 辅助缩放系数。
// 主布局仍应优先使用 Qt Layout；这里的缩放只用于按钮高度、边距、进度条等辅助尺寸。
bool UIComponentsImpl::Init(const char* /*appDirUtf8*/) {
    // availableGeometry() 取当前主屏可用区域，不包含任务栏。
    // 这里不是做绝对坐标缩放，只是给控件高度、边距等辅助尺寸一个温和比例。
    // 注意：主布局仍交给 Qt Layout，自适应多语言文本长度；比例只影响辅助尺寸。
    const QRect geometry = QApplication::desktop()->availableGeometry();

    // 限制在 0.75~2.0，避免小屏过小不可点，也避免 4K 屏控件无限放大。
    // qMax/qMin 是 Qt 的模板函数，写法比手工 if 更紧凑，含义仍是“夹紧到范围内”。
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
    // objectName 方便样式表、自动化测试或调试器定位这个页面。
    root->setObjectName("MeyerScanWaitWidget");

    // 等待页使用布局而非固定坐标，适配多语言和不同分辨率。
    auto* layout = new QVBoxLayout(root);
    // setContentsMargins 控制内部留白，避免等待内容贴边。
    layout->setContentsMargins(36, 36, 36, 36);
    // setSpacing 控制子控件之间的固定间距，保持页面视觉稳定。
    layout->setSpacing(14);

    // title/message 都由调用方决定，UIComponents 不写业务文案。
    auto* title = CreatePageTitle(titleUtf8, root);
    auto* message = new QLabel(QString::fromUtf8(messageUtf8 ? messageUtf8 : ""), root);
    // 居中展示等待信息，不让不同长度文本影响布局方向。
    message->setAlignment(Qt::AlignCenter);
    message->setStyleSheet("color:#607080;font-size:15px;");

    // 不确定总进度时使用忙碌进度条：range(0,0) 是 Qt 的无限进度模式。
    auto* progress = new QProgressBar(root);
    progress->setRange(0, 0);
    // 关闭百分比文本，因为无限进度没有明确百分比。
    progress->setTextVisible(false);
    // 高度使用缩放后的辅助尺寸，但设置下限保证小屏仍可见。
    progress->setFixedHeight(static_cast<int>(qMax(6.0, 8.0 * m_scaleY)));

    // 上下 stretch 把内容推到垂直居中，窗口高度变化时等待内容仍在视觉中心。
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
    // 保留旧接口，内部转到新标准按钮工厂。
    return CreateButton(MeyerButtonRolePrimary, MeyerButtonContentTextOnly, textUtf8, "", parent);
}

// 创建次操作按钮。
// 仅统一外观和基本尺寸，不内置任何业务行为。
QPushButton* UIComponentsImpl::CreateSecondaryButton(const char* textUtf8, QWidget* parent) {
    // 保留旧接口，内部转到新标准按钮工厂。
    return CreateButton(MeyerButtonRoleSecondary, MeyerButtonContentTextOnly, textUtf8, "", parent);
}

// 创建标准 QPushButton。
// role 决定视觉层级，contentLayout 决定图标和文字的组合方式。
QPushButton* UIComponentsImpl::CreateButton(int role,
                                            int contentLayout,
                                            const char* textUtf8,
                                            const char* iconResourcePathUtf8,
                                            QWidget* parent) {
    // textUtf8 必须由调用方先 tr()，UIComponents 不写业务文案。
    auto* button = new QPushButton(QString::fromUtf8(textUtf8 ? textUtf8 : ""), parent);
    // iconResourcePathUtf8 可以为空；LoadIcon 为空时返回 null icon，不影响纯文字按钮。
    const QIcon icon = LoadIcon(iconResourcePathUtf8);
    if (!icon.isNull()) {
        // setIcon 只设置图标数据；图标大小由 ApplyButtonStyle 统一决定。
        button->setIcon(icon);
    }
    // 样式和尺寸集中到一个函数，便于后续统一调整所有模块按钮外观。
    ApplyButtonStyle(button, role, contentLayout);
    return button;
}

// 创建标准 QToolButton。
// ToolButton 更适合工具栏和纯图标按钮，不替代普通业务按钮。
QToolButton* UIComponentsImpl::CreateToolButton(int role,
                                                int contentLayout,
                                                const char* textUtf8,
                                                const char* iconResourcePathUtf8,
                                                QWidget* parent) {
    auto* button = new QToolButton(parent);
    // QToolButton 支持文字在图标下方/旁边，比 QPushButton 更适合工具栏。
    button->setText(QString::fromUtf8(textUtf8 ? textUtf8 : ""));
    const QIcon icon = LoadIcon(iconResourcePathUtf8);
    if (!icon.isNull()) {
        button->setIcon(icon);
    }
    ApplyToolButtonStyle(button, role, contentLayout);
    return button;
}

// 给已有 QPushButton 应用统一样式。
// 这让 HomeUI/CaseUI 等已有代码可以逐步迁移，不需要一次性改完所有按钮创建方式。
void UIComponentsImpl::ApplyButtonStyle(QPushButton* button, int role, int contentLayout) {
    if (!button) {
        // 允许调用方传空指针，方便业务代码在控件可选创建时直接调用而不崩溃。
        return;
    }

    // 高度统一由角色决定，宽度交给布局和 sizePolicy 自适应多语言文本。
    button->setMinimumHeight(ButtonMinimumHeight(role));
    // Entry 类按钮通常用于首页入口卡片，允许横向扩展；其它按钮也最少按内容扩展。
    button->setSizePolicy(role == MeyerButtonRoleEntry ? QSizePolicy::Expanding : QSizePolicy::MinimumExpanding,
                          QSizePolicy::Fixed);
    // setIconSize 只影响按钮内部图标显示尺寸，不改变原始图片文件。
    button->setIconSize(ButtonIconSize(contentLayout));
    // Qt 样式表按控件类型选择器生效；这里集中生成，避免各模块手写不同颜色/圆角。
    button->setStyleSheet(ButtonStyleSheet(role));

    if (contentLayout == MeyerButtonContentIconOnly) {
        // 纯图标按钮不强制清空 text，调用方可保留 text 用于无障碍/tooltip。
        button->setMinimumWidth(ButtonMinimumHeight(role));
        // 最大宽度限制避免图标按钮在布局里被拉成长条。
        button->setMaximumWidth(qMax(ButtonMinimumHeight(role), static_cast<int>(52 * m_scaleX)));
    } else if (contentLayout == MeyerButtonContentIconTopText) {
        // QPushButton 原生不擅长“上图下文”，这里只给更高的最小高度。
        // 复杂工具栏建议使用 CreateToolButton()，它能通过 toolButtonStyle 原生支持。
        button->setMinimumHeight(qMax(ButtonMinimumHeight(role), static_cast<int>(74 * m_scaleY)));
    }
}

// 给已有 QToolButton 应用统一样式。
void UIComponentsImpl::ApplyToolButtonStyle(QToolButton* button, int role, int contentLayout) {
    if (!button) {
        return;
    }

    button->setMinimumHeight(ButtonMinimumHeight(role));
    button->setIconSize(ButtonIconSize(contentLayout));
    button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    // ButtonStyleSheet 生成的是 QPushButton 选择器，ToolButton 复用颜色体系时替换选择器即可。
    QString style = ButtonStyleSheet(role);
    style.replace("QPushButton", "QToolButton");
    button->setStyleSheet(style);

    if (contentLayout == MeyerButtonContentIconOnly) {
        // QToolButton 原生支持 ToolButtonIconOnly，不需要自己写布局。
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setMinimumWidth(ButtonMinimumHeight(role));
    } else if (contentLayout == MeyerButtonContentIconTopText) {
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setMinimumHeight(qMax(ButtonMinimumHeight(role), static_cast<int>(74 * m_scaleY)));
    } else if (contentLayout == MeyerButtonContentIconLeftText) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    } else {
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }
}

// 创建通用输入框。
// placeholder 由调用方翻译后传入，避免 UIComponents 持有业务文案。
QLineEdit* UIComponentsImpl::CreateLineEdit(const char* placeholderUtf8, QWidget* parent) {
    auto* edit = new QLineEdit(parent);

    // placeholder 只作为输入提示；正式错误提示应由业务模块在旁边/弹窗中展示。
    // fromUtf8 保证多语言占位文字从跨 DLL 字节转换成 Qt 字符串。
    edit->setPlaceholderText(QString::fromUtf8(placeholderUtf8 ? placeholderUtf8 : ""));
    edit->setMinimumHeight(static_cast<int>(36 * m_scaleY));
    // 宽度交给父布局拉伸，高度固定，避免输入框因为文字变化上下跳动。
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
    // 下拉项不在这里添加，因为 UIComponents 不理解业务枚举。
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
    // Preferred 高度让换行标题可以自然变高，不会强行截断。
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

// 生成不同按钮角色对应的样式表。
// 角色只表示视觉层级，不包含任何业务权限语义。
QString UIComponentsImpl::ButtonStyleSheet(int role) const {
    // switch 让每个视觉角色的样式集中在一个地方，后续换肤时只改这里。
    switch (role) {
    case MeyerButtonRolePrimary:
        return "QPushButton{background:#007d68;color:white;border:0;border-radius:4px;padding:8px 18px;}"
               "QPushButton:hover{background:#009176;}"
               "QPushButton:pressed{background:#006652;}"
               "QPushButton:disabled{background:#d8d8d8;color:#888;}";
    case MeyerButtonRoleText:
        return "QPushButton{background:transparent;color:#007d68;border:0;border-radius:4px;padding:6px 10px;}"
               "QPushButton:hover{background:#e7f3f0;}"
               "QPushButton:pressed{background:#d6ebe6;}"
               "QPushButton:disabled{color:#999;}";
    case MeyerButtonRoleDanger:
        return "QPushButton{background:#c0392b;color:white;border:0;border-radius:4px;padding:8px 16px;}"
               "QPushButton:hover{background:#d24a3c;}"
               "QPushButton:pressed{background:#9f2f25;}"
               "QPushButton:disabled{background:#e1d2cf;color:#9a8f8c;}";
    case MeyerButtonRoleEntry:
        return "QPushButton{text-align:left;background:white;color:#23313f;border:1px solid #d8e1e7;border-radius:6px;padding:14px;}"
               "QPushButton:hover{background:#edf8f5;border-color:#9ccfc3;}"
               "QPushButton:pressed{background:#dff1ec;}"
               "QPushButton:disabled{background:#f4f4f4;color:#999;border-color:#e2e2e2;}";
    case MeyerButtonRoleSecondary:
    default:
        return "QPushButton{background:#f6f8fa;color:#23313f;border:1px solid #cfd8dc;border-radius:4px;padding:8px 16px;}"
               "QPushButton:hover{background:#edf2f5;}"
               "QPushButton:pressed{background:#e1e8ec;}"
               "QPushButton:disabled{color:#999;border-color:#ddd;background:#f4f4f4;}";
    }
}

// 根据角色返回最小高度。
int UIComponentsImpl::ButtonMinimumHeight(int role) const {
    if (role == MeyerButtonRoleEntry) {
        // 首页入口卡片需要更高，容纳标题和说明两行文本。
        return static_cast<int>(qMax(86.0, 104.0 * m_scaleY));
    }
    if (role == MeyerButtonRoleText) {
        // 文本按钮更轻，通常用于“返回”“取消”等弱操作。
        return static_cast<int>(qMax(30.0, 34.0 * m_scaleY));
    }
    // 普通按钮保持可点击高度下限，兼顾鼠标和触控设备。
    return static_cast<int>(qMax(36.0, 40.0 * m_scaleY));
}

// 根据内容布局返回图标尺寸。
QSize UIComponentsImpl::ButtonIconSize(int contentLayout) const {
    if (contentLayout == MeyerButtonContentIconTopText) {
        // 上图下文的图标可以稍大，因为按钮高度也更高。
        const int size = static_cast<int>(qMax(24.0, 32.0 * qMin(m_scaleX, m_scaleY)));
        return QSize(size, size);
    }
    if (contentLayout == MeyerButtonContentIconOnly) {
        // 纯图标按钮的图标占据主要视觉面积，但仍保留边距。
        const int size = static_cast<int>(qMax(18.0, 24.0 * qMin(m_scaleX, m_scaleY)));
        return QSize(size, size);
    }
    // 左图右文场景图标不宜过大，否则会挤压多语言文本。
    const int size = static_cast<int>(qMax(16.0, 20.0 * qMin(m_scaleX, m_scaleY)));
    return QSize(size, size);
}

// 加载图标。
// 资源路径可以是 Qt 资源路径，也可以是安装目录下的绝对/相对文件路径。
QIcon UIComponentsImpl::LoadIcon(const char* iconResourcePathUtf8) const {
    // trim 去掉配置或调用方传入路径两侧空格，避免很难察觉的图标加载失败。
    const QString path = QString::fromUtf8(iconResourcePathUtf8 ? iconResourcePathUtf8 : "").trimmed();
    if (path.isEmpty()) {
        return QIcon();
    }
    // QIcon 同时支持 :/xxx 资源路径和磁盘文件路径。
    QIcon icon(path);
    if (!icon.isNull()) {
        return icon;
    }
    // 当前不做路径猜测，避免 UIComponents 偷偷依赖 currentPath 或某个固定资源目录。
    return QIcon();
}

// C ABI 导出函数。
// MainExe 和各 UI 测试宿主通过该函数获取共享控件工厂。
extern "C" MEYERSCAN_UICOMPONENTS_API IUIComponents* GetUIComponents() {
    // C ABI 工厂函数保持导出名稳定，方便其它 DLL 用 QLibrary 动态获取。
    return &UIComponentsImpl::Instance();
}
