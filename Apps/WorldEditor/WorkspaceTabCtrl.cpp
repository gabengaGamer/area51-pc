// WorkspaceTabCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "WorkspaceTabCtrl.h"
#include "..\Editor\PaletteView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//=========================================================================
// CWorkspaceTabCtrl
//=========================================================================

CWorkspaceTabCtrl::CWorkspaceTabCtrl()
{
    m_DefaultSize    = CSize(200, 150);
    m_nLastActiveView = 0;
}

//=========================================================================

CWorkspaceTabCtrl::~CWorkspaceTabCtrl()
{
}

//=========================================================================
// CWorkspaceTabCtrl message handlers
//=========================================================================

BOOL CWorkspaceTabCtrl::Create(CWnd* pParentWnd, UINT nID, LPCTSTR pWindowName, CSize SizeDefault, DWORD dwAlignStyle, DWORD dwStyle)
{
    m_DefaultSize = SizeDefault;

    DWORD BarStyle = WS_CHILD | WS_VISIBLE | CBRS_SIZE_DYNAMIC | dwAlignStyle;
    if (dwStyle & CBRS_TOOLTIPS)
        BarStyle |= CBRS_TOOLTIPS;
    if (dwStyle & CBRS_FLYBY)
        BarStyle |= CBRS_FLYBY;

    if (!CToolBar::CreateEx(pParentWnd, TBSTYLE_FLAT, BarStyle, CRect(0, 0, SizeDefault.cx, SizeDefault.cy), nID))
        return FALSE;

    SetWindowText(pWindowName);

    if (!m_TabCtrl.Create(WS_CHILD | WS_VISIBLE | TCS_TOOLTIPS, CRect(0, 0, 0, 0), this, 1))
        return FALSE;

    return TRUE;
}

//=========================================================================

CFrameWnd* CWorkspaceTabCtrl::CreateFrameDocView(CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass, CDocument* pDocument)
{
    ASSERT(pFrameClass);
    ASSERT(pViewClass);

    CFrameWnd* pFrameWnd = DYNAMIC_DOWNCAST(CFrameWnd, pFrameClass->CreateObject());
    if (pFrameWnd == NULL)
        return NULL;

    CCreateContext Context;
    x_memset(&Context, 0, sizeof(Context));
    Context.m_pCurrentDoc  = pDocument;
    Context.m_pNewViewClass = pViewClass;
    Context.m_pCurrentFrame = pFrameWnd;

    if (!pFrameWnd->Create(NULL,
                           _T(""),
                           WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                           CRect(0, 0, 0, 0),
                           this,
                           NULL,
                           0,
                           &Context))
    {
        delete pFrameWnd;
        return NULL;
    }

    pFrameWnd->ShowWindow(SW_HIDE);
    return pFrameWnd;
}

//=========================================================================

BOOL CWorkspaceTabCtrl::AddControl(LPCTSTR pLabel, CWnd* pView)
{
    if (pView == NULL)
        return FALSE;

    const int Index = (int)m_Views.size();
    m_Views.push_back(pView);
    m_TabCtrl.InsertItem(Index, pLabel);

    if (Index == 0)
    {
        m_TabCtrl.SetCurSel(0);
        pView->ShowWindow(SW_SHOW);
    }
    else
    {
        pView->ShowWindow(SW_HIDE);
    }

    LayoutChildren();
    return TRUE;
}

//=========================================================================

CWnd* CWorkspaceTabCtrl::GetView(int Index) const
{
    if ((Index < 0) || (Index >= (int)m_Views.size()))
        return NULL;

    return m_Views[Index];
}

//=========================================================================

void CWorkspaceTabCtrl::SetActiveView(int Index)
{
    if ((Index < 0) || (Index >= (int)m_Views.size()))
        return;

    m_TabCtrl.SetCurSel(Index);
    OnTabSelChange(1, &m_TabCtrl);
}

//=========================================================================

