//==============================================================================
//  
//  dlg_BuddyList.hpp
//  
//==============================================================================

#ifndef DLG_BUDDY_LIST_HPP
#define DLG_BUDDY_LIST_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_listbox.hpp"

#include "dlg_PopUp.hpp"

//==============================================================================
//  dlg_buddy_list
//==============================================================================

extern void     dlg_buddy_list_register  ( ui_manager* pManager );
extern ui_win*  dlg_buddy_list_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_button;

class dlg_buddy_list : public ui_dialog
{
public:
                        dlg_buddy_list ( void );
    virtual            ~dlg_buddy_list ( void );

    xbool               Create                  ( s32                       UserID,
                                                  ui_manager*               pManager,
                                                  ui_manager::dialog_tem*   pDialogTem,
                                                  const irect&              Position,
                                                  ui_win*                   pParent,
                                                  s32                       Flags,
                                                  void*                     pUserData);
    virtual void        Destroy                 ( void );

    virtual void        Render                  ( s32 ox=0, s32 oy=0 );

    virtual void        OnNavigate           ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void        OnAccept             ( ui_win* pWin );
    virtual void        OnCancel               ( ui_win* pWin );
    virtual void        OnDelete             ( ui_win* pWin );
    virtual void        OnAlternate           ( ui_win* pWin );
    virtual void        OnUpdate                ( ui_win* pWin, f32 DeltaTime );

    void                RefreshBuddyList      ( void );

protected:
    ui_frame*           m_pFrame1;
    ui_listbox*         m_pBuddyList;


    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;

    s32                 m_CurrHL;

    xwstring            m_BuddyName;
    xbool               m_BuddyEntered;
    xbool               m_BuddyOk;
};

//==============================================================================
#endif // DLG_BUDDY_LIST_HPP
//==============================================================================
