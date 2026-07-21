#pragma once

#include <QWidget>
#include <cstdint>

#ifdef MEYERSCAN_SETTINGSUI_EXPORTS
#  define MEYERSCAN_SETTINGSUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_SETTINGSUI_API __declspec(dllimport)
#endif

// SettingsUI 公共虚接口版本；颜色校准上下文增加下位机版本快照后升级为 7。
static const int MEYER_SETTINGS_UI_API_VERSION = 7;
static const std::uint32_t MEYER_SETTINGS_CALIBRATION_CONTEXT_SCHEMA_VERSION = 5U;
static const std::uint32_t MEYER_SETTINGS_DEVICE_DETECTION_SCHEMA_VERSION = 1U;

// 校准入口设备预检状态。数值与 DeviceCmd 的预检状态保持一致，但 SettingsUI
// 只依赖本地 POD 合同，不包含 DeviceCmd 头或持有设备句柄。
enum SettingsCalibrationPreflightStatus {
    SettingsCalibrationPreflightNotRun = 0,
    SettingsCalibrationPreflightReady = 1,
    SettingsCalibrationPreflightWorkspaceOwnsDevice = 2,
    SettingsCalibrationPreflightDeviceNotConnected = 3,
    SettingsCalibrationPreflightUsb2Connected = 4,
    SettingsCalibrationPreflightWirelessUnsupported = 5,
    SettingsCalibrationPreflightDeviceInfoReadFailed = 6,
    SettingsCalibrationPreflightModelUnknown = 7,
    SettingsCalibrationPreflightInternalError = 8,
    // 0xD4 请求未收到合法 0xD9 回包时单独提示，不能误报成机型读取失败。
    SettingsCalibrationPreflightMachineCodeReadFailed = 9,
    // 设备编号前缀和型号代码冲突时禁止进入校准，等待现场核对设备信息。
    SettingsCalibrationPreflightProductIdentityConflict = 10,
    // 已收到错误回包或命令超时；具体 D9/C7/CE 步骤由检测记录进一步区分。
    SettingsCalibrationPreflightDeviceResponseAbnormal = 11,
    // D9 校验通过但 13 位编号格式或 620000 前缀不合法。
    SettingsCalibrationPreflightDeviceNumberInvalid = 12,
    // CE 校验通过但 8 位型号代码格式或 62 前缀不合法。
    SettingsCalibrationPreflightDeviceModelCodeInvalid = 13,
    // 设备身份已识别，但颜色校准所需的主控板/投图板版本读取失败。
    SettingsCalibrationPreflightFirmwareVersionReadFailed = 15,
};

// CE 型号代码读取状态与 DeviceCmd 公共枚举数值保持一致。SettingsUI 只用它
// 选择客户提示文本，不解析命令帧或自行决定兼容值。
enum SettingsDeviceModelCodeReadStatus {
    SettingsDeviceModelCodeReadNotRun = 0,
    SettingsDeviceModelCodeReadValid = 1,
    SettingsDeviceModelCodeReadFirmwareTooOld = 2,
    SettingsDeviceModelCodeReadFrameInvalid = 3,
    SettingsDeviceModelCodeReadChecksumInvalid = 4,
    SettingsDeviceModelCodeReadUninitialized = 5,
    SettingsDeviceModelCodeReadValueInvalid = 6,
};

// 下位机版本步骤状态与 DeviceCmd 数值保持一致，UI 仅据此判断副本是否完整。
enum SettingsFirmwareVersionStatus {
    SettingsFirmwareVersionNotRun = 0,
    SettingsFirmwareVersionValid = 1,
    SettingsFirmwareVersionNotRequired = 2,
    SettingsFirmwareVersionResponseMissing = 3,
    SettingsFirmwareVersionFrameInvalid = 4,
    SettingsFirmwareVersionPayloadInvalid = 5,
};

// MainExe 传入的下位机版本副本。主控板对所有已支持系列必需；投图板只对
// mOS MyScan 必需，其它系列明确记录 NotRequired，禁止 UI 猜测命令是否存在。
struct SettingsFirmwareVersionContext {
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t mainBoardStatus;
    std::int32_t projectionBoardStatus;
    char mainBoardVersionUtf8[32];
    char projectionBoardVersionUtf8[32];
    char detailUtf8[256];
    std::uint32_t reserved[8];
};

static_assert(sizeof(SettingsFirmwareVersionContext) == 368U,
              "SettingsFirmwareVersionContext ABI size changed");

// DeviceCmd 型号检测记录在 SettingsUI 边界上的只读副本。该结构分别保留
// 设备上报值和兼容有效值，UI 不允许把 effective 字段反写到设备。
struct SettingsDeviceDetectionContext {
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t detectionStatus;
    std::int32_t deviceNumberStatus;
    std::int32_t modelCodeStatus;
    std::int32_t seriesProbeStatus;
    std::int32_t isProductionMode;
    std::int32_t usedCompatibilityDefaults;
    std::int32_t deviceNumberSource;
    std::int32_t modelCodeSource;
    char reportedDeviceNumberUtf8[32];
    char effectiveDeviceNumberUtf8[32];
    char reportedModelCodeUtf8[16];
    char effectiveModelCodeUtf8[16];
    char detailUtf8[256];
    std::uint32_t reserved[8];
};

static_assert(sizeof(SettingsDeviceDetectionContext) == 424U,
              "SettingsDeviceDetectionContext ABI size changed");

