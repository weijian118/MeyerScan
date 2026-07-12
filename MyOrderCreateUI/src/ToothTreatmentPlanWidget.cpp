#include "ToothTreatmentPlanWidget.h"

#include "TreatmentPlanResourceRules.h"

#include <QCursor>
#include <QDir>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOption>
#include <QStringList>
#include <QToolTip>
#include <QResizeEvent>

#include <algorithm>

namespace {

// 当前治疗方案基础图、mask 图和叠加图统一是 600x400。
// 所有点击命中都回到这个坐标系处理，避免不同分辨率下重写坐标表。
const int kSourceImageWidth = 600;
const int kSourceImageHeight = 400;

// 上下颌之间保留的视觉间距。视频中两张牙弓上下分离，不能紧贴在一起。
const int kJawGap = 18;

// 桥连接点 mask 的像素值从 35 开始，每个连接点递增 15。
int BridgeMaskValueAt(int index) {
    return 35 + index * 15;
}

// 把 "26-27" 拆成两个牙位号。
// 解析失败时返回 false，调用方即可跳过这条无效桥定义。
bool ParseBridgeKey(const QString& bridgeKey, int* firstTooth, int* secondTooth) {
    const QStringList parts = bridgeKey.split('-');
    if (parts.size() != 2) {
        return false;
    }

    bool firstOk = false;
    bool secondOk = false;
    const int first = parts.at(0).toInt(&firstOk);
    const int second = parts.at(1).toInt(&secondOk);
    if (!firstOk || !secondOk) {
        return false;
    }

    if (firstTooth) {
        *firstTooth = first;
    }
    if (secondTooth) {
        *secondTooth = second;
    }
    return true;
}

// mask 图片本质是灰度/RGBA 编码图，红绿蓝三个通道一致。
// 读取 red 通道即可得到区域编码，编码再映射成牙位或桥 key。
int MaskValueAt(const QImage& image, const QPoint& point) {
    if (image.isNull()
        || point.x() < 0
        || point.y() < 0
        || point.x() >= image.width()
        || point.y() >= image.height()) {
        return 0;
    }

    return qRed(image.pixel(point));
}

} // namespace

// 创建治疗方案图片控件。
ToothTreatmentPlanWidget::ToothTreatmentPlanWidget(QWidget* parent)
    : QWidget(parent),
      m_clearButton(nullptr) {
    // objectName 让 QSS 能单独设置牙弓画布的背景和文本颜色，C++ 不保存视觉色值。
    setObjectName("OrderCreateTreatmentPlanWidget");

    // hover 反馈需要在不按鼠标键时也收到 move 事件。
    setMouseTracking(true);

    // 自绘控件仍由 QSS 绘制背景；WA_StyledBackground 确保 QWidget 子类执行样式表背景规则。
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);

    // 控件在三栏布局中应尽量吃掉中间区域，图片会在内部等比居中。
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 静态 mask 映射只依赖资源编码规则，构造时初始化一次即可。
    InitializeToothMaskMaps();
    InitializeBridgeMaskMaps();

    // 视频中“清除所有”按钮位于上下颌之间，属于牙弓交互的一部分。
    // 按钮本身只发出回调，真正清空业务状态仍由 OrderCreateUIImpl 完成。
    m_clearButton = new QPushButton(tr("Clear All"), this);
    m_clearButton->setObjectName("OrderCreateClearAllButton");
    m_clearButton->setCursor(Qt::PointingHandCursor);
    m_clearButton->setMinimumSize(180, 38);
    connect(m_clearButton, &QPushButton::clicked, [this]() {
        if (m_clearClickedCallback) {
            m_clearClickedCallback();
        }
    });
}

// 设置治疗方案资源根目录。
void ToothTreatmentPlanWidget::SetAssetRoot(const QString& assetRoot) {
    // 路径可能来自 MainExe 目录或模块测试目录，统一转换为 Qt 可识别的分隔符。
    m_assetRoot = QDir::fromNativeSeparators(assetRoot);

    // 换目录后旧缓存不再可信，必须清空后重新加载基础图和 mask。
    m_pixmapCache.clear();
    LoadAssets();

    // 资源尺寸可能影响布局推荐尺寸，通知父布局重新计算。
    updateGeometry();

    // 立即重绘，让资源缺失/加载成功状态能反馈到界面上。
    update();
}

