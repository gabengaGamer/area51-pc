//==============================================================================
//
//  PlayerNet.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "NetworkMgr/NetworkMgr.hpp"
#include "Player.hpp"
#include "x_bitstream.hpp"

#ifndef X_EDITOR

//==============================================================================
//  FUNCTIONS
//==============================================================================

void player::net_Activate( void )
{
    // Call base class.
    actor::net_Activate();

    // Record as local player?
    if( m_NetModeBits & CONTROL_LOCAL )
    {
        SetLocalPlayer( g_NetworkMgr.GetLocalSlot( m_NetSlot ) );
    }

    LOG_MESSAGE( "player::net_Activate",
                 "Addr:%08X - LocalSlot:%d - NetSlot:%d - Status:%s on %s",
                 this, m_LocalSlot, m_NetSlot,
                 (m_NetModeBits & CONTROL_LOCAL) ? "LOCAL"  : "REMOTE",
                 (m_NetModeBits & ON_SERVER    ) ? "SERVER" : "CLIENT" );
}

//==============================================================================

static const f32 s_WayPointEffectLifetime = 15.0f / 60.0f;

void player::net_Logic( f32 DeltaTime )
{
    m_WayPointTimeOut = MIN( m_WayPointTimeOut + MAX( 0.0f, DeltaTime ),
                              s_WayPointEffectLifetime );
    if( m_WayPointTimeOut >= s_WayPointEffectLifetime )
    {
        m_WayPointFlags = 0;
    }

    ASSERT( m_NetModeBits & CONTROL_LOCAL );  // Must be locally controlled.
    ASSERT( (actor::m_Net.LifeSeq & 0x01) == IsDead() );
}

//==============================================================================

xbool player::net_EquipWeapon2( inven_item WeaponItem )
{
    if( m_bIsMutated || (m_PrevWeaponItem == INVEN_NULL) )
    {
        m_PrevWeaponItem = WeaponItem;
    }

    xbool Result = actor::net_EquipWeapon2( WeaponItem );

    LoadAimAssistTweaks();
    LoadAimAssistTweakHandles();

    return( Result );
}

//==============================================================================

#endif // X_EDITOR
