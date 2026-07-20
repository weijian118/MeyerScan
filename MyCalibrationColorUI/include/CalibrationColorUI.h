#pragma once

#include <QWidget>
#include <cstdint>

#ifdef MEYERSCAN_CALIBRATIONCOLORUI_EXPORTS
#  define MEYERSCAN_CALIBRATIONCOLORUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_CALIBRATIONCOLORUI_API __declspec(dllimport)
#endif

// 颜色校准 UI 公共虚接口版本。设备快照增加完整检测记录后升级为 5。
static const int MEYER_CALIBRATION_COLOR_UI_API_VERSION = 5;
static const std::uint32_t MEYER_CALIBRATION_COLOR_CONTEXT_SCHEMA_VERSION = 4U;
static const std::uint32_t MEYER_CALIBRATION_COLOR_DETECTION_SCHEMA_VERSION = 1U;

// 颜色校准可接受的检测结果。数值与 DeviceCmd 保持一致，失败和冲突状态不会
// 被 SettingsUI 传入本模块，因此这里只允许四种可继续结果。
enum CalibrationColorDeviceDetectionStatus {
    CalibrationColorDeviceDetectionNotRun = 0,
    CalibrationColorDeviceDetectionExact = 1,
    CalibrationColorDeviceDetectionCompatibilityInferred = 2,
    CalibrationColorDeviceDetectionProductionExactModel = 3,
    CalibrationColorDeviceDetectionProductionInferred = 4,
};

// DeviceCmd 检测记录在颜色校准模块边界上的只读副本。reported 是设备真实
// 回包，effective 可能来自兼容规则；二者必须保留，不能只传最终显示值。
struct CalibrationColorDeviceDetectionContext {
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

static_assert(sizeof(CalibrationColorDeviceDetectionContext) == 424U,
              "CalibrationColorDeviceDetectionContext ABI size changed");

// MainExe/SettingsUI 传入的只读设备快照。它不包含 DeviceCmd 句柄，因而可以
// 安全复制并在多个 UI 模块间传递；后续新增字段只能追加在 reserved 之前。
struct CalibrationColorDeviceContext {
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::int32_t deviceModel;
    std::int32_t modelSource;
    std::int32_t connectionState;
    std::int32_t isUsb2;
    char modelNameUtf8[32];
    // 0xD4/0xD9 返回的 13 位设备编号，由 MainExe 设备会话宿主读取并注入。
    char deviceIdUtf8[32];
    // 旧有线 0xCE payload 前 8 字节转换得到的型号代码，便于日志和实机联调核对。
    // 本字段复用原 reserved[8] 的空间，结构体总大小不变。
    char modelCodeUtf8[32];
    // 产品识别结果由 DeviceCmd 解析、MainExe 持有并经 SettingsUI 原样复制。
    std::uint64_t productEvidence;
    std::int32_t productFamily;
    std::int32_t productModel;
    std::int32_t productIdentificationStatus;
    std::int32_t protocolProfile;
    char productSeriesNameUtf8[32];
    char productNameUtf8[96];
    // 颜色校准只保存和记录该检测副本，不重新解析 D9/C7/CE 原始回包。
    CalibrationColorDeviceDetectionContext detection;
};

// 固定尺寸用于拦截独立升级时的新旧 DLL 合同错位。
static_assert(sizeof(CalibrationColorDeviceContext) == 696U,
               "CalibrationColorDeviceContext ABI size changed");

// ICalibrationColorUI 是颜色校准 UI 模块的公共接口。
// 模块边界:
//   - 本 DLL 可以使用 Qt，并在内部接入颜色校准算法和设备相关 DLL。
//   - 对外只暴露界面创建和生命周期，不让 MainExe 了解颜色校准内部步骤。
//   - 后续若颜色校准流程变化，只需要改本模块和算法适配层。
class MEYERSCAN_CALIBRATIONCOLORUI_API ICalibrationColorUI {
public:
    // 虚析构函数用于保持接口类的正确多态析构语义。
    virtual ~ICalibrationColorUI() = default;

    // 初始化颜色校准模块。
    // appDirUtf8 必须来自 QApplication::applicationDirPath() 或 MainExe 显式传入。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 注入已经通过 MainExe 设备会话宿主校验的设备快照。
    // 必须在 CreateWidget 前调用；本模块只保存副本，不持有调用方内存。
    virtual bool SetDeviceContext(const CalibrationColorDeviceContext* context) = 0;

    // 创建颜色校准 QWidget。
    // 返回对象一般由调用方页面容器接管父子关系。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭模块，清理缓存指针和路径。
    virtual void Shutdown() = 0;
};

// C ABI 导出函数，便于 MainExe 后续用动态加载方式获取模块接口。
extern "C" MEYERSCAN_CALIBRATIONCOLORUI_API ICalibrationColorUI* GetCalibrationColorUI();
