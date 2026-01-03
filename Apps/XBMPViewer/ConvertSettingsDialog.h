//==============================================================================
//
//  ConvertSettingsDialog.h
//
//==============================================================================

#ifndef XBMP_CONVERT_SETTINGS_DIALOG_H
#define XBMP_CONVERT_SETTINGS_DIALOG_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QDialog>

//==============================================================================

class QCheckBox;
class QComboBox;
class QSpinBox;

//==============================================================================
//  ConvertSettingsDialog CLASS
//==============================================================================

class ConvertSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConvertSettingsDialog    (QWidget* pParent = NULL);
								      
    QString GetPlatform               (void) const;
    QString GetFormat                 (void) const;
    s32     GetMipLevels              (void) const;
    xbool   GetGenericCompression     (void) const;
									  
private slots:                        
    void OnGenericCompressionChanged  (s32 State);
    void OnPlatformChanged            (s32 Index);
									  
private:                              
    void UpdatePlatformList           (void);
    void UpdateFormatsByPlatform      (const QString& Platform, xbool GenericCompression);

private:
    QComboBox*  m_pPlatform;
    QComboBox*  m_pFormat;
    QSpinBox*   m_pMipLevels;
    QCheckBox*  m_pGenericCompression;
};

//==============================================================================
#endif // XBMP_CONVERT_SETTINGS_DIALOG_H
//==============================================================================
