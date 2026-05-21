// ProjectView.cpp : implementation file
//

#include "BaseStdAfx.h"
#include "editor.h"
#include "TextEditor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTextEditor

IMPLEMENT_DYNCREATE(CTextEditor, CFrameWnd)

CTextEditor::CTextEditor()
{
    m_strFontSize      = AfxGetApp()->GetProfileString(_T("Settings"), _T("m_strFontSize"), _T("10"));
	m_strFontName      = AfxGetApp()->GetProfileString(_T("Settings"), _T("m_strFontName"), _T("Verdana"));
    m_CurrentTextColor = RGB(0x00, 0x00, 0xff);
}

CTextEditor::~CTextEditor()
{
}

BEGIN_MESSAGE_MAP(CTextEditor, CFrameWnd)
	//{{AFX_MSG_MAP(CTextEditor)
    ON_WM_CREATE()
	ON_WM_ERASEBKGND()
    ON_COMMAND(ID_PROJ_TEXT_UNDO, OnEditUndo)
    ON_COMMAND(ID_PROJ_TEXT_REDO, OnEditRedo)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_UNDO, OnEditUndoEnable )
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_REDO, OnEditRedoEnable )
	ON_COMMAND(ID_PROJ_TEXT_BOLD,    OnBold)
	ON_COMMAND(ID_PROJ_TEXT_ITALICS, OnItalic)
	ON_COMMAND(ID_PROJ_TEXT_UNDERL,  OnUnderline)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_BOLD,    OnBoldEnable)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_ITALICS, OnItalicEnable)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_UNDERL,  OnUnderlineEnable)
    ON_COMMAND(ID_PROJ_TEXT_COLOR,    OnSetColor)
	ON_COMMAND(ID_PROJ_TEXT_L_ALIGN, OnParagraphLeft)
	ON_COMMAND(ID_PROJ_TEXT_C_ALIGN, OnParagraphCenter)
	ON_COMMAND(ID_PROJ_TEXT_R_ALIGN, OnParagraphRight)
    ON_COMMAND(ID_PROJ_TEXT_BULLETS, OnParagraphBulleted)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_L_ALIGN, OnParagraphLeftEnable)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_C_ALIGN, OnParagraphCenterEnable)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_R_ALIGN, OnParagraphRightEnable)
    ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_BULLETS, OnParagraphBulletedEnable)
	ON_COMMAND(ID_PROJ_TEXT_CUT,   OnCut)
	ON_COMMAND(ID_PROJ_TEXT_COPY,  OnCopy)
	ON_COMMAND(ID_PROJ_TEXT_PASTE, OnPaste)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_CUT,   OnCutEnable)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_COPY,  OnCopyEnable)
	ON_UPDATE_COMMAND_UI(ID_PROJ_TEXT_PASTE, OnPasteEnable)
    ON_COMMAND(ID_PROJ_TEXT_FONT, OnFormatFont)
    ON_COMMAND(ID_PROJ_TEXT_SELECTALL, OnSelectAll)    
	//}}AFX_MSG_MAP
	ON_CBN_SELENDOK(ID_PROJ_TEXT_FONT_TYPE, OnSelEndOk)
	ON_CBN_SELENDOK(ID_PROJ_TEXT_FONT_SIZE, OnSelEndOk)
	ON_NOTIFY(NM_RETURN, ID_PROJ_TEXT_FONT_TYPE, OnReturn)
	ON_NOTIFY(NM_RETURN, ID_PROJ_TEXT_FONT_SIZE, OnReturn)

END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// ALL This code deals with the font selector and the font size and color
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


LRESULT CTextEditor::OnSelEndOkColor( WPARAM wParam, LPARAM lParam )
{
	m_rtf.SetColor( ( COLORREF )wParam );
    m_rtf.SetFocus();	    
	return 0;
}

void CTextEditor::OnReturn(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER( pNMHDR );
	UNREFERENCED_PARAMETER( pResult );
	
	OnSelEndOk( );
}

