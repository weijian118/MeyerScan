#include "SendUIImpl.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QSizePolicy>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
namespace ModuleInfo {
const char* Name = "MeyerScan_SendUI";
const char* Version = "MeyerScan_SendUI v0.1.0 (2026-07-07)";
}

const char* kPageBackground = "#dfe4ea";
const char* kPanelBackground = "#ffffff";
const char* kPrimaryColor = "#007d68";
const char* kBorderColor = "#d8e0e7";
}

SendUIImpl& SendUIImpl::Instance() {
    static SendUIImpl instance;
    return instance;
}

bool SendUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }

    LoadUIComponents();
    WriteLog(LogLevel::Info, "Init", "Send UI initialized");
    return true;
}

QWidget* SendUIImpl::CreateWidget(QWidget* parent) {
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanSendUIRoot");
    root->setMinimumSize(920, 560);
    root->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->setStyleSheet(QString(
        "#MeyerScanSendUIRoot{background:%1;}"
        "QLabel{color:#1f2b36;font-size:13px;}"
        "QLabel[sectionTitle=\"true\"]{font-size:20px;font-weight:600;color:#132536;}"
        "QLabel[fieldLabel=\"true\"]{color:#1f2b36;font-size:13px;font-weight:500;}"
        "QLineEdit,QComboBox,QTextEdit{border:1px solid %2;border-radius:5px;background:#ffffff;color:#162433;font-size:14px;}"
        "QLineEdit,QComboBox{min-height:34px;padding:6px 10px;}"
        "QTextEdit{padding:8px 10px;}"
        "QLineEdit[readonly=\"true\"]{background:#f2f4f6;color:#1f2b36;}"
        "QPushButton{border:1px solid %3;border-radius:5px;background:#ffffff;color:%3;min-height:38px;padding:6px 16px;font-size:14px;}"
        "QPushButton:hover{background:#edf8f5;}"
        "QPushButton[primary=\"true\"]{background:%3;color:#ffffff;font-weight:600;}"
        "QPushButton[primary=\"true\"]:hover{background:#008b72;}"
        "QFrame[card=\"true\"]{background:%4;border:1px solid %2;border-radius:6px;}"
        "QFrame[divider=\"true\"]{background:#e8edf1;}"
    ).arg(kPageBackground, kBorderColor, kPrimaryColor, kPanelBackground));

    auto* outerLayout = new QVBoxLayout(root);
    outerLayout->setContentsMargins(22, 20, 22, 22);
    outerLayout->setSpacing(0);

    auto* card = new QFrame(root);
    card->setObjectName("SendUICard");
    card->setProperty("card", true);
    card->setMaximumWidth(900);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(38, 28, 38, 30);
    cardLayout->setSpacing(22);
    cardLayout->addWidget(CreateCaseInfoSection(card));
    cardLayout->addWidget(CreateSendActionSection(card));

    outerLayout->addStretch(1);
    outerLayout->addWidget(card, 0, Qt::AlignHCenter);
    outerLayout->addStretch(1);

    m_root = root;
    ApplyContextToWidgets();
    WriteLog(LogLevel::Info, "CreateWidget", "Send UI widget created");
    return root;
}

bool SendUIImpl::SetSessionContextJson(const char* contextJsonUtf8) {
    m_contextJson = QByteArray(contextJsonUtf8 ? contextJsonUtf8 : "");
    m_contextObject = QJsonObject();

    if (!m_contextJson.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(m_contextJson, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            WriteLog(LogLevel::Warning,
                     "SetSessionContextJson",
                     QString("Invalid context json: %1").arg(parseError.errorString()));
            return false;
        }
        m_contextObject = document.object();
    }

    ApplyContextToWidgets();
    WriteLog(LogLevel::Info,
             "SetSessionContextJson",
             QString("Session context bytes: %1").arg(m_contextJson.size()));
    return true;
}

void SendUIImpl::SetActionCallback(void (*callback)(void* context, int actionId), void* context) {
    m_actionCallback = callback;
    m_actionContext = context;
}

const char* SendUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

void SendUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "Send UI shutdown");
    if (m_logger) {
        m_logger->Flush();
    }
    m_root = nullptr;
    m_nameEdit = nullptr;
    m_doctorEdit = nullptr;
    m_orderNoEdit = nullptr;
    m_orderTypeEdit = nullptr;
    m_clinicEdit = nullptr;
    m_dataFormatCombo = nullptr;
    m_noteEdit = nullptr;
    m_contextJson.clear();
    m_contextObject = QJsonObject();
    m_uiComponents = nullptr;
    m_logger = nullptr;
    m_actionCallback = nullptr;
    m_actionContext = nullptr;
}

