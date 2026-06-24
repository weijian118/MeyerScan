#pragma once

#include <QCoreApplication>
#include <QMainWindow>
#include <QStringList>

#include "CaseUI.h"
#include "ConfigCenter.h"
#include "HomeUI.h"
#include "MeyerLoginWidget.h"
#include "Permission.h"
#include "UIComponents.h"

class QLabel;
class QVBoxLayout;
class ILogger;

// MainWindow 是 MeyerScan.exe 的主窗口和轻量编排层。
// 它只负责启动流程、登录衔接、页面容器切换和资源释放，
// 不在这里写病例/订单业务 SQL、权限算法或扫描算法。
class MainWindow : public QMainWindow {
    Q_DECLARE_TR_FUNCTIONS(MainExe)

public:
    // 构造主窗口、创建页面容器，并尽早初始化日志。
    // 日志必须早于登录模块启动，便于记录启动早期问题。
    explicit MainWindow(QWidget* parent = nullptr);

    // 退出时按模块生命周期反向释放，最后关闭日志。
    ~MainWindow() override;

    // 正常客户启动入口：先做基础设施检查，再显示登录模块。
    void StartLogin();

    // 自动化/冒烟测试入口：跳过登录，用定时器验证首页和案例页切换。
    void StartWithoutLoginForSmoke();

    // 单实例激活时用于判断是否已经登录完成。
    // 登录前重复双击只忽略，不弹出未完成初始化的窗口。
    bool IsLoginCompleted() const { return m_loginCompleted; }

private:
    // 接收既有登录 DLL 通过信号返回的登录状态。
    void OnLoginStatusReturn(const LoginReturnParameters& result);

    // HomeUI 是 C ABI 回调，必须用静态函数把 context 转回 MainWindow。
    static void OnHomeEntryClicked(void* context, int entryId);

    // CaseUI 是 C ABI 回调，必须用静态函数把 context 转回 MainWindow。
    static void OnCaseAction(void* context, int actionId);

    // 处理首页入口点击，例如浏览、创建、练习、设置。
    void HandleHomeEntryClicked(int entryId);

    // 处理案例管理页面动作，例如返回首页或打开订单。
    void HandleCaseAction(int actionId);

    // 判断登录 DLL 的状态码是否代表可以进入主界面。
    bool IsLoginAcceptedStatus(int status) const;

    // 显示首页；如果首页尚未创建则延迟创建。
    void ShowHome();

    // 显示案例管理页；如果页面尚未创建则延迟创建。
    void ShowCase();

    // 进入扫描重建前释放案例管理页，把内存/显存资源留给扫描重建。
    void PrepareForScanReconstruct();

    // 确保 HomeUI 已初始化并已创建 QWidget。
    bool EnsureHomePage();

    // 确保 CaseUI 已初始化并已创建 QWidget。
    bool EnsureCasePage();

    // 释放非当前显示的首页 QWidget。
    void ReleaseHomePage();

    // 释放非当前显示的案例管理 QWidget。
    void ReleaseCasePage();

    // 释放等待页。登录窗口显示后必须释放等待页，避免挡在主窗口中。
    void ReleaseWaitPage();

    // 释放指定页面指针。allowActive=false 时不会释放当前正在显示的页面。
    void ReleasePageWidget(QWidget*& pageWidget, const QString& pageName, bool allowActive);

    // 组装既有登录模块需要的参数。
    UserLoginParameters BuildLoginParameters() const;

    // 初始化配置、权限、UI 组件、数据库和版本清单等基础设施。
    void InitInfrastructure();

    // 初始化业务页面入口。当前页面采用懒加载，这里只保留统一入口。
    void InitPages();

    // 显示启动/切换等待页。
    void ShowWaitPage(const QString& message);

    // 关闭等待页；登录阶段会同时隐藏主窗口。
    void HideWaitPage();

    // 基于应用目录解析数据库配置路径，禁止依赖当前工作目录。
    QString ResolveDatabaseConfigPath() const;

    // 基于应用目录解析日志目录，禁止依赖当前工作目录。
    QString ResolveLogDir() const;

    // 将主窗口内容区替换为指定页面。
    // MainExe 内容区一次只挂载一个全屏页面，不把首页和浏览页作为并列兄弟页长期放在容器中。
    void ReplaceContentWidget(QWidget* widget, const QString& pageName);

    // 读取 ConfigCenter 与 Permission 后统一下发首页入口规则。
    void ApplyHomeEntryRules();

    // 读取 ConfigCenter 与 Permission 后统一下发案例管理动作规则。
    void ApplyCaseActionRules();

    // 合并“配置默认显隐”和“权限显隐”。
    bool IsFeatureVisible(const char* featureId, const char* configKey, bool defaultVisible) const;

    // 判断权限 enabled；用于按钮禁用态和动作执行前复核。
    bool IsFeatureEnabled(const char* featureId, bool defaultEnabled) const;

    // 将首页入口 ID 映射为权限 featureId。
    const char* HomeEntryFeatureId(int entryId) const;

    // 将案例管理动作 ID 映射为权限 featureId。
    const char* CaseActionFeatureId(int actionId) const;

    // 启动时生成 EXE/DLL 版本清单。
    // 当前该能力并入 MainExe，后续复杂化后可再拆为独立模块。
    void WriteVersionList();

    // 版本清单只读取 manifest 中列出的拆分模块文件，避免把 Qt/OpenSSL/AWS 等第三方库写入清单。
    QStringList LoadVersionManifest(const QString& manifestPath) const;

    // 首次运行没有 manifest 时写入默认清单，后续新增模块只维护该文件即可。
    void EnsureDefaultVersionManifest(const QString& manifestPath) const;

    // 从 Windows 文件版本资源读取文件版本号。
    QString ReadFileVersion(const QString& filePath) const;

    // 写入客户操作日志。
    void WriteUserAction(const QString& operation, const QString& content);

    // 更新状态栏文本。
    void WriteStatus(const QString& text);

    // 尽早初始化日志，并把 Logger 单例缓存到成员变量中。
    // 后续写日志只用 m_logger，避免每次操作都重新调用 GetLogger()。
    void InitLoggerEarly();

private:
    CBLMeyerLoginWidget m_loginWidget;
    IHomeUI* m_home = nullptr;
    ICaseUI* m_case = nullptr;
    IConfigCenter* m_config = nullptr;
    IPermission* m_permission = nullptr;
    IUIComponents* m_uiComponents = nullptr;
    ILogger* m_logger = nullptr;
    QWidget* m_homeWidget = nullptr;
    QWidget* m_caseWidget = nullptr;
    QWidget* m_waitWidget = nullptr;
    QWidget* m_contentRoot = nullptr;
    QWidget* m_activeWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QLabel* m_status = nullptr;
    QByteArray m_databaseConfigPathUtf8;
    QByteArray m_logDirUtf8;
    QByteArray m_appDirUtf8;
    bool m_infrastructureInitialized = false;
    bool m_databaseReady = false;
    bool m_loginCompleted = false;
    bool m_homeInitialized = false;
    bool m_caseInitialized = false;
    bool m_loggerInitialized = false;
};
