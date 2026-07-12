#pragma once

#include <QImage>
#include <QMap>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QString>
#include <QWidget>

#include <functional>

class QEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPushButton;

// ToothTreatmentPlanWidget 负责“治疗方案选择”中的牙弓图片交互。
//
// 设计边界：
// 1. 本控件只做图片绘制、mask 命中和点击回调，不保存数据库，也不生成订单。
// 2. 牙位状态由 OrderCreateUIImpl 统一维护，本控件只接收状态快照并重绘。
// 3. 控件始终把 QWidget 坐标反算到 600x400 原始图片坐标，保证多分辨率缩放后点击不偏移。
class ToothTreatmentPlanWidget : public QWidget {
public:
    // 点击牙位时回调给外层，toothNumber 使用 FDI 牙位编号，例如 26。
    using ToothClickedCallback = std::function<void(int toothNumber)>;

    // 点击桥连接点时回调给外层，bridgeKey 使用 "26-27" 这种稳定文本。
    using BridgeClickedCallback = std::function<void(const QString& bridgeKey)>;

    // 点击“清空所有”按钮时回调给外层，由外层决定是否弹确认框和清理业务状态。
    using ClearClickedCallback = std::function<void()>;

    explicit ToothTreatmentPlanWidget(QWidget* parent = nullptr);

    // 设置治疗方案资源根目录。
    // 目录下必须包含 maxilla.png、mandible.png、mask*.png 以及 maxilla/mandible 子目录。
    void SetAssetRoot(const QString& assetRoot);

    // 设置当前治疗类型；为空时牙位点击不会触发选择。
    void SetCurrentTreatmentType(const QString& typeCode);

    // 设置当前牙位和桥连接状态。
    // 控件只缓存这份状态用于绘图，不在内部修改业务状态。
    void SetPlanState(const QMap<int, QString>& toothTypeCodes, const QSet<QString>& selectedBridgeKeys);

    // 返回关键资源是否已经加载完整，供测试和日志判断。
    bool HasRequiredAssets() const;

    // 设置牙位点击回调。
    void SetToothClickedCallback(const ToothClickedCallback& callback);

    // 设置桥连接点点击回调。
    void SetBridgeClickedCallback(const BridgeClickedCallback& callback);

    // 设置清空所有点击回调。
    void SetClearClickedCallback(const ClearClickedCallback& callback);

    // Qt 布局读取的推荐尺寸。真实大小由父布局按窗口分辨率分配。
    QSize sizeHint() const override;

    // Qt 布局读取的最小尺寸。低分辨率下仍能完整显示上下颌。
    QSize minimumSizeHint() const override;

protected:
    // 绘制上下颌基础图、牙位治疗叠加图和桥连接点叠加图。
    void paintEvent(QPaintEvent* event) override;

    // 鼠标点击时通过 mask 反查牙位或桥连接点。
    void mousePressEvent(QMouseEvent* event) override;

    // 鼠标移动时刷新手型光标和 tooltip。
    void mouseMoveEvent(QMouseEvent* event) override;

    // 鼠标离开控件后恢复默认光标。
    void leaveEvent(QEvent* event) override;

    // 控件尺寸变化时重新摆放中间的“清空所有”按钮。
    void resizeEvent(QResizeEvent* event) override;

private:
    // 颌枚举避免反复使用 bool 猜测 true/false 的含义。
    enum Jaw {
        JawMaxilla,
        JawMandible
    };

    // 一个桥连接点的静态定义。
    struct BridgeConnector {
        QString key;
        int firstTooth;
        int secondTooth;
        int maskValue;

        BridgeConnector()
            : firstTooth(0),
              secondTooth(0),
              maskValue(0) {
        }
    };

    // 读取基础图、mask 图和桥点 mask 图。
    void LoadAssets();

    // 初始化牙位 mask 像素值到 FDI 牙位号的映射。
    void InitializeToothMaskMaps();

    // 初始化桥连接点 mask 像素值到桥 key 的映射。
    void InitializeBridgeMaskMaps();

