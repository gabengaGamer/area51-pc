//==============================================================================
//
// MatchMgr.cpp - Matchup manager interface functions (STUB VERSION)
//
//==============================================================================
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//  Not to be used without express permission of Inevitable Entertainment, Inc.
//
//==============================================================================

#if !defined(TARGET_PC)
#error This should only be included for PC.
#endif

#include "x_files.hpp"
#include "x_threads.hpp"
#include "e_Network.hpp"
#include "NetworkMgr/MatchMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "ServerVersion.hpp"
#include "Configuration/GameConfig.hpp"
#include "x_log.hpp"
#include "../Apps/GameApp/Config.hpp"

//=========================================================================
//  Static data
//=========================================================================
static byte* s_pAuthData = NULL;

//=========================================================================
//  Periodic updater thread
//=========================================================================
void s_MatchPeriodicUpdater( s32, char** argv )
{
    match_mgr& MatchMgr = (match_mgr&)*argv;
    xtimer deltatime;
    deltatime.Start();

    while( TRUE )
    {
        MatchMgr.Update( deltatime.TripSec() );
        x_DelayThread( MatchMgr.m_UpdateInterval );
    }
}

//=========================================================================
//  match_mgr implementation
//=========================================================================

//------------------------------------------------------------------------------
xbool match_mgr::Init( net_socket& Local, const net_address Broadcast )
{
    ASSERT( !m_Initialized );
    
    LockLists();
    m_Initialized               = TRUE;
    m_LocalIsServer             = FALSE;
    m_pSocket                   = &Local;
    m_AccumulatedTime           = 0.0f;
    m_ConnectStatus             = MATCH_CONN_CONNECTED;
    m_pBrowser                  = NULL;
    m_IsLoggedOn                = TRUE;
    m_AcquisitionMode           = ACQUIRE_INVALID;
    m_ExtendedServerInfoOwner   = -1;
    m_PendingAcquisitionMode    = ACQUIRE_INVALID;
    m_pDownloader               = NULL;
    m_UpdateInterval            = 50;
    m_HasNewContent             = FALSE;
    m_AuthStatus                = AUTH_STAT_OK;
    m_UserStatus                = BUDDY_STATUS_ONLINE;
    m_PendingUserStatus         = BUDDY_STATUS_ONLINE;
    m_pMessageOfTheDay          = NULL;
    m_MessageOfTheDayReceived   = TRUE;
    m_pSecurityChallenge        = NULL;
    m_Presence                  = NULL;
    m_IsVoiceCapable            = TRUE;
    m_FilterMaxPlayers          = -1;
    m_FilterMinPlayers          = -1;
    m_FilterGameType            = GAME_MP;
    m_RecentPlayers.Clear();

    x_memset( m_Nickname, 0, sizeof(m_Nickname) );
    x_strcpy( m_Nickname, "StubPlayer" );
    
    SetBuddies( "" );
    SetState(MATCH_IDLE);

    m_pThread = new xthread( s_MatchPeriodicUpdater, "MatchMgr periodic Updater", 8192, 1, 1, (char**)this );
    
    UnlockLists();
    return TRUE;
}

//------------------------------------------------------------------------------
void match_mgr::Kill( void )
{
    if( m_Initialized )
    {
        LockLists();
        m_Initialized = FALSE;
        DisconnectFromMatchmaker();

        if( m_pThread )
        {
            delete m_pThread;
            m_pThread = NULL;
        }
        
        SetState( MATCH_IDLE );
        UnlockLists();
        
        m_RecentPlayers.Clear();
        ResetServerList();
        m_Buddies.Clear();
        
        if( m_pMessageOfTheDayBuffer )
        {
            x_free( m_pMessageOfTheDayBuffer );
            m_pMessageOfTheDay = NULL;
            m_pMessageOfTheDayBuffer = NULL;
        }
        ReleasePatch();
    }
    else
    {
        SetState( MATCH_IDLE );
    }
}

