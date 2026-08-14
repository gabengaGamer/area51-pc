//=========================================================================
//
//  dlg_OnlineConnect.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_listbox.hpp"

#include "dlg_OnlineConnect.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/MatchMgr.hpp"
#include "e_Network.hpp"

#if defined(CONFIG_VIEWER)
#include "../../Apps/ArtistViewer/Config.hpp"
#else
#include "../../Apps/GameApp/Config.hpp"
#endif

//=========================================================================
//  Main Menu Dialog
//=========================================================================

enum online_connect_controls
{
    IDC_ONLINE_CONNECT_LISTBOX,
};


ui_manager::control_tem OnlineConnectControls[] = 
{
    { IDC_ONLINE_CONNECT_LISTBOX,   "IDS_SIGN_IN",          "listbox",  70,  60, 220, 238, 0, 0, 1, 1, ui_win::WF_VISIBLE },
};


ui_manager::dialog_tem OnlineConnectDialog =
{
    "IDS_ONLINE_CONNECT",
        1, 9,
        sizeof(OnlineConnectControls)/sizeof(ui_manager::control_tem),
        &OnlineConnectControls[0],
        0
};

//=========================================================================
//  Defines
//=========================================================================

//=========================================================================
//  Structs
//=========================================================================

//========================================================================= 
//  Registration function
//========================================================================= 

void dlg_online_connect_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "connect info", &OnlineConnectDialog, &dlg_online_connect_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_online_connect_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_online_connect* pDialog = new dlg_online_connect;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_online_connect
//=========================================================================

dlg_online_connect::dlg_online_connect( void )    
{    
}

//=========================================================================

dlg_online_connect::~dlg_online_connect( void )
{
}

//=========================================================================

xbool dlg_online_connect::Create( s32                        UserID,
                                 ui_manager*                pManager,
                                 ui_manager::dialog_tem*    pDialogTem,
                                 const irect&               Position,
                                 ui_win*                    pParent,
                                 s32                        Flags,
                                 void*                      pUserData )
{
    xbool   Success = FALSE;

    (void)pUserData;

    ASSERT( pManager );

    // Do dialog creation
    Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    // find controls
    m_pUserList     = (ui_listbox*) FindChildByID( IDC_ONLINE_CONNECT_LISTBOX  );

    // hide them
    m_pUserList  ->SetFlag(ui_win::WF_VISIBLE, FALSE);

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );

    SetNavText( navText );


    // set up user list
    m_pUserList->SetActive( TRUE );
    m_pUserList->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pUserList->DisableFrame();
    m_pUserList->SetExitOnSelect(FALSE);
    m_pUserList->SetExitOnBack(TRUE);
    m_pUserList->EnableHeaderBar();
    m_pUserList->SetHeaderBarColor( xcolor(19,59,14,196) );
    m_pUserList->SetHeaderColor( xcolor(255,252,204,255) );

    // set initial focus
    GotoControl( (ui_control*)m_pUserList );
    m_CurrentControl = IDC_ONLINE_CONNECT_LISTBOX;
    m_PopUp = NULL;

    // initialize connect state
    m_ConnectState = CONNECT_IDLE;

    // disable highlight
    g_UiMgr->DisableScreenHighlight();
    m_Position = Position;

    // initialize screen scaling
    InitScreenScaling( Position );

    // make the dialog active
    SetState( DIALOG_STATE_ACTIVE );

    // Return success code
    return Success;
}

//=========================================================================

void dlg_online_connect::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_online_connect::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb = g_UiMgr->GetUserBounds( m_UserID );

    g_UiMgr->RenderGouraudRect( rb, xcolor(0,0,0,180),
        xcolor(0,0,0,180),
        xcolor(0,0,0,180),
        xcolor(0,0,0,180), FALSE);


    // render transparent screen
    rb.l = m_CurrPos.l + 22;
    rb.t = m_CurrPos.t;
    rb.r = m_CurrPos.r - 23;
    rb.b = m_CurrPos.b;

    g_UiMgr->RenderGouraudRect( rb, xcolor(56,115,58,64),
        xcolor(56,115,58,64),
        xcolor(56,115,58,64),
        xcolor(56,115,58,64), FALSE);


    // render the screen bars
    s32 y = rb.t + offset;    

    while ( y < rb.b )
    {
        irect bar;

        if ( ( y + width ) > rb.b )
        {
            bar.Set( rb.l, y, rb.r, rb.b );
        }
        else
        {
            bar.Set( rb.l, y, rb.r, ( y + width ) );
        }

        // draw the bar
        g_UiMgr->RenderGouraudRect( bar, xcolor(56,115,58,30),
            xcolor(56,115,58,30),
            xcolor(56,115,58,30),
            xcolor(56,115,58,30), FALSE);

        y += gap;
    }

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//=========================================================================

