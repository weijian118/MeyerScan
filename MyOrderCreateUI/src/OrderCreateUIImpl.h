#pragma once

#include "OrderCreateUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QMap>
#include <QJsonObject>
#include <QLibrary>
#include <QSet>
#include <QString>
#include <QStringList>

#include "Logger.h"
#include "ScanSchemaService.h"
#include "ToothTreatmentPlanWidget.h"
#include "UIComponents.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTableWidget;
class QTextEdit;
class QToolButton;

using GetUIComponentsFunc = IUIComponents* (*)();
using GetScanSchemaServiceFunc = IScanSchemaService* (*)();

// OrderCreateUIImpl 是建单界面的初版实现。
// 设计重点是把患者基本信息、扫描方案、牙位选择和订单明细放在同一个工作台内。
class OrderCreateUIImpl : public IOrderCreateUI {
    Q_DECLARE_TR_FUNCTIONS(OrderCreateUI)

public:
    // 返回进程内单例。
    static OrderCreateUIImpl& Instance();

    // 初始化路径、日志和默认 UI 状态。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override;

    // 创建建单根界面。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;

    // 设置外部动作回调。
    void SetActionCallback(void (*callback)(void* context, int actionId), void* context) override;

    // 接收标准建单上下文 JSON，并把其中的患者、订单和扫描方案显示到界面。
    bool SetOrderContextJson(const char* contextJsonUtf8) override;

    // 返回模块版本。
    const char* GetModuleVersion() const override;

    // 清理模块状态。
    void Shutdown() override;

    // 生成并返回当前扫描流程 JSON。
    const char* GetCurrentScanProcessJson() override;

    // 生成并返回当前患者、订单、治疗方案和扫描流程上下文。
    const char* GetCurrentOrderContextJson() override;

private:
    // 构造/析构私有化，模块使用单例承载当前 UI 状态。
    OrderCreateUIImpl() = default;
    ~OrderCreateUIImpl() = default;

    // 禁止复制，避免 QWidget 指针和牙位状态被复制出多份。
    OrderCreateUIImpl(const OrderCreateUIImpl&) = delete;
    OrderCreateUIImpl& operator=(const OrderCreateUIImpl&) = delete;

    // 创建左侧患者/订单基本信息区。
    QWidget* CreateBasicInfoPanel(QWidget* parent);

    // 创建左侧工作区：治疗类型选择在上，基础信息在下，贴近当前软件视频布局。
    QWidget* CreateLeftWorkflowPanel(QWidget* parent);

    // 创建治疗类型选择区。
    QWidget* CreateTreatmentTypePanel(QWidget* parent);

    // 创建中间牙位和扫描类型选择区。
    QWidget* CreateToothPlanPanel(QWidget* parent);

    // 创建右侧订单明细和标信息区。
    QWidget* CreateOrderSummaryPanel(QWidget* parent);

    // 创建一个带标题的分组面板。
    QWidget* CreateSection(QWidget* parent, const QString& objectName, const QString& title, QLayout* contentLayout) const;

    // 创建“修复/正畸”等复选按钮。
    QPushButton* CreateCheckButton(QWidget* parent, const QString& text, bool checked) const;

    // 创建扫描类型按钮。
    QToolButton* CreateTypeButton(QWidget* parent, const QString& text, const QString& code, bool checked);

    // 从标准建单上下文中读取字符串字段。
    QString ReadString(const QJsonObject& object, const QString& key, const QString& defaultValue = QString()) const;

    // 从标准建单上下文中读取整数字段并转成界面文本。
    QString ReadIntText(const QJsonObject& object, const QString& key, const QString& defaultValue = QString()) const;

    // 根据文本值设置下拉框；不存在的值自动追加，避免第三方医生/技工所名称丢失。
    void SetComboText(QComboBox* combo, const QString& text);

    // 根据上下文设置性别单选按钮。
    void SetGender(const QString& gender);

    // 把标准上下文里的扫描方案 items 应用到牙位状态。
    void ApplyScanPlanItems(const QJsonObject& scanPlanObject);

    // 把标准上下文里的扫描流程配置应用到新增控件。
    void ApplyScanProcessConfig(const QJsonObject& scanProcessObject);

    // 清理弱引用，避免 QWidget 被外部释放后模块还保留悬空控件指针。
    void ResetWidgetReferences();

