# MeyerScan_HomeUI

HomeUI is the lightweight Qt Widgets entry module for MeyerScan.

- Shows the first-screen entries: Create, Browse, Practice, Settings.
- Calls `MeyerScan_Database.dll` to verify database initialization and connection.
- Loads `MeyerScan_Logger.dll` at runtime with `QLibrary`.
- Keeps business rules out of the UI. Order routing will be delegated to `OrderWorkflowService`.

Build:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\MeyerScan_HomeUI.sln /p:Configuration=Release /p:Platform=x64 /m
```
