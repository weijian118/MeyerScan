#pragma once

#include <QWidget>

#ifdef MEYERSCAN_DATAPROCESSUI_EXPORTS
#  define MEYERSCAN_DATAPROCESSUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_DATAPROCESSUI_API __declspec(dllimport)
#endif

// Stable action ids reported by the data-processing stage to its shell.
// These ids describe user intent; algorithm execution stays behind module boundaries.
enum DataProcessActionId {
    DataProcessActionPrevious = 1,
    DataProcessActionNext = 2,
    DataProcessActionScreenshot = 3,
    DataProcessActionEdit = 4,
    DataProcessActionMargin = 5,
    DataProcessActionUndercut = 6,
    DataProcessActionColor = 7,
    DataProcessActionMeasure = 8,
};

// Public interface of the data-processing module.
// The module owns the post-processing UI and its QVTK/OpenGL resources.
class MEYERSCAN_DATAPROCESSUI_API IDataProcessUI {
public:
    // Virtual destructor keeps polymorphic destruction safe across the ABI.
    virtual ~IDataProcessUI() = default;

    // Initializes paths and logger. appDirUtf8 must be the executable folder.
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // Creates the data-processing root widget.
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // Stores lightweight order/session context as UTF-8 JSON.
    virtual bool SetSessionContextJson(const char* contextJsonUtf8) = 0;

    // Registers a stable action callback.
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;

    // Called when the data-processing page becomes active.
    virtual void Activate() = 0;

    // Called before leaving this page; releases QVTK/OpenGL resources.
    virtual void DeactivateAndRelease() = 0;

    // Returns the module code version.
    virtual const char* GetModuleVersion() const = 0;

    // Shuts down the module.
    virtual void Shutdown() = 0;
};

// C ABI factory used by ScanReconstructStudio.exe.
extern "C" MEYERSCAN_DATAPROCESSUI_API IDataProcessUI* GetDataProcessUI();