    // 指定扫描类型显示名。
    QString TypeText(const QString& typeCode) const;

    // 返回治疗类型按钮图标路径；highlighted 区分 b/h，highResolution 区分 1x/2x。
    QString TypeButtonIconPath(const QString& typeCode, bool highlighted, bool highResolution) const;

    // 根据按钮所在显示器判断本次应使用 1x 还是 2x 图标。
    bool UseHighResolutionTreatmentIcons(const QWidget* widget) const;

    // 切换当前扫描类型。
    void SetCurrentType(const QString& typeCode);

    // 切换某颗牙位的选中状态。
    void ToggleTooth(int toothNumber);

    // 清空所有牙位选择。
    void ClearAllTeeth();

    // 切换桥连接点选中状态。
    void ToggleBridgeConnector(const QString& bridgeKey);

    // 刷新右侧明细表。
    void RefreshSelectionTable();

    // 刷新治疗方案图片控件。
    void RefreshTreatmentPlanWidget();

    // 返回治疗方案资源目录。
    QString ResolveTreatmentPlanAssetRoot() const;

    // 把桥连接点合并成给用户看的桥记录文本。
    QStringList BuildBridgeRangeTexts() const;

    // 更新桥记录摘要标签。
    void RefreshBridgeSummary();

    // 刷新基本信息摘要。
    void RefreshBasicSummary();

    // 触发外部动作回调。
    void EmitAction(int actionId);

    // 写模块日志。
    void WriteLog(LogLevel level, const char* operation, const QString& content) const;

    // 动态加载共享 UI 组件模块。
    void LoadUIComponents();

    // 动态加载扫描方案服务；UI 只提供配置，不再自行生成业务步骤。
    void LoadScanSchemaService();

    // 创建统一按钮；共享 UI 不可用时返回本地降级按钮。
    QPushButton* CreateStandardButton(QWidget* parent, const QString& text, int role) const;

    // 创建统一单行输入框；共享 UI 不可用时返回本地降级输入框。
    QLineEdit* CreateStandardLineEdit(QWidget* parent, const QString& text = QString(), const QString& placeholder = QString()) const;

    // 创建统一下拉框；共享 UI 不可用时返回本地降级下拉框。
    QComboBox* CreateStandardComboBox(QWidget* parent) const;

    // 创建统一日期输入框；共享 UI 不可用时返回本地降级日期框。
    QDateEdit* CreateStandardDateEdit(QWidget* parent) const;

    // 创建统一多行文本框；共享 UI 不可用时返回本地降级文本框。
    QTextEdit* CreateStandardTextEdit(QWidget* parent, int fixedHeight) const;

    // 创建统一字段标签；共享 UI 不可用时返回本地降级标签。
    QLabel* CreateStandardFieldLabel(QWidget* parent, const QString& text) const;

    // 创建统一表格；共享 UI 不可用时返回本地降级表格。
    QTableWidget* CreateStandardTableWidget(QWidget* parent) const;

    // 创建扫描流程配置开关。
    QCheckBox* CreateScanProcessSwitch(QWidget* parent, const QString& text, const QString& objectName) const;

    // 创建扫描流程配置区；这些输入只存在于正式建单页，练习模式使用 MainExe 默认流程。
    QWidget* CreateScanProcessConfigPanel(QWidget* parent);

    // 根据当前页面控件和牙位类型推导扫描流程配置。
    QJsonObject BuildScanProcessConfigObject() const;

    // 当前咬合类型编码。
    QString CurrentOcclusionTypeCode() const;

    // 把服务返回的稳定步骤编码转换为当前语言的显示文本。
    QString ScanStepDisplayText(const QString& code) const;

    // 扫描流程输入变化后统一刷新 JSON 缓存、预览和动作回调。
    void RefreshScanProcessPreview(bool emitAction);

private:
    // MeyerScan.exe 所在目录。
    QByteArray m_appDir;

    // 统一日志目录。
    QByteArray m_logDir;

    // 缓存日志接口，避免每次写日志都重新 GetLogger()。
    ILogger* m_logger = nullptr;

    // UIComponents DLL 句柄；建单 UI 只借用共享控件工厂，不拥有其生命周期。
    QLibrary m_uiComponentsLibrary;

    // 缓存后的共享 UI 接口；不可用时本模块保留本地样式降级。
    IUIComponents* m_uiComponents = nullptr;

