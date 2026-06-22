# MeyerScan_HomeUI

HomeUI is the lightweight Qt Widgets entry module for MeyerScan.

- Shows the first-screen entries: Create, Browse, Practice, Settings.
- Temporarily calls `MeyerScan_Database.dll` only to verify database initialization and connection during framework smoke testing.
- Resolves `MeyerScan_Logger.dll` at runtime with `QLibrary`; when a log directory is provided by the smoke host, HomeUI opens and closes that local logger session explicitly.
- Test host enables Qt High DPI attributes before `QApplication` and centers the window within the current screen's available geometry.
- UI strings use `QApplication::translate("HomeUI", "...")` so this module can later ship its own `.qm` files.
- Keeps business rules out of the UI. Formal order routing will be delegated to `Permission` and `OrderWorkflowService`; HomeUI must not execute business SQL. Its direct Database call is a temporary framework health check only.
- Module change notes are recorded in `CHANGELOG.md`.

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_HomeUI.sln /p:Configuration=Release /p:Platform=x64 /m
```