//------------------------------------------------------------------------------
void match_mgr::Update( f32 DeltaTime )
{
    if( !m_pSocket || m_pSocket->IsEmpty() )
    {
        Reset();
        m_pSocket = NULL;
        return;
    }

    m_AccumulatedTime += DeltaTime;

    if( (m_PendingAcquisitionMode != ACQUIRE_INVALID) && 
        ((m_State==MATCH_IDLE) || (m_State==MATCH_ACQUIRE_IDLE)) )
    {
        m_AcquisitionMode = m_PendingAcquisitionMode;
        m_PendingAcquisitionMode = ACQUIRE_INVALID;
        
        switch( m_AcquisitionMode )
        {
        case MATCH_ACQUIRE_SERVERS:
            SetState( MATCH_ACQUIRE_SERVERS );
            break;
        case ACQUIRE_EXTENDED_SERVER_INFO:
            SetState( MATCH_ACQUIRE_EXTENDED_SERVER_INFO );
            break;
        case MATCH_ACQUIRE_BUDDIES:
            SetState( MATCH_ACQUIRE_BUDDIES );
            break;
        default:
            break;
        }
    }

    UpdateState(DeltaTime);
    
    // Stub: minimal downloader update
    if( m_pDownloader )
    {
        m_pDownloader->Update( DeltaTime );
        m_UpdateInterval = 1;
    }
    else
    {
        m_UpdateInterval = 50;
    }
}

//------------------------------------------------------------------------------
void match_mgr::Reset( void )
{
    LOG_MESSAGE( "match_mgr", "Reset" );
    
    if( !m_pSocket )
        return;

    LockLists();
    ResetServerList();
    m_LobbyList.Clear();
    UnlockLists();

    m_LocalIsServer = FALSE;
}

//------------------------------------------------------------------------------
void match_mgr::UpdateState( f32 DeltaTime )
{
    if( m_StateTimeout >= 0.0f )
    {
        m_StateTimeout -= DeltaTime;
    }

    switch( m_State )
    {
    case MATCH_IDLE:
        break;

    case MATCH_AUTHENTICATE_MACHINE:
        SetConnectStatus( MATCH_CONN_CONNECTED );
        SetState( MATCH_GET_MESSAGES );
        break;

    case MATCH_AUTH_DONE:
        break;

    case MATCH_CONNECT_MATCHMAKER:
        SetConnectStatus( MATCH_CONN_CONNECTED );
        SetState( MATCH_IDLE );
        break;

    case MATCH_AUTHENTICATE_USER:
        m_AuthStatus = AUTH_STAT_OK;
        SetState( MATCH_AUTH_USER_CONNECT );
        break;

    case MATCH_AUTH_USER_CONNECT:
        m_AuthStatus = AUTH_STAT_OK;
        SetState( MATCH_SECURITY_CHECK );
        SetUserStatus( BUDDY_STATUS_ONLINE );
        break;

    case MATCH_AUTH_USER_CREATE:
        m_AuthStatus = AUTH_STAT_OK;
        SetState( MATCH_SECURITY_CHECK );
        SetUserStatus( BUDDY_STATUS_ONLINE );
        break;

    case MATCH_SECURITY_CHECK:
        m_SecurityChallengeReceived = TRUE;
        SetConnectStatus( MATCH_CONN_CONNECTED );
        SetState( MATCH_IDLE );
        break;

    case MATCH_GET_MESSAGES:
        m_MessageOfTheDayReceived = TRUE;
        SetState( MATCH_AUTH_DONE );
        break;

    case MATCH_ACQUIRE_SERVERS:
        if( m_StateTimeout <= 0.0f )
        {
            SetConnectStatus( MATCH_CONN_IDLE );
            SetState( MATCH_IDLE );
        }
        break;

    case MATCH_ACQUIRE_BUDDIES:
        SetConnectStatus( MATCH_CONN_IDLE );
        SetState( MATCH_IDLE );
        break;

    case MATCH_BECOME_SERVER:
        LockLists();
        ResetServerList();
        m_LobbyList.Clear();
        m_LocalIsServer = TRUE;
        SetState(MATCH_VALIDATE_SERVER_NAME);
        UnlockLists();
        break;

    case MATCH_VALIDATE_SERVER_NAME:
        SetState(MATCH_REGISTER_SERVER);
        break;

    case MATCH_REGISTER_SERVER:
        SetState( MATCH_SERVER_ACTIVE );
        break;

    case MATCH_SERVER_ACTIVE:
        break;

    case MATCH_UNREGISTER_SERVER:
        SetConnectStatus( MATCH_CONN_IDLE );
        SetState(MATCH_IDLE);
        break;

    case MATCH_LOGIN:
        SetState( MATCH_CLIENT_ACTIVE );
        break;

    case MATCH_CLIENT_ACTIVE:
        break;

    default:
        break;
    }
}

