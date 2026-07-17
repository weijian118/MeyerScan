// =============================================================================
// 文件: DeviceSessionHost.h
// 作用: 声明 MeyerScan.exe 进程内唯一的设备命令会话宿主。
//
// 设计说明:
//   1. 本类只持有一个 MeyerDeviceCmdHandle，避免设置、校准、扫描各开一套 USB。
//   2. DeviceCmd/DeviceTransport 使用绝对路径动态加载，MainExe 不链接其 import lib。
//   3. 对外缓存并复制 DeviceCmd 公共 POD 快照，不复制内部 C++ 对象或 USB 句柄。
// =============================================================================
#pragma once

#include "DeviceCmd.h"

#include <QLibrary>
#include <QString>

class ILogger;
enum class LogLevel : int;

class DeviceSessionHost
{
public:
    // 创建尚未加载 DeviceCmd DLL 的空宿主。
    DeviceSessionHost();
    // 析构时关闭设备、销毁句柄并释放动态库引用。
    ~DeviceSessionHost();

    // 保存 MeyerScan.exe 目录和进程级 Logger；真正 DLL 加载延迟到首次预检。
    void Initialize(const QString& applicationDir, ILogger* logger);

    // 执行颜色校准设备预检。业务拦截原因写入 preflight.status，返回值只表示 API 调用结果。
    std::int32_t PrepareColorCalibration(MeyerDeviceCalibrationPreflight* preflight);

    // 关闭当前设备会话；颜色校准弹窗关闭和主程序退出都会调用。
    void CloseSession();

    // 查询 DeviceCmd 当前是否仍持有打开连接。
    bool IsSessionOpen() const;

    // 返回最近一次预检状态的只读副本；没有快照时返回 false。
    bool GetSnapshot(MeyerDeviceStateSnapshot* snapshot) const;

    // 执行完整释放；允许重复调用。
    void Shutdown();

private:
    // 动态加载 DeviceCmd、校验 ABI 并解析本宿主需要的全部导出函数。
    bool EnsureLoaded();
    // 使用 DeviceCmd 两次缓冲区接口读取最近错误文本。
    QString ReadLastError() const;
    // 写结构化日志；Logger 不可用时保持业务流程可返回明确状态。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

private:
    typedef std::int32_t (*GetApiVersionFunction)();
    typedef std::int32_t (*InitOpenParamsFunction)(MeyerDeviceCmdOpenParams*);
    typedef std::int32_t (*InitPreflightFunction)(MeyerDeviceCalibrationPreflight*);
    typedef MeyerDeviceCmdHandle (*CreateFunction)();
    typedef void (*DestroyFunction)(MeyerDeviceCmdHandle);
    typedef std::int32_t (*PrepareFunction)(MeyerDeviceCmdHandle,
                                            const MeyerDeviceCmdOpenParams*,
                                            MeyerDeviceCalibrationPreflight*);
    typedef std::int32_t (*SimpleHandleFunction)(MeyerDeviceCmdHandle);
    typedef std::int32_t (*GetLastErrorFunction)(MeyerDeviceCmdHandle,
                                                 char*,
                                                 std::size_t,
                                                 std::size_t*);

    QString m_applicationDir;
    ILogger* m_logger;
    QLibrary m_deviceCmdLibrary;
    MeyerDeviceCmdHandle m_handle;
    MeyerDeviceStateSnapshot m_snapshot;
    bool m_hasSnapshot;

    InitOpenParamsFunction m_initOpenParams;
    InitPreflightFunction m_initPreflight;
    CreateFunction m_create;
    DestroyFunction m_destroy;
    PrepareFunction m_prepare;
    SimpleHandleFunction m_close;
    SimpleHandleFunction m_isOpen;
    GetLastErrorFunction m_getLastError;
};