// MainExe 在预检成功后填充的设备上下文。SettingsUI 只负责把副本继续注入
// CalibrationColorUI，不解释机型差异，也不共享 MainExe 的 DeviceCmd 句柄。
struct SettingsCalibrationDeviceContext {
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t status;
    std::int32_t deviceModel;
    std::int32_t modelSource;
    std::int32_t connectionState;
    std::int32_t isUsb2;
    char modelNameUtf8[32];
    // 0xD4/0xD9 返回的 13 位设备编号；DeviceCmd 的 MachineCode 是历史协议命名。
    char deviceIdUtf8[32];
    char detailUtf8[256];
    // 旧有线 0xCE payload 前 8 字节转换得到的型号代码；无线布局可能留空。
    // 本字段占用原 reserved[8] 的 32 字节，因此结构总大小保持不变。
    char modelCodeUtf8[32];
    // 以下字段是 DeviceCmd 产品识别 POD 的必要副本。UI 只展示和记录，不能
    // 根据这些整数重新解释协议或自行选择 Transport。
    std::uint64_t productEvidence;
    std::int32_t productFamily;
    std::int32_t productModel;
    std::int32_t productIdentificationStatus;
    std::int32_t protocolProfile;
    char productSeriesNameUtf8[32];
    char productNameUtf8[96];
    // 完整步骤记录放在结构末尾，老字段次序保持稳定；API/schema 已同步升级。
    SettingsDeviceDetectionContext detection;
    // 设备身份和型号确认后读取的下位机版本快照，不能通过再次访问 USB 获取。
    SettingsFirmwareVersionContext firmwareVersions;
};

// MainExe 和 SettingsUI 必须使用同一 API/schema；结构变化时编译期尺寸断言会拦截。
static_assert(sizeof(SettingsCalibrationDeviceContext) == 1328U,
              "SettingsCalibrationDeviceContext ABI size changed");

// 同步回调必须在返回前完成预检并写满 context；返回值使用
// SettingsCalibrationPreflightStatus。同步调用保证弹窗不会在检查完成前闪现。
typedef int (*SettingsCalibrationPreflightCallback)(void* context,
                                                     int actionId,
                                                     SettingsCalibrationDeviceContext* deviceContext);

// ISettingsUI 是设置模块的公共接口。
// 模块边界:
//   - 设置模块负责设置主界面、设置分类切换、校准入口和设置内轻量流程。
//   - MainExe、首页、案例管理、扫描重建只负责请求打开设置，不直接拼设置页面。
//   - 颜色校准和三维校准作为独立模块嵌入设置模块，后续算法/设备细节仍留在各校准 DLL 内部。
class MEYERSCAN_SETTINGSUI_API ISettingsUI {
public:
    // 虚析构函数保证跨 DLL 多态接口行为正确。
    virtual ~ISettingsUI() = default;

    // 初始化设置模块。
    // appDirUtf8 必须是 MeyerScan.exe 所在目录；logDirUtf8 是统一 logs 目录。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 设置动作回调。
    // callback 由 MainExe 提供，SettingsUI 只上报稳定 actionId。
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;

    // 注册校准设备预检回调。设备会话由 MainExe 持有，SettingsUI 不加载 DeviceCmd。
    virtual void SetCalibrationPreflightCallback(SettingsCalibrationPreflightCallback callback,
                                                  void* context) = 0;

    // 设置本次打开设置界面的来源上下文。
    // openSource 使用 SettingsOpenSource；allowCalibration=false 时必须隐藏/禁用校准入口。
    // 该接口必须在 CreateWidget() 之前调用；如果页面已经创建，模块也要尽量即时刷新可见状态。
    virtual void SetOpenContext(int openSource, bool allowCalibration) = 0;

    // 创建设置主页面。
    // 调用方负责把返回的 QWidget 挂入 MainExe 或扫描重建壳子的内容区。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 通知设置模块：调用方即将销毁 CreateWidget() 返回的页面。
    // 由于设置模块内部会缓存少量页面指针用于刷新状态，页面释放前必须调用该接口清空缓存。
    virtual void DestroyWidget() = 0;

    // 返回模块版本字符串，用于版本清单和现场排查。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭设置模块并释放缓存引用。
    // QWidget 的销毁仍由调用方页面容器或 Qt 父子关系负责。
    virtual void Shutdown() = 0;

    // 接收 MainExe 注入的医生、诊所、技工所等只读 domain 快照。
    virtual bool SetDataContextJson(const char* contextJsonUtf8) = 0;
};

// 设置模块动作 ID。
// MainExe 通过这些 ID 处理跨页面返回和后续设置保存流程。
enum SettingsActionId {
    SettingsActionClose = 1,          // 关闭设置并返回来源页面
    SettingsActionApply = 2,          // 应用设置
    SettingsActionConfirm = 3,        // 确认设置并关闭
    SettingsActionRestore = 4,        // 恢复默认设置
    SettingsActionOpen3DCalibration = 101,    // 打开三维校准页面
    SettingsActionOpenColorCalibration = 102, // 打开颜色校准页面
    SettingsActionColorCalibrationClosed = 103, // 颜色校准关闭，宿主释放设备会话
};

// 设置界面打开来源。
// 用 int 枚举跨 DLL 传递，避免把 QObject/QString/复杂对象放到 ABI 边界上。
enum SettingsOpenSource {
    SettingsOpenSourceUnknown = 0,         // 未指定来源，默认按首页来源处理
    SettingsOpenSourceHome = 1,            // 首页打开设置
    SettingsOpenSourceCase = 2,            // 案例管理打开设置
    SettingsOpenSourceScanReconstruct = 3, // 扫描重建打开设置
};

// C ABI 工厂函数。
// 后续 MainExe / ScanReconstructStudio 可用静态链接或动态加载方式获取设置模块。
extern "C" MEYERSCAN_SETTINGSUI_API ISettingsUI* GetSettingsUI();