void dlg_online_connect::OnAccept( ui_win* pWin )
{
    (void)pWin;

    // check for match manager busy
    if( g_MatchMgr.IsBusy() || g_NetworkMgr.IsOnlineStateChanging() )
    {
        // wait for the user accounts to populate the listbox
        return;
    }

    switch( m_ConnectState )
    {
    case CONNECT_SELECT_USER:
        {
            // check if this is an existing user
            // get the profile index from the list
            s32 index = m_pUserList->GetSelectedItemData();
            if ( index < m_NumUsers ) //user exists 
            {
                // tell the match manager to start authentication
                g_MatchMgr.SetUserAccount( index );

                // try to connect using this user account
                g_MatchMgr.SetState( MATCH_CONNECT_MATCHMAKER );
                SetConnectState( CONNECT_AUTHENTICATE_USER );

                // hide list
                m_pUserList ->SetFlag(ui_win::WF_VISIBLE, FALSE);

                // disable highlight
                g_UiMgr->DisableScreenHighlight();
                irect r = GetPosition();
                r.Inflate( 50, 0 );
                InitScreenScaling( r ); // Resize dialog back to original width
                m_CurrentControl = 0;
            }
            else
            {
                SetConnectState( CONNECT_DISCONNECT );
            }
        }
        break;

        //------------------------------------------------------
    case CONNECT_FAILED_WAIT:
        SetConnectState( CONNECT_DISCONNECT );
        break;
        //------------------------------------------------------
    default:
        break;
        // Brian, I removed this assert because it would trigger if you pressed the select button 
        // at anytime during the connection logic.  Is that what you really wanted???
        //ASSERT(FALSE);
    }
}

//=========================================================================

void dlg_online_connect::OnCancel( ui_win* pWin )
{
    (void)pWin;
    if( (m_ConnectState == CONNECT_IDLE) || (m_ConnectState == CONNECT_SELECT_USER) )
    {
        // check for menu automation
        if( CONFIG_IS_AUTOSERVER || CONFIG_IS_AUTOCLIENT )
        {
            return;
        }

        if( GetState() == DIALOG_STATE_ACTIVE )
        {
            SetState( DIALOG_STATE_BACK );

            SetNavTextVisible( FALSE );
            // cancel connecting
            //g_AudioMgr.Play( "OptionBack" );
        }
    }
}

//=========================================================================

