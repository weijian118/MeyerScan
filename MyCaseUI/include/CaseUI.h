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
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;
    virtual void SetActionVisible(int actionId, bool visible) = 0;
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;
    virtual const char* GetModuleVersion() const = 0;
    virtual void Shutdown() = 0;
};

enum CaseActionId {
    CaseActionBackHome = 1,
    CaseActionSwitchTab = 2,
    CaseActionImportPatient = 101,
    CaseActionExportPatient = 102,
    CaseActionDeletePatient = 103,
    CaseActionNewPatient = 104,
    CaseActionSearchPatient = 105,
    CaseActionImportOrder = 201,
    CaseActionExportOrder = 202,
    CaseActionOpenOrder = 203,
    CaseActionDeleteOrder = 204,
    CaseActionSearchOrder = 205,
};

extern "C" MEYERSCAN_CASEUI_API ICaseUI* GetCaseUI();
