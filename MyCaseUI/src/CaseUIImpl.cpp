#include "CaseUIImpl.h"
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

void CaseUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    m_actionCallback = callback;
    m_actionCallbackContext = context;
}

void CaseUIImpl::SetActionVisible(int actionId, bool visible) {
    if (actionId == CaseActionBackHome) {
        m_backHomeVisible = visible;
    }
}

void CaseUIImpl::LoadLogger(const char* logDir) {
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

void CaseUIImpl::InitDatabase(const char* databaseConfigPath) {
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

QWidget* CaseUIImpl::CreateWidget(QWidget* parent) {
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanCaseUIRoot");
    root->setMinimumSize(900, 560);

    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* headerLayout = new QHBoxLayout();
    auto* backButton = new QPushButton(tr("Back Home"), root);
    backButton->setMinimumWidth(120);
    backButton->setVisible(IsActionVisible(CaseActionBackHome));
    QObject::connect(backButton, &QPushButton::clicked, [this]() {
        NotifyAction(CaseActionBackHome, "BackHome");
    });

    auto* header = new QLabel(tr("Case Management"), root);
    QFont headerFont = header->font();
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    header->setFont(headerFont);
    headerLayout->addWidget(header, 1);
    headerLayout->addWidget(backButton);
    layout->addLayout(headerLayout);

    auto* tabs = new QTabWidget(root);
    tabs->addTab(CreatePatientTab(tabs), tr("Patients"));
    tabs->addTab(CreateOrderTab(tabs), tr("Orders"));
    QObject::connect(tabs, &QTabWidget::currentChanged, [this, tabs](int index) {
        NotifyAction(CaseActionSwitchTab, QString("SwitchTab:%1").arg(tabs->tabText(index)));
    });
    layout->addWidget(tabs, 1);

    auto* status = new QLabel(QString("%1: %2").arg(tr("Status")).arg(m_lastStatus), root);
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
    search->setPlaceholderText(tr("Search patient name, phone or case id"));
    toolbar->addWidget(search, 1);

    const QStringList buttons = {
        tr("Import"),
        tr("Export"),
        tr("Delete"),
        tr("New Patient")
    };
    const int actionIds[] = {
        CaseActionImportPatient,
        CaseActionExportPatient,
        CaseActionDeletePatient,
        CaseActionNewPatient,
    };
    for (int i = 0; i < buttons.size(); ++i) {
        auto* button = new QPushButton(buttons[i], page);
        const int actionId = actionIds[i];
        const QString actionName = buttons[i];
        QObject::connect(button, &QPushButton::clicked, [this, actionId, actionName]() {
            NotifyAction(actionId, actionName);
        });
        toolbar->addWidget(button);
    }
    QObject::connect(search, &QLineEdit::returnPressed, [this]() {
        NotifyAction(CaseActionSearchPatient, "SearchPatient");
    });
    layout->addLayout(toolbar);

    auto* table = new QTableWidget(0, 6, page);
    table->setHorizontalHeaderLabels({
        tr("Patient"),
        tr("Gender"),
        tr("Age"),
        tr("Case ID"),
        tr("Orders"),
        tr("Updated")
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
    search->setPlaceholderText(tr("Search order id, patient, doctor or type"));
    toolbar->addWidget(search, 1);

    const QStringList buttons = {
        tr("Import Order"),
        tr("Export Order"),
        tr("Open"),
        tr("Delete")
    };
    const int actionIds[] = {
        CaseActionImportOrder,
        CaseActionExportOrder,
        CaseActionOpenOrder,
        CaseActionDeleteOrder,
    };
    for (int i = 0; i < buttons.size(); ++i) {
        auto* button = new QPushButton(buttons[i], page);
        const int actionId = actionIds[i];
        const QString actionName = buttons[i];
        QObject::connect(button, &QPushButton::clicked, [this, actionId, actionName]() {
            NotifyAction(actionId, actionName);
        });
        toolbar->addWidget(button);
    }
    QObject::connect(search, &QLineEdit::returnPressed, [this]() {
        NotifyAction(CaseActionSearchOrder, "SearchOrder");
    });
    layout->addLayout(toolbar);

    auto* table = new QTableWidget(0, 7, page);
    table->setHorizontalHeaderLabels({
        tr("Order ID"),
        tr("Patient"),
        tr("Type"),
        tr("Doctor"),
        tr("Status"),
        tr("Created"),
        tr("Data")
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
    if (m_logger) {
        m_logger->Flush();
    }
    m_database = nullptr;
    m_databaseConnected = false;
    m_logger = nullptr;
    // QLibrary uses PreventUnloadHint to avoid process-exit unload-order issues.
}

void CaseUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) {
    if (!m_logger) {
        return;
    }
    QByteArray bytes = content.toUtf8();
    m_logger->Write(level, kModuleName, operation, "", "", "System", bytes.constData());
}

void CaseUIImpl::NotifyAction(int actionId, const QString& actionName) {
    WriteLog(LogLevel::Info, "UserAction", QString("%1 (%2)").arg(actionName).arg(actionId));
    if (m_actionCallback) {
        m_actionCallback(m_actionCallbackContext, actionId);
    }
}

bool CaseUIImpl::IsActionVisible(int actionId) const {
    if (actionId == CaseActionBackHome) {
        return m_backHomeVisible;
    }
    return true;
}

extern "C" MEYERSCAN_CASEUI_API ICaseUI* GetCaseUI() {
    return &CaseUIImpl::Instance();
}
