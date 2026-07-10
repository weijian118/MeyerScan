#pragma once

#ifdef MEYERSCAN_PERMISSION_EXPORTS
#  define MEYERSCAN_PERMISSION_API __declspec(dllexport)
#else
#  define MEYERSCAN_PERMISSION_API __declspec(dllimport)
#endif

// IPermission 是功能授权和功能阉割的核心判断入口。
// UI 显隐可以使用它，但高价值动作在 Service/Workflow/IPC 入口还要再次校验。
class MEYERSCAN_PERMISSION_API IPermission {
public:
    virtual ~IPermission() = default;

    // 初始化权限规则。appDirUtf8 必须是 MeyerScan.exe 所在目录。
    // TODO(v0.2.0): 当前骨架阶段为保持 MainExe 集成链路稳定暂时返回 bool。
    // 等权限失败语义稳定后，应升级为可区分规则缺失、规则损坏、版本不兼容、
    // 验签失败等原因的返回类型；是否抽公共结果类型由真实复用决定。
    virtual bool Init(const char* appDirUtf8) = 0;

    // 查询某个功能是否应该显示。
    // visible=false 用于隐藏入口，降低误触和简化界面。
    virtual bool IsFeatureVisible(const char* featureId, bool defaultValue) const = 0;

    // 查询某个功能是否允许点击/执行。
    // enabled=false 表示入口可见但不可操作，后续可配合提示原因。
    virtual bool IsFeatureEnabled(const char* featureId, bool defaultValue) const = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 清空内存中的权限规则。
    virtual void Shutdown() = 0;
};

// 获取进程内权限模块单例。
extern "C" MEYERSCAN_PERMISSION_API IPermission* GetPermission();
