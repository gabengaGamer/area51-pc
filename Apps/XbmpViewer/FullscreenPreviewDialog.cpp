//==============================================================================
//
//  FullscreenPreviewDialog.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "FullscreenPreviewDialog.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

FullscreenPreviewDialog::FullscreenPreviewDialog(QWidget* pParent)
    : QDialog(pParent)
{
    setWindowTitle("Preview");
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setWindowState(windowState() | Qt::WindowFullScreen);
    setModal(TRUE);
}

//==============================================================================

void FullscreenPreviewDialog::SetImage(const QImage& Image)
{
    m_Image = Image;
    update();
}

//==============================================================================

void FullscreenPreviewDialog::resizeEvent(QResizeEvent* pEvent)
{
    QDialog::resizeEvent(pEvent);
    update();
}

//==============================================================================

void FullscreenPreviewDialog::showEvent(QShowEvent* pEvent)
{
    QDialog::showEvent(pEvent);
    if (!(windowState() & Qt::WindowFullScreen))
        setWindowState(windowState() | Qt::WindowFullScreen);
}

//==============================================================================

void FullscreenPreviewDialog::paintEvent(QPaintEvent* pEvent)
{
    (void)pEvent;
    QPainter Painter(this);

    const s32 Tile = 16;
    for (s32 y = 0; y < height(); y += Tile)
    {
        for (s32 x = 0; x < width(); x += Tile)
        {
            const bool Dark = ((x / Tile) + (y / Tile)) & 1;
            Painter.fillRect(QRect(x, y, Tile, Tile), Dark ? QColor(70, 70, 70) : QColor(110, 110, 110));
        }
    }

    if (m_Image.isNull())
        return;

    QSize Target = m_Image.size();
    Target.scale(size(), Qt::KeepAspectRatio);
    const s32 X = (width() - Target.width()) / 2;
    const s32 Y = (height() - Target.height()) / 2;
    QRect TargetRect(QPoint(X, Y), Target);
    Painter.drawImage(TargetRect, m_Image);
}

//==============================================================================

void FullscreenPreviewDialog::mousePressEvent(QMouseEvent* pEvent)
{
    if (pEvent->button() != Qt::NoButton)
        accept();
    QDialog::mousePressEvent(pEvent);
}

//==============================================================================

void FullscreenPreviewDialog::keyPressEvent(QKeyEvent* pEvent)
{
    if (pEvent->key() == Qt::Key_Escape)
    {
        accept();
        return;
    }
    QDialog::keyPressEvent(pEvent);
}
