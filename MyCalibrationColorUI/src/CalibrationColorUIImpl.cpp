#include "CalibrationColorUIImpl.h"

#include "MeyerQtModuleUtils.h"
#include "UIComponents.h"

#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

#include <cstring>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与 vcxproj 中的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_CalibrationColorUI";

// 模块版本用于 GetModuleVersion()，必须与 Version.rc 文件版本同步维护。
const char* Version = "MeyerScan_CalibrationColorUI v0.6.0 (2026-07-20)";
}

// CalibrationPreviewWidget 保持预览区域为正方形，并按比例绘制色卡采集图片。
// 这里使用 paintEvent 而不是 QLabel::setScaledContents(true)，是为了在窗口缩小时
// 保持原图宽高比，避免 400x400 的相机占位图被布局拉伸成矩形。
class CalibrationPreviewWidget final : public QWidget {
public:
    // 构造方形预览控件，并缓存资源 DLL 中读取到的原始图片。
    explicit CalibrationPreviewWidget(const QPixmap& image, QWidget* parent = nullptr)
        : QWidget(parent), m_image(image) {
        // objectName 仅用于 QSS 定位背景和边界，不在源码中设置视觉样式。
        setObjectName("CalibrationColorPreview");
        // 最小尺寸保证 1366x768 下仍可辨认；最大尺寸对应 1920x1080 参考图的 400px 区域。
        setMinimumSize(280, 280);
        setMaximumSize(400, 400);
        // Preferred 允许布局在空间不足时缩小，但不会无上限放大低分辨率位图。
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }

    // 返回参考图中的预览区设计尺寸，父布局据此计算弹窗首选大小。
    QSize sizeHint() const override {
        return QSize(400, 400);
    }

    // 告诉 Qt 高度应跟随宽度，使布局协商后仍保持正方形。
    bool hasHeightForWidth() const override {
        return true;
    }

    // 方形区域的高度等于布局分配的宽度。
    int heightForWidth(int width) const override {
        return width;
    }

protected:
    // 在控件中心按比例绘制原图；缩放发生在绘制阶段，不修改源图片数据。
    void paintEvent(QPaintEvent* event) override {
        // 先让 QWidget 处理 QSS 背景，再叠加图片内容。
        QWidget::paintEvent(event);
        if (m_image.isNull()) {
            // 资源加载失败时保留 QSS 背景，页面仍可显示并由日志定位资源问题。
            return;
        }

        QPainter painter(this);
        // SmoothPixmapTransform 在非 400px 尺寸下使用平滑采样，减少锯齿和像素抖动。
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QSize targetSize = m_image.size().scaled(size(), Qt::KeepAspectRatio);
        // 用整数中心定位，避免每次重绘因浮点舍入造成一像素位置漂移。
        const QPoint topLeft((width() - targetSize.width()) / 2,
                             (height() - targetSize.height()) / 2);
        painter.drawPixmap(QRect(topLeft, targetSize), m_image);
    }

private:
    // 保存原始图片，窗口缩放时始终从原图重采样，避免连续缩放导致画质累积下降。
    QPixmap m_image;
};

