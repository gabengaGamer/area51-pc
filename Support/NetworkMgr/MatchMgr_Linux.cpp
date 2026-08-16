//==============================================================================
//
//  MatchMgr_Linux.cpp
//
//==============================================================================

#include "x_files.hpp"
#include "e_Network.hpp"
#include "NetworkMgr/MatchMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "x_log.hpp"

//==============================================================================

xbool match_mgr::Init( net_socket& Local, const net_address Broadcast )
{
    m_Initialized             = TRUE;
    m_LocalIsServer           = FALSE;
    m_pSocket                 = &Local;
    m_BroadcastAddress        = Broadcast;
    m_AccumulatedTime         = 0.0f;
    m_ConnectStatus           = MATCH_CONN_UNAVAILABLE;
    m_AuthStatus              = AUTH_STAT_DISCONNECTED;
    m_UserStatus              = BUDDY_STATUS_OFFLINE;
    m_PendingUserStatus       = BUDDY_STATUS_OFFLINE;
    m_AcquisitionMode         = ACQUIRE_INVALID;
    m_PendingAcquisitionMode  = ACQUIRE_INVALID;
    m_ExtendedServerInfoOwner = -1;
    m_pDownloader             = NULL;
    m_IsLoggedOn              = FALSE;
    m_IsVisible               = FALSE;
    m_RegistrationComplete    = FALSE;
    m_UpdateRegistration      = FALSE;
    m_HasNewContent           = FALSE;
    m_IsVoiceCapable          = FALSE;
    m_ConnectErrorCode        = 0;
    m_ConnectErrorMessage[0]  = '\0';
    m_ActiveAccount            = 0;
    m_NeedNewPing             = FALSE;
    m_PingIndex               = 0;
    m_MessageOfTheDay.Clear();
    m_UserAccounts.Clear();
    m_Buddies.Clear();
    m_RecentPlayers.Clear();
    m_LobbyList.Clear();
    m_PendingResponseList.Clear();
    x_memset( &m_ActiveUserAccount, 0, sizeof( m_ActiveUserAccount ) );
    x_memset( &m_ExtendedServerInfo, 0, sizeof( m_ExtendedServerInfo ) );
    x_memset( m_Nickname, 0, sizeof( m_Nickname ) );
    x_memset( m_UniqueId, 0, sizeof( m_UniqueId ) );
    x_memset( m_PlayerIdentifier, 0, sizeof( m_PlayerIdentifier ) );
    x_memset( &m_GameStats, 0, sizeof( m_GameStats ) );
    x_memset( &m_CareerStats, 0, sizeof( m_CareerStats ) );

    ResetServerList();
    SetState( MATCH_IDLE );
    return FALSE;
}

//==============================================================================

void match_mgr::SetAllGameStats( const player_stats& Stats )
{
    m_GameStats = Stats;
}

//==============================================================================

void match_mgr::SetAllCareerStats( const player_stats& Stats )
{
    m_CareerStats.Stats = Stats;
}

//==============================================================================

void match_mgr::UpdateCareerStatsWithGameStats( void )
{
    m_CareerStats.Stats.KillsAsHuman  += m_GameStats.KillsAsHuman;
    m_CareerStats.Stats.KillsAsMutant += m_GameStats.KillsAsMutant;
    m_CareerStats.Stats.Deaths        += m_GameStats.Deaths;
    m_CareerStats.Stats.PlayTime      += m_GameStats.PlayTime;
    m_CareerStats.Stats.Games         += m_GameStats.Games;
    m_CareerStats.Stats.Wins          += m_GameStats.Wins;
    m_CareerStats.Stats.Gold          += m_GameStats.Gold;
    m_CareerStats.Stats.Silver        += m_GameStats.Silver;
    m_CareerStats.Stats.Bronze        += m_GameStats.Bronze;
    m_CareerStats.Stats.Kicks         += m_GameStats.Kicks;
    m_CareerStats.Stats.VotesStarted  += m_GameStats.VotesStarted;
}

//==============================================================================

void match_mgr::InitiateCareerStatsWrite( void )
{
}

//==============================================================================

void match_mgr::Kill( void )
{
    m_Initialized = FALSE;
    m_pSocket     = NULL;
    m_State       = MATCH_IDLE;
    m_ConnectStatus = MATCH_CONN_UNAVAILABLE;
    m_AuthStatus  = AUTH_STAT_DISCONNECTED;
    m_Buddies.Clear();
    m_RecentPlayers.Clear();
    m_UserAccounts.Clear();
    ResetServerList();
}

//==============================================================================

void match_mgr::Update( f32 DeltaTime )
{
    (void)DeltaTime;
}

//==============================================================================

xbool match_mgr::ReceivePacket( net_address& Remote, bitstream& Bitstream )
{
    (void)Remote;
    (void)Bitstream;
    return FALSE;
}

//==============================================================================

xbool match_mgr::SendLookup( net_address& Remote )
{
    (void)Remote;
    return FALSE;
}

//==============================================================================

void match_mgr::Reset( void )
{
    ResetServerList();
    m_LobbyList.Clear();
    m_PendingResponseList.Clear();
    m_State = MATCH_IDLE;
}