// 设置当前治疗类型。
void ToothTreatmentPlanWidget::SetCurrentTreatmentType(const QString& typeCode) {
    // 当前类型只决定后续牙位点击是否有效，以及 tooltip 文案。
    m_currentTreatmentType = typeCode;

    // 鼠标可能正停在某颗牙上，切换类型后需要立刻刷新手型光标和 tooltip。
    UpdateHoverFeedback(mapFromGlobal(QCursor::pos()));
}

// 设置当前牙位和桥状态。
void ToothTreatmentPlanWidget::SetPlanState(const QMap<int, QString>& toothTypeCodes,
                                            const QSet<QString>& selectedBridgeKeys) {
    // 拷贝一份状态快照用于 paintEvent，避免绘制过程中依赖外层容器生命周期。
    m_toothTypeCodes = toothTypeCodes;
    m_selectedBridgeKeys = selectedBridgeKeys;

    // 把当前空心/实心桥点候选同步成只读诊断属性。
    // 测试宿主只通过 QWidget::property 读取该值，不需要链接 DLL 内部 C++ 类方法。
    QStringList bridgeCandidates;
    for (auto it = m_bridgeConnectors.constBegin(); it != m_bridgeConnectors.constEnd(); ++it) {
        if (IsBridgeCandidate(it.key())) {
            bridgeCandidates.append(it.key());
        }
    }
    bridgeCandidates.sort();
    setProperty("bridgeCandidates", bridgeCandidates);

    // 状态变化后只需要重绘，不需要重新加载资源。
    update();
}

// 判断关键资源是否完整。
bool ToothTreatmentPlanWidget::HasRequiredAssets() const {
    return !m_maxillaPixmap.isNull()
        && !m_mandiblePixmap.isNull()
        && !m_maxillaMask.isNull()
        && !m_mandibleMask.isNull()
        && !m_maxillaBridgeMask.isNull()
        && !m_mandibleBridgeMask.isNull();
}

// 设置牙位点击回调。
void ToothTreatmentPlanWidget::SetToothClickedCallback(const ToothClickedCallback& callback) {
    m_toothClickedCallback = callback;
}

// 设置桥连接点点击回调。
void ToothTreatmentPlanWidget::SetBridgeClickedCallback(const BridgeClickedCallback& callback) {
    m_bridgeClickedCallback = callback;
}

// 设置清空所有点击回调。
void ToothTreatmentPlanWidget::SetClearClickedCallback(const ClearClickedCallback& callback) {
    m_clearClickedCallback = callback;
}

// 返回推荐尺寸。
QSize ToothTreatmentPlanWidget::sizeHint() const {
    // 牙弓保持主视觉，但较旧值适当收敛，减少大屏 Scan Plan 两侧空白。
    return QSize(560, 720);
}

// 返回最小尺寸。
QSize ToothTreatmentPlanWidget::minimumSizeHint() const {
    // 低分辨率下仍给上下颌各自保留可点击面积。
    return QSize(340, 500);
}

// 绘制治疗方案。
void ToothTreatmentPlanWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    // 每次绘制前都根据当前控件尺寸重新计算图片矩形。
    // 这样 resize 后绘制区域和点击区域始终保持一致。
    UpdateImageRects();

    QPainter painter(this);

    // 先让 Qt Style 绘制 QSS 背景，再叠加牙弓图片；这样更换主题时无需修改 C++ 色值。
    QStyleOption styleOption;
    styleOption.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!HasRequiredAssets()) {
        // 资源缺失时显示占位提示，避免控件空白导致排查困难。
        // WindowText 由同一控件的 QSS color 映射到 palette，避免在绘制代码中重复颜色。
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(rect(), Qt::AlignCenter, tr("Treatment plan assets are missing."));
        return;
    }

    // 上下颌分别绘制。叠加图和基础图共用同一个 targetRect，所以视觉天然对齐。
    DrawJaw(&painter, JawMaxilla, m_maxillaRect);
    DrawJaw(&painter, JawMandible, m_mandibleRect);
}

