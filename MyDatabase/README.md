# MeyerScan Database 模块

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/weijian118/MeyerScan_Database)
[![Platform](https://img.shields.io/badge/platform-Windows-green.svg)](https://github.com/weijian118/MeyerScan_Database)
[![License](https://img.shields.io/badge/license-MIT-orange.svg)](LICENSE)

## 📖 项目简介

MeyerScan Database 模块（`MeyerScan_Database.dll`）是 MeyerScan 项目的数据库基础设施模块，提供统一的数据访问抽象层。支持 MySQL 和 SQLite 双数据库，具备线程安全、自动备份、事务管理等企业级特性。

### ✨ 核心特性

| 特性 | 描述 |
|------|------|
| 🔀 **双数据库支持** | MySQL 和 SQLite 无缝切换 |
| 🔒 **线程安全** | QMutex 互斥锁保护所有操作 |
| 💾 **自动备份** | 支持数据库备份和时间戳管理 |
| 📝 **事务管理** | Begin/Commit/Rollback 完整支持 |
| ⚙️ **配置驱动** | JSON 配置文件，易于部署维护 |
| 📊 **完整测试** | 18 个测试用例全部通过 |

---

## 📁 项目结构

```
F:\MeyerScan\MyDatabase\
├── 📂 include/              # 公共头文件
│   └── 📄 Database.h        # 接口定义（详细中文注释）
├── 📂 src/                  # 实现源码
│   ├── 📄 DatabaseImpl.h    # 实现类头文件
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
| Qt | 5.6.3 (msvc2015_64) |
| Windows SDK | 8.1 |
| C++ 标准 | C++14 |

---

## 🚀 快速开始

### 1. 克隆仓库

```bash
git clone https://github.com/weijian118/MeyerScan_Database.git
cd MeyerScan_Database
```

### 2. 配置数据库

编辑 `config/db_config.json`：

```json
{
    "databaseType": "mysql",
    "mysql": {
        "host": "127.0.0.1",
        "port": 3308,
        "service": "MSCANDB",
        "database": "mscan"
    },
    "sqlitePath": "F:/MeyerScan/MyDatabase/data/MeyerScanSQLite.db"
}
```

### 3. 编译项目

#### 方式一：命令行编译

```cmd
"C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe" MeyerScan_Database.sln /p:Configuration=Release /p:Platform=x64
```

#### 方式二：VSCode 任务

按 `Ctrl+Shift+B`，选择构建任务。

### 4. 运行测试

```cmd
cd bin\Debug
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
if (!db->Init("config/db_config.json")) {
    qDebug() << "初始化失败";
    return;
}

// 2. 连接数据库
if (!db->Connect()) {
    qDebug() << "连接失败";
    return;
}

// 3. 执行查询
DbResult result = db->ExecuteQuery("SELECT * FROM user_tbl");
if (result.success) {
    qDebug() << "查询成功";
}

// 4. 执行更新
result = db->ExecuteUpdate("UPDATE user_tbl SET name='张三' WHERE id=1");
qDebug() << "影响行数:" << result.affectedRows;

// 5. 备份数据库
if (db->Backup("F:/backup")) {
    qDebug() << "备份时间:" << db->GetLastBackupTime();
}

// 6. 断开连接
db->Disconnect();
db->Shutdown();
```

### 事务操作

```cpp
if (db->BeginTransaction()) {
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

### 主要接口方法

| 方法 | 说明 | 返回值 |
|------|------|--------|
| `Init(configPath)` | 初始化模块 | bool |
| `Connect()` | 建立连接 | bool |
| `Disconnect()` | 断开连接 | void |
| `IsConnected()` | 检查连接状态 | bool |
| `ExecuteQuery(sql)` | 执行查询 | DbResult |
| `ExecuteUpdate(sql)` | 执行更新 | DbResult |
| `BeginTransaction()` | 开始事务 | bool |
| `Commit()` | 提交事务 | bool |
| `Rollback()` | 回滚事务 | bool |
| `Backup(path)` | 备份数据库 | bool |
| `GetConfig()` | 获取配置 | DbConfig |
| `Shutdown()` | 关闭模块 | void |

详细接口文档请参考 [Database.h](include/Database.h)，包含完整的中文注释。

---

## 🧪 测试结果

### 测试套件概览

```
===========================================
MeyerScan Database Module Test Suite
===========================================

Test 1: 模块初始化测试
Test 2: 配置加载测试
Test 3: 数据库连接测试
Test 4: 查询执行测试
Test 5: 事务管理测试
Test 6: 备份功能测试
Test 7: 数据库类型切换测试
Test 8: 断开连接和清理测试
Test 9: 线程安全测试

===========================================
测试汇总
===========================================
通过测试数: 18
失败测试数: 0
总测试数: 18
所有测试通过 ✓
===========================================
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
        "database": "mscan"        // 数据库名称
    }
}
```

**注意**：MySQL 连接使用硬编码账号密码（admin/123456）。

### SQLite 配置

```json
{
    "databaseType": "sqlite",
    "sqlitePath": "F:/path/to/database.db"
}
```

---

## 📦 部署清单

运行时需要以下 DLL 文件：

| 文件 | 说明 |
|------|------|
| `MeyerScan_Database.dll` | 主模块 |
| `Qt5Core.dll` | Qt 核心库 |
| `Qt5Sql.dll` | Qt SQL 模块 |
| `plugins/sqldrivers/qsqlmysql.dll` | MySQL 驱动 |
| `plugins/sqldrivers/qsqlite.dll` | SQLite 驱动 |
| `libmysql.dll` | MySQL 客户端库 |
| `vcruntime140.dll` | VC++ 运行时 |

---

## 🏗️ 架构设计

### 设计模式

- **单例模式**：全局唯一实例，通过 `GetDatabase()` 获取
- **工厂模式**：工厂函数封装对象创建
- **接口隔离**：`IDatabase` 接口与 `DatabaseImpl` 实现分离

### 线程安全

- 所有公共方法使用 `QMutexLocker` 加锁
- RAII 模式确保异常安全
- 单连接 + 锁模式，适合低并发场景

### 备份策略

- **MySQL**：使用 `robocopy` 复制数据目录
- **SQLite**：使用 `QFile::copy` 复制文件
- 备份文件名包含时间戳（yyyyMMddHHmmss）

---

## 📝 版本历史

### v1.0.0 (2026-06-17)

**新增功能**
- ✅ MySQL 数据库连接支持
- ✅ SQLite 数据库连接支持
- ✅ 线程安全访问机制
- ✅ 自动数据库备份功能
- ✅ JSON 配置文件解析
- ✅ 事务管理（Begin/Commit/Rollback）
- ✅ 完整的中文注释文档
- ✅ 18 个单元测试用例

**技术细节**
- Qt 5.6.3 Sql 模块
- VS2015 (v140 toolset) 编译
- x64 平台支持

---

## 🤝 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建 Pull Request

---

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

---

## 📧 联系方式

- 项目地址: [https://github.com/weijian118/MeyerScan_Database](https://github.com/weijian118/MeyerScan_Database)
- 作者: weijian118
- 邮箱: weijian118@github.com

---

## 🙏 致谢

- Qt Framework - 跨平台应用框架
- MySQL - 开源关系型数据库
- SQLite - 嵌入式数据库引擎
