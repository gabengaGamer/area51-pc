//==============================================================================
//
//  SettingsDialog.h
//
//==============================================================================

#ifndef XBMP_SETTINGS_DIALOG_H
#define XBMP_SETTINGS_DIALOG_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QDialog>

//==============================================================================

class QComboBox;

//==============================================================================
//  SettingsDialog CLASS
//==============================================================================

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog    (QWidget* pParent = NULL);

    void     SetGameType       (const QString& GameType);
    void     SetThemeType      (const QString& ThemeType);

    QString  GetGameType       (void) const;
    QString  GetThemeType      (void) const;

private:
    QComboBox* m_pGameType;
    QComboBox* m_pThemeType;
};

//==============================================================================
#endif // XBMP_SETTINGS_DIALOG_H
//==============================================================================