// 鼠标点击处理。
void ToothTreatmentPlanWidget::mousePressEvent(QMouseEvent* event) {
    if (!event || event->button() != Qt::LeftButton || !HasRequiredAssets()) {
        QWidget::mousePressEvent(event);
        return;
    }

    Jaw jaw = JawMaxilla;
    QPoint imagePoint;
    if (!HitJaw(event->pos(), &jaw, &imagePoint)) {
        QWidget::mousePressEvent(event);
        return;
    }

    // 先处理桥连接点。
    // 桥连接点很小，如果先读牙位 mask，点击小圆点时可能被附近牙位吞掉。
    const QString bridgeKey = HitBridge(jaw, imagePoint);
    if (!bridgeKey.isEmpty() && IsBridgeCandidate(bridgeKey)) {
        if (m_bridgeClickedCallback) {
            m_bridgeClickedCallback(bridgeKey);
        }
        return;
    }

    // 再处理普通牙位。
    // 没有选择治疗类型时不响应牙位，符合“先选类型，再选牙位”的规则。
    const int toothNumber = HitTooth(jaw, imagePoint);
    if (toothNumber > 0 && !m_currentTreatmentType.isEmpty()) {
        if (m_toothClickedCallback) {
            m_toothClickedCallback(toothNumber);
        }
        return;
    }

    QWidget::mousePressEvent(event);
}

// 鼠标移动时更新 hover 反馈。
void ToothTreatmentPlanWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event) {
        UpdateHoverFeedback(event->pos());
    }

    QWidget::mouseMoveEvent(event);
}

// 鼠标离开后恢复默认反馈。
void ToothTreatmentPlanWidget::leaveEvent(QEvent* event) {
    unsetCursor();
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}

// 控件尺寸变化时重新摆放按钮。
void ToothTreatmentPlanWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    UpdateImageRects();
    UpdateClearButtonGeometry();
}

// 加载基础图片和 mask。
void ToothTreatmentPlanWidget::LoadAssets() {
    const QDir rootDir(m_assetRoot);

    // QPixmap 用于绘制可见图片；QImage 用于逐像素读取 mask 编码。
    m_maxillaPixmap = QPixmap(rootDir.filePath("maxilla.png"));
    m_mandiblePixmap = QPixmap(rootDir.filePath("mandible.png"));
    m_maxillaMask = QImage(rootDir.filePath("maskMaxilla.png")).convertToFormat(QImage::Format_ARGB32);
    m_mandibleMask = QImage(rootDir.filePath("maskMandible.png")).convertToFormat(QImage::Format_ARGB32);
    m_maxillaBridgeMask = QImage(rootDir.filePath("maskPonticMaxilla.png")).convertToFormat(QImage::Format_ARGB32);
    m_mandibleBridgeMask = QImage(rootDir.filePath("maskPonticMandible.png")).convertToFormat(QImage::Format_ARGB32);
}

// 初始化牙位 mask 映射。
void ToothTreatmentPlanWidget::InitializeToothMaskMaps() {
    m_maxillaMaskToTooth.clear();
    m_mandibleMaskToTooth.clear();

    for (int index = 0; index < 16; ++index) {
        // 牙位 mask 的像素值从 30 开始，每个牙位递增 15。
        const int maskValue = 30 + index * 15;
        m_maxillaMaskToTooth.insert(
            maskValue,
            MeyerScanTreatmentPlanRules::ToothNumberForMaskValue(true, maskValue));
        m_mandibleMaskToTooth.insert(
            maskValue,
            MeyerScanTreatmentPlanRules::ToothNumberForMaskValue(false, maskValue));
    }
}

