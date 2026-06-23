#include "HomeUIImpl.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
const char* kModuleVersion = "MeyerScan_HomeUI v0.1.0 (2026-06-17)";
const char* kModuleName = "HomeUI";
}

HomeUIImpl& HomeUIImpl::Instance() {
    static HomeUIImpl instance;
    return instance;
}

bool HomeUIImpl::Init(const char* databaseConfigPath, const char* logDir) {
    LoadLogger(logDir);
    InitDatabase(databaseConfigPath);
    WriteLog(LogLevel::Info, "Init", m_lastStatus);
    return true;
}

void HomeUIImpl::SetEntryCallback(void (*callback)(void* context, int entryId), void* context) {
    m_entryCallback = callback;
    m_entryCallbackContext = context;
}

void HomeUIImpl::SetEntryVisible(int entryId, bool visible) {
    if (entryId > 0 && entryId < 5) {
        m_entryVisible[entryId] = visible;
    }
}

void HomeUIImpl::LoadLogger(const char* logDir) {
    if (m_logger) {
        return;
    }
    if (!logDir || !logDir[0]) {
        m_lastStatus = "Logger log directory not configured; continuing without log output";
        return;
    }

    m_loggerLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_loggerLibrary.setFileName("MeyerScan_Logger");
    if (!m_loggerLibrary.load()) {
        m_lastStatus = "Logger unavailable; continuing without log output";
        return;
    }

    auto getLogger = reinterpret_cast<GetLoggerFunc>(m_loggerLibrary.resolve("GetLogger"));
    if (!getLogger) {
        m_lastStatus = "GetLogger export not found";
        return;
    }

    m_logger = getLogger();
    if (!m_logger->Init(logDir, LogLevel::Info)) {
        m_logger = nullptr;
        m_lastStatus = "Logger init failed; continuing without log output";
        return;
    }
}

void HomeUIImpl::InitDatabase(const char* databaseConfigPath) {
    m_database = GetDatabase();
    if (!m_database) {
        m_lastStatus = "Database instance unavailable";
        return;
    }

    if (m_database->IsConnected()) {
        m_databaseConnected = true;
        m_lastStatus = QString("Database already connected, %1").arg(m_database->GetModuleVersion());
        return;
    }

    VoidResult initResult = m_database->Init(databaseConfigPath ? databaseConfigPath : "");
    if (initResult.IsError()) {
        m_lastStatus = QString("Database init failed: %1").arg(initResult.message ? initResult.message : "unknown");
        return;
    }

    VoidResult connectResult = m_database->Connect();
    if (connectResult.IsError()) {
        m_lastStatus = QString("Database connect failed: %1").arg(connectResult.message ? connectResult.message : "unknown");
        return;
    }

    m_databaseConnected = true;
    m_lastStatus = QString("Database connected, %1").arg(m_database->GetModuleVersion());
}

QWidget* HomeUIImpl::CreateWidget(QWidget* parent) {
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanHomeUIRoot");
    root->setMinimumSize(760, 480);

    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(18);

    auto* title = new QLabel(tr("MeyerScan"), root);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* subtitle = new QLabel(tr("Create, browse, practice, and settings entry shell"), root);
    subtitle->setStyleSheet("color:#555;");
    layout->addWidget(subtitle);

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    const QStringList names = {
        tr("Create"),
        tr("Browse"),
        tr("Practice"),
        tr("Settings")
    };
    const QStringList descs = {
        tr("Create patient and order information"),
        tr("Manage patients, orders, import/export and delete operations"),
        tr("Open scan practice without formal case data"),
        tr("Account, scan, calibration and common settings")
    };

    const int entryIds[] = {
        HomeEntryCreate,
        HomeEntryBrowse,
        HomeEntryPractice,
        HomeEntrySettings,
    };

    for (int i = 0; i < names.size(); ++i) {
        auto* button = new QPushButton(names[i] + "\n" + descs[i], root);
        button->setMinimumSize(220, 110);
        button->setStyleSheet("QPushButton{text-align:left;padding:14px;font-size:15px;} QPushButton:hover{background:#eef5ff;}");
        const int entryId = entryIds[i];
        QObject::connect(button, &QPushButton::clicked, [this, entryId]() { NotifyEntryClicked(entryId); });
        button->setVisible(IsEntryVisible(entryId));
        grid->addWidget(button, i / 2, i % 2);
    }

    layout->addLayout(grid);
    layout->addStretch();

    auto* status = new QLabel(QString("%1: %2").arg(tr("Status")).arg(m_lastStatus), root);
    status->setStyleSheet(m_databaseConnected ? "color:#1f7a3a;" : "color:#9a3412;");
    layout->addWidget(status);

    WriteLog(LogLevel::Info, "CreateWidget", "Home widget created");
    return root;
}

const char* HomeUIImpl::GetModuleVersion() const {
    return kModuleVersion;
}

void HomeUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "HomeUI shutdown");
    if (m_logger) {
        m_logger->Flush();
    }
    m_database = nullptr;
    m_databaseConnected = false;
    m_logger = nullptr;
    // QLibrary uses PreventUnloadHint to avoid process-exit unload-order issues.
}

void HomeUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) {
    if (!m_logger) {
        return;
    }
    QByteArray bytes = content.toUtf8();
    m_logger->Write(level, kModuleName, operation, "", "", "System", bytes.constData());
}

void HomeUIImpl::NotifyEntryClicked(int entryId) {
    WriteLog(LogLevel::Info, "EntryClicked", QString("Home entry clicked: %1").arg(entryId));
    if (m_entryCallback) {
        m_entryCallback(m_entryCallbackContext, entryId);
    }
}

bool HomeUIImpl::IsEntryVisible(int entryId) const {
    return entryId > 0 && entryId < 5 ? m_entryVisible[entryId] : true;
}

extern "C" MEYERSCAN_HOMEUI_API IHomeUI* GetHomeUI() {
    return &HomeUIImpl::Instance();
}
