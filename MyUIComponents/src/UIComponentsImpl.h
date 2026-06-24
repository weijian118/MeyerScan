#pragma once

#include "UIComponents.h"

// UIComponentsImpl 提供跨 UI 模块共享的基础控件工厂。
// 它只统一尺寸、样式和等待页，不承载业务点击行为、权限判断或配置读取。
class UIComponentsImpl : public IUIComponents {
public:
    // 返回进程内单例。
    static UIComponentsImpl& Instance();

    // 初始化屏幕缩放系数。
    bool Init(const char* appDirUtf8) override;

    // 返回横向辅助缩放系数，主要用于边距、图标和控件高度。
    double ScaleX() const override;

    // 返回纵向辅助缩放系数，主要用于边距、图标和控件高度。
    double ScaleY() const override;

    // 创建启动/切换等待页。
    QWidget* CreateWaitWidget(const char* titleUtf8, const char* messageUtf8, QWidget* parent = nullptr) override;

    // 创建主操作按钮。
    QPushButton* CreatePrimaryButton(const char* textUtf8, QWidget* parent = nullptr) override;

    // 创建次操作按钮。
    QPushButton* CreateSecondaryButton(const char* textUtf8, QWidget* parent = nullptr) override;

    // 创建通用单行输入框。
    QLineEdit* CreateLineEdit(const char* placeholderUtf8, QWidget* parent = nullptr) override;

    // 创建通用下拉框。
    QComboBox* CreateComboBox(QWidget* parent = nullptr) override;

    // 创建页面标题标签。
    QLabel* CreatePageTitle(const char* textUtf8, QWidget* parent = nullptr) override;

    // 返回模块版本字符串。
    const char* GetModuleVersion() const override;

    // 重置模块内状态。
    void Shutdown() override;

private:
    UIComponentsImpl() = default;
    ~UIComponentsImpl() = default;
    UIComponentsImpl(const UIComponentsImpl&) = delete;
    UIComponentsImpl& operator=(const UIComponentsImpl&) = delete;

private:
    // 基于当前屏幕相对 1920x1080 的横向辅助缩放系数。
    double m_scaleX = 1.0;

    // 基于当前屏幕相对 1920x1080 的纵向辅助缩放系数。
    double m_scaleY = 1.0;
};