// CalibrationTitleBar 只负责把鼠标拖动转换为宿主位置变化，不承载颜色校准业务。
// 独立运行时移动无边框顶层窗口；设置页弹窗中移动颜色校准子面板，避免把全屏遮罩一起拖走。
class CalibrationTitleBar final : public QFrame {
public:
    // 保存实际需要移动的面板指针，标题栏本身只作为鼠标事件接收区域。
    explicit CalibrationTitleBar(QWidget* dragTarget, QWidget* parent = nullptr)
        : QFrame(parent), m_dragTarget(dragTarget) {
        // objectName 交给 QSS 处理标题栏颜色和圆角，不在代码中设置样式表。
        setObjectName("CalibrationColorTitleBar");
        // 空白标题栏区域显示可拖动光标，按下后切换为抓取状态。
        setCursor(Qt::OpenHandCursor);
    }

protected:
    // 记录按下时的屏幕坐标和面板坐标，后续移动只使用增量，避免跳动。
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_dragTarget) {
            m_dragging = true;
            m_pressGlobal = event->globalPos();
            m_targetStartPos = m_dragTarget->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        QFrame::mousePressEvent(event);
    }

    // 根据鼠标位移移动目标面板，并把子面板限制在遮罩宿主可见范围内。
    void mouseMoveEvent(QMouseEvent* event) override {
        if (!m_dragging || !m_dragTarget || !(event->buttons() & Qt::LeftButton)) {
            QFrame::mouseMoveEvent(event);
            return;
        }

        const QPoint delta = event->globalPos() - m_pressGlobal;
        QPoint targetPosition = m_targetStartPos + delta;
        if (!m_dragTarget->isWindow() && m_dragTarget->parentWidget()) {
            // 子控件的 pos() 是相对父窗口的坐标，因此在父窗口客户区内夹紧位置。
            const QWidget* parent = m_dragTarget->parentWidget();
            const int maxX = qMax(0, parent->width() - m_dragTarget->width());
            const int maxY = qMax(0, parent->height() - m_dragTarget->height());
            targetPosition.setX(qBound(0, targetPosition.x(), maxX));
            targetPosition.setY(qBound(0, targetPosition.y(), maxY));
        }

        // QWidget::move() 只改变位置，不重建布局和子控件，拖动过程不会闪烁。
        m_dragTarget->move(targetPosition);
        event->accept();
    }

    // 释放鼠标后恢复可拖动提示，下一次按下重新建立起点。
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_dragging) {
            m_dragging = false;
            setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    // 目标面板可能是独立窗口，也可能是设置遮罩中的普通子控件。
    QWidget* m_dragTarget = nullptr;
    // 鼠标左键是否处于拖动状态。
    bool m_dragging = false;
    // 鼠标按下时的全局屏幕坐标。
    QPoint m_pressGlobal;
    // 鼠标按下时目标面板相对父对象/桌面的初始位置。
    QPoint m_targetStartPos;
};
}

// 返回颜色校准模块单例。
CalibrationColorUIImpl& CalibrationColorUIImpl::Instance() {
    // 颜色校准模块保存日志和页面弱引用，单例能保证 MainExe 多次获取时状态一致。
    static CalibrationColorUIImpl instance;
    return instance;
}

// 初始化颜色校准模块。
// 当前只准备路径和日志；真实算法/设备资源后续也应在这里按需初始化。
bool CalibrationColorUIImpl::Init(const char* appDirUtf8, const char* logDirUtf8) {
    // 保存为 QByteArray，避免调用方传入临时 UTF-8 字节数组后本模块持有悬空指针。
    // 这是跨 DLL const char* 参数最常见的安全处理方式：入口处立即复制。
    m_appDir = QByteArray(appDirUtf8 ? appDirUtf8 : "");
    m_logDir = QByteArray(logDirUtf8 ? logDirUtf8 : "");

    // 每个模块缓存一份日志接口指针，减少重复获取单例和空指针判断分散。
    m_logger = GetLogger();
    if (m_logger && !m_logDir.isEmpty()) {
        m_logger->Init(m_logDir.constData(), LogLevel::Info);
    }
    // 共享组件只在初始化阶段尝试一次；失败时 CreatePrimaryButton 会使用本地 Qt 控件降级。
    LoadUIComponents();
    WriteLog(LogLevel::Info, "Init", "CalibrationColorUI initialized");
    return true;
}

