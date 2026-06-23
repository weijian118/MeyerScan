#pragma once

#include <QWidget>

#ifdef MEYERSCAN_UICOMPONENTS_EXPORTS
#  define MEYERSCAN_UICOMPONENTS_API __declspec(dllexport)
#else
#  define MEYERSCAN_UICOMPONENTS_API __declspec(dllimport)
#endif

class QPushButton;
class QLabel;
class QComboBox;
class QLineEdit;

class MEYERSCAN_UICOMPONENTS_API IUIComponents {
public:
    virtual ~IUIComponents() = default;

    virtual bool Init(const char* appDirUtf8) = 0;
    virtual double ScaleX() const = 0;
    virtual double ScaleY() const = 0;
    virtual QWidget* CreateWaitWidget(const char* titleUtf8, const char* messageUtf8, QWidget* parent = nullptr) = 0;
    virtual QPushButton* CreatePrimaryButton(const char* textUtf8, QWidget* parent = nullptr) = 0;
    virtual QPushButton* CreateSecondaryButton(const char* textUtf8, QWidget* parent = nullptr) = 0;
    virtual QLineEdit* CreateLineEdit(const char* placeholderUtf8, QWidget* parent = nullptr) = 0;
    virtual QComboBox* CreateComboBox(QWidget* parent = nullptr) = 0;
    virtual QLabel* CreatePageTitle(const char* textUtf8, QWidget* parent = nullptr) = 0;
    virtual const char* GetModuleVersion() const = 0;
    virtual void Shutdown() = 0;
};

extern "C" MEYERSCAN_UICOMPONENTS_API IUIComponents* GetUIComponents();