void dlg_online_connect::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;

    s32 Result = m_PopUpResult;
    m_Timeout += DeltaTime;
    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {            
            // connect
            // Do we need this? Won't the state already be CONNECT_INIT? Scaling the dialog
            // would cause us not to enter the state.
            if( m_ConnectState == CONNECT_DISPLAY_MOTD )
            {
                // set nav text
                xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_OK" ));

                // pop up modal dialog and wait for response
                irect r( 0, -10, 320, 290 );

                xwstring MessageText( g_MatchMgr.GetMessageOfTheDay() );

                m_PopUp->Configure( r,
                    g_StringTableMgr( "ui", "IDS_ONLINE_MOTD" ), 
                    TRUE, 
                    TRUE, 
                    FALSE, 
                    MessageText,
                    navText,
                    &m_PopUpResult );

                m_PopUp->EnableBlackout( FALSE );
            }
            else if( m_ConnectState == CONNECT_IDLE )
            {
                SetConnectState( CONNECT_INIT );
            }
            else if( m_ConnectState == CONNECT_SELECT_USER )
            {
                m_pUserList ->SetFlag(ui_win::WF_VISIBLE, TRUE);
                GotoControl( (ui_control*)m_pUserList );

                // set highlight
                g_UiMgr->SetScreenHighlight( m_pUserList->GetPosition() );
            }
            else if( (m_ConnectState == CONNECT_INIT) || (m_ConnectState == CONNECT_AUTHENTICATE_USER) )
            {
                ASSERT( m_PopUp == NULL );
                irect r = g_UiMgr->GetUserBounds( g_UiUserID );
                LOG_MESSAGE( "dlg_online_connect::OnUpdate", "Opening dialog. ConnectState:%d", m_ConnectState );
                m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, 
                    ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
                m_PopUpResult = DLG_POPUP_IDLE;

                r = irect(0,0,300,160);
                m_PopUp->Configure( r,
                    -1.0f,
                    g_StringTableMgr( "ui", "IDS_NETWORK_POPUP"     ),
                    g_StringTableMgr( "ui", "IDS_ONLINE_PLEASE_WAIT"),
                    g_StringTableMgr( "ui", "IDS_ONLINE_CONNECT_INIT") );

                m_PopUp->EnableBlackout( FALSE );               
            }
        }
    }
    else
    {
        if( m_PopUp && (m_PopUpResult!=DLG_POPUP_IDLE) )
        {
            m_PopUp = NULL;
        }
        if( ( g_UiMgr->IsWipeActive() == FALSE ) )
        {
            // window is scaled to correct size - do the main update
            switch( m_ConnectState )
            {
                //------------------------------------------
            case CONNECT_IDLE:
            case CONNECT_SELECT_USER:
                // wait for state change (user account selected)
                break;

                //------------------------------------------
            case CONNECT_INIT:
                UpdateConnectInit();
                break;

                //------------------------------------------
            case CONNECT_WAIT:
                {
                    SetConnectState( CONFIG_INIT );
                }
                break;

                //------------------------------------------
            case CONFIG_INIT:
                {
                    irect r( 0, 0, 300, 160 );

                    m_PopUp->Configure( r,
                        -1.0f,
                        g_StringTableMgr( "ui", "IDS_NETWORK_POPUP"            ),
                        g_StringTableMgr( "ui", "IDS_ONLINE_PLEASE_WAIT"       ),
                        g_StringTableMgr( "ui", "IDS_ONLINE_CONNECT_ESTABLISH" ) );

                    m_PopUp->EnableBlackout( FALSE );
                    g_NetworkMgr.BeginOnlineStateChange( TRUE );
                    SetConnectState( CONFIG_ONLINE_WAIT );
                }
                break;

                //------------------------------------------
            case CONFIG_ONLINE_WAIT:
                {
                    if( !g_NetworkMgr.IsOnlineStateChanging() )
                    {
                        g_NetworkMgr.BeginOnlineStateChange( TRUE );
                        break;
                    }

                    if( !g_NetworkMgr.IsOnlineStateChangeDone() )
                    {
                        break;
                    }

                    g_NetworkMgr.FinishOnlineStateChange();
                    SetConnectState( ACTIVATE_INIT );
                }
                break;

                //------------------------------------------
            case ACTIVATE_INIT:
                UpdateActivateInit();
                break;

                //------------------------------------------
            case CONNECT_MATCH_INIT:
                {
                    irect r( 0, 0, 300, 160 );

                    if( m_PopUp==NULL )
                    {
                        LOG_MESSAGE( "dlg_online_connect::UpdateAuthMachine", "Opening dialog" );
                        m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, 
                            ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
                        m_Timeout = 0.0f;
                        m_PopUpResult = DLG_POPUP_IDLE;
                    }

                    xwstring MessageText  = g_StringTableMgr( "ui", "IDS_ONLINE_PLEASE_WAIT" );
                    MessageText += "\n";
                    MessageText += g_StringTableMgr( "ui", "IDS_ONLINE_CONNECT_AUTHENTICATING" );

                    m_PopUp->Configure( r,
                        g_StringTableMgr( "ui", "IDS_NETWORK_POPUP" ), 
                        FALSE, 
                        FALSE, 
                        FALSE, 
                        MessageText,
                        xwstring(""),
                        &m_PopUpResult );
                    m_PopUp->EnableBlackout( FALSE );

                    s32 SystemId = net_GetSystemId();

                    g_MatchMgr.SetUniqueId( (const byte*)&SystemId, sizeof(SystemId) );

                    net_socket& LocalSocket = g_NetworkMgr.GetSocket();
                    if( LocalSocket.IsEmpty()  )
                    {
                        s32 Status = LocalSocket.Bind( NET_GAME_PORT, NET_FLAGS_BROADCAST );

                        net_address     Broadcast;
                        Broadcast = net_address( m_Info.Broadcast, LocalSocket.GetPort() );
                        ASSERT( Status );
                        x_DebugMsg( "Network socket opened. Address is %s\n",LocalSocket.GetStrAddress() );
                        g_MatchMgr.Init( LocalSocket, Broadcast );
                    }
                    g_MatchMgr.SetState( MATCH_AUTHENTICATE_MACHINE );

                    m_Timeout = 0.0f;

                    SetConnectState( CONNECT_AUTHENTICATE_MACHINE );
                }
                break;

                //------------------------------------------
            case CONNECT_AUTHENTICATE_MACHINE:
                UpdateAuthMachine();
                break;

                //------------------------------------------
            case CONNECT_AUTHENTICATE_USER:
                UpdateAuthUser();
                break;

                //------------------------------------------
            case CONNECT_FAILED:
                {
                    xbool bEnableYes = FALSE;
                    xbool bEnableNo = FALSE;
                    xbool bEnableMaybe = FALSE;


                    //SetState(  DIALOG_STATE_BACK;
                    //g_UiMgr->EndDialog( g_UiUserID, TRUE );
                    //return;

                    // set nav text
                    xwstring navText;

                    // don't want "Manage" option coming up.
                    switch( m_CancelMode )
                    {
                    case CANCEL_MANAGE:
                        navText = g_StringTableMgr( "ui", "IDS_NAV_NETWORK_TROUBLESHOOTER" );
                        navText += g_StringTableMgr( "ui", "IDS_NAV_SIGN_OUT" );
                        bEnableYes = TRUE;
                        bEnableNo = TRUE;
                        break;
                    case OK_ONLY:
                        navText = g_StringTableMgr( "ui", "IDS_NAV_OK" );
                        bEnableYes = TRUE;
                        break;
                    case CANCEL_RETRY_MANAGE:
                        navText = g_StringTableMgr( "ui", "IDS_NAV_NETWORK_TROUBLESHOOTER" );   // X
                        navText += g_StringTableMgr( "ui", "IDS_NAV_CONNECT_RETRY" );           // Square
                        navText += g_StringTableMgr( "ui", "IDS_NAV_SIGN_OUT" );                // Triangle
                        bEnableYes = TRUE;
                        bEnableNo = TRUE;
                        bEnableMaybe = TRUE;
                        break;

                    }

                    // pop up modal dialog and wait for response
                    irect r( 0, 0, 340, 260 );

                    xstring LabelText( g_StringTableMgr( "ui", m_LabelText ) );
                    xwstring ErrorText( xfs(LabelText, m_LastErrorCode ) );
                    xwstring Title( g_StringTableMgr( "ui", "IDS_SIGN_IN") );

                    if( m_PopUp==NULL )
                    {
                        m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, 
                            ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
                    }
                    m_PopUp->Configure( r,
                        Title, 
                        bEnableYes, 
                        bEnableNo, 
                        bEnableMaybe, 
                        ErrorText,
                        navText,
                        &m_PopUpResult );

                    m_PopUp->EnableBlackout( FALSE );
                    SetConnectState( CONNECT_FAILED_WAIT );
                }
                break;

                //------------------------------------------
            case CONNECT_FAILED_WAIT:
                m_PopUpResult = DLG_POPUP_IDLE;
                // wait for response
                if( Result != DLG_POPUP_IDLE )
                {
                    switch( m_CancelMode )
                    {
                    case OK_ONLY:
                        SetConnectState( CONNECT_DISCONNECT );
                        break;
                    case CANCEL_MANAGE:
                        if( Result==DLG_POPUP_YES )
                        {
                            SetConnectState( CONNECT_DISCONNECT );
                            ASSERT( FALSE );                        
                        }
                        else
                        {
                            SetConnectState( CONNECT_DISCONNECT );
                        }
                        break;
                    case CANCEL_RETRY_MANAGE:
                        if( Result==DLG_POPUP_YES )
                        {
                            SetConnectState( CONNECT_DISCONNECT );
                            ASSERT( FALSE );    
                        }
                        else if( Result==DLG_POPUP_NO )
                        {
                            SetConnectState( CONNECT_DISCONNECT );
                        }
                        else
                        {
                            SetConnectState( m_RetryDestination );
                        }
                        break;
                    }
                }
                break;

                //------------------------------------------
            case CONNECT_DISCONNECT:
                {
                    irect r( 0, 0, 300, 260 );
                    xstring LabelText;

                    LabelText = g_StringTableMgr( "ui", m_LabelText );
                    xwstring ErrorText( xfs(LabelText, m_LastErrorCode ) );
                    xwstring Title( g_StringTableMgr( "ui", "IDS_SIGN_IN") );

                    ASSERT( m_PopUp==NULL );
                    if( m_PopUp )
                    {
                        g_UiMgr->EndDialog( g_UiUserID, TRUE );
                        m_PopUp = NULL;
                    }
                    LOG_MESSAGE( "dlg_online_connect::OnUpdate", "Opening dialog (CONNECT_FAILED)" );
                    m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, 
                        ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
                    m_PopUpResult = DLG_POPUP_IDLE;

                    m_PopUp->Configure( r,
                        Title, 
                        FALSE, 
                        FALSE, 
                        FALSE, 
                        ErrorText,
                        g_StringTableMgr( "ui", "IDS_ONLINE_PLEASE_WAIT" ),                   // Nav text will be PLEASE WAIT.
                        &m_PopUpResult );

                    m_PopUp->EnableBlackout( FALSE );

                    SetNavTextVisible( FALSE );
                    g_NetworkMgr.BeginOnlineStateChange( FALSE );
                    SetConnectState( CONNECT_DISCONNECT_WAIT );
                }
                break;

                //------------------------------------------
            case CONNECT_DISCONNECT_WAIT:
                {
                    if( !g_NetworkMgr.IsOnlineStateChanging() )
                    {
                        g_NetworkMgr.BeginOnlineStateChange( FALSE );
                        break;
                    }

                    if( !g_NetworkMgr.IsOnlineStateChangeDone() )
                    {
                        break;
                    }

                    g_NetworkMgr.FinishOnlineStateChange();

                    //
                    // Reset exit reason so that we don't get two messages telling us the network is down.
                    //
                    g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );

                    // Once we have actually disconnected, then we go back.
                    SetState( DIALOG_STATE_BACK );
                }
                break;
                //------------------------------------------
            case CONNECT_DONE:

                {
                    ASSERT( m_PopUp==NULL );
                    irect r(0,0,300,160);

                    m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, 
                        ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

                    xwstring MessageText( g_StringTableMgr( "ui", "IDS_ONLINE_CONNECTED" ) );

                    m_PopUp->Configure( r,
                        g_StringTableMgr( "ui", "IDS_NETWORK_POPUP" ), 
                        TRUE, 
                        FALSE, 
                        FALSE, 
                        MessageText,
                        g_StringTableMgr( "ui", "IDS_NAV_OK" ),
                        &m_PopUpResult );
                    m_PopUp->EnableBlackout( FALSE );
                    SetConnectState( CONNECT_DONE_WAIT );
                }
                break;
            case CONNECT_DONE_WAIT:
                if( CONFIG_IS_AUTOSERVER || CONFIG_IS_AUTOCLIENT )
                {
                    g_UiMgr->EndDialog( g_UiUserID, TRUE );
                    m_PopUp = NULL;
                    SetState( DIALOG_STATE_EXIT );
                    return;
                }

                if( m_PopUpResult != DLG_POPUP_IDLE )
                {
                    // Dialog will self close.
                    m_PopUp = NULL;
                    SetState( DIALOG_STATE_EXIT );
                    return;
                }
                break;

                //------------------------------------------
            case CONNECT_CHECK_MOTD:
                if( g_MatchMgr.GetMessageOfTheDay() )
                {
                    SetConnectState( CONNECT_DISPLAY_MOTD );
                    irect Position = m_Position;
                    Position.Inflate( 80, 0 );

                    InitScreenScaling( Position );
                    m_PopUpResult = DLG_POPUP_IDLE;

                    // set state
                }
                else
                {
                    ASSERT( g_UiMgr->GetTopmostDialog( g_UiUserID ) == m_PopUp );
                    g_UiMgr->EndDialog( g_UiUserID, TRUE );
                    SetState( DIALOG_STATE_EXIT );
                    return;
                }
                break;
                //------------------------------------------
            case CONNECT_DISPLAY_MOTD:
                if( m_PopUpResult != DLG_POPUP_IDLE )
                {
                    m_PopUp = NULL;
                    // Dialog will self close.
                    SetState( DIALOG_STATE_EXIT );
                }
                break;
            }
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    // update everything else
    ui_dialog::OnUpdate( pWin, DeltaTime );
}

