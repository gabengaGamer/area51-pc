//==============================================================================
//  
//  dlg_OnlinePlayers.hpp
//  
//==============================================================================

#ifndef DLG_ONLINE_PLAYERS_HPP
#define DLG_ONLINE_PLAYERS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_playerlist.hpp"
#include "UI/ui_blankbox.hpp"

#include "Dialogs/dlg_PopUp.hpp"

#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/GameMgr.hpp"

//==============================================================================
//  dlg_online_join
//==============================================================================

extern void     dlg_online_players_register  ( ui_manager* pManager );
extern ui_win*  dlg_online_players_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_button;

class dlg_online_players : public ui_dialog
{
public:
                        dlg_online_players  ( void );
    virtual            ~dlg_online_players  ( void );

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
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );
    
    void                FillPlayerList      ( void );
    void                PopulateServerInfo  (const server_info *pServerInfo);

    s32                 PingToColor         ( f32 ping, xcolor& responsecolor );

protected:
    ui_frame*           m_pFrame1;
    ui_playerlist*      m_pPlayerList;
    ui_blankbox*        m_pServerDetails;

    ui_text*            m_pGameType;
    ui_text*            m_pCurrentMap;
    ui_text*            m_pNextMap;
    ui_text*            m_pFriendlyFire;
    ui_text*            m_pPctComplete;
    ui_text*            m_pConnectionSpeed;

    ui_text*            m_pGameTypeInfo;
    ui_text*            m_pCurrentMapInfo;
    ui_text*            m_pNextMapInfo;
    ui_text*            m_pFriendlyFireInfo;
    ui_text*            m_pPctCompleteInfo;
    ui_text*            m_pConnectionSpeedInfo;


    s32                 m_CurrHL;
    xbool               m_lockedOut;

    player_score        m_PlayerData[NET_MAX_PLAYERS];

    xbool               m_JoinPasswordEntered;
    xbool               m_JoinPasswordOk;
    xwstring            m_JoinPassword;
};

//==============================================================================
#endif // DLG_ONLINE_PLAYERS_HPP
//==============================================================================
