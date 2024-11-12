// XBMPSettings.cpp : implementation file
//

#include "stdafx.h"
#include "xbmpviewer.h"
#include "ConvertSettingsDialog.h"

/////////////////////////////////////////////////////////////////////////////
// CConvertSettingsDialog dialog

CConvertSettingsDialog::CConvertSettingsDialog(CWnd* pParent /*=NULL*/)
    : CDialog(CConvertSettingsDialog::IDD, pParent)
{
    //Default settings INI.
    m_SelectedPlatform   = _T("PC");
    m_GenericCompression = 0;
    m_SelectedFormat     = _T("32_ARGB_8888");
    m_MipLevels          = 0;
}

void CConvertSettingsDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_CBString(pDX, IDC_PLATFORM_COMBO, m_SelectedPlatform);      //Platform definications
    DDX_Check(pDX, IDC_GENERIC_COMPRESSION, m_GenericCompression);  //Generic definications
    DDX_CBString(pDX, IDC_FORMAT_COMBO, m_SelectedFormat);          //Compression definications
    DDX_Text(pDX, IDC_MIP_EDIT, m_MipLevels);                       //Mip Levels definications
}

BEGIN_MESSAGE_MAP(CConvertSettingsDialog, CDialog)
    //ON_BN_CLICKED(IDC_GENERIC_COMPRESSION, &CConvertSettingsDialog::OnUpdateDialog)
END_MESSAGE_MAP()

BOOL CConvertSettingsDialog::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_CtrlPlatformCombo.SubclassDlgItem(IDC_PLATFORM_COMBO, this);
    m_CtrlPlatformCombo.AddString(_T("PC"));
    m_CtrlPlatformCombo.AddString(_T("Xbox"));
    m_CtrlPlatformCombo.AddString(_T("PS2"));
    m_CtrlPlatformCombo.AddString(_T("GameCube"));
    m_CtrlPlatformCombo.AddString(_T("Native"));

    m_CtrlFormatCombo.SubclassDlgItem(IDC_FORMAT_COMBO, this);
    m_CtrlFormatCombo.AddString(_T("32_RGBA_8888"));
    m_CtrlFormatCombo.AddString(_T("32_RGBU_8888"));
    m_CtrlFormatCombo.AddString(_T("32_ARGB_8888"));
    m_CtrlFormatCombo.AddString(_T("32_URGB_8888"));
    m_CtrlFormatCombo.AddString(_T("24_RGB_888"));
    m_CtrlFormatCombo.AddString(_T("16_RGBA_4444"));
    m_CtrlFormatCombo.AddString(_T("16_ARGB_4444"));
    m_CtrlFormatCombo.AddString(_T("16_RGBA_5551"));
    m_CtrlFormatCombo.AddString(_T("16_RGBU_5551"));
    m_CtrlFormatCombo.AddString(_T("16_ARGB_1555"));
    m_CtrlFormatCombo.AddString(_T("16_URGB_1555"));
    m_CtrlFormatCombo.AddString(_T("16_RGB_565"));
    
    //Initialize settings for UI.
    m_CtrlPlatformCombo.SetCurSel(0);  //"PC" by default.
    m_CtrlFormatCombo.SetCurSel(2);    //"32_ARGB_8888" by default, for PC.

    return TRUE;
}

void CConvertSettingsDialog::OnUpdateDialog()
{
    UpdateData(TRUE);
    
    m_CtrlFormatCombo.ResetContent();
    m_CtrlPlatformCombo.ResetContent();
    
    if (m_GenericCompression)
    {
        m_CtrlPlatformCombo.AddString(_T("PC"));
        m_CtrlPlatformCombo.AddString(_T("Xbox"));
    }
    else
    {
        m_CtrlPlatformCombo.AddString(_T("PC"));
        m_CtrlPlatformCombo.AddString(_T("Xbox"));
        m_CtrlPlatformCombo.AddString(_T("PS2"));
        m_CtrlPlatformCombo.AddString(_T("GameCube"));
        m_CtrlPlatformCombo.AddString(_T("Native"));
    }
    
    m_CtrlFormatCombo.AddString(_T("32_RGBA_8888"));
    m_CtrlFormatCombo.AddString(_T("32_RGBU_8888"));
    m_CtrlFormatCombo.AddString(_T("32_ARGB_8888"));
    m_CtrlFormatCombo.AddString(_T("32_URGB_8888"));
    m_CtrlFormatCombo.AddString(_T("24_RGB_888"));
    m_CtrlFormatCombo.AddString(_T("16_RGBA_4444"));
    m_CtrlFormatCombo.AddString(_T("16_ARGB_4444"));
    m_CtrlFormatCombo.AddString(_T("16_RGBA_5551"));
    m_CtrlFormatCombo.AddString(_T("16_RGBU_5551"));
    m_CtrlFormatCombo.AddString(_T("16_ARGB_1555"));
    m_CtrlFormatCombo.AddString(_T("16_URGB_1555"));
    m_CtrlFormatCombo.AddString(_T("16_RGB_565"));

    if (m_GenericCompression)
    {
        m_CtrlFormatCombo.AddString(_T("DXT1"));
        m_CtrlFormatCombo.AddString(_T("DXT2"));
        m_CtrlFormatCombo.AddString(_T("DXT3"));
        m_CtrlFormatCombo.AddString(_T("DXT4"));
        m_CtrlFormatCombo.AddString(_T("DXT5"));
        m_CtrlFormatCombo.AddString(_T("A8"));
    }

    //Update settings for UI.
    m_CtrlPlatformCombo.SetCurSel(0);  //"PC" by default.
    m_CtrlFormatCombo.SetCurSel(2);    //"32_ARGB_8888" by default, for PC.
    

    UpdateData(FALSE);
}
