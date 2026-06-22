# MeyerScan Logger Change Log

## 2026-06-22

- Added module-level change log.
- Current boundary remains unchanged: Logger is a low-dependency infrastructure DLL and must not depend on Qt or business modules.
- Keep public ABI based on `const char*`; `std::string` and `QString` helpers stay inline in the caller.
- Guarded `Write()` so calls before `Init()` or after `Shutdown()` are dropped safely. This prevents late module shutdown logs from reusing a closed logger session.

## 2026-06-17

- Added `GetModuleVersion()` and `Version.rc`.
- Kept `/MT` static CRT to allow Logger to load before Qt and other modules.
- Verified as the logging template for later modules.

## 2026-06-15

- Completed VS2015 and VSCode/MSBuild build setup.
- Verified `LoggerTest.exe` including multi-thread stress logging.
- Pushed the module to GitHub under `MyLogger/`.
