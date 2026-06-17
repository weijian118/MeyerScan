# MeyerScan_CaseUI

CaseUI is the first Qt Widgets shell for patient and order management.

- Provides Patients and Orders tabs based on the help-center Browse module.
- Calls `MeyerScan_Database.dll` to verify database initialization and connection.
- Loads `MeyerScan_Logger.dll` at runtime with `QLibrary`.
- Keeps CRUD and workflow rules outside the UI. Those will move into Case/Order/Workflow services.

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_CaseUI.sln /p:Configuration=Release /p:Platform=x64 /m
```