    // 根据当前 QWidget 大小计算上下颌图片显示矩形。
    void UpdateImageRects();

    // 把“清空所有”按钮放到上下颌之间，贴近旧软件视频里的位置。
    void UpdateClearButtonGeometry();

    // 绘制单个牙弓。
    void DrawJaw(QPainter* painter, Jaw jaw, const QRect& targetRect);

    // 把 QWidget 坐标转换成 600x400 原始图片坐标。
    QPoint ToImagePoint(const QPoint& widgetPoint, const QRect& targetRect) const;

    // 判断鼠标位置落在哪一颌，并返回原始图片坐标。
    bool HitJaw(const QPoint& widgetPoint, Jaw* jaw, QPoint* imagePoint) const;

    // 用牙位 mask 读取牙位号；命中失败返回 0。
    int HitTooth(Jaw jaw, const QPoint& imagePoint) const;

    // 用桥 mask 读取桥连接点 key；命中失败返回空字符串。
    QString HitBridge(Jaw jaw, const QPoint& imagePoint) const;

    // 判断某个桥连接点是否满足显示条件：相邻两颗牙都已经设置治疗方案。
    bool IsBridgeCandidate(const QString& bridgeKey) const;

    // 判断某颗牙是否属于指定颌。
    bool IsToothInJaw(int toothNumber, Jaw jaw) const;

    // 返回某个牙位治疗叠加图路径。
    QString ToothOverlayPath(Jaw jaw, int toothNumber, const QString& typeCode) const;

    // 返回某个桥连接点叠加图路径。
    QString BridgeOverlayPath(Jaw jaw, const QString& bridgeKey, bool selected) const;

    // 读取 pixmap，并用缓存避免 paintEvent 反复访问磁盘。
    QPixmap LoadCachedPixmap(const QString& filePath) const;

    // 根据鼠标位置更新光标和 tooltip。
    void UpdateHoverFeedback(const QPoint& widgetPoint);

private:
    // 治疗方案资源根目录。
    QString m_assetRoot;

    // 当前治疗类型；为空时禁止选择牙位。
    QString m_currentTreatmentType;

    // 上颌基础图。
    QPixmap m_maxillaPixmap;

    // 下颌基础图。
    QPixmap m_mandiblePixmap;

    // 上颌牙位 mask。
    QImage m_maxillaMask;

    // 下颌牙位 mask。
    QImage m_mandibleMask;

    // 上颌桥连接点 mask。
    QImage m_maxillaBridgeMask;

    // 下颌桥连接点 mask。
    QImage m_mandibleBridgeMask;

    // 上颌图片当前显示矩形。
    QRect m_maxillaRect;

    // 下颌图片当前显示矩形。
    QRect m_mandibleRect;

    // mask 像素值到牙位号的映射。
    QMap<int, int> m_maxillaMaskToTooth;
    QMap<int, int> m_mandibleMaskToTooth;

    // mask 像素值到桥连接点 key 的映射。
    QMap<int, QString> m_maxillaMaskToBridge;
    QMap<int, QString> m_mandibleMaskToBridge;

    // bridgeKey 到桥连接点定义的映射。
    QMap<QString, BridgeConnector> m_bridgeConnectors;

    // toothNumber -> typeCode，来自 OrderCreateUIImpl。
    QMap<int, QString> m_toothTypeCodes;

    // 已被用户确认的桥连接点。
    QSet<QString> m_selectedBridgeKeys;

    // 图片缓存。mutable 只允许 const LoadCachedPixmap 复用缓存，不改变业务状态。
    mutable QMap<QString, QPixmap> m_pixmapCache;

    // 牙位点击回调。
    ToothClickedCallback m_toothClickedCallback;

    // 桥连接点点击回调。
    BridgeClickedCallback m_bridgeClickedCallback;

    // 清空所有点击回调。
    ClearClickedCallback m_clearClickedCallback;

    // 位于上下颌中间的清空按钮。它是控件子对象，由 Qt 父子关系释放。
    QPushButton* m_clearButton;
};
