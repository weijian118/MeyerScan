#pragma once

#include <QWidget>

#ifdef MEYERSCAN_HOMEUI_EXPORTS
#  define MEYERSCAN_HOMEUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_HOMEUI_API __declspec(dllimport)
#endif

class MEYERSCAN_HOMEUI_API IHomeUI {
public:
    virtual ~IHomeUI() = default;
    virtual bool Init(const char* databaseConfigPath, const char* logDir) = 0;
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;
    virtual const char* GetModuleVersion() const = 0;
    virtual void Shutdown() = 0;
};

extern "C" MEYERSCAN_HOMEUI_API IHomeUI* GetHomeUI();
