#pragma once

#include "SettingsUIImpl.h"

#include "Calibration3DUI.h"
#include "CalibrationColorUI.h"
#include "MeyerQtModuleUtils.h"

#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPoint>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include <cstring>

// =============================================================================
// 文件说明:
//   设置模块 UI 实现，负责创建设置主界面、读取运行时只读数据快照、
//   管理校准入口可用性，并把确认/应用/关闭等动作回调给 MainExe。
//
// 模块边界:
//   - 不直接拼 SQL，不直接包含 Database.h。
//   - 医生/诊所/技工所等列表由 MainExe 以版本化 JSON 快照注入。
//   - 3D 校准和颜色校准按需懒加载，扫描重建来源打开设置时禁止校准。
//   - Logger、Calibration DLL 都是借用接口，不由 SettingsUI delete。
//
// 阅读重点:
//   - UI 文案统一写英文并用 tr() 包裹，后续由 qm 翻译。
//   - 界面布局使用 Qt Layout，不使用固定坐标，降低多语言和多分辨率维护成本。
//   - 设置页内部 QStackedWidget 只管理设置分类页，不代表 MainExe 级页面常驻缓存。
// =============================================================================

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_SettingsUI";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_SettingsUI v0.9.0 (2026-07-23)";
}

// 设置主页面内部页索引。只在 SettingsUI 内部使用，不暴露给 MainExe。
enum SettingsPageIndex {
    PageGeneral = 0,
    PageInfo = 1,
    PageCalibration = 2,
    PageCloud = 3,
    PageScan = 4,
    PageData = 5,
    PageAbout = 6,
};
}