// 保存颜色校准使用的设备快照。
// MainExe 已经完成连接、USB3 和型号检查，本模块仍重复校验关键字段，避免错误宿主绕过门禁。
bool CalibrationColorUIImpl::SetDeviceContext(const CalibrationColorDeviceContext* context) {
    if (!context ||
        context->structSize != sizeof(CalibrationColorDeviceContext) ||
        context->schemaVersion != MEYER_CALIBRATION_COLOR_CONTEXT_SCHEMA_VERSION ||
        context->connectionState != 1 ||
        context->isUsb2 != 0 ||
        context->deviceModel <= 0 ||
        context->detection.structSize !=
            sizeof(CalibrationColorDeviceDetectionContext) ||
        context->detection.schemaVersion !=
            MEYER_CALIBRATION_COLOR_DETECTION_SCHEMA_VERSION ||
        context->detection.detectionStatus < CalibrationColorDeviceDetectionExact ||
        context->detection.detectionStatus >
            CalibrationColorDeviceDetectionProductionInferred ||
        context->detection.effectiveDeviceNumberUtf8[0] == '\0' ||
        context->detection.effectiveModelCodeUtf8[0] == '\0') {
        m_hasDeviceContext = false;
        std::memset(&m_deviceContext, 0, sizeof(m_deviceContext));
        WriteLog(LogLevel::Warning,
                 "SetDeviceContextRejected",
                 "Color calibration device context is invalid or not ready");
        return false;
    }

    // 按值复制固定 POD，调用返回后不依赖 SettingsUI 栈上临时结构的生命周期。
    m_deviceContext = *context;
    m_hasDeviceContext = true;
    WriteLog(LogLevel::Info,
             "SetDeviceContext",
              QString("Device context accepted: detection=%1 profile=%2 profileName=%3 "
                      "reportedNumber=%4 effectiveNumber=%5 reportedModelCode=%6 "
                      "effectiveModelCode=%7 product=%8 identityStatus=%9 production=%10 "
                      "compatibility=%11 detail=%12")
                  .arg(m_deviceContext.detection.detectionStatus)
                  .arg(m_deviceContext.deviceModel)
                  .arg(QString::fromUtf8(m_deviceContext.modelNameUtf8))
                  .arg(QString::fromUtf8(
                      m_deviceContext.detection.reportedDeviceNumberUtf8))
                  .arg(QString::fromUtf8(
                      m_deviceContext.detection.effectiveDeviceNumberUtf8))
                  .arg(QString::fromUtf8(
                      m_deviceContext.detection.reportedModelCodeUtf8))
                  .arg(QString::fromUtf8(
                      m_deviceContext.detection.effectiveModelCodeUtf8))
                  .arg(QString::fromUtf8(m_deviceContext.productNameUtf8))
                  .arg(m_deviceContext.productIdentificationStatus)
                  .arg(m_deviceContext.detection.isProductionMode)
                  .arg(m_deviceContext.detection.usedCompatibilityDefaults)
                  .arg(QString::fromUtf8(m_deviceContext.detection.detailUtf8)));
    return true;
}