//------------------------------------------------------------------------------
void match_mgr::ExitState( match_mgr_state OldState )
{
    switch( OldState )
    {
    case MATCH_ACQUIRE_SERVERS:
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------
void match_mgr::EnterState( match_mgr_state NewState )
{
    m_StateRetries = STATE_RETRIES;
    SetTimeout(STATE_TIMEOUT);
    
    switch( NewState )
    {
    case MATCH_AUTHENTICATE_MACHINE:
        break;

    case MATCH_CONNECT_MATCHMAKER:
        break;

    case MATCH_AUTHENTICATE_USER:
        m_AuthStatus = AUTH_STAT_OK;
        break;

    case MATCH_AUTH_USER_CREATE:
    case MATCH_AUTH_USER_CONNECT:
        m_AuthStatus = AUTH_STAT_OK;
        break;

    case MATCH_SECURITY_CHECK:
        m_SecurityChallengeReceived = TRUE;
        break;

    case MATCH_ACQUIRE_SERVERS:
        LockLists();
        ResetServerList();
        m_ExtendedServerInfoOwner = -1;
        UnlockLists();
        SetConnectStatus( MATCH_CONN_ACQUIRING_SERVERS );
        break;

    case MATCH_ACQUIRE_BUDDIES:
        SetConnectStatus( MATCH_CONN_ACQUIRING_BUDDIES );
        break;

    case MATCH_REGISTER_SERVER:
        m_RegistrationComplete = TRUE;
        m_UpdateRegistration = FALSE;
        break;

    case MATCH_LOGIN:
        LockLists();
        ResetServerList();
        m_LobbyList.Clear();
        UnlockLists();
        m_LocalIsServer = FALSE;
        break;

    default:
        break;
    }
    
    m_State = NewState;
}

//------------------------------------------------------------------------------
void match_mgr::SetState( match_mgr_state NewState )
{
    SetTimeout( STATE_TIMEOUT );

    LOG_MESSAGE( "match_mgr::SetState", "Old:%s - New:%s",
        GetStateName(m_State), GetStateName(NewState) );

    if( NewState != m_State )
    {
        ExitState(m_State);
        EnterState(NewState);
    }
}

//------------------------------------------------------------------------------
xbool match_mgr::ReceivePacket( net_address& Remote, bitstream& Bitstream )
{
    (void)Remote;
    (void)Bitstream;
    return FALSE;
}

//------------------------------------------------------------------------------
void match_mgr::AppendServer( const server_info& Response )
{
    LockLists();
    s32 Index = m_ServerList.Find( Response );
    
    if( Index >= 0 )
    {
        m_ServerList[Index] = Response;
    }
    else
    {
        AppendToServerList( Response );
    }
    UnlockLists();
}

//------------------------------------------------------------------------------
void match_mgr::LocalLookups( f32 DeltaTime )
{
    (void)DeltaTime;
}

//------------------------------------------------------------------------------
void match_mgr::RemoteLookups( f32 DeltaTime )
{
    (void)DeltaTime;
}

//------------------------------------------------------------------------------
f32 match_mgr::GetPingTime( s32 Index )
{
    (void)Index;
    return 50.0f;
}

//------------------------------------------------------------------------------
void match_mgr::DisconnectFromMatchmaker( void )
{
    SetConnectStatus( MATCH_CONN_DISCONNECTED );
}

//------------------------------------------------------------------------------
void match_mgr::ConnectToMatchmaker( match_mgr_state PendingState )
{
    m_PendingState = PendingState;
    SetState(MATCH_CONNECT_MATCHMAKER);
    SetConnectStatus( MATCH_CONN_CONNECTING );
}

//------------------------------------------------------------------------------
xbool match_mgr::ValidateLobbyInfo( const lobby_info &info )
{
    return ( info.Name[0] != 0 );
}

//------------------------------------------------------------------------------
xbool match_mgr::CheckSecurityChallenge( const char* pChallenge )
{
    (void)pChallenge;
    return TRUE;
}

//------------------------------------------------------------------------------
void match_mgr::IssueSecurityChallenge( void )
{
}

//------------------------------------------------------------------------------
void match_mgr::SetBuddies( const char* pBuddyString )
{
    ASSERT( x_strlen(pBuddyString) < sizeof(m_BuddyList)-1 );
    x_strcpy( m_BuddyList, pBuddyString );
}

//------------------------------------------------------------------------------
void match_mgr::NotifyKick( const char* UniqueId )
{
    (void)UniqueId;
}

//------------------------------------------------------------------------------
void match_mgr::SetUserAccount( s32 UserIndex )
{
    ASSERT( UserIndex < m_UserAccounts.GetCount() );
    m_ActiveAccount = UserIndex;
}

//------------------------------------------------------------------------------
s32 match_mgr::GetAuthResult( char* pLabelBuffer )
{
    x_strcpy( pLabelBuffer, m_ConnectErrorMessage );
    return m_ConnectErrorCode;
}

//------------------------------------------------------------------------------
void match_mgr::StartIndirectLookup( void )
{
    m_SessionID = x_rand();
    SetState( MATCH_INDIRECT_LOOKUP );
}

//------------------------------------------------------------------------------
void match_mgr::StartLogin( void )
{
    SetState( MATCH_LOGIN );
}

//------------------------------------------------------------------------------
void match_mgr::BecomeClient( const server_info& Config )
{
    m_SessionID = x_rand();
    game_config::Commit( Config );
    
    SetState( MATCH_LOGIN );
}

//------------------------------------------------------------------------------
void match_mgr::MarkBuddyPresent( const net_address& Remote )
{
    LockLists();
    for( s32 i = 0; i < m_ServerList.GetCount(); i++ )
    {
        if( m_ServerList[i].Remote == Remote )
        {
            m_ServerList[i].Flags |= SERVER_HAS_BUDDY;
        }
    }
    UnlockLists();
}

//------------------------------------------------------------------------------
const extended_info* match_mgr::GetExtendedServerInfo( s32 Index )
{
    if( Index != m_ExtendedServerInfoOwner )
    {
        m_ExtendedServerInfoOwner = Index;
        StartAcquisition( ACQUIRE_EXTENDED_SERVER_INFO );
        return NULL;
    }
    
    if( GetState() == MATCH_IDLE )
    {
        return &m_ExtendedServerInfo;
    }
    
    return NULL;
}

//------------------------------------------------------------------------------
void match_mgr::StartAcquisition( match_acquire AcquisitionMode )
{
    LockLists();
    switch( AcquisitionMode )
    {
    case MATCH_ACQUIRE_SERVERS:
        ResetServerList();
        break;
    case MATCH_ACQUIRE_BUDDIES:
        break;
    case ACQUIRE_EXTENDED_SERVER_INFO:
        break;
    default:
        break;
    }
    m_PendingAcquisitionMode = AcquisitionMode;
    UnlockLists();
}

//------------------------------------------------------------------------------
xbool match_mgr::IsAcquireComplete( void )
{
    switch( m_AcquisitionMode )
    {
    case MATCH_ACQUIRE_SERVERS:
        return (m_State == MATCH_ACQUIRE_IDLE);
    case MATCH_ACQUIRE_BUDDIES:
        return (m_State == MATCH_IDLE);
    default:
        break;
    }
    return TRUE;
}

//------------------------------------------------------------------------------
void match_mgr::InitDownload( const char* pURL )
{
    ASSERT( m_pDownloader == NULL );
    m_pDownloader = new downloader;
    ASSERT( m_pDownloader );
    m_pDownloader->Init( pURL );
}

//------------------------------------------------------------------------------
void match_mgr::KillDownload( void )
{
    if( m_pDownloader )
    {
        m_pDownloader->Kill();
        delete m_pDownloader;
        m_pDownloader = NULL;
    }
}

//------------------------------------------------------------------------------
download_status match_mgr::GetDownloadStatus( void )
{
    ASSERT( m_pDownloader );
    return m_pDownloader->GetStatus();
}

//------------------------------------------------------------------------------
f32 match_mgr::GetDownloadProgress( void )
{
    ASSERT( m_pDownloader );
    return m_pDownloader->GetProgress();
}

//------------------------------------------------------------------------------
void* match_mgr::GetDownloadData( s32& Length )
{
    ASSERT( m_pDownloader );
    Length = m_pDownloader->GetFileLength();
    return m_pDownloader->GetFileData();
}

//------------------------------------------------------------------------------
xbool match_mgr::AddBuddy( const buddy_info& Buddy )
{
    LockBrowser();
    
    if( m_Buddies.Find( Buddy ) == -1 )
    {
        s32 Index = m_Buddies.GetCount();
        m_Buddies.Append( Buddy );
        m_Buddies[Index].Status = BUDDY_STATUS_ADD_PENDING;
        m_Buddies[Index].Flags = USER_REQUEST_SENT;
        
        LOG_MESSAGE( "match_mgr::AddBuddy","Buddy added. ID:%d, name:%s", 
                    Buddy.Identifier, Buddy.Name );
    }
    
    UnlockBrowser();
    return TRUE;
}

//------------------------------------------------------------------------------
xbool match_mgr::DeleteBuddy( const buddy_info& Buddy )
{
    LockBrowser();
    
    for( s32 i = 0; i < m_Buddies.GetCount(); i++ )
    {
        if( m_Buddies[i] == Buddy )
        {
            LOG_MESSAGE( "match_mgr::DeleteBuddy","Buddy removed. ID:%d, name:%s", 
                        m_Buddies[i].Identifier, m_Buddies[i].Name );
            m_Buddies.Delete(i);
            UnlockBrowser();
            return TRUE;
        }
    }
    
    UnlockBrowser();
    return FALSE;
}

//------------------------------------------------------------------------------
void match_mgr::AnswerBuddyRequest( buddy_info& Buddy, buddy_answer Answer )
{
    LockBrowser();
    s32 BuddyIndex = m_Buddies.Find( Buddy );
    
    if( BuddyIndex != -1 )
    {
        m_Buddies[BuddyIndex].Flags &= ~USER_REQUEST_RECEIVED;
        
        switch( Answer )
        {
        case BUDDY_ANSWER_YES:      
            LOG_MESSAGE( "match_mgr::AnswerBuddyRequest", "Replied affirmative. Identifier:%d", Buddy.Identifier );
            break;
        case BUDDY_ANSWER_NO:       
            LOG_MESSAGE( "match_mgr::AnswerBuddyRequest", "Replied negative. Identifier:%d", Buddy.Identifier );
            m_Buddies.Delete( BuddyIndex );
            break;
        case BUDDY_ANSWER_BLOCK:    
            LOG_MESSAGE( "match_mgr::AnswerBuddyRequest", "Added to ignore list. Identifier:%d", Buddy.Identifier );
            m_Buddies[BuddyIndex].Flags |= USER_REQUEST_IGNORED;
            break;
        default:
            break;
        }
    }
    
    UnlockBrowser();
}

//------------------------------------------------------------------------------
xbool match_mgr::InviteBuddy( buddy_info& Buddy )
{
    s32 Index = m_Buddies.Find( Buddy );
    if( Index >= 0 )
    {
        LockBrowser();
        m_Buddies[Index].Flags |= USER_SENT_INVITE;
        UnlockBrowser();
        return TRUE;
    }
    return FALSE;
}

//------------------------------------------------------------------------------
void match_mgr::CancelBuddyInvite( buddy_info& Buddy )
{
    s32 Index = m_Buddies.Find( Buddy );
    if( Index >= 0 )
    {
        m_Buddies[Index].Flags &= ~USER_SENT_INVITE;
    }
}

//------------------------------------------------------------------------------
xbool match_mgr::AnswerBuddyInvite( buddy_info& Buddy, buddy_answer Answer )
{
    s32 Index = m_Buddies.Find( Buddy );
    if( Index >= 0 )
    {
        buddy_info& Info = m_Buddies[Index];
        Info.Flags &= ~USER_HAS_INVITE;
        
        switch( Answer )
        {
        case BUDDY_ANSWER_YES:
            g_PendingConfig.GetConfig().ConnectionType = CONNECT_INDIRECT;
            g_PendingConfig.GetConfig().Remote = Info.Remote;
            break;
        case BUDDY_ANSWER_NO:
            break;
        case BUDDY_ANSWER_BLOCK:
            Info.Flags |= USER_INVITE_IGNORED;
            break;
        default:
            break;
        }
        return TRUE;
    }
    return FALSE;
}

//------------------------------------------------------------------------------
xbool match_mgr::JoinBuddy( buddy_info& Buddy )
{
    s32 Index = m_Buddies.Find( Buddy );
    if( Index >= 0 )
    {
        buddy_info& Info = m_Buddies[Index];
        Info.Flags &= ~USER_HAS_INVITE;
        g_PendingConfig.GetConfig().ConnectionType = CONNECT_INDIRECT;
        g_PendingConfig.GetConfig().Remote = Info.Remote;
        return TRUE;
    }
    return FALSE;
}

//------------------------------------------------------------------------------
const char* match_mgr::GetConnectErrorMessage( void )
{
    switch( m_ConnectStatus )
    {
    case MATCH_CONN_UNAVAILABLE:
        return "IDS_ONLINE_CONNECT_MATCHMAKER_FAILED";
    }
    return m_ConnectErrorMessage;
}

//------------------------------------------------------------------------------
xbool match_mgr::IsPlayerMuted( u64 Identifier )
{
    (void)Identifier;
    return FALSE;
}

//------------------------------------------------------------------------------
void match_mgr::SetIsPlayerMuted( u64 Identifier, xbool IsMuted )
{
    (void)Identifier;
    (void)IsMuted;
}

//------------------------------------------------------------------------------
void match_mgr::SendFeedback( u64 Identifier, const char* pName, player_feedback Type )
{
    (void)Identifier;
    (void)pName;
    (void)Type;
}

//------------------------------------------------------------------------------
void match_mgr::ReleasePatch( void )
{
    if( m_pMessageOfTheDayBuffer )
    {
        x_free( m_pMessageOfTheDayBuffer );
        m_pMessageOfTheDayBuffer = NULL;
        m_pMessageOfTheDay = NULL;
    }
}

//------------------------------------------------------------------------------
void match_mgr::CheckVisibility( void )
{
    m_IsVisible = TRUE;
}

//==============================================================================
//  Stats functions (stubs)
//==============================================================================

void match_mgr::EnableStatsConnection( void )
{
}

void match_mgr::InitiateStatsConnection( void )
{
}

void match_mgr::InitiateCareerStatsRead( void )
{
}

xbool match_mgr::IsCareerStatsReadComplete( void )
{
    return TRUE;
}

void match_mgr::InitiateCareerStatsWrite( void )
{
}

xbool match_mgr::IsCareerStatsWriteComplete( void )
{
    return TRUE;
}

void match_mgr::StatsUpdate( void )
{
}

void match_mgr::SetAllGameStats( const player_stats& Stats )
{
    m_GameStats = Stats;
}

void match_mgr::SetAllCareerStats( const player_stats& Stats )
{
    m_CareerStats.Stats = Stats;
}

void match_mgr::UpdateCareerStatsWithGameStats( void )
{
}

void match_mgr::StatsUpdateConnect( void )
{
}

void match_mgr::StatsUpdateRead( void )
{
}

void match_mgr::StatsUpdateWrite( void )
{
}