void SendUIImpl::LoadUIComponents() {
    if (m_uiComponents) {
        return;
    }

    m_uiComponentsLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    m_uiComponentsLibrary.setFileName("MeyerScan_UIComponents");
    if (!m_uiComponentsLibrary.load()) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "UIComponents unavailable; fallback to local styles");
        return;
    }

    auto getUIComponents = reinterpret_cast<GetUIComponentsFunc>(m_uiComponentsLibrary.resolve("GetUIComponents"));
    if (!getUIComponents) {
        WriteLog(LogLevel::Warning, "LoadUIComponents", "GetUIComponents export not found");
        return;
    }

    m_uiComponents = getUIComponents();
    if (m_uiComponents) {
        const QByteArray appDirBytes = m_appDir.isEmpty()
            ? QCoreApplication::applicationDirPath().toUtf8()
            : m_appDir;
        m_uiComponents->Init(appDirBytes.constData());
    }
}

QWidget* SendUIImpl::CreateCaseInfoSection(QWidget* parent) {
    auto* section = new QWidget(parent);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto* title = new QLabel(tr("Case Information"), section);
    title->setProperty("sectionTitle", true);
    layout->addWidget(title);

    auto* divider = new QFrame(section);
    divider->setProperty("divider", true);
    divider->setFixedHeight(1);
    layout->addWidget(divider);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(14);

    m_nameEdit = CreateReadOnlyLineEdit(section);
    m_doctorEdit = CreateReadOnlyLineEdit(section);
    m_orderNoEdit = CreateReadOnlyLineEdit(section);
    m_orderTypeEdit = CreateReadOnlyLineEdit(section);
    m_clinicEdit = CreateReadOnlyLineEdit(section);
    m_dataFormatCombo = m_uiComponents ? m_uiComponents->CreateComboBox(section) : new QComboBox(section);
    m_dataFormatCombo->addItem("STL");
    m_dataFormatCombo->addItem("PLY");
    m_dataFormatCombo->addItem("OBJ");

    grid->addWidget(CreateFieldLabel(tr("Name"), section), 0, 0);
    grid->addWidget(CreateFieldLabel(tr("Doctor"), section), 0, 1);
    grid->addWidget(m_nameEdit, 1, 0);
    grid->addWidget(m_doctorEdit, 1, 1);
    grid->addWidget(CreateFieldLabel(tr("Order No."), section), 2, 0);
    grid->addWidget(CreateFieldLabel(tr("Order Type"), section), 2, 1);
    grid->addWidget(m_orderNoEdit, 3, 0);
    grid->addWidget(m_orderTypeEdit, 3, 1);
    grid->addWidget(CreateFieldLabel(tr("Clinic"), section), 4, 0);
    grid->addWidget(CreateFieldLabel(tr("Data Format"), section), 4, 1);
    grid->addWidget(m_clinicEdit, 5, 0);
    grid->addWidget(m_dataFormatCombo, 5, 1);
    grid->addWidget(CreateFieldLabel(tr("Note"), section), 6, 0, 1, 2);

    m_noteEdit = m_uiComponents ? m_uiComponents->CreateTextEdit(section) : new QTextEdit(section);
    m_noteEdit->setMinimumHeight(78);
    grid->addWidget(m_noteEdit, 7, 0, 1, 2);

    layout->addLayout(grid);
    return section;
}

QWidget* SendUIImpl::CreateSendActionSection(QWidget* parent) {
    auto* section = new QWidget(parent);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto* title = new QLabel(tr("Send"), section);
    title->setProperty("sectionTitle", true);
    layout->addWidget(title);

    auto* divider = new QFrame(section);
    divider->setProperty("divider", true);
    divider->setFixedHeight(1);
    layout->addWidget(divider);

    auto* actionGrid = new QGridLayout();
    actionGrid->setContentsMargins(0, 0, 0, 0);
    actionGrid->setHorizontalSpacing(16);
    actionGrid->setVerticalSpacing(12);

    auto* localLabel = CreateFieldLabel(tr("Local"), section);
    auto* labLabel = CreateFieldLabel(tr("Lab"), section);
    auto* exportButton = CreateActionButton(tr("Export"), MeyerButtonRoleSecondary, section);
    auto* compressButton = CreateActionButton(tr("Compress"), MeyerButtonRoleSecondary, section);
    auto* emailButton = CreateActionButton(tr("Email Send"), MeyerButtonRoleSecondary, section);
    auto* uploadButton = CreateActionButton(tr("Upload"), MeyerButtonRoleSecondary, section);

    QObject::connect(exportButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionExport, "Export");
    });
    QObject::connect(compressButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionCompress, "Compress");
    });
    QObject::connect(emailButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionEmailSend, "EmailSend");
    });
    QObject::connect(uploadButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionUpload, "Upload");
    });

    actionGrid->addWidget(localLabel, 0, 0);
    actionGrid->addWidget(exportButton, 1, 0);
    actionGrid->addWidget(compressButton, 1, 1);
    actionGrid->addWidget(labLabel, 2, 0);
    actionGrid->addWidget(emailButton, 3, 0);
    actionGrid->addWidget(uploadButton, 3, 1);
    actionGrid->setColumnStretch(0, 1);
    actionGrid->setColumnStretch(1, 1);
    layout->addLayout(actionGrid);

    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 10, 0, 0);
    bottomLayout->setSpacing(12);
    auto* previousButton = CreateActionButton(tr("Previous"), MeyerButtonRoleSecondary, section);
    auto* finishButton = CreateActionButton(tr("Finish"), MeyerButtonRolePrimary, section);
    finishButton->setProperty("primary", true);
    finishButton->setMinimumWidth(190);

    QObject::connect(previousButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionPrevious, "Previous");
    });
    QObject::connect(finishButton, &QPushButton::clicked, [this]() {
        EmitAction(SendUIActionFinish, "Finish");
    });

    bottomLayout->addWidget(previousButton, 0, Qt::AlignLeft);
    bottomLayout->addStretch(1);
    bottomLayout->addWidget(finishButton, 0, Qt::AlignRight);
    layout->addLayout(bottomLayout);
    return section;
}