//=========================================================================

void dlg_online_connect::RefreshUserList( void )
{
    s32         i;

    // get number user accounts from network mgr.
    m_NumUsers = g_MatchMgr.GetUserAccountCount();

    // clear the list
    m_pUserList->DeleteAllItems();

    // fill it with the user information
    for( i=0; i<m_NumUsers; i++ )
    {
        // add the profile to the list
        const online_user& Profile = g_MatchMgr.GetUserAccount(i);
        m_pUserList->AddItem( Profile.Name, i );
    }    

    // add an empty slot
    m_pUserList->AddItem( g_StringTableMgr("ui", "IDS_SIGN_IN_NEW_ACCOUNT"), m_NumUsers );

}

//=========================================================================
// Will give option of managing or dropping back to main menu

void dlg_online_connect::Failed( const char* pFailureReason, s32 ErrorCode, cancel_mode CancelMode, connect_states RetryDestination )
{
    (void)ErrorCode;

    LOG_MESSAGE( "dlg_online_connect::Failed", "Failure Reason:%s", pFailureReason );
    x_strcpy( m_LabelText, pFailureReason);

    m_LastErrorCode     = ErrorCode;
    m_CancelMode        = CancelMode;
    m_RetryDestination  = RetryDestination;

    SetConnectState( CONNECT_FAILED );
}

