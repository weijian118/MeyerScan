#include "CaseUIImpl.h"
#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace {
const char* kModuleVersion = "MeyerScan_CaseUI v0.1.0 (2026-06-17)";
const char* kModuleName = "CaseUI";
}

CaseUIImpl& CaseUIImpl::Instance() {
    static CaseUIImpl instance;
    return instance;
}

bool CaseUIImpl::Init(const char* databaseConfigPath, const char* logDir) {
    LoadLogger(logDir);
    InitDatabase(databaseConfigPath);
    WriteLog(LogLevel::Info, "Init", m_lastStatus);
    return true;
}

void CaseUIImpl::LoadLogger(const char* logDir) {
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

void CaseUIImpl::InitDatabase(const char* databaseConfigPath) {
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

QWidget* CaseUIImpl::CreateWidget(QWidget* parent) {
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanCaseUIRoot");
    root->setMinimumSize(900, 560);

    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* header = new QLabel(QApplication::translate("CaseUI", "Case Management"), root);
    QFont headerFont = header->font();
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    header->setFont(headerFont);
    layout->addWidget(header);

    auto* tabs = new QTabWidget(root);
    tabs->addTab(CreatePatientTab(tabs), QApplication::translate("CaseUI", "Patients"));
    tabs->addTab(CreateOrderTab(tabs), QApplication::translate("CaseUI", "Orders"));
    layout->addWidget(tabs, 1);

    auto* status = new QLabel(QString("%1: %2").arg(QApplication::translate("CaseUI", "Status")).arg(m_lastStatus), root);
    status->setStyleSheet(m_databaseConnected ? "color:#1f7a3a;" : "color:#9a3412;");
    layout->addWidget(status);

    WriteLog(LogLevel::Info, "CreateWidget", "Case widget created");
    return root;
}

QWidget* CaseUIImpl::CreatePatientTab(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);

    auto* toolbar = new QHBoxLayout();
    auto* search = new QLineEdit(page);
    search->setPlaceholderText(QApplication::translate("CaseUI", "Search patient name, phone or case id"));
    toolbar->addWidget(search, 1);

    const QStringList buttons = {
        QApplication::translate("CaseUI", "Import"),
        QApplication::translate("CaseUI", "Export"),
        QApplication::translate("CaseUI", "Delete"),
        QApplication::translate("CaseUI", "New Patient")
    };
    for (const QString& text : buttons) {
        toolbar->addWidget(new QPushButton(text, page));
    }
    layout->addLayout(toolbar);

    auto* table = new QTableWidget(0, 6, page);
    table->setHorizontalHeaderLabels({
        QApplication::translate("CaseUI", "Patient"),
        QApplication::translate("CaseUI", "Gender"),
        QApplication::translate("CaseUI", "Age"),
        QApplication::translate("CaseUI", "Case ID"),
        QApplication::translate("CaseUI", "Orders"),
        QApplication::translate("CaseUI", "Updated")
    });
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table, 1);
    return page;
}

QWidget* CaseUIImpl::CreateOrderTab(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);

    auto* toolbar = new QHBoxLayout();
    auto* search = new QLineEdit(page);
    search->setPlaceholderText(QApplication::translate("CaseUI", "Search order id, patient, doctor or type"));
    toolbar->addWidget(search, 1);

    const QStringList buttons = {
        QApplication::translate("CaseUI", "Import Order"),
        QApplication::translate("CaseUI", "Export Order"),
        QApplication::translate("CaseUI", "Open"),
        QApplication::translate("CaseUI", "Delete")
    };
    for (const QString& text : buttons) {
        toolbar->addWidget(new QPushButton(text, page));
    }
    layout->addLayout(toolbar);

    auto* table = new QTableWidget(0, 7, page);
    table->setHorizontalHeaderLabels({
        QApplication::translate("CaseUI", "Order ID"),
        QApplication::translate("CaseUI", "Patient"),
        QApplication::translate("CaseUI", "Type"),
        QApplication::translate("CaseUI", "Doctor"),
        QApplication::translate("CaseUI", "Status"),
        QApplication::translate("CaseUI", "Created"),
        QApplication::translate("CaseUI", "Data")
    });
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table, 1);
    return page;
}

const char* CaseUIImpl::GetModuleVersion() const {
    return kModuleVersion;
}

void CaseUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "CaseUI shutdown");
    m_database = nullptr;
    m_databaseConnected = false;
    m_logger = nullptr;
    if (m_loggerLibrary.isLoaded()) {
        m_loggerLibrary.unload();
    }
}

void CaseUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) {
    if (!m_logger) {
        return;
    }
    QByteArray bytes = content.toUtf8();
    m_logger->Write(level, kModuleName, operation, "", "", "System", bytes.constData());
}

extern "C" MEYERSCAN_CASEUI_API ICaseUI* GetCaseUI() {
    return &CaseUIImpl::Instance();
}
