//==============================================================================
//
//  PreviewPanel.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "PreviewPanel.h"

#include "BitmapPreviewWidget.h"

#include <QDockWidget>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

PreviewPanel::PreviewPanel(QWidget* pParent)
    : QWidget(pParent)
{
    m_pBitmapColor = new BitmapPreviewWidget(this);
    m_pBitmapAlpha = new BitmapPreviewWidget(this);
    m_pBitmapAlpha->SetAlpha(TRUE);

    m_pMipSlider = new QSlider(Qt::Vertical, this);
    m_pMipSlider->setRange(0, 0);
    m_pMipSlider->setTickPosition(QSlider::TicksBothSides);
    m_pMipSlider->setTickInterval(1);
    m_pMipSlider->setMinimumSize(20, 20);
}

//==============================================================================

void PreviewPanel::ConfigureDock(QDockWidget* pDock) const
{
    if (!pDock)
        return;

    pDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
}

//==============================================================================

BitmapPreviewWidget* PreviewPanel::GetColorWidget(void) const
{
    return m_pBitmapColor;
}

//==============================================================================

BitmapPreviewWidget* PreviewPanel::GetAlphaWidget(void) const
{
    return m_pBitmapAlpha;
}

//==============================================================================

QSlider* PreviewPanel::GetMipSlider(void) const
{
    return m_pMipSlider;
}

//==============================================================================

void PreviewPanel::resizeEvent(QResizeEvent* pEvent)
{
    QWidget::resizeEvent(pEvent);

    const s32 SliderSize = 50;
    QRect R = rect();
    R.adjust(1, 1, -1, -1);

    QRect R1 = R;
    QRect R2 = R;
    QRect R3 = R;

    if (R.width() >= R.height())
    {
        R1.setRight(R1.left() + (R.width() - SliderSize) / 2);
        R2.setLeft(R2.right() - (R.width() - SliderSize) / 2);
        R3.setLeft(R1.right());
        R3.setRight(R2.left());
        R3.adjust(4, 4, -4, -4);
        m_pMipSlider->setOrientation(Qt::Vertical);
    }
    else
    {
        R1.setBottom(R1.top() + (R.height() - SliderSize) / 2);
        R2.setTop(R2.bottom() - (R.height() - SliderSize) / 2);
        R3.setTop(R1.bottom());
        R3.setBottom(R2.top());
        R3.adjust(4, 4, -4, -4);
        m_pMipSlider->setOrientation(Qt::Horizontal);
    }

    m_pBitmapColor->setGeometry(R1);
    m_pBitmapAlpha->setGeometry(R2);
    m_pMipSlider->setGeometry(R3);
}
