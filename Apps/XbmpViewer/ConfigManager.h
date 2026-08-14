//==============================================================================
//
//  ConfigManager.h
//
//==============================================================================

#ifndef XBMP_CONFIG_MANAGER_H
#define XBMP_CONFIG_MANAGER_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QString>

//==============================================================================
//  ConfigManager CLASS
//==============================================================================

class ConfigManager
{
public:
    static QString GetConfigPath  (void);
    static xbool   LoadConfig     (QString& GameType, QString& ThemeType, QByteArray& Geometry, QByteArray& State);
    static xbool   SaveConfig     (const QString& GameType, const QString& ThemeType, const QByteArray& Geometry, const QByteArray& State);
};

//==============================================================================
#endif // XBMP_CONFIG_MANAGER_H
//==============================================================================
