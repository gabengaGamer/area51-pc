#if !defined(AFX_EDITOR_WORKSPACETABCTRL_H__C470295D_D622_42A3_81C8_30039D622991__INCLUDED_)
#define AFX_EDITOR_WORKSPACETABCTRL_H__C470295D_D622_42A3_81C8_30039D622991__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <vector>

class CEditorWorkspaceTabCtrl : public CToolBar
{
public:
	CEditorWorkspaceTabCtrl();
	virtual ~CEditorWorkspaceTabCtrl();

    BOOL Create(CWnd* pParentWnd, UINT nID, LPCTSTR pWindowName, CSize SizeDefault, DWORD dwAlignStyle, DWORD dwStyle);
    CFrameWnd* CreateFrameDocView(CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass, CDocument* pDocument);
    BOOL AddControl(LPCTSTR pLabel, CWnd* pView);
    CWnd* GetView(int Index) const;
    void SetActiveView(int Index);
    void SetCaption(LPCTSTR pCaption);
    void SetTabImageList(CImageList* pImageList);
    BOOL EnableToolTips(BOOL bEnable);
    CTabCtrl& GetTabCtrl() { return m_TabCtrl; }

    virtual void OnTabSelChange(int nIDCtrl, CTabCtrl* pTabCtrl);
    CWnd* GetWorkspaceView(CRuntimeClass* pViewClass);

protected:
    virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);

    void UpdateView(int Index, BOOL bActivate);
    void LayoutChildren();

    CTabCtrl            m_TabCtrl;
    std::vector<CWnd*>  m_Views;
    CSize               m_DefaultSize;
    int                 m_nLastActiveView;
};

#endif // !defined(AFX_EDITOR_WORKSPACETABCTRL_H__C470295D_D622_42A3_81C8_30039D622991__INCLUDED_)
