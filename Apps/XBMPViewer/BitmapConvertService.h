//==============================================================================
//
//  BitmapConvertService.h
//
//==============================================================================

#ifndef XBMP_VIEWER_BITMAP_CONVERT_SERVICE_H
#define XBMP_VIEWER_BITMAP_CONVERT_SERVICE_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QString>
#include <QVector>

//==============================================================================

struct FileRecord;

//==============================================================================
//  BitmapConvertService CLASS
//==============================================================================

class BitmapConvertService
{
public:
    struct ContextNeeds
    {
        xbool NeedTga;
        xbool NeedXbmp;
    };

    ContextNeeds GetContextNeeds  (const QVector<const FileRecord*>& Records) const;
								  
    void ConvertToTga             (const QVector<const FileRecord*>& Records,
                                   const QString& OutputDir) const;
    void ConvertToXbmp            (const QVector<const FileRecord*>& Records,
                                   const QString& OutputDir,
                                   const QString& Platform,
                                   xbitmap::format TargetFormat,
                                   s32 MipLevels,
                                   const QString& SelectedGameType) const;
    void ConvertContext           (const QVector<const FileRecord*>& Records,
                                   const QString& OutputDir,
                                   const QString& Platform,
                                   xbitmap::format TargetFormat,
                                   s32 MipLevels,
                                   const QString& SelectedGameType) const;
};

//==============================================================================
#endif // XBMP_VIEWER_BITMAP_CONVERT_SERVICE_H
//==============================================================================
