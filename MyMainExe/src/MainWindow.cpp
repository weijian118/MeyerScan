#include "MainWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QLabel>
#include <QStatusBar>
#include <QStackedWidget>
#include <QString>
#include <QToolBar>
#include <QTimer>

#include "Database.h"
#include "Logger.h"

namespace {
const char* kDefaultLoginUrl = "https://myscan.meyerop.com/login";
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("MeyerScan"));
    resize(1180, 760);

    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    auto* homeAction = toolbar->addAction(tr("Home"));
    auto* caseAction = toolbar->addAction(tr("Case Management"));
    connect(homeAction, &QAction::triggered, [this]() {
        WriteUserAction("ToolbarHome", "Toolbar home clicked");
        ShowHome();
    });
    connect(caseAction, &QAction::triggered, [this]() {
        WriteUserAction("ToolbarCase", "Toolbar case clicked");
        ShowCase();
    });

    m_stack = new QStackedWidget(this);
    m_stack->setUpdatesEnabled(true);
    setCentralWidget(m_stack);

    m_status = new QLabel(this);
    statusBar()->addPermanentWidget(m_status, 1);

    connect(&m_loginWidget,
            &CBLMeyerLoginWidget::loginStatusReturn,
            this,
            [this](const LoginReturnParameters& result) { OnLoginStatusReturn(result); });
}

MainWindow::~MainWindow() {
    if (m_home) {
        m_home->Shutdown();
    }
    if (m_case) {
        m_case->Shutdown();
    }
    if (m_versionManager) {
        m_versionManager->Shutdown();
    }
    if (m_permission) {
        m_permission->Shutdown();
    }
    if (m_config) {
        m_config->Shutdown();
    }
    if (m_uiComponents) {
        m_uiComponents->Shutdown();
    }

    if (m_infrastructureInitialized) {
        IDatabase* database = GetDatabase();
        if (database) {
            database->Disconnect();
            database->Shutdown();
        }

        ILogger* logger = GetLogger();
        if (logger) {
            logger->Write(LogLevel::Info, "MainExe", "Shutdown", "", "", "", "MainExe shutdown");
            logger->Flush();
            logger->Shutdown();
        }
    }
}

void MainWindow::StartLogin() {
    ShowWaitPage(tr("Preparing login"));
    InitInfrastructure();
    HideWaitPage();
    WriteStatus(tr("Login module starting"));
    m_loginWidget.initLoginWidgetAndShow(BuildLoginParameters());
}

void MainWindow::StartWithoutLoginForSmoke() {
    ShowWaitPage(tr("Preparing modules"));
    InitInfrastructure();
    m_loginCompleted = true;
    InitPages();
    ShowHome();
    show();
}

UserLoginParameters MainWindow::BuildLoginParameters() const {
    const QString appDir = QCoreApplication::applicationDirPath();

    UserLoginParameters params;
    params.nfaktoW = 1.0;
    params.nfaktoH = 1.0;
    params.dataPath = QDir(appDir).filePath("license.lic");
    params.languageType = G_LANG_SIMPLIFIED_CHINESE;
    params.AppPath = appDir;
    params.loginUrl = QString::fromUtf8(kDefaultLoginUrl);
    params.registerUrl = QString::fromUtf8(kDefaultLoginUrl);
    params.version = 100;
    return params;
}

void MainWindow::OnLoginStatusReturn(const LoginReturnParameters& result) {
    qDebug() << "Login status:" << result.currentStatus;

    ILogger* logger = GetLogger();
    if (logger) {
        const QByteArray statusText = QString("Login returned status %1").arg(result.currentStatus).toUtf8();
        logger->Write(IsLoginAcceptedStatus(result.currentStatus) ? LogLevel::Info : LogLevel::Warning,
                      "MainExe",
                      "LoginStatus",
                      "",
                      "",
                      "",
                      statusText.constData());
    }

    if (!IsLoginAcceptedStatus(result.currentStatus)) {
        WriteStatus(tr("Login returned status %1").arg(result.currentStatus));
        return;
    }

    m_loginCompleted = true;
    InitInfrastructure();
    InitPages();
    ShowHome();
    show();
}