// 创建颜色校准主界面。
// 页面按参考软件还原为“自定义标题栏 + 方形相机预览 + 校准/退出按钮”的独立流程面板。
QWidget* CalibrationColorUIImpl::CreateWidget(QWidget* parent) {
    if (!m_hasDeviceContext) {
        // 颜色校准不得绕过 MainExe 设备预检直接创建；测试宿主需显式注入模拟快照。
        WriteLog(LogLevel::Warning,
                 "CreateWidgetRejected",
                 "Device context must be set before creating color calibration UI");
        return nullptr;
    }
    // root 挂到 parent 下时由设置模块遮罩弹窗接管；无 parent 时作为独立人工测试窗口。
    auto* root = new QWidget(parent);
    // objectName 是 QSS、自动化测试和设置模块定位颜色校准面板的稳定锚点。
    root->setObjectName("MeyerScanCalibrationColorUIRoot");
    // 动态属性仅供自动化和现场诊断读取，不参与样式或业务判断。
    root->setProperty("deviceModel", m_deviceContext.deviceModel);
    root->setProperty("deviceId", QString::fromUtf8(
        m_deviceContext.detection.effectiveDeviceNumberUtf8));
    root->setProperty("modelCode", QString::fromUtf8(
        m_deviceContext.detection.effectiveModelCodeUtf8));
    root->setProperty("reportedDeviceId", QString::fromUtf8(
        m_deviceContext.detection.reportedDeviceNumberUtf8));
    root->setProperty("reportedModelCode", QString::fromUtf8(
        m_deviceContext.detection.reportedModelCodeUtf8));
    root->setProperty("deviceDetectionStatus", m_deviceContext.detection.detectionStatus);
    root->setProperty("deviceProductionMode", m_deviceContext.detection.isProductionMode);
    root->setProperty("deviceUsesCompatibilityDefaults",
                      m_deviceContext.detection.usedCompatibilityDefaults);
    root->setProperty("productFamily", m_deviceContext.productFamily);
    root->setProperty("productModel", m_deviceContext.productModel);
    root->setProperty("productName", QString::fromUtf8(m_deviceContext.productNameUtf8));
    // 独立运行时去掉系统标题栏，使用页面内的标题和关闭按钮复刻参考界面。
    if (!parent) {
        root->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    }
    // 透明窗口背景让 QSS 圆角外侧不出现矩形底色；面板白底仍由 QSS 负责。
    root->setAttribute(Qt::WA_TranslucentBackground, true);
    // 360x500 是低分辨率下的可用下限；首选尺寸由布局中的 400x400 预览图推导。
    root->setMinimumSize(360, 500);
    root->setMaximumSize(520, 680);
    // 颜色、边框、字体、hover 和图片按钮状态全部从模块 QSS 读取。
    MeyerQtModule::ApplyModuleQss(root, "MyCalibrationColorUI", "calibration_color.qss", m_logger);

    // 根布局不留外边距，使 64px 标题栏和白色内容区完整贴合弹窗边缘。
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 自定义标题栏替代 Qt/Windows 标题栏，保证嵌入设置模块和独立运行时视觉一致。
    auto* titleBar = new CalibrationTitleBar(root, root);
    titleBar->setFixedHeight(64);
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 4, 0, 4);
    titleLayout->setSpacing(0);

    // 左侧保留与关闭按钮相同的 58px 空间，使标题不受右侧按钮影响并保持严格居中。
    titleLayout->addSpacing(58);
    titleLayout->addStretch();
    auto* title = new QLabel(tr("Color Calibration"), titleBar);
    title->setObjectName("CalibrationColorTitle");
    // 标题文字不抢占鼠标事件，让拖动事件统一由标题栏接收。
    title->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    title->setAlignment(Qt::AlignCenter);
    titleLayout->addWidget(title);
    titleLayout->addStretch();

    // 关闭按钮直接使用用户提供的 normal/hover 图片，按钮本身不写文字以避免语言影响尺寸。
    auto* closeButton = new QPushButton(titleBar);
    closeButton->setObjectName("CalibrationColorCloseButton");
    closeButton->setFixedSize(58, 56);
    closeButton->setToolTip(tr("Close"));
    closeButton->setCursor(Qt::PointingHandCursor);
    titleLayout->addWidget(closeButton);
    rootLayout->addWidget(titleBar);

    // 独立内容容器负责绘制白色背景；顶层窗口透明时不能依赖根 QWidget 填充整个客户区。
    auto* content = new QFrame(root);
    content->setObjectName("CalibrationColorBody");
    // 内容布局按参考图保持 24px 左右留白、20px 顶部留白和 26px 底部留白。
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 20, 24, 26);
    contentLayout->setSpacing(24);

    // init_image.png 是设备尚未返回相机画面时的初始状态；真实图像后续由采集接口替换。
    const QString previewPath = MeyerQtModule::ModuleResourceFile(
        "MyCalibrationColorUI", "icon/color_calibration", "init_image.png");
    const QPixmap previewImage(previewPath);
    if (previewImage.isNull()) {
        WriteLog(LogLevel::Warning,
                 "LoadPreviewResource",
                 QString("Color calibration preview image unavailable: %1").arg(previewPath));
    }
    auto* preview = new CalibrationPreviewWidget(previewImage, root);
    contentLayout->addWidget(preview, 1, Qt::AlignHCenter);

    // 按钮从共享 UIComponents 创建，视觉角色与浏览页 Search 主按钮一致。
    // 模块仍负责按钮文案、尺寸约束和业务信号，不把校准行为放进共享组件。
    auto* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(16);
    buttonRow->addStretch();
    auto* calibrateButton = CreatePrimaryButton(tr("Calibrate"), root);
    auto* exitButton = CreatePrimaryButton(tr("Exit"), root);
    // 业务按钮使用稳定 objectName，自动化测试不依赖翻译后的文字查找控件。
    calibrateButton->setObjectName("CalibrationColorCalibrateButton");
    exitButton->setObjectName("CalibrationColorExitButton");
    buttonRow->addWidget(calibrateButton);
    buttonRow->addWidget(exitButton);
    contentLayout->addLayout(buttonRow);
    rootLayout->addWidget(content, 1);

    // 当前阶段先打通界面和操作日志；设备取图、颜色计算和结果保存随后接入此点击点。
    QObject::connect(calibrateButton, &QPushButton::clicked, [this]() {
        WriteLog(LogLevel::Info, "CalibrateClicked", "User clicked color calibration action");
    });
    QObject::connect(exitButton, &QPushButton::clicked, [this, root]() {
        CloseHostWindow(root, "ExitClicked");
    });
    QObject::connect(closeButton, &QPushButton::clicked, [this, root]() {
        CloseHostWindow(root, "CloseClicked");
    });

    // 保存弱引用便于 Shutdown 清空状态，不代表本模块拥有 root 的删除权。
    m_root = root;
    WriteLog(LogLevel::Info, "CreateWidget", "CalibrationColorUI dialog widget created");
    return root;
}