// 初始化桥连接点映射。
void ToothTreatmentPlanWidget::InitializeBridgeMaskMaps() {
    m_maxillaMaskToBridge.clear();
    m_mandibleMaskToBridge.clear();
    m_bridgeConnectors.clear();

    for (int index = 0; index < 15; ++index) {
        const int maskValue = BridgeMaskValueAt(index);

        const QString maxillaKey = MeyerScanTreatmentPlanRules::BridgeKeyForMaskValue(true, maskValue);
        m_maxillaMaskToBridge.insert(maskValue, maxillaKey);

        int firstTooth = 0;
        int secondTooth = 0;
        if (ParseBridgeKey(maxillaKey, &firstTooth, &secondTooth)) {
            BridgeConnector connector;
            connector.key = maxillaKey;
            connector.firstTooth = firstTooth;
            connector.secondTooth = secondTooth;
            connector.maskValue = maskValue;
            m_bridgeConnectors.insert(maxillaKey, connector);
        }

        const QString mandibleKey = MeyerScanTreatmentPlanRules::BridgeKeyForMaskValue(false, maskValue);
        m_mandibleMaskToBridge.insert(maskValue, mandibleKey);

        if (ParseBridgeKey(mandibleKey, &firstTooth, &secondTooth)) {
            BridgeConnector connector;
            connector.key = mandibleKey;
            connector.firstTooth = firstTooth;
            connector.secondTooth = secondTooth;
            connector.maskValue = maskValue;
            m_bridgeConnectors.insert(mandibleKey, connector);
        }
    }
}

// 根据控件大小计算上下颌显示矩形。
void ToothTreatmentPlanWidget::UpdateImageRects() {
    const QRect available = rect().adjusted(8, 8, -8, -8);
    if (available.width() <= 0 || available.height() <= 0) {
        m_maxillaRect = QRect();
        m_mandibleRect = QRect();
        return;
    }

    // 上下颌各占一半高度，中间保留固定视觉间距。
    const int halfHeight = std::max(1, (available.height() - kJawGap) / 2);

    // 先按高度反推最大宽度，再和可用宽度取较小值，保证图片永远完整显示。
    const int maxJawWidthByHeight = halfHeight * kSourceImageWidth / kSourceImageHeight;
    const int jawWidth = std::min(available.width(), maxJawWidthByHeight);

    // 宽度确定后按 600:400 原始比例反推高度，避免牙弓被拉伸。
    const int jawHeight = jawWidth * kSourceImageHeight / kSourceImageWidth;
    const int x = available.left() + (available.width() - jawWidth) / 2;

    const int maxillaY = available.top() + (halfHeight - jawHeight) / 2;
    const int mandibleBandTop = available.top() + halfHeight + kJawGap;
    const int mandibleY = mandibleBandTop + (halfHeight - jawHeight) / 2;

    m_maxillaRect = QRect(x, maxillaY, jawWidth, jawHeight);
    m_mandibleRect = QRect(x, mandibleY, jawWidth, jawHeight);

    // 图片矩形变化后同步移动中间按钮。
    UpdateClearButtonGeometry();
}

// 更新清空按钮位置。
void ToothTreatmentPlanWidget::UpdateClearButtonGeometry() {
    if (!m_clearButton || m_maxillaRect.isEmpty() || m_mandibleRect.isEmpty()) {
        return;
    }

    // 按钮宽度跟随牙弓宽度轻微变化，但设置上下限，避免 4K 下过宽或低分辨率下过窄。
    const int buttonWidth = std::max(180, std::min(280, m_maxillaRect.width() / 2));
    const int buttonHeight = 38;
    const int x = m_maxillaRect.center().x() - buttonWidth / 2;

    // 放在上颌底部和下颌顶部之间的中心位置，与视频里的中间清空按钮一致。
    const int gapTop = m_maxillaRect.bottom();
    const int gapBottom = m_mandibleRect.top();
    const int y = gapTop + (gapBottom - gapTop - buttonHeight) / 2;

    m_clearButton->setGeometry(x, y, buttonWidth, buttonHeight);
    m_clearButton->raise();
}