bool MainWindow::IsLoginAcceptedStatus(int status) const {
    return status == LOGIN_SUCCESS || status == WRITECLOUDMSG_SUCCESS;
}

void MainWindow::InitInfrastructure() {
    if (m_infrastructureInitialized) {
        return;
    }

    const QString logDir = ResolveLogDir();
    const QString databaseConfigPath = ResolveDatabaseConfigPath();
    QDir().mkpath(logDir);

    m_appDirUtf8 = QDir::fromNativeSeparators(QCoreApplication::applicationDirPath()).toUtf8();
    m_logDirUtf8 = QDir::fromNativeSeparators(logDir).toUtf8();
    m_databaseConfigPathUtf8 = QDir::fromNativeSeparators(databaseConfigPath).toUtf8();

    m_config = GetConfigCenter();
    if (m_config) {
        m_config->Init(m_appDirUtf8.constData());
    }

    m_permission = GetPermission();
    if (m_permission) {
        m_permission->Init(m_appDirUtf8.constData());
    }

    m_uiComponents = GetUIComponents();
    if (m_uiComponents) {
        m_uiComponents->Init(m_appDirUtf8.constData());
    }

    ILogger* logger = GetLogger();
    if (logger && logger->Init(m_logDirUtf8.constData(), LogLevel::Info)) {
        logger->Write(LogLevel::Info, "MainExe", "Startup", "", "", "", "Logger initialized");
    }

    IDatabase* database = GetDatabase();
    if (database && !m_databaseConfigPathUtf8.isEmpty()) {
        const VoidResult initResult = database->Init(m_databaseConfigPathUtf8.constData());
        if (initResult.IsSuccess()) {
            char databaseType[32] = {0};
            if (m_config && m_config->GetString("database.type", "mysql", databaseType, sizeof(databaseType))) {
                const QString databaseTypeText = QString::fromUtf8(databaseType).toLower();
                database->SetDatabaseType(databaseTypeText == "sqlite" ? DatabaseType::SQLite : DatabaseType::MySQL);
            }
            const VoidResult connectResult = database->Connect();
            m_databaseReady = connectResult.IsSuccess() && database->IsConnected();
        }
    }

    if (logger) {
        const char* databaseMessage = m_databaseConfigPathUtf8.isEmpty()
            ? "Database config not found"
            : (m_databaseReady ? "Database connected" : "Database is not ready");
        logger->Write(m_databaseReady ? LogLevel::Info : LogLevel::Warning,
                      "MainExe",
                      "DatabaseCheck",
                      "",
                      "",
                      "",
                      databaseMessage);
    }

    m_versionManager = GetVersionManager();
    if (m_versionManager && m_versionManager->Init(m_appDirUtf8.constData(), m_logDirUtf8.constData())) {
        const bool versionWritten = m_versionManager->WriteVersionList();
        if (logger) {
            logger->Write(versionWritten ? LogLevel::Info : LogLevel::Warning,
                          "MainExe",
                          "VersionList",
                          "",
                          "",
                          "",
                          versionWritten ? m_versionManager->GetLastVersionListPath() : "Failed to write version list");
        }
    }

    m_infrastructureInitialized = true;
    WriteStatus(m_databaseReady ? tr("Infrastructure ready") : tr("Infrastructure ready, database unavailable"));
}

void MainWindow::InitPages() {
    InitInfrastructure();

    WriteStatus(tr("Modules loaded"));
}