void CTextEditor::OnSelEndOk()
{
	int iSel = m_wndComboSize.GetCurSel( );

	if ( iSel != CB_ERR ) {
		m_wndComboSize.GetLBText( iSel, m_strFontSize );
	}
	else {
		m_wndComboSize.GetWindowText( m_strFontSize );
	}

	m_wndComboFont.GetWindowText( m_strFontName );

    m_rtf.SetFontName(m_strFontName);
    m_rtf.SetFontSize(atoi(m_strFontSize)*10);
    m_rtf.SetFocus();
}

bool CTextEditor::InitComboFont()
{
    const int FontIndex = m_wndToolBar.CommandToIndex(ID_PROJ_TEXT_FONT_TYPE);
    if (FontIndex == -1)
        return false;

    m_wndToolBar.SetButtonInfo(FontIndex, ID_PROJ_TEXT_FONT_TYPE, TBBS_SEPARATOR, 180);

    CRect Rect;
    m_wndToolBar.GetItemRect(FontIndex, &Rect);
    Rect.bottom += 300;

	if (!m_wndComboFont.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWN | CBS_SORT | CBS_HASSTRINGS | WS_CLIPCHILDREN,
		Rect, &m_wndToolBar, ID_PROJ_TEXT_FONT_TYPE ))
	{
		TRACE0("Failed to create font combo.\n");
		return false;
	}

    static LPCTSTR kFonts[] =
    {
        _T("Arial"),
        _T("Courier New"),
        _T("Tahoma"),
        _T("Times New Roman"),
        _T("Verdana")
    };

    for (int i = 0; i < _countof(kFonts); ++i)
        m_wndComboFont.AddString(kFonts[i]);

    if (m_wndComboFont.FindStringExact(-1, m_strFontName) == CB_ERR)
        m_wndComboFont.AddString(m_strFontName);

    m_wndComboFont.SetWindowText(m_strFontName);

    return true;
}


bool CTextEditor::InitComboSize()
{
    const int SizeIndex = m_wndToolBar.CommandToIndex(ID_PROJ_TEXT_FONT_SIZE);
    if (SizeIndex == -1)
        return false;

    m_wndToolBar.SetButtonInfo(SizeIndex, ID_PROJ_TEXT_FONT_SIZE, TBBS_SEPARATOR, 60);

    CRect Rect;
    m_wndToolBar.GetItemRect(SizeIndex, &Rect);
    Rect.bottom += 250;

	if (!m_wndComboSize.Create( WS_CHILD|WS_VISIBLE|WS_VSCROLL|CBS_DROPDOWN|WS_CLIPCHILDREN,
		Rect, &m_wndToolBar, ID_PROJ_TEXT_FONT_SIZE ))
	{
		TRACE0("Failed to create size combo.\n");
		return false;
	}

	// insert strings into the size combo box.
	m_wndComboSize.AddString( _T( "8" ) );
    m_wndComboSize.AddString( _T( "9" ) );
	m_wndComboSize.AddString( _T( "10" ) );
	m_wndComboSize.AddString( _T( "12" ) );
	m_wndComboSize.AddString( _T( "14" ) );
    m_wndComboSize.AddString( _T( "16" ) );
	m_wndComboSize.AddString( _T( "18" ) );
    m_wndComboSize.AddString( _T( "20" ) );
    m_wndComboSize.AddString( _T( "22" ) );
	m_wndComboSize.AddString( _T( "24" ) );
    m_wndComboSize.AddString( _T( "26" ) );
    m_wndComboSize.AddString( _T( "28" ) );
	m_wndComboSize.AddString( _T( "36" ) );
    m_wndComboSize.AddString( _T( "48" ) );
    m_wndComboSize.AddString( _T( "72" ) );

	// set the selection.
	if ( m_wndComboSize.SelectString( -1, m_strFontSize ) == CB_ERR )
	{
		m_wndComboSize.SetWindowText( m_strFontSize );
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////
// CTextEditor diagnostics

#ifdef _DEBUG
void CTextEditor::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CTextEditor::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CTextEditor message handlers

void CTextEditor::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class		
}

int CTextEditor::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!m_wndToolBar.CreateEx( this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC ) ||
		!m_wndToolBar.LoadToolBar(IDR_PROJ_RICHTEXTCTRL))
	{
		TRACE0("Failed to create toolbar\n");
	}

    if( !InitComboSize() || !InitComboFont() )
    {
        TRACE0("Failed to create font size\n");
    }

    return 0;
}


