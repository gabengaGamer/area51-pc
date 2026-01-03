//==============================================================================
//
//  BitmapPreviewWidget.h
//
//==============================================================================

#ifndef XBMP_BITMAP_PREVIEW_WIDGET_H
#define XBMP_BITMAP_PREVIEW_WIDGET_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QImage>
#include <QWidget>

#include <functional>

//==============================================================================
//  BitmapPreviewWidget CLASS
//==============================================================================

class BitmapPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BitmapPreviewWidget  (QWidget* pParent = NULL);
								  
    void SetImage                 (const QImage& Image);
    void SetAlpha                 (xbool Alpha);
    void SetHoverCallback         (const std::function<void(s32, s32, s32, s32, s32, s32)>& Callback);
						          
signals:                          
    void ImageClicked             (const QImage& Image, xbool Alpha);
						          
protected:                        
    void paintEvent               (QPaintEvent* pEvent) override;
    void mouseMoveEvent           (QMouseEvent* pEvent) override;
    void mousePressEvent          (QMouseEvent* pEvent) override;
    void leaveEvent               (QEvent* pEvent) override;
						          
private:                          
    QRect GetTargetRect           (void) const;

private:
    QImage m_Image;
    xbool m_Alpha;
    std::function<void(s32, s32, s32, s32, s32, s32)> m_OnHover;
};

//==============================================================================
#endif // XBMP_BITMAP_PREVIEW_WIDGET_H
//==============================================================================
