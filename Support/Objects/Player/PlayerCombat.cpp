//==============================================================================
//
//  PlayerCombat.cpp
// 
//==============================================================================
#include "Player.hpp"
#include "Objects/ParticleEmiter.hpp"
#include "Sound/EventSoundEmitter.hpp"
#include "Objects/WeaponMutation.hpp"
#include "StateMgr/StateMgr.hpp"

//=========================================================================
// EXTERNALS
//=========================================================================

////////////////////////////////////////////////////////
// KSS -- FIXME -- move these to tweak table
// Combo tweaks 
static const f32 s_MeleeShakeSpeed[MAX_COMBO_HITS]      = { 4.0f, 4.0f, 4.0f };
static const f32 s_MeleeShakeTime[MAX_COMBO_HITS]       = { 0.4f, 0.4f, 0.5f };
static const f32 s_MeleeShakeAmount[MAX_COMBO_HITS]     = { 3.0f, 3.0f, 4.0f };
static const f32 s_MeleeFeedbackAmount[MAX_COMBO_HITS]  = { 1.0f, 1.0f, 1.0f };
// END -- KSS -- FIXME -- move these to tweak table
////////////////////////////////////////////////////////

xbool player::IsAltFiring( void )
{
    return m_Combat.BuildInput( m_Input.GetState(), m_bIsMutated ).SecondaryHeld;
}

//==============================================================================

xbool player::IsAltFirePressed( void )
{
    return m_Combat.BuildInput( m_Input.GetState(), m_bIsMutated ).SecondaryPressed;
}

//==============================================================================

xbool player::IsAimToggleEnabled( void )
{
#ifndef X_EDITOR
    player_profile& Profile = g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex( m_LocalSlot ) );
    return Profile.IsAimToggleEnabled();
#else
    return TRUE;
#endif
}

//==============================================================================

xbool player::IsFiring( void )
{
    xbool PrimaryDown = m_Combat.BuildInput( m_Input.GetState(), m_bIsMutated ).PrimaryHeld;
    return( PrimaryDown && !m_bRespawnButtonPressed );
}

//===========================================================================

player::animation_state player::SetupMutationMeleeWeapon( void )
{
    // Do not create a 3rd person camera for network ghosts.
    if( m_LocalSlot == -1 )
    {
        return ANIM_STATE_MELEE;
    }

    // set a default
    animation_state AnimState = ANIM_STATE_MELEE;

    // make sure we have our "mutant melee weapon" out
    if( !GetMutationMeleeWeapon() )
    {
        return AnimState;
    }

    g_AudioMgr.Play( "Mut_Melee" );
    AnimState = GetNextMeleeState();

    m_NextAnimState = ANIM_STATE_UNDEFINED;

    SetAnimation( AnimState, ANIM_PRIORITY_DEFAULT );
    GetMutationMeleeWeapon()->Setup( GetGuid(), AnimState );    

    return AnimState;
}

//===========================================================================

weapon_mutation* player::GetMutationMeleeWeapon( void )
{
    new_weapon* pWeapon = GetCurrentWeaponPtr();

    // make sure this is a mutation weapon
    if( pWeapon && pWeapon->IsKindOf( weapon_mutation::GetRTTI()) )
    {
        return (weapon_mutation*)pWeapon;
    }

    return NULL;
}

//===========================================================================

void player::SetMeleeState( animation_state MeleeState )
{
    // Get a reference to the state that we are considering
    state_anims& State = m_Anim[inventory2::ItemToWeaponIndex(m_CurrentWeaponItem)][MeleeState];

    // Can we fire the secondary weapon?
    if( State.nPlayerAnims> 0 )
    {
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if( pWeapon )
        {
            pWeapon->ClearZoom();
        }

        SetAnimState( MeleeState );

#ifndef X_EDITOR
        // Play melee on 3rd person avatar
        net_Melee();
#endif // X_EDITOR

    }
}

//==============================================================================

