#pragma once

#include <QWidget>

#ifdef MEYERSCAN_UICOMPONENTS_EXPORTS
#  define MEYERSCAN_UICOMPONENTS_API __declspec(dllexport)
#else
#  define MEYERSCAN_UICOMPONENTS_API __declspec(dllimport)
#endif

class QPushButton;
class QLabel;
class QComboBox;
class QLineEdit;
class QToolButton;

// 通用按钮视觉角色。
// 角色只描述外观重要性，不描述业务含义；业务权限和点击行为仍由调用模块决定。
enum MeyerButtonRole {
    MeyerButtonRolePrimary = 1,      // 高亮主按钮，例如 Confirm、Log In、Calibrate
    MeyerButtonRoleSecondary = 2,    // 普通次按钮，例如 Cancel、Back、Edit
    MeyerButtonRoleText = 3,         // 纯文字按钮，例如表格行内操作或轻量链接
    MeyerButtonRoleDanger = 4,       // 危险动作按钮，例如 Delete
    MeyerButtonRoleEntry = 5,        // 首页/工作台入口类大按钮
};

// 通用按钮内容布局。
// 当前先服务 QPushButton/QToolButton 的图标和文字排列，后续可继续扩展图标尺寸、tooltip 等属性。
enum MeyerButtonContentLayout {
    MeyerButtonContentTextOnly = 1,       // 纯文字按钮
    MeyerButtonContentIconOnly = 2,       // 纯图标按钮
    MeyerButtonContentIconLeftText = 3,   // 左图标右文字
    MeyerButtonContentIconTopText = 4,    // 上图标下文字
};

// IUIComponents 是共享 UI 控件模块的公共接口。
// 模块边界:
//   - 统一常用控件样式、等待页、辅助缩放系数。
//   - 不承载业务逻辑、不读取权限、不访问数据库。
//   - 可被 MainExe、HomeUI、CaseUI、校准 UI 等界面模块复用。
class MEYERSCAN_UICOMPONENTS_API IUIComponents {
public:
    // 虚析构函数用于保持跨 DLL 多态接口安全。
    virtual ~IUIComponents() = default;

    // 初始化共享 UI 模块。
    // appDirUtf8 是 MeyerScan.exe 所在目录，后续可用于加载统一图标/样式资源。
    virtual bool Init(const char* appDirUtf8) = 0;

    // 返回横向辅助缩放系数。
    // 注意: 正式布局仍优先使用 Qt layout，缩放系数只用于图标、间距、固定控件高度等少量场景。
    virtual double ScaleX() const = 0;

    // 返回纵向辅助缩放系数。
    virtual double ScaleY() const = 0;

    // 创建统一等待页。
    // titleUtf8/messageUtf8 应传英文源文本，内部显示可由调用方翻译后传入。
    virtual QWidget* CreateWaitWidget(const char* titleUtf8, const char* messageUtf8, QWidget* parent = nullptr) = 0;

    // 创建主按钮，用于页面中的主要确认/进入动作。
    virtual QPushButton* CreatePrimaryButton(const char* textUtf8, QWidget* parent = nullptr) = 0;

    // 创建次按钮，用于返回、取消、辅助动作。
    virtual QPushButton* CreateSecondaryButton(const char* textUtf8, QWidget* parent = nullptr) = 0;

    // 创建标准 QPushButton。
    // textUtf8 必须是调用方 tr("English source text") 后的显示文本；iconResourcePathUtf8 为空时创建无图标按钮。
    virtual QPushButton* CreateButton(int role,
                                      int contentLayout,
                                      const char* textUtf8,
                                      const char* iconResourcePathUtf8,
                                      QWidget* parent = nullptr) = 0;

    // 创建标准 QToolButton。
    // 适合工具栏、纯图标或上图标下文字的紧凑按钮；业务模块仍负责连接 clicked 信号。
    virtual QToolButton* CreateToolButton(int role,
                                          int contentLayout,
                                          const char* textUtf8,
                                          const char* iconResourcePathUtf8,
                                          QWidget* parent = nullptr) = 0;

    // 将统一按钮样式应用到已有 QPushButton。
    // 用于模块内部已经创建了按钮但希望复用 UIComponents 样式的场景。
    virtual void ApplyButtonStyle(QPushButton* button, int role, int contentLayout) = 0;

    // 将统一按钮样式应用到已有 QToolButton。
    virtual void ApplyToolButtonStyle(QToolButton* button, int role, int contentLayout) = 0;

    // 创建统一单行输入框。
    virtual QLineEdit* CreateLineEdit(const char* placeholderUtf8, QWidget* parent = nullptr) = 0;

    // 创建统一下拉框。
    virtual QComboBox* CreateComboBox(QWidget* parent = nullptr) = 0;

    // 创建页面标题标签。
    virtual QLabel* CreatePageTitle(const char* textUtf8, QWidget* parent = nullptr) = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 清理模块状态。
    virtual void Shutdown() = 0;
};

// C ABI 工厂函数。
extern "C" MEYERSCAN_UICOMPONENTS_API IUIComponents* GetUIComponents();
