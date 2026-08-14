//==============================================================================
//
//  XBMPViewerConfig.h
//
//==============================================================================

#ifndef XBMP_VIEWER_CONFIG_H
#define XBMP_VIEWER_CONFIG_H

//==============================================================================
//  INCLUDES/DEFINES
//==============================================================================

#include <new>

#ifdef new
#undef new
#endif

#ifdef array
#undef array
#endif

#define USE_SYSTEM_NEW_DELETE
#define X_DISABLE_LEGACY_ARRAY_MACRO

#include "x_files.hpp"

#ifdef new
#undef new
#endif

#ifdef array
#undef array
#endif

//==============================================================================
#endif // XBMP_VIEWER_CONFIG_H
//==============================================================================