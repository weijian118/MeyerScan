#include "MainWindow.h"

#include <QCoreApplication>
#include <QDir>

#include "DatabaseQtAdapter.h"
#include "Logger.h"

// 统一加载 MeyerScan.exe 同目录下的自研 DLL，并解析指定 C ABI 工厂函数。
QFunctionPointer MainWindow::ResolveFactory(QLibrary& library,
                                            const char* dllName,
                                            const char* factoryName,
                                            int expectedApiVersion,
                                            QString* errorMessage) {
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!dllName || !dllName[0] || !factoryName || !factoryName[0]) {
        if (errorMessage) {
            *errorMessage = "DLL name or factory name is empty";
        }
        return nullptr;
    }

    // QLibrary 默认按进程当前目录和 PATH 查找；这里显式给出 EXE 同目录绝对路径，
    // 避免第三方软件从其它 current directory 拉起 MeyerScan.exe 时加载错 DLL。
    if (library.fileName().isEmpty()) {
        library.setFileName(QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromLatin1(dllName)));
        library.setLoadHints(QLibrary::PreventUnloadHint);
    }

    if (!library.isLoaded() && !library.load()) {
        if (errorMessage) {
            *errorMessage = library.errorString();
        }
        WriteUserAction("ModuleLoadFailed",
                        QString("%1: %2").arg(dllName, library.errorString()));
        return nullptr;
    }

    // C++ 虚接口的布局可能随函数增删而变化。必须先校验 API 版本，再获取并调用工厂返回的对象。
    typedef int (*ApiVersionFunction)();
    QFunctionPointer apiPointer = library.resolve("GetMeyerModuleApiVersion");
    if (!apiPointer) {
        if (errorMessage) {
            *errorMessage = QString("API version export not found in %1").arg(dllName);
        }
        WriteUserAction("ModuleApiRejected", QString("%1: missing API version export").arg(dllName));
        return nullptr;
    }
    const int actualApiVersion = reinterpret_cast<ApiVersionFunction>(apiPointer)();
    if (actualApiVersion != expectedApiVersion) {
        if (errorMessage) {
            *errorMessage = QString("API version mismatch in %1: expected %2, actual %3")
                .arg(dllName)
                .arg(expectedApiVersion)
                .arg(actualApiVersion);
        }
        WriteUserAction("ModuleApiRejected",
                        QString("%1 expected=%2 actual=%3")
                            .arg(dllName)
                            .arg(expectedApiVersion)
                            .arg(actualApiVersion));
        return nullptr;
    }

    QFunctionPointer pointer = library.resolve(factoryName);
    if (!pointer) {
        if (errorMessage) {
            *errorMessage = QString("Factory not found: %1 in %2").arg(factoryName).arg(dllName);
        }
        WriteUserAction("ModuleFactoryMissing", QString("%1:%2").arg(dllName, factoryName));
        return nullptr;
    }
    WriteUserAction("ModuleFactoryResolved",
                    QString("%1:%2 api=%3").arg(dllName, factoryName).arg(actualApiVersion));
    return pointer;
}