//=========================================================================

void dlg_online_connect::SetConnectState( connect_states State )
{
    LOG_MESSAGE( "dlg_online_connect::SetConnectState","State transition from %s to %s",StateName( m_ConnectState ), StateName( State ) );
    m_ConnectState = State;
    m_Timeout = 0.0f;
}

//=========================================================================

const char* dlg_online_connect::StateName( connect_states State )
{
    switch( State )
    {
    case CONNECT_IDLE:                  return "CONNECT_IDLE";
    case CONNECT_INIT:                  return "CONNECT_INIT";
    case CONNECT_WAIT:                  return "CONNECT_WAIT";
    case CONFIG_INIT:                   return "CONFIG_INIT";
    case CONFIG_ONLINE_WAIT:            return "CONFIG_ONLINE_WAIT";
    case CONNECT_AUTHENTICATE_MACHINE:  return "CONNECT_AUTHENTICATE_MACHINE";
    case CONNECT_SELECT_USER:           return "CONNECT_SELECT_USER";
    case ACTIVATE_INIT:                 return "ACTIVATE_INIT";
    case CONNECT_MATCH_INIT:            return "CONNECT_MATCH_INIT";
    case CONNECT_AUTHENTICATE_USER:     return "CONNECT_AUTHENTICATE_USER";
    case CONNECT_FAILED:                return "CONNECT_FAILED";
    case CONNECT_FAILED_WAIT:           return "CONNECT_FAILED_WAIT";
    case CONNECT_DONE:                  return "CONNECT_DONE";
    case CONNECT_DONE_WAIT:             return "CONNECT_DONE_WAIT";
    case CONNECT_CHECK_MOTD:            return "CONNECT_CHECK_MOTD";
    case CONNECT_DISPLAY_MOTD:          return "CONNECT_DISPLAY_MOTD";
    case CONNECT_DISCONNECT:            return "CONNECT_DISCONNECT";
    case CONNECT_DISCONNECT_WAIT:       return "CONNECT_DISCONNECT_WAIT";
    case NUM_CONNECT_STATES:            return "NUM_CONNECT_STATES";
    default:                            ASSERT( FALSE );
    }
    return "";
}