// 绘制单个牙弓。
void ToothTreatmentPlanWidget::DrawJaw(QPainter* painter, Jaw jaw, const QRect& targetRect) {
    if (!painter || targetRect.isEmpty()) {
        return;
    }

    const QPixmap& basePixmap = (jaw == JawMaxilla) ? m_maxillaPixmap : m_mandiblePixmap;
    painter->drawPixmap(targetRect, basePixmap);

    // 绘制单颗牙位的治疗叠加图。
    // 叠加图本身也是 600x400，直接绘制到同一个 targetRect 即可对齐。
    for (auto it = m_toothTypeCodes.begin(); it != m_toothTypeCodes.end(); ++it) {
        const int toothNumber = it.key();
        if (!IsToothInJaw(toothNumber, jaw)) {
            continue;
        }

        const QString overlayPath = ToothOverlayPath(jaw, toothNumber, it.value());
        if (overlayPath.isEmpty()) {
            continue;
        }

        const QPixmap overlay = LoadCachedPixmap(overlayPath);
        if (!overlay.isNull()) {
            painter->drawPixmap(targetRect, overlay);
        }
    }

    // 绘制桥连接点。
    // 任意相邻两颗牙都已选择时显示空心圈；用户点击确认后改画实心圈。
    const QMap<int, QString>& bridgeMap = (jaw == JawMaxilla) ? m_maxillaMaskToBridge : m_mandibleMaskToBridge;
    for (auto it = bridgeMap.begin(); it != bridgeMap.end(); ++it) {
        const QString bridgeKey = it.value();
        if (!IsBridgeCandidate(bridgeKey)) {
            continue;
        }

        const bool selected = m_selectedBridgeKeys.contains(bridgeKey);
        const QString overlayPath = BridgeOverlayPath(jaw, bridgeKey, selected);
        const QPixmap overlay = LoadCachedPixmap(overlayPath);
        if (!overlay.isNull()) {
            painter->drawPixmap(targetRect, overlay);
        }
    }
}

// QWidget 坐标转换成原始图片坐标。
QPoint ToothTreatmentPlanWidget::ToImagePoint(const QPoint& widgetPoint, const QRect& targetRect) const {
    if (targetRect.isEmpty()) {
        return QPoint(-1, -1);
    }

    // 使用浮点比例换算，避免 4K 或奇数尺寸下整数除法造成边缘像素偏移。
    const double scaleX = static_cast<double>(kSourceImageWidth) / static_cast<double>(targetRect.width());
    const double scaleY = static_cast<double>(kSourceImageHeight) / static_cast<double>(targetRect.height());

    int imageX = static_cast<int>((widgetPoint.x() - targetRect.left()) * scaleX);
    int imageY = static_cast<int>((widgetPoint.y() - targetRect.top()) * scaleY);

    // 对边缘坐标做钳制，防止点在矩形最右/最下边时得到 600 或 400。
    imageX = std::max(0, std::min(kSourceImageWidth - 1, imageX));
    imageY = std::max(0, std::min(kSourceImageHeight - 1, imageY));
    return QPoint(imageX, imageY);
}

// 判断鼠标命中的颌。
bool ToothTreatmentPlanWidget::HitJaw(const QPoint& widgetPoint, Jaw* jaw, QPoint* imagePoint) const {
    if (m_maxillaRect.contains(widgetPoint)) {
        if (jaw) {
            *jaw = JawMaxilla;
        }
        if (imagePoint) {
            *imagePoint = ToImagePoint(widgetPoint, m_maxillaRect);
        }
        return true;
    }

    if (m_mandibleRect.contains(widgetPoint)) {
        if (jaw) {
            *jaw = JawMandible;
        }
        if (imagePoint) {
            *imagePoint = ToImagePoint(widgetPoint, m_mandibleRect);
        }
        return true;
    }

    return false;
}

// 命中牙位。
int ToothTreatmentPlanWidget::HitTooth(Jaw jaw, const QPoint& imagePoint) const {
    const QImage& mask = (jaw == JawMaxilla) ? m_maxillaMask : m_mandibleMask;
    const int maskValue = MaskValueAt(mask, imagePoint);
    if (maskValue <= 0) {
        return 0;
    }

    const QMap<int, int>& mapping = (jaw == JawMaxilla) ? m_maxillaMaskToTooth : m_mandibleMaskToTooth;
    return mapping.value(maskValue, 0);
}

