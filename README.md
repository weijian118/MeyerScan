# MeyerScan — Oral Scanner Software

MeyerScan 是美亚光电口腔数字印模仪（mOS MyScan / mOS MyScan 5/5H）的配套 PC 软件。

## Repository Structure

```
MeyerScan/
├── MyLogger/         # MeyerScan_Logger.dll — 结构化日志基础设施
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
- Runtime: `/MT` static CRT linking
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

## GitHub

- **Repository**: https://github.com/weijian118/MeyerScan
- **Branch**: `master`