// 动态加载 Logger.dll 并返回 ILogger 单例。
ILogger* MainWindow::LoggerModule() {
    if (m_logger) {
        return m_logger;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_loggerLibrary, "MeyerScan_Logger.dll", "GetLogger", 1, &error);
    if (!pointer) {
        WriteStatus(tr("Logger unavailable"));
        return nullptr;
    }
    typedef ILogger* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 DatabaseQtAdapter.dll。
IDatabaseQtAdapter* MainWindow::DatabaseAdapterModule() {
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_databaseAdapterLibrary,
                                              "MeyerScan_DatabaseQtAdapter.dll",
                                              "GetDatabaseQtAdapter",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Database adapter unavailable"));
        return nullptr;
    }
    typedef DatabaseQtAdapter* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 ConfigCenter.dll。
IConfigCenter* MainWindow::ConfigCenterModule() {
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_configLibrary,
                                              "MeyerScan_ConfigCenter.dll",
                                              "GetConfigCenter",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Config center unavailable"));
        return nullptr;
    }
    typedef IConfigCenter* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 Permission.dll。
IPermission* MainWindow::PermissionModule() {
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_permissionLibrary,
                                              "MeyerScan_Permission.dll",
                                              "GetPermission",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Permission unavailable"));
        return nullptr;
    }
    typedef IPermission* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 UIComponents.dll。
IUIComponents* MainWindow::UIComponentsModule() {
    if (m_uiComponents) {
        return m_uiComponents;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_uiComponentsLibrary,
                                              "MeyerScan_UIComponents.dll",
                                              "GetUIComponents",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("UI components unavailable"));
        return nullptr;
    }
    typedef IUIComponents* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 RuntimeDataCenter.dll。
IRuntimeDataCenter* MainWindow::RuntimeDataCenterModule() {
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_runtimeDataCenterLibrary,
                                              "MeyerScan_RuntimeDataCenter.dll",
                                              "GetRuntimeDataCenter",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Runtime data center unavailable"));
        return nullptr;
    }
    typedef IRuntimeDataCenter* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 CaseOrderService.dll。
ICaseOrderService* MainWindow::CaseOrderServiceModule() {
    if (m_caseOrderService) {
        return m_caseOrderService;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_caseOrderServiceLibrary,
                                              "MeyerScan_CaseOrderService.dll",
                                              "GetCaseOrderService",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Case/order service unavailable"));
        WriteUserAction("ModuleLoadFailed", QString("CaseOrderService: %1").arg(error));
        return nullptr;
    }
    typedef ICaseOrderService* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 HomeUI.dll。
IHomeUI* MainWindow::HomeUIModule() {
    if (m_home) {
        return m_home;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_homeLibrary, "MeyerScan_HomeUI.dll", "GetHomeUI", 2, &error);
    if (!pointer) {
        WriteStatus(tr("HomeUI unavailable"));
        return nullptr;
    }
    typedef IHomeUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 CaseUI.dll。
ICaseUI* MainWindow::CaseUIModule() {
    if (m_case) {
        return m_case;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_caseLibrary, "MeyerScan_CaseUI.dll", "GetCaseUI", 2, &error);
    if (!pointer) {
        WriteStatus(tr("CaseUI unavailable"));
        return nullptr;
    }
    typedef ICaseUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 SettingsUI.dll。
ISettingsUI* MainWindow::SettingsUIModule() {
    if (m_settings) {
        return m_settings;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_settingsLibrary,
                                              "MeyerScan_SettingsUI.dll",
                                              "GetSettingsUI",
                                              MEYER_SETTINGS_UI_API_VERSION,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("SettingsUI unavailable"));
        return nullptr;
    }
    typedef ISettingsUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 OrderScanWorkspaceShell.dll。
IOrderScanWorkspaceShell* MainWindow::OrderWorkspaceModule() {
    if (m_orderWorkspace) {
        return m_orderWorkspace;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_orderWorkspaceLibrary,
                                              "MeyerScan_OrderScanWorkspaceShell.dll",
                                              "GetOrderScanWorkspaceShell",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Order workspace unavailable"));
        return nullptr;
    }
    typedef IOrderScanWorkspaceShell* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 OrderCreateUI.dll。
IOrderCreateUI* MainWindow::OrderCreateUIModule() {
    if (m_orderCreate) {
        return m_orderCreate;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_orderCreateLibrary,
                                              "MeyerScan_OrderCreateUI.dll",
                                              "GetOrderCreateUI",
                                              2,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("OrderCreateUI unavailable"));
        return nullptr;
    }
    typedef IOrderCreateUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 ScanWorkflowUI.dll。
IScanWorkflowUI* MainWindow::ScanWorkflowModule() {
    if (m_scanWorkflow) {
        return m_scanWorkflow;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_scanWorkflowLibrary,
                                              "MeyerScan_ScanWorkflowUI.dll",
                                              "GetScanWorkflowUI",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Scan workflow unavailable"));
        return nullptr;
    }
    typedef IScanWorkflowUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 DataProcessUI.dll。
IDataProcessUI* MainWindow::DataProcessModule() {
    if (m_dataProcess) {
        return m_dataProcess;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_dataProcessLibrary,
                                              "MeyerScan_DataProcessUI.dll",
                                              "GetDataProcessUI",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("Data process unavailable"));
        return nullptr;
    }
    typedef IDataProcessUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 SendUI.dll。
ISendUI* MainWindow::SendUIModule() {
    if (m_send) {
        return m_send;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_sendLibrary,
                                              "MeyerScan_SendUI.dll",
                                              "GetSendUI",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("SendUI unavailable"));
        return nullptr;
    }
    typedef ISendUI* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}

// 动态加载 ExternalLaunchAdapter.dll。
IExternalLaunchAdapter* MainWindow::ExternalLaunchAdapterModule() {
    if (m_externalLaunchAdapter) {
        return m_externalLaunchAdapter;
    }
    QString error;
    QFunctionPointer pointer = ResolveFactory(m_externalLaunchAdapterLibrary,
                                              "MeyerScan_ExternalLaunchAdapter.dll",
                                              "GetExternalLaunchAdapter",
                                              1,
                                              &error);
    if (!pointer) {
        WriteStatus(tr("External launch adapter unavailable"));
        return nullptr;
    }
    typedef IExternalLaunchAdapter* (*Factory)();
    return reinterpret_cast<Factory>(pointer)();
}