// 返回模块版本字符串。
const char* CalibrationColorUIImpl::GetModuleVersion() const {
    return ModuleInfo::Version;
}

// 动态加载共享 UI 组件。
// 颜色校准只借用按钮工厂，不让 UIComponents 知道“校准”这一业务含义。
void CalibrationColorUIImpl::LoadUIComponents() {
    if (m_uiComponents) {
        // 同一模块实例可能被多次 Init；已有有效接口时无需重复加载 DLL。
        return;
    }

    // PreventUnloadHint 保证已取得的虚函数表在进程退出前持续有效。
    m_uiComponentsLibrary.setLoadHints(QLibrary::PreventUnloadHint);
    const QString libraryPath = QDir(QString::fromUtf8(m_appDir)).filePath(
        "MeyerScan_UIComponents.dll");
    m_uiComponentsLibrary.setFileName(libraryPath);
    if (!m_uiComponentsLibrary.load()) {
        WriteLog(LogLevel::Warning,
                 "LoadUIComponents",
                 QString("UIComponents unavailable; using local button fallback: %1")
                     .arg(m_uiComponentsLibrary.errorString()));
        return;
    }

    // 先验证公共 ABI 版本，避免把不兼容 DLL 返回的接口指针当成当前 vtable 使用。
    auto apiVersion = reinterpret_cast<int (*)()>(
        m_uiComponentsLibrary.resolve("GetMeyerModuleApiVersion"));
    if (!apiVersion || apiVersion() != 1) {
        WriteLog(LogLevel::Warning,
                 "LoadUIComponents",
                 "UIComponents API version mismatch; using local button fallback");
        return;
    }

    // extern "C" 工厂没有 C++ 名字修饰，可以通过固定符号名跨 DLL 获取。
    auto factory = reinterpret_cast<IUIComponents* (*)()>(
        m_uiComponentsLibrary.resolve("GetUIComponents"));
    if (!factory) {
        WriteLog(LogLevel::Warning,
                 "LoadUIComponents",
                 "GetUIComponents export missing; using local button fallback");
        return;
    }

    IUIComponents* components = factory();
    const QByteArray appDirBytes = m_appDir;
    if (!components || !components->Init(appDirBytes.constData())) {
        WriteLog(LogLevel::Warning,
                 "LoadUIComponents",
                 "UIComponents initialization failed; using local button fallback");
        return;
    }

    // 只有工厂、ABI 和 Init 都成功后才发布成员指针，避免半初始化状态被 CreateWidget 使用。
    m_uiComponents = components;
    WriteLog(LogLevel::Info, "LoadUIComponents", "UIComponents loaded for calibration buttons");
}

