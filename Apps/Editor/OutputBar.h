// OutputBar.h : header file
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_OUTPUTBAR_H__71ED6C04_9ADF_4BC9_9224_F227C7EB94E3__INCLUDED_)
#define AFX_OUTPUTBAR_H__71ED6C04_9ADF_4BC9_9224_F227C7EB94E3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "..\EDRscDesc\CompErrorDisplayCtrl.h"
#include "..\Editor\OutputCtrl.h"
#include "LogView.h"
#include "WorkspaceTabCtrl.h"

/////////////////////////////////////////////////////////////////////////////
// COutputBar dock window class

class COutputBar : public CEditorWorkspaceTabCtrl
{
public:
	COutputBar();
	virtual ~COutputBar();
    BOOL Create(CWnd* pParentWnd, UINT nID, LPCTSTR pWindowName, CSize SizeDefault, DWORD dwAlignStyle, DWORD dwStyle);

// Attributes
public:
	CListBox		        m_sheet2;
    CFont                   m_Font;
    CompErrorDisplayCtrl    m_CompileOutput;
    COutputCtrl             m_DebugMsgOutput;
    CLogView                m_LogMsgOutput;
    COutputCtrl             m_SelectionsOutput;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OUTPUTBAR_H__71ED6C04_9ADF_4BC9_9224_F227C7EB94E3__INCLUDED_)
