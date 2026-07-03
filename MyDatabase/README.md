# MeyerScan Database 模块

[![Version](https://img.shields.io/badge/version-1.3.0-blue.svg)](https://github.com/weijian118/MeyerScan)
[![Platform](https://img.shields.io/badge/platform-Windows-green.svg)](https://github.com/weijian118/MeyerScan)
[![License](https://img.shields.io/badge/license-MIT-orange.svg)](LICENSE)

## 📖 项目简介

MeyerScan Database 模块（`MeyerScan_Database.dll`）是 MeyerScan 项目的数据库基础设施模块，提供统一的数据库访问抽象层。2026-07-03 起本模块已改为纯 C++ 实现，不再包含 QtCore、QtSql、QSqlDatabase、QSqlQuery、QString 或 QMutex。SQLite 通过运行时动态加载 `sqlite3.dll` 并调用 SQLite C API；MySQL 配置和枚举继续保留，原生 MySQL C API 待 SDK 进入工程后接入。本模块只做连接、SQL 执行、事务、备份等基础能力，不承载病例、订单、权限、UI 等业务逻辑。Qt UI/Service 模块如需 `QString` / `QJsonDocument` 便利能力，必须通过 `MeyerScan_DatabaseQtAdapter.dll` 转换后再访问本模块。

### ✨ 核心特性

| 特性 | 描述 |
|------|------|
| 🔀 **数据库类型保留** | 当前 SQLite 原生链路可用，MySQL 原生链路待 MySQL C API SDK 接入 |
| 🔒 **线程安全** | `std::mutex` 互斥锁保护所有操作 |
| 💾 **自动备份** | 支持数据库备份和时间戳管理 |
| 📝 **事务管理** | Begin/Commit/Rollback 完整支持 |
| ⚙️ **配置驱动** | JSON 配置文件，易于部署维护 |
| 📤 **查询 JSON 出口** | `ExecuteQueryJson()` 将通用 SELECT 结果转 UTF-8 JSON，供服务层读取 |
| 📊 **完整测试** | 9 个测试用例覆盖全部功能 |
| 📋 **标准化接口** | Result\<T\> / VoidResult 统一返回值规范 |
| 📝 **结构化日志** | 集成 MeyerScan_Logger.dll，动态加载 |

### 职责边界

| 允许 | 禁止 |
|------|------|
| 使用纯 C++、Windows API 和 sqlite3 C API 管理基础数据库能力 | 写入患者、订单、扫描方案等业务规则 |
| 执行上层 Service 传入的 SQL 或迁移脚本 | 被 UI 直接当作业务入口调用 |
| 管理连接、事务、备份和数据库类型 | 处理权限判断、云端上传、设备通信、界面展示 |
| 把 SELECT 行列结果通用转成 JSON | 决定 JSON 中哪些字段代表患者、订单、医生、诊所或技工所 |
| 返回 `Result<T>` / `VoidResult`，由调用方检查结果 | 返回或持有 QWidget/QObject UI 对象 |

`MyCaseManager/mysql.sql` is treated as a legacy schema reference only. Database does not auto-load it. Future schema creation and migration should be versioned and selected by ConfigCenter or service modules before calling `ExecuteScript()`.

Module change notes are recorded in `CHANGELOG.md`.

Database 是非界面基础设施模块，当前不依赖 Qt。CaseOrderService、RuntimeDataCenter、MainExe、CaseUI、SettingsUI 等 Qt 模块不得直接包含 `Database.h` 做 Qt 类型转换，应通过 `MyDatabaseQtAdapter` 集中转换。

`ExecuteQueryJson()` 仅为服务层补齐读取结果的基础能力。正式患者/订单、医生、诊所、技工所等数据读取必须由 `CaseOrderService` 等领域服务封装，UI 模块不得绕过服务层直接拼 SQL。

Maintenance notes added on 2026-06-22:

- Database does not use Qt internally in the current implementation.
- Callers must not place patient/order/workflow rules or UI decisions in this module.
- `Disconnect()` closes the native SQLite connection handle; Qt module cleanup is handled outside this DLL.
- `MyCaseManager/mysql.sql` is a legacy schema reference. Formal schema creation and migration should be versioned by ConfigCenter or service modules before calling `ExecuteScript()`.

---

## 📁 项目结构

```
F:\MeyerScan\MyDatabase\
├── 📂 include/              # 公共头文件
│   └── 📄 Database.h        # 接口定义（Result<T> 风格，详细中文注释）
├── 📂 src/                  # 实现源码
│   ├── 📄 DatabaseImpl.h    # 实现类头文件（含 Logger 动态加载）
│   └── 📄 DatabaseImpl.cpp  # 实现代码
├── 📂 test/                 # 测试程序
│   └── 📄 main.cpp          # 完整测试套件
├── 📂 config/               # 配置文件
│   └── 📄 db_config.json    # 数据库配置
├── 📂 bin/                  # 编译输出（已忽略）
├── 📂 backup/               # 备份目录（已忽略）
├── 📄 MeyerScan_Database.sln    # VS2015 解决方案
├── 📄 MeyerScan_Database.vcxproj    # DLL 项目
├── 📄 DatabaseTest.vcxproj      # 测试项目
├── 📄 README.md             # 项目文档
└── 📄 .gitignore            # Git 忽略配置
```

---

## 🛠️ 构建要求

| 工具/库 | 版本要求 |
|---------|----------|
| Visual Studio | 2015 (v140 toolset) |
| sqlite3.dll | x64 SQLite C API runtime，固定放在 `F:\MeyerScan\ThirdParty\SQLite\win-x64\sqlite3.dll` |
| Windows SDK | 8.1 |
| C++ 标准 | C++14 |

`sqlite3.dll` 必须与当前工程平台一致。当前 VS2015 工程按 x64 编译，禁止从旧 `MyCaseManager\SQLite\sqlitestudio311\SQLiteStudio` 目录复制 32 位 DLL，否则 `LoadLibraryA("sqlite3.dll")` 会失败。第三方 DLL 本体不提交仓库，只提交 `ThirdParty/SQLite/win-x64/README.md` 说明。

---

## 🚀 快速开始

### 1. 构建顺序

Database 模块依赖 Logger 模块，必须先构建 Logger：

```bash
# 步骤 1: 构建 Logger 模块
cd F:/MeyerScan/MyLogger
MSBuild MeyerScan_Logger.sln /p:Configuration=Release /p:Platform=x64

# 步骤 2: 构建 Database 模块
cd F:/MeyerScan/MyDatabase
MSBuild MeyerScan_Database.sln /p:Configuration=Release /p:Platform=x64
```

构建后请运行 `DatabaseTest.exe`。当前测试配置固定使用 SQLite，连接失败会直接计为失败，不再以“MySQL 可能未运行”为理由跳过；这可以防止 DLL 位数不匹配、缺少运行时或路径不可写被误判为测试通过。

### 2. 配置数据库

编辑 `config/db_config.json`：

```json
{
    "databaseType": "sqlite",
    "mysql": {
        "host": "127.0.0.1",
        "port": 3308,
        "service": "MSCANDB",
        "database": "mscan",
        "dataDir": "../MySQL/data/mscan"
    },
    "sqlitePath": "../Data/MeyerScanSQLite.db"
}
```

当前默认数据库类型是 SQLite，MySQL 仍作为可切换能力保留。运行时相对路径以 `db_config.json` 所在目录为基准解析。正式发布时配置文件位于 `MeyerScan.exe` 同级 `config/` 目录，因此示例中的 `../MySQL/...` 和 `../Data/...` 指向安装根目录下的对应文件夹。SQLite 首次连接前会自动创建数据库文件父目录。测试宿主会生成自己的 `bin/Release/config/db_config_test.json`，避免复用正式发布配置时出现路径语义混淆。

### 3. 构建项目

#### 方式一：命令行编译

```cmd
"C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe" MeyerScan_Database.sln /p:Configuration=Release /p:Platform=x64
```

#### 方式二：VSCode 任务

按 `Ctrl+Shift+B`，选择构建任务。

### 4. 运行测试

```cmd
cd bin\Release
DatabaseTest.exe
```

---

## 📚 API 文档

### 获取数据库实例

```cpp
#include "Database.h"

// 获取单例实例
IDatabase* db = GetDatabase();
```

### 基本使用流程

```cpp
// 1. 初始化（加载配置）
VoidResult result = db->Init("config/db_config.json");
if (result.IsError()) {
    return;
}

// 2. 连接数据库
result = db->Connect();
if (result.IsError()) {
    return;
}

// 3. 执行查询
Result<DbResult> queryResult = db->ExecuteQuery("SELECT * FROM user_tbl");
if (queryResult.IsSuccess()) {
    // 处理结果
}

// 4. 执行更新
Result<DbResult> updateResult = db->ExecuteUpdate(
    "UPDATE user_tbl SET name='张三' WHERE id=1");
if (updateResult.IsSuccess()) {
    printf("影响行数: %lld", updateResult.data.affectedRows);
}

// 5. 备份数据库
if (db->Backup("F:/backup").IsSuccess()) {
    printf("备份时间: %s", db->GetLastBackupTime());
}

// 6. 断开连接
db->Disconnect();
db->Shutdown();
```

### 事务操作

```cpp
if (db->BeginTransaction().IsSuccess()) {
    db->ExecuteUpdate("INSERT INTO table1 VALUES (1, 'a')");
    db->ExecuteUpdate("INSERT INTO table2 VALUES (2, 'b')");

    if (success) {
        db->Commit();
    } else {
        db->Rollback();
    }
}
```

---

## 📋 接口说明

### 返回值规范

所有可能失败的方法均返回 `VoidResult` 或 `Result<T>`，调用方必须检查 `IsSuccess()`：

| 返回类型 | 适用场景 | 示例方法 |
|---------|---------|---------|
| `VoidResult` | 不需要返回数据 | Init, Connect, Disconnect, Backup |
| `Result<DbResult>` | 需要返回执行结果数据 | ExecuteQuery, ExecuteUpdate |
| `bool` | 纯状态查询 | IsConnected |
| `enum` | 纯枚举类型查询 | GetDatabaseType |

### 主要接口方法

| 方法 | 说明 | 返回值 |
|------|------|--------|
| `Init(configPath)` | 初始化模块 | VoidResult |
| `Connect()` | 建立连接 | VoidResult |
| `Disconnect()` | 断开连接 | VoidResult |
| `IsConnected()` | 检查连接状态 | bool |
| `ExecuteQuery(sql)` | 执行查询 | Result\<DbResult\> |
| `ExecuteUpdate(sql)` | 执行更新 | Result\<DbResult\> |
| `BeginTransaction()` | 开始事务 | VoidResult |
| `Commit()` | 提交事务 | VoidResult |
| `Rollback()` | 回滚事务 | VoidResult |
| `Backup(path)` | 备份数据库 | VoidResult |
| `GetConfig()` | 获取配置 | const DbConfig& |
| `Shutdown()` | 关闭模块 | VoidResult |

详细接口文档请参考 [Database.h](include/Database.h)，包含完整的中文注释。

---

## 🧪 测试结果

### 测试套件概览

```
============================================
MeyerScan Database Module Test Suite
Database Interface: Result<T> / VoidResult
============================================

Test 1: 模块初始化测试
Test 2: 配置加载测试
Test 3: 数据库连接测试
Test 4: 查询执行测试
Test 5: 事务管理测试
Test 6: 备份功能测试
Test 7: 数据库类型切换测试
Test 8: 断开连接和清理测试
Test 9: 线程安全测试

============================================
测试汇总
============================================
通过测试数: 23
失败测试数: 0
总测试数: 23
所有测试通过 ✓
============================================
```

---

## 🔧 配置说明

### MySQL 配置

```json
{
    "databaseType": "mysql",
    "mysql": {
        "host": "127.0.0.1",      // MySQL 服务器地址
        "port": 3308,              // MySQL 端口
        "service": "MSCANDB",      // 服务名称（标识用）
        "database": "mscan",       // 数据库名称
        "dataDir": "../MySQL/data/mscan" // 相对 config 目录的 MySQL 数据目录，用于备份
    }
}
```

**注意**：MySQL 连接使用硬编码账号密码（admin/123456），后续会改为通过 ConfigCenter 加密存储。MySQL 备份源目录当前从 `mysql.dataDir` 读取，支持相对 `db_config.json` 所在目录解析；后续迁入 ConfigCenter，并结合安装目录生成默认值。
当前产品默认链路为 SQLite，只有客户部署或兼容旧库需要时才切换到 MySQL。

### SQLite 配置

```json
{
    "databaseType": "sqlite",
    "sqlitePath": "../Data/MeyerScanSQLite.db"
}
```

---

## 🔗 依赖关系

### 构建时依赖

- 无 Qt 构建时依赖。

### 运行时依赖（动态加载）

| 模块 | 加载方式 | 说明 |
|------|---------|------|
| **MeyerScan_Logger.dll** | 运行时 LoadLibrary | 结构化日志，无编译时链接依赖 |

### Logger 动态加载机制

Database 和 Logger 都按轻量基础模块处理，当前可使用静态 CRT；Database 仍采用运行时动态加载 Logger 的方式，主要是为了减少基础设施模块之间的编译期耦合，并避免 Database 必须链接 Logger.lib：

1. 首次需要写日志时调用 `LoadLibrary("MeyerScan_Logger.dll")`
2. 通过 `GetProcAddress` 获取 `GetLogger()` 函数指针
3. 缓存 `ILogger*` 实例，后续直接使用
4. 不主动调用 `Init()`；Logger 应由主程序在启动阶段最先初始化
5. 如果 Logger.dll 不存在或加载失败，日志操作静默跳过

详见 [DatabaseImpl.h](src/DatabaseImpl.h) 中的 Logger 集成说明。

---

## 🏗️ 架构设计

### 设计模式

- **单例模式**：全局唯一实例，通过 `GetDatabase()` 获取
- **工厂模式**：工厂函数封装对象创建
- **接口隔离**：`IDatabase` 接口与 `DatabaseImpl` 实现分离
- **Result 模式**：统一返回值包装，强制调用方检查错误

### 线程安全

- 所有公共方法使用 `std::lock_guard<std::mutex>` 加锁
- RAII 模式确保异常安全
- 单连接 + 锁模式，适合低并发场景

### 备份策略

- **MySQL**：使用 `robocopy` 复制 `mysql.dataDir` 指向的数据目录
- **SQLite**：使用 Windows `CopyFileA` 复制数据库文件
- 备份文件名包含时间戳（yyyyMMddHHmmss）

### 接口规范对齐

本模块遵循架构文档定义的接口规范：
- 所有接口返回 `Result<T>` / `VoidResult`
- 日志使用 MeyerScan_Logger.dll（动态加载）
- 注释全部使用中文
- 本模块无界面；若测试宿主或示例出现 UI 文案，仍遵守 `tr("English source text")` 规则。

---

## 📝 版本历史

### v1.1.0 (2026-06-17)

**接口规范对齐**
- ✅ 所有公共方法改为返回 `VoidResult` / `Result<DbResult>`
- ✅ 新增 `ErrorCode` 枚举和 `Result<T>` / `VoidResult` 模板
- ✅ 集成 MeyerScan_Logger.dll（动态加载，无编译时链接）
- ✅ `Init` 和 `Shutdown` 等接口返回 `VoidResult`

**技术改进**
- ✅ MySQL 密码硬编码添加 @todo 迁移注释
- ✅ MySQL 备份源目录改为从 `mysql.dataDir` 读取，后续迁移到 ConfigCenter
- ✅ 所有日志输出改用 Logger 模块（替代 qDebug）
- ✅ DatabaseTest 的 CRT 当时改为 `/MD` 匹配旧版 Qt Database.dll；2026-07-03 起 Database/DatabaseTest 已改为 `/MT`
- ✅ 预处理器定义添加 `MEYER_MODULE_NAME`
- ✅ 当时明确 Qt SQL 是过渡实现依赖；2026-07-03 起已替换为纯 C++ / sqlite3 C API 实现
- ✅ 修复 `SetDatabaseType()` 递归锁导致的死锁风险
- ✅ 修复 SQL 错误返回消息使用临时 Qt 字节数组导致的悬空指针风险
- ✅ Database 动态获取 Logger 实例时不再用空路径调用 `Init()`

### v1.0.0 (2026-06-17)

**基础功能**
- ✅ MySQL 数据库连接支持
- ✅ SQLite 数据库连接支持
- ✅ 线程安全访问机制
- ✅ 自动数据库备份功能
- ✅ JSON 配置文件解析
- ✅ 事务管理（Begin/Commit/Rollback）
- ✅ 完整的中文注释文档
- ✅ 9 个单元测试用例

---

## 📄 许可

本项目基于 MIT 许可证发布。
