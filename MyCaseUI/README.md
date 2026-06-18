# MeyerScan_CaseUI

CaseUI is the first Qt Widgets shell for patient and order management.

- Provides Patients and Orders tabs based on the help-center Browse module.
- Calls `MeyerScan_Database.dll` to verify database initialization and connection.
- Loads `MeyerScan_Logger.dll` at runtime with `QLibrary`.
- Test host enables Qt High DPI attributes before `QApplication` and centers the window within the current screen's available geometry.
- UI strings use `QApplication::translate("CaseUI", "...")` so this module can later ship its own `.qm` files.
- Keeps CRUD and workflow rules outside the UI. Those will move into Case/Order/Workflow services.

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_CaseUI.sln /p:Configuration=Release /p:Platform=x64 /m
```
