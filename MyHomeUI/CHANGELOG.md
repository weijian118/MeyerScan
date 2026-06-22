# MeyerScan HomeUI Change Log

## 2026-06-22

- Added module-level change log.
- Reconfirmed current Database use is temporary health-check only: `Init()` / `Connect()` for framework smoke testing.
- Formal HomeUI behavior should call Permission and OrderWorkflowService, not business SQL or case/order services directly.
- Fixed Logger lifecycle boundary: HomeUI initializes and shuts down the smoke-host logger session when a log directory is provided, and uses `QLibrary::PreventUnloadHint` to avoid unload-order crashes.
- Calls Database `Disconnect()` / `Shutdown()` before closing the logger session during module shutdown.
- Updated `HomeUITest.exe` lifecycle to close and delete the top-level widget before module shutdown.

## 2026-06-18

- Added High DPI setup in the test host before `QApplication`.
- Added current-screen sizing and centering in `HomeUITest.exe`.
- Wrapped visible UI text with `QApplication::translate("HomeUI", "...")`.
- Updated README with DPI and translation notes.

## 2026-06-17

- Created Qt Widgets DLL framework and `HomeUITest.exe`.
- Added four entry shell: Create, Browse, Practice, Settings.
- Connected Logger and Database for startup flow verification.
- Added Qt 5.6.3 runtime copy rules in PostBuild.