QLineEdit* SendUIImpl::CreateReadOnlyLineEdit(QWidget* parent) {
    auto* edit = m_uiComponents ? m_uiComponents->CreateLineEdit("", parent) : new QLineEdit(parent);
    edit->setReadOnly(true);
    edit->setProperty("readonly", true);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return edit;
}

QLabel* SendUIImpl::CreateFieldLabel(const QString& text, QWidget* parent) {
    const QByteArray textBytes = text.toUtf8();
    QLabel* label = m_uiComponents
        ? m_uiComponents->CreateFieldLabel(textBytes.constData(), parent)
        : new QLabel(text, parent);
    label->setProperty("fieldLabel", true);
    return label;
}

QPushButton* SendUIImpl::CreateActionButton(const QString& text, int role, QWidget* parent) {
    const QByteArray textBytes = text.toUtf8();
    QPushButton* button = m_uiComponents
        ? m_uiComponents->CreateButton(role, MeyerButtonContentTextOnly, textBytes.constData(), "", parent)
        : new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

void SendUIImpl::ApplyContextToWidgets() {
    if (!m_root) {
        return;
    }

    const QJsonObject patient = m_contextObject.value("patient").toObject();
    const QJsonObject order = m_contextObject.value("order").toObject();
    const QJsonObject clinic = m_contextObject.value("clinic").toObject();

    if (m_nameEdit) {
        m_nameEdit->setText(ReadString(patient, "name", "Test Patient"));
    }
    if (m_doctorEdit) {
        m_doctorEdit->setText(ReadString(order, "doctor", "Default Doctor"));
    }
    if (m_orderNoEdit) {
        m_orderNoEdit->setText(ReadString(order, "orderId", ReadString(order, "id", "LOCAL_ORDER")));
    }
    if (m_orderTypeEdit) {
        m_orderTypeEdit->setText(ReadString(order, "caseType", ReadString(order, "type", "restoration")));
    }
    if (m_clinicEdit) {
        m_clinicEdit->setText(ReadString(clinic, "name", ReadString(order, "clinic", "Default Clinic")));
    }
    if (m_dataFormatCombo) {
        const QString format = ReadString(order, "dataFormat", "STL");
        const int index = m_dataFormatCombo->findText(format, Qt::MatchFixedString);
        if (index >= 0) {
            m_dataFormatCombo->setCurrentIndex(index);
        }
    }
    if (m_noteEdit) {
        m_noteEdit->setPlainText(ReadString(order, "note", QString()));
    }
}

QString SendUIImpl::ReadString(const QJsonObject& object, const char* key, const QString& fallback) const {
    if (!key || !key[0]) {
        return fallback;
    }
    const QJsonValue value = object.value(QString::fromLatin1(key));
    if (value.isString()) {
        const QString text = value.toString().trimmed();
        return text.isEmpty() ? fallback : text;
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'f', 0);
    }
    return fallback;
}

void SendUIImpl::EmitAction(int actionId, const QString& operation) {
    WriteLog(LogLevel::Info, operation.toUtf8().constData(), QString("Send UI action: %1").arg(actionId));
    if (m_actionCallback) {
        m_actionCallback(m_actionContext, actionId);
    }
}

void SendUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    if (!m_logger) {
        return;
    }
    const QByteArray bytes = content.toUtf8();
    m_logger->Write(level, ModuleInfo::Name, operation ? operation : "", "", "", "", bytes.constData());
}

extern "C" MEYERSCAN_SENDUI_API ISendUI* GetSendUI() {
    return &SendUIImpl::Instance();
}

extern "C" MEYERSCAN_SENDUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
