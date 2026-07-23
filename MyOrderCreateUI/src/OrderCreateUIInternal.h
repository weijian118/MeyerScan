#pragma once

#include "OrderCreateUIImpl.h"

#include "MeyerQtModuleUtils.h"
#include "TreatmentPlanResourceRules.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDesktopWidget>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <functional>

namespace {
namespace ModuleInfo {
// 模块名用于日志 [Mod:] 字段，必须与工程里的 MEYER_MODULE_NAME 保持一致。
const char* Name = "MeyerScan_OrderCreateUI";

// 模块版本用于 GetModuleVersion()，需要和 Version.rc 同步维护。
const char* Version = "MeyerScan_OrderCreateUI v0.6.0 (2026-07-23)";
}

const char* kOcclusionNatural = "natural";
const char* kOcclusionMaxillaTemporary = "maxilla_temporary";
const char* kOcclusionMandibleTemporary = "mandible_temporary";
const char* kOcclusionFullTemporary = "full_temporary";
const char* kOcclusionRecord = "record";

// 旧安装包把治疗方案散文件放在运行目录下，本常量只用于兼容回退。
// 当前正式方案优先从 MeyerScan_UIResources.dll 注册的 qrc 路径读取，不再发布这些散文件。
const char* kTreatmentPlanRuntimeRelativePath = "Resources/Modules/MyOrderCreateUI/icon/createModule/sacanPlan";

// 治疗方案资源在模块源码目录下的位置，用于测试宿主和开发期 fallback。
const char* kTreatmentPlanSourceRelativePath = "Resources/icon/createModule/sacanPlan";

// 历史资源目录，当前保留 fallback，后续资源迁移完成后可以删除。
const char* kTreatmentPlanLegacyRelativePath = "icon/createModule/sacanPlan";

// 治疗类型按钮需要在普通、悬停和选中状态之间切换不同 PNG。
// QSS 负责按钮背景/边框，子类负责切换资源，并把白色 h 图合成到仅限图标区域的彩色圆底上。
class TreatmentTypeButton : public QToolButton {
public:
    // 只扩展图标状态切换，按钮几何、字体、边框和背景继续由 QSS 管理。
    explicit TreatmentTypeButton(QWidget* parent = nullptr)
        : QToolButton(parent) {
    }

    // 保存普通态和高亮态资源；路径可以是 qrc，也可以是源码调试降级目录。
    void SetIconPaths(const QString& normalPath,
                      const QString& highlightedPath,
                      bool useCircularHighlight) {
        m_normalPath = normalPath;
        m_highlightedPath = highlightedPath;
        m_useCircularHighlight = useCircularHighlight;
        m_normalIcon = LoadIcon(m_normalPath);
        m_highlightedIcon = BuildHighlightedIcon();
        RefreshIcon();
    }

    // 选中态持续使用 h 图，未选中按钮仅在鼠标进入时使用 h 图。
    void SetTypeSelected(bool selected) {
        m_selected = selected;
        setChecked(selected);
        setProperty("typeSelected", selected);
        RefreshIcon();
    }

protected:
    // 鼠标进入按钮时切换到资源约定的 h 图。
    void enterEvent(QEvent* event) override {
        m_hovered = true;
        RefreshIcon();
        QToolButton::enterEvent(event);
    }

    // 鼠标离开后，选中按钮保留 h 图，未选中按钮恢复 b 图。
    void leaveEvent(QEvent* event) override {
        m_hovered = false;
        RefreshIcon();
        QToolButton::leaveEvent(event);
    }

private:
    // 从资源路径加载普通 QIcon；路径失效时返回空图标，让上层日志和资源测试暴露缺失问题。
    QIcon LoadIcon(const QString& path) const {
        return path.isEmpty() || !QFileInfo::exists(path) ? QIcon() : QIcon(path);
    }

    // 为前四种修复类型合成“彩色圆底 + 白色 h 图”。
    //
    // 原始 h PNG 只有白色图形，并不包含视频中的彩色圆底；如果给整个 QToolButton 加背景，
    // 就会出现用户反馈的绿色矩形。这里从 b PNG 的非透明像素自动计算主色，再只在图标画布内画圆，
    // 所有颜色仍来自图片资源，不在 C++ 中维护第二套主题色。种植体由 QSS 绘制整行背景，不走此分支。
    QIcon BuildHighlightedIcon() const {
        if (!m_useCircularHighlight) {
            return LoadIcon(m_highlightedPath);
        }

        QImage normalImage(m_normalPath);
        QImage highlightedImage(m_highlightedPath);
        if (normalImage.isNull() || highlightedImage.isNull()) {
            return QIcon();
        }
        normalImage = normalImage.convertToFormat(QImage::Format_ARGB32);
        highlightedImage = highlightedImage.convertToFormat(QImage::Format_ARGB32);

        // 使用 alpha 作为权重求普通图标主色；抗锯齿半透明像素对结果影响较小，1x/2x 都能得到一致颜色。
        quint64 alphaSum = 0;
        quint64 redSum = 0;
        quint64 greenSum = 0;
        quint64 blueSum = 0;
        for (int y = 0; y < normalImage.height(); ++y) {
            const QRgb* scanLine = reinterpret_cast<const QRgb*>(normalImage.constScanLine(y));
            for (int x = 0; x < normalImage.width(); ++x) {
                const QRgb pixel = scanLine[x];
                const int alpha = qAlpha(pixel);
                if (alpha == 0) {
                    continue;
                }
                alphaSum += static_cast<quint64>(alpha);
                redSum += static_cast<quint64>(qRed(pixel) * alpha);
                greenSum += static_cast<quint64>(qGreen(pixel) * alpha);
                blueSum += static_cast<quint64>(qBlue(pixel) * alpha);
            }
        }
        if (alphaSum == 0) {
            return LoadIcon(m_highlightedPath);
        }

        const QColor circleColor(static_cast<int>(redSum / alphaSum),
                                 static_cast<int>(greenSum / alphaSum),
                                 static_cast<int>(blueSum / alphaSum));
        QImage composedImage(highlightedImage.size(), QImage::Format_ARGB32_Premultiplied);
        composedImage.fill(Qt::transparent);

        // QPainter 的抗锯齿圆只占图标画布，不改变按钮矩形背景；随后把白色 h 图原尺寸叠在圆上。
        QPainter painter(&composedImage);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(circleColor);
        painter.drawEllipse(QRectF(0.5,
                                   0.5,
                                   composedImage.width() - 1.0,
                                   composedImage.height() - 1.0));
        painter.drawImage(0, 0, highlightedImage);
        painter.end();
        return QIcon(QPixmap::fromImage(composedImage));
    }

