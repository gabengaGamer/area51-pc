//==============================================================================
//
//  ConvertUtils.h
//
//==============================================================================

#ifndef XBMP_VIEWER_CONVERT_UTILS_H
#define XBMP_VIEWER_CONVERT_UTILS_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"
#include "aux_Bitmap.hpp"

#include <QString>

//==============================================================================

xbool           IsPowerOfTwo             (s32 Value);
xbitmap::format ConvertFormatString      (const QString& Format);
xbitmap::format RemapFormatForHobbit     (xbitmap::format Format);
void            PatchHobbitFormatInFile  (const QString& FileName, xbitmap::format Format);

//==============================================================================
#endif // XBMP_VIEWER_CONVERT_UTILS_H
//==============================================================================
