//==============================================================================
//  
//  dlg_OnlineJoin.hpp
//  
//==============================================================================

#ifndef DLG_ONLINE_JOIN_HPP
#define DLG_ONLINE_JOIN_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_listbox.hpp"
#include "UI/ui_blankbox.hpp"
#include "UI/ui_bitmap.hpp"

#include "Dialogs/dlg_PopUp.hpp"

#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/GameMgr.hpp"

//==============================================================================
//  dlg_online_join
//==============================================================================

extern void     dlg_online_join_register  ( ui_manager* pManager );
extern ui_win*  dlg_online_join_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_button;

class dlg_online_join : public ui_dialog
{
public:
                        dlg_online_join       ( void );
    virtual            ~dlg_online_join       ( void );

    xbool               Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData);
    virtual void        Destroy             ( void );

    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnNavigate       ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void        OnAccept         ( ui_win* pWin );
    virtual void        OnCancel           ( ui_win* pWin );
    virtual void        OnAlternate       ( ui_win* pWin );
    virtual void        OnDelete         ( ui_win* pWin );
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );
    
    void                FillMatchList       ( void );
    void                PopulateServerInfo  (const server_info *pServerInfo);


protected:
    ui_frame*               m_pFrame1;
    ui_listbox*             m_pMatchList;
    ui_blankbox*            m_pServerDetails;

    ui_text*                m_pHeadset;
    ui_text*                m_pFriend;
    ui_text*                m_pPassword;
    ui_text*                m_pSkillLevel;
    ui_text*                m_pMutationMode;
    ui_text*                m_pPctComplete;
    ui_text*                m_pConnectionSpeed;

    ui_text*                m_pInfoHeadset;
    ui_text*                m_pInfoFriend;
    ui_text*                m_pInfoPassword;
    ui_text*                m_pInfoSkillLevel;
    ui_text*                m_pInfoMutationMode;
    ui_text*                m_pInfoPctComplete;
    ui_text*                m_pInfoConnectionSpeed;

    ui_bitmap*              m_pStatusBox;
    ui_text*                m_pStatusText;


    dlg_popup*              m_PopUp;

    s32                     m_CurrHL;
    f32                     m_RefreshLockedTimeout;

    f32                     m_AutoRefreshTimeout;
    xwstring                m_JoinPassword;

    xarray<s32>             m_SortList;

    s32                     m_LastCount;
    s32                     m_LastSortKey;

    server_info             m_ServerInfo;
};

//==============================================================================
#endif // DLG_ONLINE_JOIN_HPP
//==============================================================================
