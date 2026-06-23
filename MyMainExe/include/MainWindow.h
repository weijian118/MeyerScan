#pragma once

#include <QCoreApplication>
#include <QMainWindow>

#include "CaseUI.h"
#include "ConfigCenter.h"
#include "HomeUI.h"
#include "MeyerLoginWidget.h"
#include "Permission.h"
#include "UIComponents.h"
#include "VersionManager.h"

class QLabel;
class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_DECLARE_TR_FUNCTIONS(MainExe)

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void StartLogin();
    void StartWithoutLoginForSmoke();
    bool IsLoginCompleted() const { return m_loginCompleted; }

private:
    void OnLoginStatusReturn(const LoginReturnParameters& result);
    static void OnHomeEntryClicked(void* context, int entryId);
    static void OnCaseAction(void* context, int actionId);
    void HandleHomeEntryClicked(int entryId);
    void HandleCaseAction(int actionId);
    bool IsLoginAcceptedStatus(int status) const;
    void ShowHome();
    void ShowCase();
    void PrepareForScanReconstruct();
    bool EnsureHomePage();
    bool EnsureCasePage();
    void ReleaseHomePage();
    void ReleaseCasePage();
    void ReleaseWaitPage();
    void ReleaseInactivePage(QWidget* activeWidget);

    UserLoginParameters BuildLoginParameters() const;
    void InitInfrastructure();
    void InitPages();
    void ShowWaitPage(const QString& message);
    void HideWaitPage();
    QString ResolveDatabaseConfigPath() const;
    QString ResolveLogDir() const;
    void SwitchToWidget(QWidget* widget, const QString& pageName);
    void WriteUserAction(const QString& operation, const QString& content);
    void WriteStatus(const QString& text);

private:
    CBLMeyerLoginWidget m_loginWidget;
    IHomeUI* m_home = nullptr;
    ICaseUI* m_case = nullptr;
    IConfigCenter* m_config = nullptr;
    IPermission* m_permission = nullptr;
    IUIComponents* m_uiComponents = nullptr;
    IVersionManager* m_versionManager = nullptr;
    QWidget* m_homeWidget = nullptr;
    QWidget* m_caseWidget = nullptr;
    QWidget* m_waitWidget = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_status = nullptr;
    QByteArray m_databaseConfigPathUtf8;
    QByteArray m_logDirUtf8;
    QByteArray m_appDirUtf8;
    bool m_infrastructureInitialized = false;
    bool m_databaseReady = false;
    bool m_loginCompleted = false;
    bool m_homeInitialized = false;
    bool m_caseInitialized = false;
};
