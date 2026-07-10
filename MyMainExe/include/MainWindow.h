#pragma once

#include <QCoreApplication>
#include <QJsonObject>
#include <QLibrary>
#include <QList>
#include <QMainWindow>
#include <QPair>
#include <QStringList>

#include "CaseUI.h"
#include "ConfigCenter.h"
#include "ExternalLaunchAdapter.h"
#include "HomeUI.h"
#include "MeyerLoginWidget.h"
#include "DataProcessUI.h"
#include "OrderCreateUI.h"
#include "OrderScanWorkspaceShell.h"
#include "Permission.h"
#include "RuntimeDataCenter.h"
#include "ScanWorkflowUI.h"
#include "SendUI.h"
#include "SettingsUI.h"
#include "UIComponents.h"

class QLabel;
class QVBoxLayout;
class IDatabaseQtAdapter;
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

    // 第三方拉起入口：跳过首页视觉展示，直接进入建单扫描工作台。
    // inputJsonPath 是第三方下发的建单 JSON 路径，thirdPartyType 可为空。
    void StartExternalOrder(const QString& inputJsonPath, const QString& thirdPartyType);

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

    // SettingsUI 是 C ABI 回调，必须用静态函数把 context 转回 MainWindow。
    static void OnSettingsAction(void* context, int actionId);

    // OrderCreateUI 是 C ABI 回调，必须用静态函数把 context 转回 MainWindow。
    static void OnOrderCreateAction(void* context, int actionId);

    // OrderScanWorkspaceShell 右上角按钮回调，必须用静态函数把 context 转回 MainWindow。
    static void OnWorkspaceShellAction(void* context, int actionId);

    // OrderScanWorkspaceShell 步骤变化回调，MainExe 用它懒加载扫描/处理页面。
    static void OnWorkspaceStepChanged(void* context, int step);

    // ScanWorkflowUI 动作回调，必须用静态函数把 context 转回 MainWindow。
    static void OnScanWorkflowAction(void* context, int actionId);

    // DataProcessUI 动作回调，必须用静态函数把 context 转回 MainWindow。
    static void OnDataProcessAction(void* context, int actionId);

    // SendUI 动作回调，必须用静态函数把 context 转回 MainWindow。
    static void OnSendAction(void* context, int actionId);

    // 处理首页入口点击，例如浏览、创建、练习、设置。
    void HandleHomeEntryClicked(int entryId);

    // 处理案例管理页面动作，例如返回首页或打开订单。
    void HandleCaseAction(int actionId);

    // 处理设置页面动作，例如关闭、确认、应用和校准入口。
    void HandleSettingsAction(int actionId);

    // 处理建单页面动作，例如取消、确认、下一步或牙位变化。
    void HandleOrderCreateAction(int actionId);

    // 处理工作台右上角按钮动作，例如最小化或关闭工作台。
    void HandleWorkspaceShellAction(int actionId);

    // 处理工作台步骤变化，并按步骤懒加载/释放重资源页面。
    void HandleWorkspaceStepChanged(int step);

    // 处理扫描页面动作，例如上一页、下一页或扫描工具切换。
    void HandleScanWorkflowAction(int actionId);

    // 处理数据处理页面动作，例如上一页、下一页或处理工具切换。
    void HandleDataProcessAction(int actionId);

    // 处理发送页面动作，例如返回处理页、导出、上传或完成。
    void HandleSendAction(int actionId);

    // 第三方拉起建单时，后台准备首页创建入口并复用同一套权限/配置规则。
    bool PrepareHomeCreateEntryForExternalOrder();

    // 判断登录 DLL 的状态码是否代表可以进入主界面。
    bool IsLoginAcceptedStatus(int status) const;

    // 显示首页；如果首页尚未创建则延迟创建。
    void ShowHome();

    // 显示案例管理页；如果页面尚未创建则延迟创建。
    void ShowCase();

    // 显示设置页；openSource 记录关闭设置后回到哪个主页面，并决定校准入口是否可用。
    void ShowSettings(int openSource);

    // 显示建单/扫描工作台，并把建单 UI 挂入工作台第一步。
    void ShowOrderWorkspace(const QString& orderContextJson = QString());

    // 显示练习工作台，只包含 Scan 和 Process 两步，订单信息使用默认上下文。
    void ShowPracticeWorkspace();

    // 进入扫描重建前释放案例管理页，把内存/显存资源留给扫描重建。
    void PrepareForScanReconstruct();

    // 确保 HomeUI 已初始化并已创建 QWidget。
    bool EnsureHomePage();

    // 确保 CaseUI 已初始化并已创建 QWidget。
    bool EnsureCasePage();

    // 确保 SettingsUI 已初始化并已创建 QWidget。
    bool EnsureSettingsPage();

    // 确保建单/扫描工作台和建单 UI 已初始化并已创建 QWidget。
    bool EnsureOrderWorkspacePage(const QString& orderContextJson);

    // 确保练习工作台、扫描页和处理页入口已初始化。
    bool EnsurePracticeWorkspacePage();

    // 确保扫描步骤页面已创建并挂入工作台。
    bool EnsureScanWorkflowPage();

    // 确保数据处理步骤页面已创建并挂入工作台。
    bool EnsureDataProcessPage();

    // 确保发送步骤页面已创建并挂入工作台。
    bool EnsureSendPage();

    // 根据设置来源返回设置关闭后应该回到的页面名称。
    QString SettingsReturnPageName(int openSource) const;

    // 判断当前来源是否允许打开三维/颜色校准。
    bool IsCalibrationAllowedForSettingsSource(int openSource) const;

    // 释放非当前显示的首页 QWidget。
    void ReleaseHomePage();

    // 释放非当前显示的案例管理 QWidget。
    void ReleaseCasePage();

    // 释放等待页。登录窗口显示后必须释放等待页，避免挡在主窗口中。
    void ReleaseWaitPage();

    // 释放设置页。
    void ReleaseSettingsPage();

    // 释放建单/扫描工作台和建单 UI。
    void ReleaseOrderWorkspacePage();

    // 释放扫描页面根控件和 VTK/OpenGL 资源。
    void ReleaseScanWorkflowPage();

    // 释放数据处理页面根控件和 VTK/OpenGL 资源。
    void ReleaseDataProcessPage();

    // 释放发送页面根控件。
    void ReleaseSendPage();

    // 统一显示主窗口。
    // 首页、浏览、创建、练习都必须在无边框全屏主窗口中显示，不能散落调用普通 show()。
    void ShowMainWindow();

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

    // 调用 ExternalLaunchAdapter，把第三方 JSON 文件转换成标准建单上下文。
    bool NormalizeExternalOrderContext(const QString& inputJsonPath,
                                       const QString& thirdPartyType,
                                       QString* outputContextJson);

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
    QList<QPair<QString, QString>> LoadVersionManifest(const QString& manifestPath) const;

    // 首次运行没有 manifest 时写入默认清单，后续新增模块只维护该文件即可。
    void EnsureDefaultVersionManifest(const QString& manifestPath) const;

    // 从 Windows 文件版本资源读取文件版本号。
    QString ReadFileVersion(const QString& filePath) const;

    // 从自研 DLL 的统一 C ABI 版本函数读取代码版本。
    QString ReadCodeVersion(const QString& filePath, const QString& versionFunctionName, QString* errorMessage) const;

    // 从 "MeyerScan_Logger v1.1.0 (2026-06-24)" 这类字符串中提取 "1.1.0"。
    QString NormalizeVersionText(const QString& versionText) const;

    // 比较文件版本和代码版本是否一致；允许文件版本末尾多一个 ".0"。
    bool AreVersionsConsistent(const QString& fileVersion, const QString& codeVersion) const;

    // 运行时加载 DLL 并解析 C ABI 工厂函数。
    QFunctionPointer ResolveFactory(QLibrary& library,
                                    const char* dllName,
                                    const char* factoryName,
                                    QString* errorMessage = nullptr) const;

    // 以下函数分别返回各自模块的接口指针。
    // MainExe 只保存接口头文件，不链接这些自研 DLL 的 import lib。
    ILogger* LoggerModule();
    IDatabaseQtAdapter* DatabaseAdapterModule();
    IConfigCenter* ConfigCenterModule();
    IPermission* PermissionModule();
    IUIComponents* UIComponentsModule();
    IRuntimeDataCenter* RuntimeDataCenterModule();
    IHomeUI* HomeUIModule();
    ICaseUI* CaseUIModule();
    ISettingsUI* SettingsUIModule();
    IOrderScanWorkspaceShell* OrderWorkspaceModule();
    IOrderCreateUI* OrderCreateUIModule();
    IScanWorkflowUI* ScanWorkflowModule();
    IDataProcessUI* DataProcessModule();
    ISendUI* SendUIModule();
    IExternalLaunchAdapter* ExternalLaunchAdapterModule();

    // 构造练习/默认扫描上下文 JSON，供 ScanWorkflowUI/DataProcessUI 暂时使用。
    QString BuildDefaultWorkspaceContextJson(const QString& mode) const;

    // 构造默认练习扫描流程，练习模式没有建单页，流程由 MainExe 固定提供。
    QJsonObject BuildDefaultScanProcessObject() const;

    // 从 OrderCreateUI 读取最新扫描流程并合并到工作台上下文。
    void RefreshWorkspaceScanProcessFromOrder();

    // 把扫描流程对象写入 m_workspaceContextJson。
    void SetWorkspaceScanProcess(const QJsonObject& scanProcessObject);

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
    ISettingsUI* m_settings = nullptr;
    IOrderScanWorkspaceShell* m_orderWorkspace = nullptr;
    IOrderCreateUI* m_orderCreate = nullptr;
    IScanWorkflowUI* m_scanWorkflow = nullptr;
    IDataProcessUI* m_dataProcess = nullptr;
    ISendUI* m_send = nullptr;
    IExternalLaunchAdapter* m_externalLaunchAdapter = nullptr;
    IConfigCenter* m_config = nullptr;
    IPermission* m_permission = nullptr;
    IRuntimeDataCenter* m_runtimeDataCenter = nullptr;
    IUIComponents* m_uiComponents = nullptr;
    ILogger* m_logger = nullptr;
    QLibrary m_loggerLibrary;
    QLibrary m_databaseAdapterLibrary;
    QLibrary m_configLibrary;
    QLibrary m_permissionLibrary;
    QLibrary m_uiComponentsLibrary;
    QLibrary m_runtimeDataCenterLibrary;
    QLibrary m_homeLibrary;
    QLibrary m_caseLibrary;
    QLibrary m_settingsLibrary;
    QLibrary m_orderWorkspaceLibrary;
    QLibrary m_orderCreateLibrary;
    QLibrary m_scanWorkflowLibrary;
    QLibrary m_dataProcessLibrary;
    QLibrary m_sendLibrary;
    QLibrary m_externalLaunchAdapterLibrary;
    QWidget* m_homeWidget = nullptr;
    QWidget* m_caseWidget = nullptr;
    QWidget* m_settingsWidget = nullptr;
    QWidget* m_orderWorkspaceWidget = nullptr;
    QWidget* m_orderCreateWidget = nullptr;
    QWidget* m_scanWorkflowWidget = nullptr;
    QWidget* m_dataProcessWidget = nullptr;
    QWidget* m_sendWidget = nullptr;
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
    bool m_settingsInitialized = false;
    bool m_orderWorkspaceInitialized = false;
    bool m_orderCreateInitialized = false;
    bool m_scanWorkflowInitialized = false;
    bool m_dataProcessInitialized = false;
    bool m_sendInitialized = false;
    bool m_externalLaunchAdapterInitialized = false;
    bool m_loggerInitialized = false;
    int m_settingsOpenSource = SettingsOpenSourceHome;
    int m_currentWorkspaceMode = WorkspaceModeOrderCreate;
    QString m_workspaceContextJson;
};
