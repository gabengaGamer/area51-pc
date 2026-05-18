// OutputBar.cpp : implementation file
//
/////////////////////////////////////////////////////////////////////////////

#include "BaseStdAfx.h"
#include "Editor.h"
#include "OutputBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COutputBar

COutputBar::COutputBar()
{

}

COutputBar::~COutputBar()
{
	// TODO: add destruction code here.
}

BOOL COutputBar::Create(CWnd* pParentWnd, UINT nID, LPCTSTR pWindowName, CSize SizeDefault, DWORD dwAlignStyle, DWORD dwStyle)
{
    // Create a font that is friendly to read 
    m_Font.CreatePointFont( 10, "Courier");//"Fixedsys" );

	if (!CEditorWorkspaceTabCtrl::Create(pParentWnd, nID, pWindowName, SizeDefault, dwAlignStyle, dwStyle))
		return FALSE;

	// Define the default style for the output list boxes.
	DWORD WindowStyle = WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | WS_CLIPCHILDREN| ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL ;

	// Create the sheet1 list box.
	if (!m_CompileOutput.Create( WindowStyle, CRect(0,0,0,0), this, IDC_SHEET1 ))
	{
		TRACE0( "Failed to create sheet1.\n" );
		return FALSE;
	}

    // Set the output window for the messages
	if (!m_DebugMsgOutput.Create( WindowStyle, CRect(0,0,0,0), this, IDC_SHEET1 + 10 ))
	{
		TRACE0( "Failed to create sheet1.\n" );
		return FALSE;
	}
    m_DebugMsgOutput.SetFont( &m_Font );


    // Set the output window for the messages
	if (!m_LogMsgOutput.Create( WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | LVS_REPORT,
                                CRect(0,0,0,0), this, IDC_SHEET3 ))
	{
		TRACE0( "Failed to create sheet1.\n" );
		return FALSE;
	}
    m_LogMsgOutput.SetFont( &m_Font );
    m_LogMsgOutput.OnInitialUpdate();

 
	// Create the sheet2 list box.
	WindowStyle = WS_CHILD | WS_VSCROLL | WS_TABSTOP | LBS_NOINTEGRALHEIGHT;
	if (!m_sheet2.Create( WindowStyle, CRect(0,0,0,0), this, IDC_SHEET2 ))
	{
		TRACE0( "Failed to create sheet2.\n" );
		return FALSE;
	}
    m_sheet2.SetFont( &m_Font );

    WindowStyle = WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | WS_CLIPCHILDREN | ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL;
    if (!m_SelectionsOutput.Create( WindowStyle, CRect(0,0,0,0), this, IDC_SHEET4 ))
    {
	    TRACE0( "Failed to create Selectioned sheet.\n" );
	    return FALSE;
    }
    m_SelectionsOutput.SetFont( &m_Font );
    
    
    AddControl(_T("Compile"), &m_CompileOutput);
    AddControl(_T("DebugMsg"), &m_DebugMsgOutput);
    AddControl(_T("Sheet 2"), &m_sheet2);
    AddControl(_T("Log"), &m_LogMsgOutput);
    AddControl(_T("Selected Objects"), &m_SelectionsOutput);

	// Insert text into the list box.
//	m_CompileOutput.AddString(_T("Compile Output..."));
    m_DebugMsgOutput.SetWindowText( _T("Ready 2 Output...\n") );

    //m_CompileOutput.LimitText(1 );

	m_sheet2.AddString(_T("Sheet 2 Output...This is a test"));

    m_LogMsgOutput.SetWindowText( _T("Ready 3 Log Output...\n") );

    SetActiveView(0);

	return TRUE;
}
