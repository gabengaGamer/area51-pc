//==============================================================================
//
//  ConfigManager.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

QString ConfigManager::GetConfigPath(void)
{
    return QCoreApplication::applicationDirPath() + QDir::separator() + "xbmpViewer.inl";
}

//==============================================================================

xbool ConfigManager::LoadConfig(QString& GameType, QString& ThemeType, QByteArray& Geometry, QByteArray& State)
{
    const QString Path = GetConfigPath();
    QFile File(Path);
    if (!File.exists())
        return FALSE;

    if (!File.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        x_DebugMsg("xbmpViewer: failed to open config: %s\n", Path.toLocal8Bit().constData());
        return FALSE;
    }

    QTextStream Stream(&File);
    while (!Stream.atEnd())
    {
        const QString Line = Stream.readLine().trimmed();
        if (Line.isEmpty() || Line.startsWith("#"))
            continue;

        const s32 Equals = Line.indexOf('=');
        if (Equals <= 0)
            continue;

        const QString Key = Line.left(Equals).trimmed();
        const QString Value = Line.mid(Equals + 1).trimmed();

        if (Key == "GameType")
            GameType = Value;
        else if (Key == "ThemeType")
            ThemeType = Value;
        else if (Key == "Geometry")
            Geometry = QByteArray::fromBase64(Value.toLatin1());
        else if (Key == "State")
            State = QByteArray::fromBase64(Value.toLatin1());
    }

    return TRUE;
}

//==============================================================================

xbool ConfigManager::SaveConfig(const QString& GameType, const QString& ThemeType,
                                const QByteArray& Geometry, const QByteArray& State)
{
    const QString Path = GetConfigPath();
    QFile File(Path);
    if (!File.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        x_DebugMsg("xbmpViewer: failed to write config: %s\n", Path.toLocal8Bit().constData());
        return FALSE;
    }

    QTextStream Stream(&File);
    Stream << "GameType=" << GameType << "\n";
    Stream << "ThemeType=" << ThemeType << "\n";
    Stream << "Geometry=" << QString::fromLatin1(Geometry.toBase64()) << "\n";
    Stream << "State=" << QString::fromLatin1(State.toBase64()) << "\n";
    return TRUE;
}
