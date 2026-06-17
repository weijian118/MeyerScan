#pragma once

#include <QWidget>

#ifdef MEYERSCAN_CASEUI_EXPORTS
#  define MEYERSCAN_CASEUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_CASEUI_API __declspec(dllimport)
#endif

class MEYERSCAN_CASEUI_API ICaseUI {
public:
    virtual ~ICaseUI() = default;
    virtual bool Init(const char* databaseConfigPath, const char* logDir) = 0;
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;
    virtual const char* GetModuleVersion() const = 0;
    virtual void Shutdown() = 0;
};

extern "C" MEYERSCAN_CASEUI_API ICaseUI* GetCaseUI();