    // 公共双按钮弹窗使用独立 C ABI；旧 UIComponents 缺少该导出时降级为 QMessageBox。
    MeyerShowDecisionDialogFunc m_showDecisionDialog = nullptr;

    // ScanSchemaService DLL 句柄；规则服务通过 EXE 同级绝对路径加载。
    QLibrary m_scanSchemaServiceLibrary;

    // 借用的扫描方案服务接口，负责由输入配置生成步骤合同。
    IScanSchemaService* m_scanSchemaService = nullptr;

    // 建单根 QWidget 弱引用，由调用方/Qt 父子树负责释放。
    QWidget* m_root = nullptr;

    // 外部动作回调函数。
    void (*m_actionCallback)(void* context, int actionId) = nullptr;

    // 外部动作回调上下文。
    void* m_actionContext = nullptr;

    // 当前选中的扫描类型编码，例如 crown / implant。
    QString m_currentTypeCode;

    // 最近一次收到的标准建单上下文 JSON。
    // 允许外部先传上下文、后创建 QWidget，避免第三方拉起流程受调用顺序限制。
    QByteArray m_pendingContextJson;

    // 标记 m_pendingContextJson 是否已经保存有效上下文。
    bool m_hasPendingContext = false;

    // 当前选中的牙位集合。
    QSet<int> m_selectedTeeth;

    // toothNumber -> typeCode，用于保存每颗牙当前选择的扫描类型。
    QMap<int, QString> m_toothTypeCodes;

    // 已确认的桥连接点，例如 "26-27"。
    QSet<QString> m_selectedBridgeKeys;

    // typeCode -> button，用于刷新治疗类型按钮状态。
    QMap<QString, QToolButton*> m_typeButtons;

    // 右侧已选牙位表格。
    QTableWidget* m_selectionTable = nullptr;

    // 中间治疗方案图片控件。
    ToothTreatmentPlanWidget* m_treatmentPlanWidget = nullptr;

    // 桥记录摘要标签。
    QLabel* m_bridgeSummaryLabel = nullptr;

    // 摘要标签。
    QLabel* m_summaryPatientName = nullptr;
    QLabel* m_summaryDoctor = nullptr;
    QLabel* m_summaryOrderId = nullptr;
    QLabel* m_summarySource = nullptr;

    // 常用输入控件，初版用于摘要刷新，后续可改为数据模型绑定。
    QLineEdit* m_patientIdEdit = nullptr;
    QLineEdit* m_patientNameEdit = nullptr;
    QLineEdit* m_ageEdit = nullptr;
    QDateEdit* m_birthDateEdit = nullptr;
    QRadioButton* m_genderMale = nullptr;
    QRadioButton* m_genderFemale = nullptr;
    QRadioButton* m_genderUnknown = nullptr;
    QComboBox* m_doctorCombo = nullptr;
    QLineEdit* m_orderIdEdit = nullptr;
    QComboBox* m_labCombo = nullptr;
    QDateEdit* m_deliveryDateEdit = nullptr;
    QLineEdit* m_contactEdit = nullptr;
    QTextEdit* m_patientNoteEdit = nullptr;
    QTextEdit* m_orderNoteEdit = nullptr;
    QString m_sourceSummary;

    // 扫描流程配置控件：上颌异性扫描杆。
    QCheckBox* m_maxillaDiffRodSwitch = nullptr;

    // 扫描流程配置控件：下颌异性扫描杆。
    QCheckBox* m_mandibleDiffRodSwitch = nullptr;

    // 扫描流程配置控件：上颌扫描杆分段。
    QCheckBox* m_maxillaSegmentedRodSwitch = nullptr;

    // 扫描流程配置控件：下颌扫描杆分段。
    QCheckBox* m_mandibleSegmentedRodSwitch = nullptr;

    // 扫描流程配置控件：咬合类型。
    QComboBox* m_occlusionTypeCombo = nullptr;

    // 扫描流程预览标签，帮助建单人员在进入扫描前确认按钮顺序。
    QLabel* m_scanProcessPreviewLabel = nullptr;

    // 最近一次生成的扫描流程 JSON。GetCurrentScanProcessJson 返回该缓存的 constData()。
    QByteArray m_currentScanProcessJson;

    // 最近一次生成的完整建单上下文。GetCurrentOrderContextJson 返回该缓存的 constData()。
    QByteArray m_currentOrderContextJson;
};

