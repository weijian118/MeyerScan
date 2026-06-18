#include "HomeUIImpl.h"
#include <QApplication>
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

void HomeUIImpl::LoadLogger(const char* logDir) {
    if (m_logger) {
        return;
    }

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
    if (m_logger && logDir && logDir[0] != '\0') {
        m_logger->Init(logDir, LogLevel::Info);
    }
}

void HomeUIImpl::InitDatabase(const char* databaseConfigPath) {
    m_database = GetDatabase();
    if (!m_database) {
        m_lastStatus = "Database instance unavailable";
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

    auto* title = new QLabel(QApplication::translate("HomeUI", "MeyerScan"), root);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* subtitle = new QLabel(QApplication::translate("HomeUI", "Create, browse, practice, and settings entry shell"), root);
    subtitle->setStyleSheet("color:#555;");
    layout->addWidget(subtitle);

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    const QStringList names = {
        QApplication::translate("HomeUI", "Create"),
        QApplication::translate("HomeUI", "Browse"),
        QApplication::translate("HomeUI", "Practice"),
        QApplication::translate("HomeUI", "Settings")
    };
    const QStringList descs = {
        QApplication::translate("HomeUI", "Create patient and order information"),
        QApplication::translate("HomeUI", "Manage patients, orders, import/export and delete operations"),
        QApplication::translate("HomeUI", "Open scan practice without formal case data"),
        QApplication::translate("HomeUI", "Account, scan, calibration and common settings")
    };

    for (int i = 0; i < names.size(); ++i) {
        auto* button = new QPushButton(names[i] + "\n" + descs[i], root);
        button->setMinimumSize(220, 110);
        button->setStyleSheet("QPushButton{text-align:left;padding:14px;font-size:15px;} QPushButton:hover{background:#eef5ff;}");
        grid->addWidget(button, i / 2, i % 2);
    }

    layout->addLayout(grid);
    layout->addStretch();

    auto* status = new QLabel(QString("%1: %2").arg(QApplication::translate("HomeUI", "Status")).arg(m_lastStatus), root);
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
    m_database = nullptr;
    m_databaseConnected = false;
    m_logger = nullptr;
    if (m_loggerLibrary.isLoaded()) {
        m_loggerLibrary.unload();
    }
}

void HomeUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) {
    if (!m_logger) {
        return;
    }
    QByteArray bytes = content.toUtf8();
    m_logger->Write(level, kModuleName, operation, "", "", "System", bytes.constData());
}

extern "C" MEYERSCAN_HOMEUI_API IHomeUI* GetHomeUI() {
    return &HomeUIImpl::Instance();
}