//=========================================================================
// Right now, this is only ever used to make sure we start user authentication
// instead of initial machine authentication.

void dlg_online_connect::Configure( connect_mode ConnectMode )
{
    switch( ConnectMode )
    {
    case CONNECT_MODE_AUTH_USER:
        // Start the matchmaker authenticating the user, then wait for the authentication 
        // to complete. Authentication is split into two stages because the profile is
        // selected before the matchmaker authenticates it.
        g_MatchMgr.SetState( MATCH_CONNECT_MATCHMAKER );
        SetConnectState( CONNECT_AUTHENTICATE_USER );
        break;
    case CONNECT_MODE_CONNECT:
        // Set up default unique id from the hardware ID prior to
        // connecting to the matchmaker.
        SetConnectState( CONNECT_INIT );
        break;
    default:
        ASSERT(FALSE);
    }
}

//=========================================================================

void dlg_online_connect::UpdateConnectInit( void )
{
    if( g_NetworkMgr.IsOnline() )
    {
        // Already connected - bail out early!
        m_ConnectState  = CONNECT_IDLE;
        m_State         = DIALOG_STATE_EXIT;
        ASSERT( g_UiMgr->GetTopmostDialog( g_UiUserID ) == m_PopUp );
        g_UiMgr->EndDialog( g_UiUserID, TRUE );
    }
    else
    {
        irect r = g_UiMgr->GetUserBounds( g_UiUserID );

        if( m_PopUp == NULL )
        {
            LOG_MESSAGE( "dlg_online_connect::UpdateConnectInit", "Opening dialog" );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, 
                ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
            m_PopUpResult = DLG_POPUP_IDLE;
        }

        r.Set( 0, 0, 300, 160 );

        m_PopUp->Configure( r,
            0.5f,
            g_StringTableMgr( "ui", "IDS_NETWORK_POPUP"         ),
            g_StringTableMgr( "ui", "IDS_ONLINE_PLEASE_WAIT"    ),
            g_StringTableMgr( "ui", "IDS_ONLINE_CONNECT_INIT"   ) );

        m_PopUp->EnableBlackout( FALSE );

        SetConnectState( CONNECT_WAIT );
    }

}