BOOL CTextEditor::OnEraseBkgnd(CDC* pDC) 
{
	// TODO: Add your message handler code here and/or call default
	return TRUE;
//	return CFrameWnd::OnEraseBkgnd(pDC);
}

void CTextEditor::OnEditUndo()
{
	if (m_rtf.CanUndo())   
		m_rtf.Undo();	
}

void CTextEditor::OnEditRedo()
{
	if (m_rtf.CanRedo())   
		m_rtf.Redo();	
}

void CTextEditor::OnEditUndoEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->Enable(m_rtf.CanUndo()); 
}

void CTextEditor::OnEditRedoEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->Enable(m_rtf.CanRedo()); 
}

void CTextEditor::OnBold()
{
    m_rtf.SetSelectionBold();
}

void CTextEditor::OnBoldEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->SetCheck( m_rtf.IsBold() ); 
}

void CTextEditor::OnItalic()
{
    m_rtf.SetSelectionItalic();
}

void CTextEditor::OnItalicEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->SetCheck( m_rtf.IsItalic() ); 
}

void CTextEditor::OnUnderline()
{
    m_rtf.SetSelectionUnderline();
}

void CTextEditor::OnUnderlineEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->SetCheck( m_rtf.IsUnderline() ); 
}

void CTextEditor::OnSetColor()
{
    CColorDialog Dialog(m_CurrentTextColor, CC_FULLOPEN, this);
	if (Dialog.DoModal() == IDOK)
    {
        m_CurrentTextColor = Dialog.GetColor();
        m_rtf.SetColor(m_CurrentTextColor);
        m_rtf.SetFocus();
	}
}

void CTextEditor::OnParagraphLeft() 
{
	m_rtf.SetParagraphLeft();		
}

void CTextEditor::OnParagraphCenter() 
{
	m_rtf.SetParagraphCenter();			
}

void CTextEditor::OnParagraphRight() 
{
	m_rtf.SetParagraphRight();			
}

void CTextEditor::OnParagraphBulleted() 
{
	m_rtf.SetParagraphBulleted();				
}

void CTextEditor::OnParagraphLeftEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->SetCheck( m_rtf.IsParagraphLeft() ); 
}

void CTextEditor::OnParagraphCenterEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->SetCheck( m_rtf.IsParagraphCenter() ); 
}

void CTextEditor::OnParagraphRightEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->SetCheck( m_rtf.IsParagraphRight() ); 
}

void CTextEditor::OnParagraphBulletedEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->SetCheck( m_rtf.IsBulleted() ); 
}

void CTextEditor::OnCut() 
{
	m_rtf.Cut();				
}

void CTextEditor::OnCopy() 
{
	m_rtf.Copy();					
}

void CTextEditor::OnPaste() 
{
	if (m_rtf.CanPaste())
		 // Replace the selected text with the text in the clipboard.
		 m_rtf.Paste();
}

void CTextEditor::OnCutEnable(CCmdUI* pCmdUI)
{ 
}

void CTextEditor::OnCopyEnable(CCmdUI* pCmdUI)
{ 
}

void CTextEditor::OnPasteEnable(CCmdUI* pCmdUI)
{ 
    pCmdUI->Enable(m_rtf.CanPaste()); 
}

void CTextEditor::OnFormatFont() 
{
    m_rtf.SetFont();
    m_rtf.SetFocus();
}

void CTextEditor::OnSelectAll() 
{
    m_rtf.SetSel(0, -1);
}

void CTextEditor::FileOpen( CString filename ) 
{
    // I obtain the file name
    m_strFilename = filename; 

    if( !m_rtf.FileOpen( filename) )
        x_DebugMsg( "Unable to open read in the readme file" );
}

void CTextEditor::FileSave( CString filename ) 
{
    // I define the file dialog
    m_strFilename = filename; 

    if( !m_rtf.FileSave( filename) )
        x_DebugMsg( "Unable to save the readme file, is it set to read only?" );
}

void CTextEditor::ImportText ( CString filename )
{   
    if( !m_rtf.ImportText( filename) )
        x_DebugMsg( "Unable to open import file" );
}
   
