// =============================================================================
// 文件: CaptureImageView.h
// 作用: 声明保持 1024:455 逻辑宽高比的采集图像预览控件。
// =============================================================================
#pragma once

#include <QImage>
#include <QLabel>

class CaptureImageView : public QLabel
{
public:
    // 创建居中预览控件，默认显示等待文本。
    explicit CaptureImageView(QWidget* parent = nullptr);
    // 保存图像副本并按当前控件尺寸等比缩放。
    void SetImage(const QImage& image);
    // 清空图像并显示状态文本。
    void SetMessage(const QString& message);

protected:
    // 窗口大小变化时重新缩放，不修改原始图像数据。
    void resizeEvent(QResizeEvent* event) override;

private:
    // 使用 SmoothTransformation 生成当前显示 pixmap。
    void UpdatePixmap();
    QImage m_image;
};
