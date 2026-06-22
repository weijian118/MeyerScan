# MeyerScan CaseUI Change Log

## 2026-06-22

- Added module-level change log.
- Reconfirmed current Database use is temporary health-check only: `Init()` / `Connect()` for framework smoke testing.
- Formal CaseUI behavior should call CaseService, OrderService, DataExport, and OrderWorkflowService; it must not execute business SQL directly.
- Fixed Logger lifecycle boundary: CaseUI initializes and shuts down the smoke-host logger session when a log directory is provided, and uses `QLibrary::PreventUnloadHint` to avoid unload-order crashes.
- Calls Database `Disconnect()` / `Shutdown()` before closing the logger session during module shutdown.
- Updated `CaseUITest.exe` lifecycle to close and delete the top-level widget before module shutdown.

## 2026-06-18

- Added High DPI setup in the test host before `QApplication`.
- Added current-screen sizing and centering in `CaseUITest.exe`.
- Wrapped visible UI text with `QApplication::translate("CaseUI", "...")`.
- Updated README with DPI and translation notes.

## 2026-06-17

- Created Qt Widgets DLL framework and `CaseUITest.exe`.
- Added Patients and Orders tab shell.
- Connected Logger and Database for startup flow verification.
- Added Qt 5.6.3 runtime copy rules in PostBuild.
