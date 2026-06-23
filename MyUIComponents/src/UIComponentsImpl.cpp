#include "UIComponentsImpl.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopWidget>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {
const char* kModuleVersion = "MeyerScan_UIComponents v0.1.0 (2026-06-23)";
const double kDesignWidth = 1920.0;
const double kDesignHeight = 1080.0;
}

UIComponentsImpl& UIComponentsImpl::Instance() {
    static UIComponentsImpl instance;
    return instance;
}

bool UIComponentsImpl::Init(const char* /*appDirUtf8*/) {
    const QRect geometry = QApplication::desktop()->availableGeometry();
    m_scaleX = qMax(0.75, qMin(2.0, geometry.width() / kDesignWidth));
    m_scaleY = qMax(0.75, qMin(2.0, geometry.height() / kDesignHeight));
    return true;
}

double UIComponentsImpl::ScaleX() const {
    return m_scaleX;
}

double UIComponentsImpl::ScaleY() const {
    return m_scaleY;
}

QWidget* UIComponentsImpl::CreateWaitWidget(const char* titleUtf8, const char* messageUtf8, QWidget* parent) {
    auto* root = new QWidget(parent);
    root->setObjectName("MeyerScanWaitWidget");

    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(36, 36, 36, 36);
    layout->setSpacing(14);

    auto* title = CreatePageTitle(titleUtf8, root);
    auto* message = new QLabel(QString::fromUtf8(messageUtf8 ? messageUtf8 : ""), root);
    message->setAlignment(Qt::AlignCenter);
    message->setStyleSheet("color:#607080;font-size:15px;");

    auto* progress = new QProgressBar(root);
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->setFixedHeight(static_cast<int>(qMax(6.0, 8.0 * m_scaleY)));

    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(message);
    layout->addWidget(progress);
    layout->addStretch();
    return root;
}

QPushButton* UIComponentsImpl::CreatePrimaryButton(const char* textUtf8, QWidget* parent) {
    auto* button = new QPushButton(QString::fromUtf8(textUtf8 ? textUtf8 : ""), parent);
    button->setMinimumHeight(static_cast<int>(42 * m_scaleY));
    button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    button->setStyleSheet("QPushButton{background:#007d68;color:white;border:0;border-radius:4px;padding:8px 18px;}"
                          "QPushButton:hover{background:#009176;}"
                          "QPushButton:disabled{background:#d8d8d8;color:#888;}");
    return button;
}

QPushButton* UIComponentsImpl::CreateSecondaryButton(const char* textUtf8, QWidget* parent) {
    auto* button = new QPushButton(QString::fromUtf8(textUtf8 ? textUtf8 : ""), parent);
    button->setMinimumHeight(static_cast<int>(40 * m_scaleY));
    button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    button->setStyleSheet("QPushButton{background:#f6f8fa;color:#23313f;border:1px solid #cfd8dc;border-radius:4px;padding:8px 16px;}"
                          "QPushButton:hover{background:#edf2f5;}"
                          "QPushButton:disabled{color:#999;border-color:#ddd;}");
    return button;
}

QLineEdit* UIComponentsImpl::CreateLineEdit(const char* placeholderUtf8, QWidget* parent) {
    auto* edit = new QLineEdit(parent);
    edit->setPlaceholderText(QString::fromUtf8(placeholderUtf8 ? placeholderUtf8 : ""));
    edit->setMinimumHeight(static_cast<int>(36 * m_scaleY));
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    edit->setStyleSheet("QLineEdit{border:1px solid #cfd8dc;border-radius:4px;padding:6px 10px;background:white;color:#23313f;}"
                        "QLineEdit:focus{border-color:#007d68;}");
    return edit;
}

QComboBox* UIComponentsImpl::CreateComboBox(QWidget* parent) {
    auto* combo = new QComboBox(parent);
    combo->setMinimumHeight(static_cast<int>(36 * m_scaleY));
    combo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    combo->setStyleSheet("QComboBox{border:1px solid #cfd8dc;border-radius:4px;padding:5px 10px;background:white;color:#23313f;}"
                         "QComboBox:focus{border-color:#007d68;}");
    return combo;
}

QLabel* UIComponentsImpl::CreatePageTitle(const char* textUtf8, QWidget* parent) {
    auto* label = new QLabel(QString::fromUtf8(textUtf8 ? textUtf8 : ""), parent);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFont font = label->font();
    font.setPointSize(20);
    font.setBold(true);
    label->setFont(font);
    label->setStyleSheet("color:#23313f;");
    return label;
}

const char* UIComponentsImpl::GetModuleVersion() const {
    return kModuleVersion;
}

void UIComponentsImpl::Shutdown() {
    m_scaleX = 1.0;
    m_scaleY = 1.0;
}

extern "C" MEYERSCAN_UICOMPONENTS_API IUIComponents* GetUIComponents() {
    return &UIComponentsImpl::Instance();
}
