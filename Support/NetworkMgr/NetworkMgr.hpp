//==============================================================================
//
//  NetworkMgr.hpp
//
//==============================================================================
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//  Not to be used without express permission of Inevitable Entertainment, Inc.
//
//==============================================================================

#ifndef NETWORK_MGR_HPP
#define NETWORK_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "e_Network.hpp"

//==============================================================================
//  ANNOUNCEMENTS
//==============================================================================

class   game_server;       // Announce game_server class
class   game_client;       // Announce game_client class
class   voice_proxy;
class   pain_queue;        // Announce the pain_queue class
struct  server_info;       // Announce server info structure

//==============================================================================
//  DEFINITIONS
//==============================================================================

//**NOTE** This port # is defined by IANA as the PlayStation AMS (Secure) port.
const s32 NET_GAME_PORT = 3658;

// Map ID of the final campaign level — used to detect game completion.
const s32 NET_LAST_MAP_ID = 1130;

//==============================================================================
//  TYPES
//==============================================================================

enum osd_destination
{
    OSD_EXIT_MANAGE,        // Run network configuration tool or troubleshooter
    OSD_EXIT_NEW_USER,      // XBOX: New user account needed
    OSD_EXIT_UPDATE,        // XBOX: Required update available
    OSD_EXIT_MESSAGE,       // XBOX: Required message available
    OSD_EXIT_REBOOT,
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

class network_mgr
{
public:
                        network_mgr             ( void );
                       ~network_mgr             ( void );

    // Lifecycle
    void                Init                    ( void );
    void                Kill                    ( void );
    void                Update                  ( f32 DeltaTime );

    // Online state
    void                SetOnline               ( xbool IsOnline );
    xbool               IsOnline                ( void ) const              { return m_IsOnline; }

    // Server / client roles
    void                BecomeServer            ( void );
    void                BeginLogin              ( void );
    void                ReenterGame             ( void );
    void                Disconnect              ( void );

    xbool               IsServer                ( void ) const              { return (m_pServer != NULL); }
    xbool               IsClient                ( void ) const              { return (m_pClient != NULL); }
    xbool               IsLoggedIn              ( void );
    xbool               IsRequestingMission     ( void );

    // Silence / kick
    void                Silence                 ( void )                    { SilenceInternal( FALSE ); }
    void                KickPlayer              ( s32 Index );

    // Local player info
    s32                 GetLocalPlayerCount     ( void ) const              { return m_LocalPlayerCount; }
    void                SetLocalPlayerCount     ( s32 Count )               { m_LocalPlayerCount = Count; }
    s32                 GetLocalPlayerSlot      ( s32 Index );
    s32                 GetLocalSlot            ( s32 NetSlot );
    xbool               IsLocal                 ( s32 NetSlot );

    // Client / server queries
    s32                 GetClientIndex          ( void );
    s32                 GetClientIndex          ( s32 PlayerIndex );
    s32                 GetClientPlayerSlot     ( s32 ClientIndex, s32 PlayerIndex );
    s32                 GetClientPlayerCount    ( s32 ClientIndex );
    f32                 GetClientPing           ( s32 ClientIndex );
    f32                 GetAveragePing          ( void );

    // Subsystem accessors
    pain_queue*         GetPainQueue            ( void );
    net_socket&         GetSocket               ( void )                    { return m_LocalSocket; }
    voice_proxy&        GetVoiceProxy           ( s32 ClientIndex );

    // Buddy checks
    xbool               IsServerABuddy          ( const char* pSearch );
    xbool               HasPlayerBuddy          ( const char* pSearch );

    // Debug
    const char*         GetStateName            ( void );
    void                SimpleDialog            ( const char* pText, f32 Timeout = 0.0f );
    void                LoadMissionComplete     ( void );

    // Object accessors — always guard with IsServer() / IsClient() before calling
    game_server&        GetServerObject         ( void )                    { ASSERT( m_pServer ); return *m_pServer; }
    game_client&        GetClientObject         ( void )                    { ASSERT( m_pClient ); return *m_pClient; }

#if defined(TARGET_PS2)
    void                OnRepollCB              ( void );
#endif

private:
    // Internal silence implementation; ForceDisconnect=TRUE closes the socket immediately.
    void                SilenceInternal         ( xbool ForceDisconnect );

    xbool               m_IsOnline;
    net_socket          m_LocalSocket;
    game_client*        m_pClient;
    game_server*        m_pServer;
    s32                 m_LocalPlayerCount;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern network_mgr g_NetworkMgr;

#endif // NETWORK_MGR_HPP