//=========================================================================

void dlg_online_connect::UpdateActivateInit( void )
{
    net_GetInterfaceInfo( -1, m_Info );
    m_Timeout = 0.0f;
    SetConnectState( CONNECT_MATCH_INIT );
}


//=========================================================================

void dlg_online_connect::UpdateAuthUser( void )
{
    //
    // This is the correct form of the return state of the matchmgr. At this point, more than one
    // status can be returned depending on why the matchmgr went idle. This will happen if there
    // is more than one user account, and the player needs to provide input, or needs to be presented
    // with other options that require user input.
    //

    if( g_MatchMgr.IsBusy() )
    {
        if( m_Timeout > 30.0f )
        {
            Failed("IDS_ONLINE_CONNECT_MATCHMAKER_FAILED");
            g_MatchMgr.SetState( MATCH_IDLE );
            return;
        }
        //
        // User pressed cancel. This test has to be done prior to the reconfigure as
        // the configure function will clear out the popup result to idle.
        //
        if( m_PopUpResult != DLG_POPUP_IDLE )
        {
            m_PopUp = NULL;
            Failed( "IDS_ONLINE_CONNECT_ABORTED", 0, OK_ONLY );
            return;
        }

        if( m_PopUp == NULL )
        {
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            LOG_MESSAGE( "dlg_online_connect::UpdateAuthUser", "Opening dialog" );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, 
                ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
            m_PopUpResult = DLG_POPUP_IDLE;
        }

        irect r(0,0,300,160);

        xwstring MessageText  = g_StringTableMgr( "ui", "IDS_ONLINE_PLEASE_WAIT" );
        MessageText += "\n";
        MessageText += g_StringTableMgr( "ui", "IDS_ONLINE_CONNECT_MATCHMAKER" );
        MessageText += "\n";
        MessageText += g_StringTableMgr( "ui", "IDS_ONLINE_TIMEOUT" );
        MessageText += xwstring( xfs("%d",(30-(s32)m_Timeout)) );

        m_PopUp->Configure( r,
            g_StringTableMgr( "ui", "IDS_NETWORK_POPUP" ), 
            FALSE, 
            TRUE, 
            FALSE, 
            MessageText,
            g_StringTableMgr( "ui", "IDS_NAV_CANCEL" ),
            &m_PopUpResult );

        m_PopUp->EnableBlackout( FALSE );
    }
    else
    {
        switch( g_MatchMgr.GetAuthStatus() )
        {
        case AUTH_STAT_CONNECTED:
            g_MatchMgr.SetUserStatus( BUDDY_STATUS_ONLINE );
            SetConnectState( CONNECT_CHECK_MOTD );
            break;
        case AUTH_STAT_SELECT_USER:
            {

                // destroy pop-up?
                if( m_PopUp )
                {
                    m_PopUp->Close();
                    m_PopUp = NULL;
                }

                // refresh the user account list
                RefreshUserList();
                g_UiMgr->DisableScreenHighlight();
                m_pUserList->SetSelection( 0 );

                m_pUserList ->SetFlag(ui_win::WF_VISIBLE, TRUE);
                SetNavTextVisible( TRUE );
                GotoControl( (ui_control*)m_pUserList );

                // set highlight
                g_UiMgr->SetScreenHighlight( m_pUserList->GetPosition() );
                // wait for input
                SetConnectState( CONNECT_SELECT_USER );
                break;
            }
        case AUTH_STAT_INVALID_ACCOUNT:
            Failed( "IDS_ONLINE_CONNECT_MATCHMAKER_REFUSED" );
            break;
        case AUTH_STAT_CANNOT_CONNECT:
            Failed( "IDS_ONLINE_CONNECT_MATCHMAKER_FAILED" );
            break;
        case AUTH_STAT_DISCONNECTED:
            Failed( "IDS_ONLINE_NETWORK_DOWN" );
            break;
        case AUTH_STAT_SECURITY_FAILED:
            Failed( "IDS_ONLINE_SECURITY_FAILED" );
            break;
        default:
            ASSERT( FALSE );
            //Failed( "IDS_SIGN_IN_NO_CONNECTION" );
            Failed( "IDS_ONLINE_CONNECT_MATCHMAKER_FAILED" );
            break;
        }
    }
}

