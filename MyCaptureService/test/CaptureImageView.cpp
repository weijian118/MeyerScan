// =============================================================================
// 文件: CaptureImageView.cpp
// 作用: 实现采集图像等比缩放和空状态显示。
// =============================================================================
#include "CaptureImageView.h"

#include <QPixmap>
#include <QResizeEvent>

CaptureImageView::CaptureImageView(QWidget* parent)
    : QLabel(parent)
{
    setObjectName("CaptureImageView");
    setAlignment(Qt::AlignCenter);
    setMinimumSize(512, 228);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CaptureImageView::SetImage(const QImage& image)
{
    m_image = image.copy();
    setText(QString());
    UpdatePixmap();
}

void CaptureImageView::SetMessage(const QString& message)
{
    m_image = QImage();
    setPixmap(QPixmap());
    setText(message);
}

void CaptureImageView::resizeEvent(QResizeEvent* event)
{
    QLabel::resizeEvent(event);
    UpdatePixmap();
}

void CaptureImageView::UpdatePixmap()
{
    if (m_image.isNull())
    {
        return;
    }
    const QSize available = size() - QSize(24, 24);
    if (available.width() <= 0 || available.height() <= 0)
    {
        return;
    }
    setPixmap(QPixmap::fromImage(m_image).scaled(
        available, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
