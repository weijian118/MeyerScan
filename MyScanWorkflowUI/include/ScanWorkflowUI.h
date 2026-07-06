#pragma once

#include <QWidget>

#ifdef MEYERSCAN_SCANWORKFLOWUI_EXPORTS
#  define MEYERSCAN_SCANWORKFLOWUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_SCANWORKFLOWUI_API __declspec(dllimport)
#endif

// Actions reported by the scan stage to the shell.
// The shell receives stable ids instead of depending on button objects.
enum ScanWorkflowActionId {
    ScanWorkflowActionPrevious = 1,
    ScanWorkflowActionNext = 2,
    ScanWorkflowActionStartPause = 3,
    ScanWorkflowActionComplete = 4,
    ScanWorkflowActionDelete = 5,
    ScanWorkflowActionJawModeChanged = 6,
    ScanWorkflowActionToolChanged = 7,
};

// Public interface of the scan workflow module.
// This module owns scan-stage UI and heavy VTK viewer resources.
class MEYERSCAN_SCANWORKFLOWUI_API IScanWorkflowUI {
public:
    // Virtual destructor keeps polymorphic destruction safe across the ABI.
    virtual ~IScanWorkflowUI() = default;

    // Initializes paths and logger. appDirUtf8 must be the executable folder.
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // Creates the scan-stage root widget.
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // Stores lightweight order/session context as UTF-8 JSON.
    virtual bool SetSessionContextJson(const char* contextJsonUtf8) = 0;

    // Registers a stable action callback.
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;

    // Called when the scan page becomes active.
    virtual void Activate() = 0;

    // Called before leaving this page; releases QVTK/OpenGL resources.
    virtual void DeactivateAndRelease() = 0;

    // Returns the module code version.
    virtual const char* GetModuleVersion() const = 0;

    // Shuts down the module.
    virtual void Shutdown() = 0;
};

// C ABI factory used by ScanReconstructStudio.exe.
extern "C" MEYERSCAN_SCANWORKFLOWUI_API IScanWorkflowUI* GetScanWorkflowUI();

