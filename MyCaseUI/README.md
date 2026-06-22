# MeyerScan_CaseUI

CaseUI is the first Qt Widgets shell for patient and order management.

- Provides Patients and Orders tabs based on the help-center Browse module.
- Temporarily calls `MeyerScan_Database.dll` only to verify database initialization and connection during framework smoke testing.
- Resolves `MeyerScan_Logger.dll` at runtime with `QLibrary`; when a log directory is provided by the smoke host, CaseUI opens and closes that local logger session explicitly.
- Test host enables Qt High DPI attributes before `QApplication` and centers the window within the current screen's available geometry.
- UI strings use `QApplication::translate("CaseUI", "...")` so this module can later ship its own `.qm` files.
- Keeps CRUD and workflow rules outside the UI. Formal patient/order operations will move into CaseService, OrderService, DataExport, and OrderWorkflowService; CaseUI must not execute business SQL. Its direct Database call is a temporary framework health check only.
- Module change notes are recorded in `CHANGELOG.md`.

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_CaseUI.sln /p:Configuration=Release /p:Platform=x64 /m
```