void MainWindow::ShowWaitPage(const QString& message) {
    if (!m_stack) {
        return;
    }
    if (!m_waitWidget) {
        m_uiComponents = GetUIComponents();
        if (m_uiComponents) {
            const QByteArray appDirBytes = QCoreApplication::applicationDirPath().toUtf8();
            m_uiComponents->Init(appDirBytes.constData());
            const QByteArray messageBytes = message.toUtf8();
            const QByteArray titleBytes = tr("MeyerScan").toUtf8();
            m_waitWidget = m_uiComponents->CreateWaitWidget(titleBytes.constData(), messageBytes.constData(), this);
        } else {
            m_waitWidget = new QLabel(message, this);
        }
        m_stack->addWidget(m_waitWidget);
    }
    SwitchToWidget(m_waitWidget, "Wait");
    show();
    QApplication::processEvents();
}

void MainWindow::HideWaitPage() {
    if (m_waitWidget && m_stack && m_stack->currentWidget() == m_waitWidget) {
        WriteStatus(tr("Ready"));
    }
}

void MainWindow::OnHomeEntryClicked(void* context, int entryId) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleHomeEntryClicked(entryId);
    }
}

void MainWindow::OnCaseAction(void* context, int actionId) {
    auto* window = static_cast<MainWindow*>(context);
    if (window) {
        window->HandleCaseAction(actionId);
    }
}

void MainWindow::HandleHomeEntryClicked(int entryId) {
    WriteUserAction("HomeEntryClicked", QString("Home entry clicked: %1").arg(entryId));

    switch (entryId) {
    case HomeEntryBrowse:
        ShowCase();
        break;
    case HomeEntryCreate:
    case HomeEntryPractice:
    case HomeEntrySettings:
    default:
        WriteStatus(tr("Home entry %1 is not implemented yet").arg(entryId));
        break;
    }
}

void MainWindow::HandleCaseAction(int actionId) {
    WriteUserAction("CaseAction", QString("Case action clicked: %1").arg(actionId));

    switch (actionId) {
    case CaseActionBackHome:
        ShowHome();
        break;
    case CaseActionSwitchTab:
        break;
    default:
        WriteStatus(tr("Case action %1 is not implemented yet").arg(actionId));
        break;
    }
}

QString MainWindow::ResolveDatabaseConfigPath() const {
    const QString deployedPath = QCoreApplication::applicationDirPath() + "/config/db_config.json";
    if (QFileInfo::exists(deployedPath)) {
        return deployedPath;
    }
    return QString();
}

QString MainWindow::ResolveLogDir() const {
    return QCoreApplication::applicationDirPath() + "/logs";
}

void MainWindow::ShowHome() {
    if (EnsureHomePage()) {
        SwitchToWidget(m_homeWidget, "Home");
        ReleaseInactivePage(m_homeWidget);
    }
}

void MainWindow::ShowCase() {
    if (EnsureCasePage()) {
        SwitchToWidget(m_caseWidget, "Case Management");
        ReleaseInactivePage(m_caseWidget);
    }
}

void MainWindow::PrepareForScanReconstruct() {
    WriteUserAction("PrepareScanReconstruct", "Release Case page before opening ScanReconstructStudio");
    ShowWaitPage(tr("Preparing scan reconstruct"));
    ReleaseCasePage();
    QTimer::singleShot(0, this, [this]() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        WriteUserAction("PrepareScanReconstructDone", "Case page release event processed");
    });
}

bool MainWindow::EnsureHomePage() {
    if (m_homeWidget) {
        return true;
    }
    m_home = GetHomeUI();
    if (!m_home) {
        WriteStatus(tr("HomeUI unavailable"));
        return false;
    }

    if (!m_homeInitialized) {
        m_homeInitialized = m_home->Init(m_databaseConfigPathUtf8.constData(), m_logDirUtf8.constData());
    }
    m_home->SetEntryCallback(&MainWindow::OnHomeEntryClicked, this);
    const bool settingsVisibleByConfig = m_config ? m_config->GetBool("feature.home.settingsVisible", true) : true;
    const bool settingsVisibleByPermission = m_permission ? m_permission->IsFeatureVisible("home.settings", true) : true;
    m_home->SetEntryVisible(HomeEntrySettings, settingsVisibleByConfig && settingsVisibleByPermission);

    m_homeWidget = m_home->CreateWidget(this);
    if (!m_homeWidget) {
        WriteStatus(tr("Home widget create failed"));
        return false;
    }
    m_stack->addWidget(m_homeWidget);
    WriteUserAction("PageCreate", "Home page created");
    return true;
}