    // 根据当前交互状态加载图标；QIcon 自身会缓存 pixmap，频繁 hover 不会重复解码图片。
    void RefreshIcon() {
        setIcon((m_selected || m_hovered) ? m_highlightedIcon : m_normalIcon);
    }

    QString m_normalPath;
    QString m_highlightedPath;
    // QIcon 在设置资源路径时一次构建并缓存，hover 时不重复解码/逐像素合成 PNG。
    QIcon m_normalIcon;
    QIcon m_highlightedIcon;
    // 前四种类型需要局部圆底，种植体使用 QSS 整行高亮。
    bool m_useCircularHighlight = false;
    // selected 和 hovered 分开保存，保证选中按钮移出鼠标后仍保持高亮。
    bool m_selected = false;
    bool m_hovered = false;
};

// 把历史/第三方类型编码收口到当前五种修复类型。
QString NormalizeTreatmentTypeCode(const QString& rawCode) {
    QString code = rawCode.trimmed().toLower();
    if (code.isEmpty()) {
        return "crown";
    }
    if (code == "full_crown") {
        return "crown";
    }
    if (code == "planting") {
        return "implant";
    }
    if (code == "crown" || code == "missing" || code == "inlay"
        || code == "veneer" || code == "implant") {
        return code;
    }
    return QString();
}

// 判断两个 FDI 牙位是否在同一牙弓中直接相邻，并返回稳定的小号在前 key。
QString NormalizeAdjacentBridgeKey(int firstTooth, int secondTooth) {
    static const int maxillaOrder[16] = {18, 17, 16, 15, 14, 13, 12, 11, 21, 22, 23, 24, 25, 26, 27, 28};
    static const int mandibleOrder[16] = {48, 47, 46, 45, 44, 43, 42, 41, 31, 32, 33, 34, 35, 36, 37, 38};

    const int* orders[2] = {maxillaOrder, mandibleOrder};
    for (const int* order : orders) {
        for (int index = 0; index < 15; ++index) {
            const int left = order[index];
            const int right = order[index + 1];
            if ((firstTooth == left && secondTooth == right)
                || (firstTooth == right && secondTooth == left)) {
                return QString("%1-%2")
                    .arg(std::min(firstTooth, secondTooth))
                    .arg(std::max(firstTooth, secondTooth));
            }
        }
    }
    return QString();
}

// 从类似 "MeyerScan_UIComponents v0.4.0 (2026-07-05)" 的版本字符串中读取主/次/补丁号。
// 动态加载 DLL 时必须先做版本判断，因为 C++ 虚接口新增方法后，旧 DLL 的 vtable 不包含新槽位。
bool ReadVersionTriplet(const QString& text, int* major, int* minor, int* patch) {
    const int marker = text.indexOf('v');
    if (marker < 0) {
        return false;
    }

    QString versionPart;
    for (int i = marker + 1; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch.isDigit() || ch == '.') {
            versionPart.append(ch);
        } else {
            break;
        }
    }

    const QStringList parts = versionPart.split('.');
    if (parts.size() < 2) {
        return false;
    }

    bool okMajor = false;
    bool okMinor = false;
    bool okPatch = true;
    const int parsedMajor = parts.at(0).toInt(&okMajor);
    const int parsedMinor = parts.at(1).toInt(&okMinor);
    int parsedPatch = 0;
    if (parts.size() >= 3) {
        parsedPatch = parts.at(2).toInt(&okPatch);
    }
    if (!okMajor || !okMinor || !okPatch) {
        return false;
    }

    if (major) {
        *major = parsedMajor;
    }
    if (minor) {
        *minor = parsedMinor;
    }
    if (patch) {
        *patch = parsedPatch;
    }
    return true;
}

// 判断运行时 UIComponents 是否满足当前建单模块需要的 ABI。
// 当前建单模块会调用表格工厂，该接口从 UIComponents v0.4.0 开始提供。
bool IsUIComponentsVersionCompatible(const char* versionUtf8) {
    int major = 0;
    int minor = 0;
    int patch = 0;
    if (!ReadVersionTriplet(QString::fromUtf8(versionUtf8 ? versionUtf8 : ""), &major, &minor, &patch)) {
        return false;
    }

    if (major > 0) {
        return true;
    }
    if (major == 0 && minor > 4) {
        return true;
    }
    return major == 0 && minor == 4 && patch >= 0;
}

}