// 命中桥连接点。
QString ToothTreatmentPlanWidget::HitBridge(Jaw jaw, const QPoint& imagePoint) const {
    const QImage& mask = (jaw == JawMaxilla) ? m_maxillaBridgeMask : m_mandibleBridgeMask;
    const int maskValue = MaskValueAt(mask, imagePoint);
    if (maskValue <= 0) {
        return QString();
    }

    const QMap<int, QString>& mapping = (jaw == JawMaxilla) ? m_maxillaMaskToBridge : m_mandibleMaskToBridge;
    return mapping.value(maskValue);
}

// 判断桥连接点是否可以显示。
bool ToothTreatmentPlanWidget::IsBridgeCandidate(const QString& bridgeKey) const {
    const BridgeConnector connector = m_bridgeConnectors.value(bridgeKey);
    if (connector.key.isEmpty()) {
        return false;
    }

    // bridge 不是修复类型。只要两端牙位都存在治疗方案，就显示可点击连接点。
    return m_toothTypeCodes.contains(connector.firstTooth)
        && m_toothTypeCodes.contains(connector.secondTooth);
}

// 判断牙位是否属于指定颌。
bool ToothTreatmentPlanWidget::IsToothInJaw(int toothNumber, Jaw jaw) const {
    if (jaw == JawMaxilla) {
        return toothNumber >= 11 && toothNumber <= 28;
    }

    return toothNumber >= 31 && toothNumber <= 48;
}

// 返回牙位叠加图路径。
QString ToothTreatmentPlanWidget::ToothOverlayPath(Jaw jaw, int toothNumber, const QString& typeCode) const {
    const int imageIndex = MeyerScanTreatmentPlanRules::TreatmentTypeImageIndex(typeCode);
    if (imageIndex <= 0) {
        return QString();
    }

    const QString jawDirName = (jaw == JawMaxilla) ? "maxilla" : "mandible";
    return QDir(m_assetRoot).filePath(QString("%1/%2_%3.png").arg(jawDirName).arg(toothNumber).arg(imageIndex));
}

// 返回桥连接点叠加图路径。
QString ToothTreatmentPlanWidget::BridgeOverlayPath(Jaw jaw, const QString& bridgeKey, bool selected) const {
    const QString jawDirName = (jaw == JawMaxilla) ? "maxilla" : "mandible";
    const QString suffix = selected ? "h" : "b";
    return QDir(m_assetRoot).filePath(QString("%1/%2-%3.png").arg(jawDirName).arg(bridgeKey).arg(suffix));
}

// 读取并缓存 pixmap。
QPixmap ToothTreatmentPlanWidget::LoadCachedPixmap(const QString& filePath) const {
    if (filePath.isEmpty()) {
        return QPixmap();
    }

    const QString normalizedPath = QDir::fromNativeSeparators(filePath);
    if (m_pixmapCache.contains(normalizedPath)) {
        return m_pixmapCache.value(normalizedPath);
    }

    const QPixmap pixmap(normalizedPath);
    m_pixmapCache.insert(normalizedPath, pixmap);
    return pixmap;
}

// 更新 hover 反馈。
void ToothTreatmentPlanWidget::UpdateHoverFeedback(const QPoint& widgetPoint) {
    if (!HasRequiredAssets()) {
        unsetCursor();
        QToolTip::hideText();
        return;
    }

    Jaw jaw = JawMaxilla;
    QPoint imagePoint;
    if (!HitJaw(widgetPoint, &jaw, &imagePoint)) {
        unsetCursor();
        QToolTip::hideText();
        return;
    }

    const QString bridgeKey = HitBridge(jaw, imagePoint);
    if (!bridgeKey.isEmpty() && IsBridgeCandidate(bridgeKey)) {
        setCursor(Qt::PointingHandCursor);
        QToolTip::showText(mapToGlobal(widgetPoint), tr("Click to toggle bridge connector %1").arg(bridgeKey), this);
        return;
    }

    const int toothNumber = HitTooth(jaw, imagePoint);
    if (toothNumber > 0 && !m_currentTreatmentType.isEmpty()) {
        setCursor(Qt::PointingHandCursor);
        QToolTip::showText(mapToGlobal(widgetPoint), tr("Click tooth %1").arg(toothNumber), this);
        return;
    }

    unsetCursor();
    QToolTip::hideText();
}
