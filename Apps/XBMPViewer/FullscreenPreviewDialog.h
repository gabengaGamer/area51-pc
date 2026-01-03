//==============================================================================
//
//  FullscreenPreviewDialog.h
//
//==============================================================================

#ifndef XBMP_FULLSCREEN_PREVIEW_DIALOG_H
#define XBMP_FULLSCREEN_PREVIEW_DIALOG_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QDialog>
#include <QImage>

//==============================================================================
//  FullscreenPreviewDialog CLASS
//==============================================================================

class FullscreenPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    FullscreenPreviewDialog  (QWidget* pParent = NULL);
    void SetImage            (const QImage& Image);
						     
protected:                   
    void resizeEvent         (QResizeEvent* pEvent) override;
    void showEvent           (QShowEvent* pEvent) override;
    void paintEvent          (QPaintEvent* pEvent) override;
    void mousePressEvent     (QMouseEvent* pEvent) override;
    void keyPressEvent       (QKeyEvent* pEvent) override;

private:
    QImage  m_Image;
};

//==============================================================================
#endif // XBMP_FULLSCREEN_PREVIEW_DIALOG_H
//==============================================================================
