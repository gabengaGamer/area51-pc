//==============================================================================
//
//  BitmapPreviewWidget.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "BitmapPreviewWidget.h"

#include <QMouseEvent>
#include <QPainter>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

BitmapPreviewWidget::BitmapPreviewWidget(QWidget* pParent)
    : QWidget(pParent)
    , m_Alpha(FALSE)
{
    setMouseTracking(TRUE);
}

//==============================================================================

void BitmapPreviewWidget::SetImage(const QImage& Image)
{
    m_Image = Image;
    update();
}

//==============================================================================

void BitmapPreviewWidget::SetAlpha(xbool Alpha)
{
    m_Alpha = Alpha;
}

//==============================================================================

void BitmapPreviewWidget::SetHoverCallback(const std::function<void(s32, s32, s32, s32, s32, s32)>& Callback)
{
    m_OnHover = Callback;
}

//==============================================================================

void BitmapPreviewWidget::paintEvent(QPaintEvent* pEvent)
{
    (void)pEvent;
    QPainter Painter(this);
    Painter.fillRect(rect(), palette().window());

    if (m_Image.isNull())
        return;

    const QRect Target = GetTargetRect();
    Painter.drawImage(Target, m_Image);
}

//==============================================================================

void BitmapPreviewWidget::mouseMoveEvent(QMouseEvent* pEvent)
{
    if (!m_OnHover || m_Image.isNull())
        return;

    const QRect Target = GetTargetRect();
    if (!Target.contains(pEvent->pos()))
        return;

    const s32 X = (pEvent->pos().x() - Target.left()) * m_Image.width() / Target.width();
    const s32 Y = (pEvent->pos().y() - Target.top()) * m_Image.height() / Target.height();

    if ((X < 0) || (Y < 0) || (X >= m_Image.width()) || (Y >= m_Image.height()))
        return;

    const QColor Color = QColor::fromRgba(m_Image.pixel(X, Y));
    m_OnHover(X, Y, Color.red(), Color.green(), Color.blue(), Color.alpha());
}

//==============================================================================

void BitmapPreviewWidget::mousePressEvent(QMouseEvent* pEvent)
{
    if ((pEvent->button() == Qt::LeftButton) && !m_Image.isNull())
        emit ImageClicked(m_Image, m_Alpha);
    QWidget::mousePressEvent(pEvent);
}

//==============================================================================

void BitmapPreviewWidget::leaveEvent(QEvent* pEvent)
{
    (void)pEvent;
    if (m_OnHover)
        m_OnHover(-1, -1, 0, 0, 0, 0);
}

//==============================================================================

QRect BitmapPreviewWidget::GetTargetRect(void) const
{
    const QSize ImageSize = m_Image.size();
    if (ImageSize.isEmpty())
        return QRect();

    QSize TargetSize = ImageSize;
    TargetSize.scale(size(), Qt::KeepAspectRatio);

    const s32 X = (width() - TargetSize.width()) / 2;
    const s32 Y = (height() - TargetSize.height()) / 2;
    return QRect(QPoint(X, Y), TargetSize);
}
