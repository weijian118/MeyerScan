#pragma once

#include <QWidget>

#ifdef MEYERSCAN_SENDUI_EXPORTS
#  define MEYERSCAN_SENDUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_SENDUI_API __declspec(dllimport)
#endif

// Stable action IDs reported by SendUI.
enum SendUIActionId {
    SendUIActionPrevious = 1,
    SendUIActionExport = 2,
    SendUIActionCompress = 3,
    SendUIActionEmailSend = 4,
    SendUIActionUpload = 5,
    SendUIActionFinish = 6,
};

// Public interface for the send page.
// The module displays context data and reports button actions only.
class MEYERSCAN_SENDUI_API ISendUI {
public:
    virtual ~ISendUI() = default;

    // appDirUtf8 is the MeyerScan.exe directory, logDirUtf8 is the unified logs directory.
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // Creates the root QWidget that will be attached to the workspace shell.
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // Accepts a UTF-8 JSON context and ignores unknown fields.
    virtual bool SetSessionContextJson(const char* contextJsonUtf8) = 0;

    // Reports actions through a plain C callback to keep the DLL boundary stable.
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;

    // Returns the code version string used by versionList.
    virtual const char* GetModuleVersion() const = 0;

    // Clears cached UI pointers and context state.
    virtual void Shutdown() = 0;
};

extern "C" MEYERSCAN_SENDUI_API ISendUI* GetSendUI();
