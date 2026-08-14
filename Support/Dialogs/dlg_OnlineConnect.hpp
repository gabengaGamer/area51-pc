//==============================================================================
//  
//  dlg_OnlineConnect.hpp
//  
//==============================================================================

#ifndef DLG_ONLINE_CONNECT_HPP
#define DLG_ONLINE_CONNECT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_listbox.hpp"

#include "Dialogs/dlg_PopUp.hpp"

#include "NetworkMgr/NetworkMgr.hpp"

//==============================================================================
//  dlg_online_connect
//==============================================================================
enum connect_states
{
    CONNECT_IDLE = 0,
    CONNECT_INIT,
    CONNECT_WAIT,
    CONFIG_INIT,
    CONFIG_ONLINE_WAIT,
    CONNECT_AUTHENTICATE_MACHINE,
    CONNECT_SELECT_USER,
    ACTIVATE_INIT,
    CONNECT_MATCH_INIT,
    CONNECT_AUTHENTICATE_USER,
    CONNECT_FAILED,
    CONNECT_FAILED_WAIT,
    CONNECT_DONE,
    CONNECT_DONE_WAIT,
    CONNECT_DISCONNECT,
    CONNECT_DISCONNECT_WAIT,
    CONNECT_CHECK_MOTD,
    CONNECT_DISPLAY_MOTD,
    NUM_CONNECT_STATES,
};

enum connect_mode
{
    CONNECT_MODE_CONNECT,
    CONNECT_MODE_AUTH_USER,
};

enum cancel_mode
{
    CANCEL_MANAGE,
    OK_ONLY,
    CANCEL_RETRY_MANAGE,

};

extern void     dlg_online_connect_register  ( ui_manager* pManager );
extern ui_win*  dlg_online_connect_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class dlg_online_connect : public ui_dialog
{
public:
                        dlg_online_connect  ( void );
    virtual            ~dlg_online_connect  ( void );
    xbool               Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData);
    virtual void        Destroy             ( void );
    virtual void        Configure           ( connect_mode ConnectMode );

    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnAccept         ( ui_win* pWin );
    virtual void        OnCancel           ( ui_win* pWin );
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );

    void                RefreshUserList     ( void );

protected:
    void                Failed              ( const char* pFailureReason, s32 ErrorCode=0, cancel_mode CancelMode = CANCEL_MANAGE, connect_states RetryDestination = CONNECT_MATCH_INIT );
    void                SetConnectState     ( connect_states State );
    const char*         StateName           ( connect_states State );
    void                UpdateConnectInit   ( void );
    void                UpdateActivateInit  ( void );
    void                UpdateAuthMachine   ( void );
    void                UpdateAuthUser      ( void );



    ui_listbox*         m_pUserList;

    connect_states      m_ConnectState;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;

    // Network interface state.
    f32                 m_Timeout;
    interface_info      m_Info;

    char                m_LabelText[256];     // String ID of reason for failure.
    irect               m_Position;

    s32                 m_NumUsers;
    s32                 m_LastErrorCode;

    cancel_mode         m_CancelMode;
    connect_states      m_RetryDestination; 
};

//==============================================================================
#endif // DLG_ONLINE_CONNECT_HPP
//==============================================================================
