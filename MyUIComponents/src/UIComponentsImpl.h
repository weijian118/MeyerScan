#pragma once

#include "UIComponents.h"

#include <QIcon>
#include <QSize>
#include <QString>

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

    // 创建标准 QPushButton，可选择角色和图标/文字布局。
    QPushButton* CreateButton(int role,
                              int contentLayout,
                              const char* textUtf8,
                              const char* iconResourcePathUtf8,
                              QWidget* parent = nullptr) override;

    // 创建标准 QToolButton，可选择角色和图标/文字布局。
    QToolButton* CreateToolButton(int role,
                                  int contentLayout,
                                  const char* textUtf8,
                                  const char* iconResourcePathUtf8,
                                  QWidget* parent = nullptr) override;

    // 给已有 QPushButton 应用统一样式。
    void ApplyButtonStyle(QPushButton* button, int role, int contentLayout) override;

    // 给已有 QToolButton 应用统一样式。
    void ApplyToolButtonStyle(QToolButton* button, int role, int contentLayout) override;

    // 创建通用单行输入框。
    QLineEdit* CreateLineEdit(const char* placeholderUtf8, QWidget* parent = nullptr) override;

    // 创建通用下拉框。
    QComboBox* CreateComboBox(QWidget* parent = nullptr) override;

    // 创建通用日期输入框。
    QDateEdit* CreateDateEdit(QWidget* parent = nullptr) override;

    // 创建通用多行文本框。
    QTextEdit* CreateTextEdit(QWidget* parent = nullptr) override;

    // 创建通用字段标签。
    QLabel* CreateFieldLabel(const char* textUtf8, QWidget* parent = nullptr) override;

    // 创建通用表格控件。
    QTableWidget* CreateTableWidget(QWidget* parent = nullptr) override;

    // 给已有表格套用统一基础样式。
    void ApplyTableStyle(QTableWidget* table) override;

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

    // 生成不同按钮角色对应的 QSS。
    QString ButtonStyleSheet(int role) const;

    // 根据按钮角色返回建议最小高度。
    int ButtonMinimumHeight(int role) const;

    // 根据按钮内容布局返回 QPushButton 的默认图标尺寸。
    QSize ButtonIconSize(int contentLayout) const;

    // 统一加载图标；空路径或加载失败时返回空图标，按钮仍正常显示文字。
    QIcon LoadIcon(const char* iconResourcePathUtf8) const;

    // 返回普通输入控件统一 QSS，供 QLineEdit/QComboBox/QDateEdit/QTextEdit 复用。
    QString InputStyleSheet(const QString& selector) const;

private:
    // 基于当前屏幕相对 1920x1080 的横向辅助缩放系数。
    double m_scaleX = 1.0;

    // 基于当前屏幕相对 1920x1080 的纵向辅助缩放系数。
    double m_scaleY = 1.0;
};
