#pragma once

#include "UIComponents.h"

class UIComponentsImpl : public IUIComponents {
public:
    static UIComponentsImpl& Instance();

    bool Init(const char* appDirUtf8) override;
    double ScaleX() const override;
    double ScaleY() const override;
    QWidget* CreateWaitWidget(const char* titleUtf8, const char* messageUtf8, QWidget* parent = nullptr) override;
    QPushButton* CreatePrimaryButton(const char* textUtf8, QWidget* parent = nullptr) override;
    QPushButton* CreateSecondaryButton(const char* textUtf8, QWidget* parent = nullptr) override;
    QLineEdit* CreateLineEdit(const char* placeholderUtf8, QWidget* parent = nullptr) override;
    QComboBox* CreateComboBox(QWidget* parent = nullptr) override;
    QLabel* CreatePageTitle(const char* textUtf8, QWidget* parent = nullptr) override;
    const char* GetModuleVersion() const override;
    void Shutdown() override;

private:
    UIComponentsImpl() = default;
    ~UIComponentsImpl() = default;
    UIComponentsImpl(const UIComponentsImpl&) = delete;
    UIComponentsImpl& operator=(const UIComponentsImpl&) = delete;

private:
    double m_scaleX = 1.0;
    double m_scaleY = 1.0;
};