void CWorkspaceTabCtrl::SetCaption(LPCTSTR pCaption)
{
    LPCTSTR pText = pCaption ? pCaption : _T("");
    SetWindowText(pText);

    const int Index = m_TabCtrl.GetCurSel();
    if (Index != -1)
    {
        TCITEM Item;
        x_memset(&Item, 0, sizeof(Item));
        Item.mask    = TCIF_TEXT;
        Item.pszText = const_cast<LPTSTR>(pText);
        m_TabCtrl.SetItem(Index, &Item);
    }
}

//=========================================================================

void CWorkspaceTabCtrl::SetTabImageList(CImageList* pImageList)
{
    m_TabCtrl.SetImageList(pImageList);
}

//=========================================================================

BOOL CWorkspaceTabCtrl::EnableToolTips(BOOL bEnable)
{
    m_TabCtrl.EnableToolTips(bEnable);
    return TRUE;
}

//=========================================================================

void CWorkspaceTabCtrl::OnTabSelChange(int nIDCtrl, CTabCtrl* pTabCtrl)
{
    UpdateView(m_nLastActiveView, FALSE);
    m_nLastActiveView = GetTabCtrl().GetCurSel();
    UpdateView(m_nLastActiveView, TRUE);

    LayoutChildren();
}

//=========================================================================

void CWorkspaceTabCtrl::UpdateView(int Index, BOOL bActivate)
{
    CWnd* pWnd = GetView(Index);
    if (pWnd == NULL)
        return;

    if( pWnd->IsKindOf( RUNTIME_CLASS(CFrameWnd) ) )
    {
        CFrameWnd* pFrame = (CFrameWnd*)pWnd;
        CView*     pView = pFrame->GetActiveView();

        if( pView->IsKindOf( RUNTIME_CLASS(CPaletteView) ) )
        {
            CPaletteView* pPaletteView = (CPaletteView*)pView;
            pPaletteView->OnTabActivate(bActivate);
        }
    }
}

//=========================================================================

CWnd* CWorkspaceTabCtrl::GetWorkspaceView( CRuntimeClass *pViewClass )
{
    CWnd* pWnd=NULL;
    for( s32 i=0; pWnd=GetView(i); i++ )
    {
        if( pWnd->IsKindOf( RUNTIME_CLASS(CFrameWnd) ) )
        {
            CFrameWnd* pFrame = (CFrameWnd*)pWnd;
            CView*     pView = pFrame->GetActiveView();

            if( pView->IsKindOf( pViewClass ) )
            {
                pWnd = pView;
                break;
            }
        }
    }

    return pWnd;
}

//=========================================================================

BOOL CWorkspaceTabCtrl::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    NMHDR* pHdr = (NMHDR*)lParam;
    if (pHdr && (pHdr->hwndFrom == m_TabCtrl.GetSafeHwnd()) && (pHdr->code == TCN_SELCHANGE))
    {
        OnTabSelChange((int)wParam, &m_TabCtrl);
        if (pResult)
            *pResult = 0;
        return TRUE;
    }

    return CToolBar::OnNotify(wParam, lParam, pResult);
}

//=========================================================================

LRESULT CWorkspaceTabCtrl::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    const LRESULT Result = CToolBar::WindowProc(message, wParam, lParam);

    if (message == WM_SIZE)
        LayoutChildren();

    return Result;
}

//=========================================================================

CSize CWorkspaceTabCtrl::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
    return m_DefaultSize;
}

//=========================================================================

void CWorkspaceTabCtrl::LayoutChildren()
{
    if (m_TabCtrl.GetSafeHwnd() == NULL)
        return;

    CRect ClientRect;
    GetClientRect(&ClientRect);

    CRect TabRect(ClientRect);
    TabRect.bottom = min(ClientRect.bottom, ClientRect.top + 24);
    m_TabCtrl.MoveWindow(TabRect);

    CRect ViewRect(ClientRect);
    ViewRect.top = TabRect.bottom;

    const int ActiveIndex = m_TabCtrl.GetCurSel();
    for (int Index = 0; Index < (int)m_Views.size(); ++Index)
    {
        CWnd* pView = m_Views[Index];
        if (pView == NULL)
            continue;

        pView->MoveWindow(ViewRect);
        pView->ShowWindow(Index == ActiveIndex ? SW_SHOW : SW_HIDE);
    }
}