xbool player::AllowedToFire( void )
{
    if( m_bHidePlayerArms )
    {
        return FALSE;
    }

    if( IsCinemaRunning() || m_LockedView.IsActive() )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================


void player::EmitMeleePain( void )
{
    tweak_handle ReachDistanceTweak("PLAYER_MeleeReachDistance");
    tweak_handle SphereRadiusTweak("PLAYER_MeleeSphereRadius");
    const f32 MeleeReachDistance = ReachDistanceTweak.GetF32();
    const f32 MeleeSphereRadius  = SphereRadiusTweak.GetF32();

    //
    // Fire a sphere out from the eye the correct distance and 
    // determine if we hit anything.
    //
    guid DirectHitGuid = 0;
    vector3 HitPosition;
    {
        vector3 StartPos = GetSimulationView().GetPosition();
        vector3 EndPos   = StartPos + GetSimulationView().GetViewZ() * MeleeReachDistance;

        g_CollisionMgr.SphereSetup( GetGuid(), StartPos, EndPos, MeleeSphereRadius );
        g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, object::ATTR_BLOCKS_LARGE_PROJECTILES, object::ATTR_COLLISION_PERMEABLE );

        if( g_CollisionMgr.m_nCollisions )
        {
            DirectHitGuid = g_CollisionMgr.m_Collisions[0].ObjectHitGuid;
            HitPosition = g_CollisionMgr.m_Collisions[0].Point;
        }
    }

    // If there was no direct hit then there's nothing left to do
    if( DirectHitGuid == 0 )
    {
        return;
    }

    // We hit something! Play a sound!
    if( m_PlayMeleeSound )
    {
        m_PlayMeleeSound = FALSE;

        // Create an event sound emitter.
        guid Guid = g_ObjMgr.CreateObject( event_sound_emitter::GetObjectType() );
        object* pSndEventObj = g_ObjMgr.GetObjectByGuid( Guid );

        event_sound_emitter& EventEmitter = event_sound_emitter::GetSafeType( *pSndEventObj );

        char DescName[64];
        x_sprintf( DescName, "Melee_%s", EventEmitter.GetMaterialName(g_CollisionMgr.m_Collisions[0].Flags) );

        EventEmitter.PlayEmitter(   DescName, 
            HitPosition, 
            GetZone1(), 
            event_sound_emitter::SINGLE_SHOT, 
            m_WeaponGuids[inventory2::ItemToWeaponIndex(m_CurrentWeaponItem)] );
        // hit something, set flag
        m_bLastMeleeHit = TRUE;
    }
    else
    {
        // hit nothing, set flag
        m_bLastMeleeHit = FALSE;
    }


    // Do shakes and feedback (skip if cloth is hit otherwise this looks weird)
    object* pHitObject = g_ObjMgr.GetObjectByGuid( DirectHitGuid );
    if(     ( pHitObject ) 
        &&  ( pHitObject->GetType() != object::TYPE_CLOTH_OBJECT )
        &&  ( pHitObject->GetType() != object::TYPE_FLAG ) )
    {
        ShakeView( s_MeleeShakeTime[m_ComboCount],
                   s_MeleeShakeAmount[m_ComboCount],
                   s_MeleeShakeSpeed[m_ComboCount] );
        DoFeedback( s_MeleeShakeTime[m_ComboCount] / 2.5f,
                    s_MeleeFeedbackAmount[m_ComboCount] );
    }

    // Build pain
    pain Pain;
    {
        Pain.Setup( xfs( "%s_MELEE_%d", GetLogicalName(), m_ComboCount ), GetGuid(), HitPosition );
        Pain.SetDirection( GetSimulationView().GetViewZ() );
        Pain.SetDirectHitGuid( DirectHitGuid );
        Pain.SetCollisionInfo( g_CollisionMgr.m_Collisions[0] );
        Pain.ApplyToObject( DirectHitGuid );

        // melee impact FX
        particle_emitter::CreateProjectileCollisionEffect( g_CollisionMgr.m_Collisions[0], GetGuid() );
    }
}
