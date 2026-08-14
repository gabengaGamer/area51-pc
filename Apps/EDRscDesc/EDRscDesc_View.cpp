// EDRscDesc_View.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "EDRscDesc_View.h"
#include "FlatRscList_View.h"

#include "TreeRsclist_View.h"

enum
{
    IDR_RSCDESC_VIEW_TAB = AFX_IDW_PANE_FIRST + 101
};

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// EDRscDesc_View

IMPLEMENT_DYNCREATE(EDRscDesc_View, CView)

EDRscDesc_View::EDRscDesc_View()
{
}

EDRscDesc_View::~EDRscDesc_View()
{
}


BEGIN_MESSAGE_MAP(EDRscDesc_View, CView)
    //{{AFX_MSG_MAP(EDRscDesc_View)
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// EDRscDesc_View drawing

void EDRscDesc_View::OnDraw(CDC* pDC)
{
    CDocument* pDoc = GetDocument();
    // TODO: add draw code here
    UNUSED_ALWAYS( pDoc );
}

/////////////////////////////////////////////////////////////////////////////
// EDRscDesc_View diagnostics

#ifdef _DEBUG
void EDRscDesc_View::AssertValid() const
{
    CView::AssertValid();
}

void EDRscDesc_View::Dump(CDumpContext& dc) const
{
    CView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// EDRscDesc_View message handlers

int EDRscDesc_View::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
    if (CView::OnCreate(lpCreateStruct) == -1)
        return -1;
    
    if( !m_TabCtrl.Create(this, IDR_RSCDESC_VIEW_TAB, _T("Resources"),
        CSize(200, 150), CBRS_TOP, 0 ) )
    {
        TRACE0("Failed to create resource tab window\n");
        return -1;
    }

    CCreateContext* pContext  = (CCreateContext*)lpCreateStruct->lpCreateParams;
    CDocument*      pDocument = pContext ? pContext->m_pCurrentDoc : GetDocument();

    CFrameWnd* pFrameWnd = m_TabCtrl.CreateFrameDocView(
        RUNTIME_CLASS(CFrameWnd), RUNTIME_CLASS(CTreeRsclist_View), pDocument);
    m_TabCtrl.AddControl(_T("Path View"), pFrameWnd);

    pFrameWnd = m_TabCtrl.CreateFrameDocView(
        RUNTIME_CLASS(CFrameWnd), RUNTIME_CLASS(CFlatRscList_View), pDocument);
    m_TabCtrl.AddControl(_T("Flat View"), pFrameWnd);

    return 0;
}

BOOL EDRscDesc_View::OnEraseBkgnd(CDC* pDC) 
{
    // TODO: Add your message handler code here and/or call default
    return TRUE;
}

void EDRscDesc_View::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);

    if( m_TabCtrl.GetSafeHwnd() )
    {
        m_TabCtrl.MoveWindow( 0, 0, cx, cy );
    }
}
