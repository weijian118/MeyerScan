# MeyerScan — Oral Scanner Software

MeyerScan 是美亚光电口腔数字印模仪（mOS MyScan / mOS MyScan 5/5H）的配套 PC 软件。

## Repository Structure

```
MeyerScan/
├── MyLogger/         # MeyerScan_Logger.dll — 结构化日志基础设施
├── MyDatabase/       # MeyerScan_Database.dll — Qt SQL database infrastructure
├── MyHomeUI/         # MeyerScan_HomeUI.dll — Qt Widgets home entry shell
├── MyCaseUI/         # MeyerScan_CaseUI.dll — Qt Widgets case/order management shell
├── MyNetworkHelper/  # (pending)
├── CaseEntity/       # (pending)
├── CaseManager/      # (pending)
├── ...               # 其他模块按同样结构
└── README.md
```

## Build Requirements

Each module must compile directly in **both VS2015 and VSCode**:

| IDE | Method |
|-----|--------|
| **VS2015** | Open `.sln` → `F7` Build |
| **VSCode** | `Ctrl+Shift+B` → Select Debug/Release → MSBuild |

- Compiler: MSVC 14.0 (VS2015), C++14, x64
- Runtime: non-Qt infrastructure modules use `/MT`; Qt modules use `/MD`
- No CMake/Ninja required for daily development

## Modules

### MyLogger (v1.0.0)

Structured logging DLL with:
- 5 log levels: Debug / Info / Warning / Error / Fatal
- Thread-safe (background flush thread)
- Cross-process safe (Windows Named Mutex)
- Log rotation by date + file size
- ABI-safe interface (const char*, inline std::string and QString overloads)
- `MEYER_LOG_*` macros with null-safety

### MyDatabase (v1.1.0)

Qt SQL database infrastructure:
- MySQL / SQLite switch by config
- Connection, SQL execution, transaction and backup helpers
- Boundary rule: infrastructure only, no UI or business logic

### MyHomeUI (v0.1.0)

Qt Widgets home entry shell:
- Create / Browse / Practice / Settings entry placeholders
- Calls Database and dynamically loads Logger
- VS2015 Release x64 build and smoke run verified

### MyCaseUI (v0.1.0)

Qt Widgets case/order management shell:
- Patients and Orders tabs based on the help-center Browse module
- Calls Database and dynamically loads Logger
- VS2015 Release x64 build and smoke run verified

## GitHub

- **Repository**: https://github.com/weijian118/MeyerScan
- **Branch**: `master`
