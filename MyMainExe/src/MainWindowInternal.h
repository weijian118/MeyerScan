#pragma once

// MainWindow 各职责翻译单元共享的只读常量和纯数据辅助函数。
// 该文件只在 MainExe 内部使用，不形成 DLL 接口，也不保存运行时可变状态。

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace {
const char* kDefaultLoginUrl = "https://myscan.meyerop.com/login";

// RuntimeDataCenter 使用调用方缓冲区返回 JSON；宿主从 512KB 开始有限扩容到 32MB。
const int kInitialRuntimeDomainBufferSize = 512 * 1024;
const int kMaxRuntimeDomainBufferSize = 32 * 1024 * 1024;

namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_MainExe";

// 模块版本用于运行时版本清单；版本号必须与 Version.rc 中的文件版本保持一致。
const char* Version = "MeyerScan_MainExe v0.8.1 (2026-07-24)";
}

// 从患者/订单读模型行中取稳定 ID；兼容新服务 camelCase 和旧库大写列名。
QString ReadModelStableId(const QString& domain, const QJsonObject& item) {
    const QStringList keys = domain == "local.orders"
        ? (QStringList() << "orderId" << "ORDER_ID")
        : (QStringList() << "patientId" << "PATIENT_ID");
    for (int keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
        const QString value = item.value(keys.at(keyIndex)).toVariant().toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return QString();
}

// 服务结果排在旧库快照之前，并按稳定 ID 去重，保证迁移阶段新旧数据都可见。
QJsonArray MergeReadModelItems(const QString& domain,
                               const QJsonArray& serviceItems,
                               const QJsonArray& legacyItems) {
    QJsonArray mergedItems;
    QSet<QString> seenIds;
    for (int itemIndex = 0; itemIndex < serviceItems.size(); ++itemIndex) {
        const QJsonObject item = serviceItems.at(itemIndex).toObject();
        const QString stableId = ReadModelStableId(domain, item);
        if (!stableId.isEmpty() && seenIds.contains(stableId)) {
            continue;
        }
        mergedItems.append(item);
        if (!stableId.isEmpty()) {
            seenIds.insert(stableId);
        }
    }
    for (int itemIndex = 0; itemIndex < legacyItems.size(); ++itemIndex) {
        const QJsonObject item = legacyItems.at(itemIndex).toObject();
        const QString stableId = ReadModelStableId(domain, item);
        if (!stableId.isEmpty() && seenIds.contains(stableId)) {
            continue;
        }
        mergedItems.append(item);
        if (!stableId.isEmpty()) {
            seenIds.insert(stableId);
        }
    }
    return mergedItems;
}

// versionFunction 为空表示只读取 Windows 文件版本；非空时还动态读取代码版本。
struct VersionModuleEntry {
    const char* file;
    const char* versionFunction;
};

const VersionModuleEntry kDefaultVersionModules[] = {
    {"MeyerScan.exe", ""},
    {"MeyerLoginWidget.dll", ""},
    {"MeyerScan_Logger.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_Database.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_DatabaseQtAdapter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_DeviceTransport.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_DeviceCmd.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CaptureProcessing.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CaptureImagePipeline.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_AutoExposure.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CaptureService.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ConfigCenter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_Permission.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_UIComponents.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_UIResources.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_HomeUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CaseUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_SettingsUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CaseOrderService.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ScanSchemaService.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_RuntimeDataCenter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_OrderCreateUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_OrderScanWorkspaceShell.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ExternalLaunchAdapter.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_Calibration3DUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_CalibrationColorUI.dll", "GetMeyerModuleVersion"},
    {"ScanReconstructStudio.exe", "GetMeyerModuleVersion"},
    {"MeyerScan_ScanReconstructStudio.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_ScanWorkflowUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_DataProcessUI.dll", "GetMeyerModuleVersion"},
    {"MeyerScan_SendUI.dll", "GetMeyerModuleVersion"},
};
}