bool MainWindow::EnsureCasePage() {
    if (m_caseWidget) {
        return true;
    }
    m_case = GetCaseUI();
    if (!m_case) {
        WriteStatus(tr("CaseUI unavailable"));
        return false;
    }

    if (!m_caseInitialized) {
        m_caseInitialized = m_case->Init(m_databaseConfigPathUtf8.constData(), m_logDirUtf8.constData());
    }
    m_case->SetActionCallback(&MainWindow::OnCaseAction, this);
    const bool backHomeVisibleByConfig = m_config ? m_config->GetBool("feature.case.backHomeVisible", true) : true;
    const bool backHomeVisibleByPermission = m_permission ? m_permission->IsFeatureVisible("case.backHome", true) : true;
    m_case->SetActionVisible(CaseActionBackHome, backHomeVisibleByConfig && backHomeVisibleByPermission);

    m_caseWidget = m_case->CreateWidget(this);
    if (!m_caseWidget) {
        WriteStatus(tr("Case widget create failed"));
        return false;
    }
    m_stack->addWidget(m_caseWidget);
    WriteUserAction("PageCreate", "Case page created");
    return true;
}

void MainWindow::ReleaseHomePage() {
    if (!m_homeWidget || !m_stack || m_stack->currentWidget() == m_homeWidget) {
        return;
    }
    WriteUserAction("PageRelease", "Home page released");
    m_stack->removeWidget(m_homeWidget);
    m_homeWidget->deleteLater();
    m_homeWidget = nullptr;
}

void MainWindow::ReleaseCasePage() {
    if (!m_caseWidget || !m_stack || m_stack->currentWidget() == m_caseWidget) {
        return;
    }
    WriteUserAction("PageRelease", "Case page released");
    m_stack->removeWidget(m_caseWidget);
    m_caseWidget->deleteLater();
    m_caseWidget = nullptr;
}

void MainWindow::ReleaseWaitPage() {
    if (!m_waitWidget || !m_stack || m_stack->currentWidget() == m_waitWidget) {
        return;
    }
    m_stack->removeWidget(m_waitWidget);
    m_waitWidget->deleteLater();
    m_waitWidget = nullptr;
}

void MainWindow::ReleaseInactivePage(QWidget* activeWidget) {
    if (activeWidget != m_homeWidget) {
        ReleaseHomePage();
    }
    if (activeWidget != m_caseWidget) {
        ReleaseCasePage();
    }
    if (activeWidget != m_waitWidget) {
        ReleaseWaitPage();
    }
}

void MainWindow::SwitchToWidget(QWidget* widget, const QString& pageName) {
    if (!widget || !m_stack) {
        return;
    }
    if (m_stack->currentWidget() == widget) {
        WriteStatus(pageName);
        return;
    }
    m_stack->setUpdatesEnabled(false);
    m_stack->setCurrentWidget(widget);
    m_stack->setUpdatesEnabled(true);
    m_stack->update();
    WriteUserAction("PageSwitch", QString("Switch to %1").arg(pageName));
    WriteStatus(pageName);
}

void MainWindow::WriteUserAction(const QString& operation, const QString& content) {
    ILogger* logger = GetLogger();
    if (!logger) {
        return;
    }
    const QByteArray operationBytes = operation.toUtf8();
    const QByteArray contentBytes = content.toUtf8();
    logger->Write(LogLevel::Info,
                  "MainExe",
                  operationBytes.constData(),
                  "",
                  "",
                  "",
                  contentBytes.constData());
}

void MainWindow::WriteStatus(const QString& text) {
    if (m_status) {
        m_status->setText(text);
    }
}