//=========================================================================

void dlg_online_connect::UpdateAuthMachine( void )
{
    //
    // This is the correct form of the return state of the matchmgr. At this point, more than one
    // status can be returned depending on why the matchmgr went idle. This will happen if there
    // is more than one user account, and the player needs to provide input, or needs to be presented
    // with other options that require user input.
    //

    if( g_MatchMgr.IsBusy() )
    {
        if( m_Timeout > 90.0f )
        {
            Failed("IDS_ONLINE_CONNECT_TIMEOUT");
            g_MatchMgr.SetState( MATCH_IDLE );
            return;
        }

        if( m_PopUpResult != DLG_POPUP_IDLE )
        {
            // Popup will have autoclosed itself.
            m_PopUp = NULL;
            Failed( "IDS_ONLINE_CONNECT_ABORTED", 0, OK_ONLY );
            return;
        }
        irect r( 0, 0, 300, 160 );

        if( m_PopUp==NULL )
        {
            LOG_MESSAGE( "dlg_online_connect::UpdateAuthMachine", "Opening dialog" );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, 
                ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
            m_Timeout = 0.0f;
            m_PopUpResult = DLG_POPUP_IDLE;
        }

        xwstring MessageText  = g_StringTableMgr( "ui", "IDS_ONLINE_PLEASE_WAIT" );
        MessageText += "\n";
        MessageText += g_StringTableMgr( "ui", "IDS_ONLINE_CONNECT_AUTHENTICATING" );

        m_PopUp->Configure( r,
            g_StringTableMgr( "ui", "IDS_NETWORK_POPUP" ), 
            FALSE, 
            FALSE, 
            FALSE, 
            MessageText,
            xwstring(""),
            &m_PopUpResult );
        m_PopUp->EnableBlackout( FALSE );
    }
    else
    {
        switch( g_MatchMgr.GetAuthStatus() )
        {
        case AUTH_STAT_CONNECTED:
            {
                if( CONFIG_IS_AUTOSERVER || CONFIG_IS_AUTOCLIENT )
                {
                    if( CONFIG_IS_AUTOSERVER  )
                    {
                        g_StateMgr.GetActiveSettings().Commit();
                    }

                    SetConnectState( CONNECT_DONE );
                    m_PopUp->Close();
                    m_PopUpResult = DLG_POPUP_YES;
                }
                else
                {
                    SetConnectState( CONNECT_DONE );
                    m_PopUp->Close();
                    m_PopUp=NULL;
                }
            }
            break;
        case AUTH_STAT_SELECT_USER:
        case AUTH_STAT_NO_ACCOUNT:
            {
                // refresh the user account list
                RefreshUserList();
                m_pUserList->SetSelection( 0 );

                // destroy pop-up?
                ASSERT( g_UiMgr->GetTopmostDialog(m_UserID) == m_PopUp );
                g_UiMgr->EndDialog( m_UserID, TRUE );
                m_PopUp = NULL;
                m_pUserList ->SetFlag(ui_win::WF_VISIBLE, TRUE);
                SetNavTextVisible( TRUE );
                GotoControl( (ui_control*)m_pUserList );

                // set highlight
                g_UiMgr->SetScreenHighlight( m_pUserList->GetPosition() );

                // wait for input
                SetConnectState( CONNECT_SELECT_USER );
                break;
            }
        case AUTH_STAT_INVALID_ACCOUNT:
            Failed("IDS_ONLINE_CONNECT_MATCHMAKER_REFUSED");
            break;
        case AUTH_STAT_CANNOT_CONNECT:
            {
                Failed( g_MatchMgr.GetConnectErrorMessage(), g_MatchMgr.GetConnectErrorCode(), CANCEL_RETRY_MANAGE );
            }
            break;
        case AUTH_STAT_DISCONNECTED:
            Failed( "IDS_ONLINE_NETWORK_DOWN" );
            break;
        default:
            ASSERT( FALSE );
            Failed( "IDS_ONLINE_CONNECT_MATCHMAKER_FAILED" );
            break;
        }
    }
}