//==============================================================================

void match_mgr::SetState( match_mgr_state NewState )
{
    m_State        = NewState;
    m_StateTimeout = STATE_TIMEOUT;
    m_StateRetries = STATE_RETRIES;
}

//==============================================================================

void match_mgr::CheckVisibility( void )
{
    m_IsVisible = FALSE;
}

//==============================================================================

void match_mgr::StartAcquisition( match_acquire AcquisitionMode )
{
    m_AcquisitionMode        = AcquisitionMode;
    m_PendingAcquisitionMode = ACQUIRE_INVALID;
    m_ConnectStatus          = MATCH_CONN_UNAVAILABLE;
    ResetServerList();
    SetState( MATCH_ACQUIRE_IDLE );
}

//==============================================================================

xbool match_mgr::IsAcquireComplete( void )
{
    return( (m_State == MATCH_ACQUIRE_IDLE) || (m_State == MATCH_IDLE) );
}

//==============================================================================

void match_mgr::SetUserAccount( s32 UserIndex )
{
    if( (UserIndex >= 0) && (UserIndex < m_UserAccounts.GetCount()) )
        m_ActiveAccount = UserIndex;
}

//==============================================================================

void match_mgr::SignOut( void )
{
    m_IsLoggedOn = FALSE;
    m_AuthStatus = AUTH_STAT_DISCONNECTED;
    m_ConnectStatus = MATCH_CONN_DISCONNECTED;
}

//==============================================================================

s32 match_mgr::GetAuthResult( char* pLabelBuffer )
{
    if( pLabelBuffer )
        x_strcpy( pLabelBuffer, m_ConnectErrorMessage );
    return m_ConnectErrorCode;
}

//==============================================================================

void match_mgr::StartIndirectLookup( void )
{
    SetState( MATCH_IDLE );
}

//==============================================================================

void match_mgr::StartLogin( void )
{
    m_ConnectStatus = MATCH_CONN_UNAVAILABLE;
    m_AuthStatus    = AUTH_STAT_CANNOT_CONNECT;
    SetState( MATCH_IDLE );
}

//==============================================================================

void match_mgr::BecomeClient( void )
{
    SetState( MATCH_IDLE );
}

//==============================================================================

const extended_info* match_mgr::GetExtendedServerInfo( s32 Index )
{
    (void)Index;
    return NULL;
}

//==============================================================================

xbool match_mgr::ValidateLobbyInfo( const lobby_info& Info )
{
    (void)Info;
    return FALSE;
}

//==============================================================================

void match_mgr::InitDownload( const char* pURL )
{
    (void)pURL;
}

//==============================================================================

void match_mgr::KillDownload( void )
{
}

//==============================================================================

download_status match_mgr::GetDownloadStatus( void )
{
    return DL_STAT_ERROR;
}

//==============================================================================

f32 match_mgr::GetDownloadProgress( void )
{
    return 0.0f;
}

//==============================================================================

void* match_mgr::GetDownloadData( s32& Length )
{
    Length = 0;
    return NULL;
}

//==============================================================================

xbool match_mgr::AddBuddy( const buddy_info& Buddy )
{
    if( IsBuddy( Buddy ) )
        return FALSE;

    m_Buddies.Append( Buddy );
    return TRUE;
}

//==============================================================================

xbool match_mgr::DeleteBuddy( const buddy_info& Buddy )
{
    const s32 Index = m_Buddies.Find( Buddy );
    if( Index < 0 )
        return FALSE;

    m_Buddies.Delete( Index );
    return TRUE;
}

//==============================================================================

void match_mgr::AnswerBuddyRequest( buddy_info& Buddy, buddy_answer Answer )
{
    if( Answer == BUDDY_ANSWER_YES )
        AddBuddy( Buddy );
    else if( Answer == BUDDY_ANSWER_REMOVE )
        DeleteBuddy( Buddy );
}

//==============================================================================

xbool match_mgr::InviteBuddy( buddy_info& Buddy )
{
    (void)Buddy;
    return FALSE;
}

//==============================================================================

void match_mgr::CancelBuddyInvite( buddy_info& Buddy )
{
    (void)Buddy;
}

//==============================================================================

xbool match_mgr::AnswerBuddyInvite( buddy_info& Buddy, buddy_answer Answer )
{
    (void)Buddy;
    (void)Answer;
    return FALSE;
}

//==============================================================================

xbool match_mgr::JoinBuddy( buddy_info& Buddy )
{
    (void)Buddy;
    return FALSE;
}

//==============================================================================

const char* match_mgr::GetConnectErrorMessage( void )
{
    return m_ConnectErrorMessage;
}

//==============================================================================

xbool match_mgr::IsPlayerMuted( u64 Identifier )
{
    (void)Identifier;
    return FALSE;
}

//==============================================================================

void match_mgr::SetIsPlayerMuted( u64 Identifier, xbool IsMuted )
{
    (void)Identifier;
    (void)IsMuted;
}

//==============================================================================

void match_mgr::SendFeedback( u64 Identifier, const char* pName, player_feedback Type )
{
    (void)Identifier;
    (void)pName;
    (void)Type;
}

//==============================================================================