// 创建颜色校准主按钮。
// UIComponents 可用时复用统一工厂；不可用时创建语义属性相同的 QPushButton 作为降级。
QPushButton* CalibrationColorUIImpl::CreatePrimaryButton(const QString& text, QWidget* parent) const {
    QPushButton* button = nullptr;
    if (m_uiComponents) {
        // QString 在调用 DLL 前转 UTF-8，并由命名 QByteArray 保证 constData() 在调用期间有效。
        const QByteArray textBytes = text.toUtf8();
        button = m_uiComponents->CreateButton(MeyerButtonRolePrimary,
                                              MeyerButtonContentTextOnly,
                                              textBytes.constData(),
                                              "",
                                              parent);
    }
    if (!button) {
        // 降级按钮只保留 Qt 控件和语义属性，所有视觉细节仍由 calibration_color.qss 提供。
        button = new QPushButton(text, parent);
        button->setObjectName("CalibrationColorPrimaryButton");
        button->setProperty("role", "primary");
    }

    // calibrationAction 让模块 QSS 同时覆盖共享工厂按钮和本地降级按钮。
    button->setProperty("calibrationAction", true);
    // 108x46 对齐参考图；只设最小值，较长翻译可以自然扩宽而不会截断。
    button->setMinimumSize(108, 46);
    // UIComponents 默认允许普通按钮横向扩展；弹窗按钮应按内容保持紧凑，不平分整行宽度。
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

// 关闭颜色校准宿主。
// 独立测试时 root 自己是窗口；设置模块中 root 位于带标记的全屏遮罩 QDialog 内。
void CalibrationColorUIImpl::CloseHostWindow(QWidget* root, const char* operation) {
    WriteLog(LogLevel::Info, operation, "User requested to close color calibration");
    if (!root) {
        return;
    }

    QWidget* hostWindow = root->window();
    if (hostWindow == root ||
        (hostWindow && hostWindow->property("meyerCalibrationDialogHost").toBool())) {
        // close() 走 Qt 正常关闭事件，测试宿主和设置模块都能继续执行退出清理。
        hostWindow->close();
        return;
    }

    // 未知宿主可能把页面直接嵌入普通布局；此时只隐藏本页面，绝不能关闭整个主程序窗口。
    root->hide();
    WriteLog(LogLevel::Warning,
             "CloseHostWindow",
             "Unknown embedded host detected; calibration widget hidden instead of closing host window");
}

// 关闭颜色校准模块。
// 不主动 delete m_root，避免和 Qt 父对象析构产生重复释放。
void CalibrationColorUIImpl::Shutdown() {
    WriteLog(LogLevel::Info, "Shutdown", "CalibrationColorUI shutdown");
    if (m_logger) {
        // 退出前刷新日志，便于现场看到颜色校准模块最后状态。
        m_logger->Flush();
    }

    // root 通常已挂在上层页面容器下，不能在这里直接 delete，避免双重释放。
    // 未来如果本模块自己打开算法句柄或设备句柄，应在这里释放那些明确属于本模块的资源。
    m_root = nullptr;
    // UIComponents 是共享 DLL 内部单例，本模块只清空借用指针，不调用其 Shutdown。
    m_uiComponents = nullptr;
    m_hasDeviceContext = false;
    std::memset(&m_deviceContext, 0, sizeof(m_deviceContext));
    m_logger = nullptr;
    m_appDir.clear();
    m_logDir.clear();
}

// 写结构化日志。
// 日志失败不应影响校准界面创建，所以没有 logger 时静默返回。
void CalibrationColorUIImpl::WriteLog(LogLevel level, const char* operation, const QString& content) const {
    // MeyerQtModule::WriteQtLog 会自动补充 MEYER_MODULE_NAME，
    // 并把 QString 在跨 DLL 前转换成 UTF-8 const char*。
    MeyerQtModule::WriteQtLog(m_logger, level, operation, content);
}

// 导出颜色校准 UI 模块实例。
extern "C" MEYERSCAN_CALIBRATIONCOLORUI_API ICalibrationColorUI* GetCalibrationColorUI() {
    // C ABI 工厂函数用于稳定动态加载，内部实现类不暴露给调用方。
    return &CalibrationColorUIImpl::Instance();
}

// 统一版本导出函数。
// 版本清单读取颜色校准 UI 代码版本时，不加载校准页面、算法 DLL 或设备资源。
extern "C" MEYERSCAN_CALIBRATIONCOLORUI_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}

// 返回颜色校准界面公共接口 ABI 版本。
extern "C" __declspec(dllexport) int GetMeyerModuleApiVersion() {
    return MEYER_CALIBRATION_COLOR_UI_API_VERSION;
}
