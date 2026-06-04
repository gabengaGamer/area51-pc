//==============================================================================
//
//
//  PlayerInput.cpp
//
// 
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#if defined(bwatson)
#define X_SUPPRESS_LOGS
#endif
#include "player.hpp"
#include "InputMgr\GamePad.hpp"
#include "objects\ParticleEmiter.hpp"
#include "objects\Render\PostEffectMgr.hpp"
#include "objects\SpawnPoint.hpp"
#include "Objects\Event.hpp"
#include "Sound\EventSoundEmitter.hpp"
#include "..\support\templatemgr\TemplateMgr.hpp"
#include "characters\Character.hpp"
#include "Characters\Conversation_Packet.hpp"
#include "GameLib\StatsMgr.hpp"
#include "GameLib\RenderContext.hpp"
#include "Dictionary\global_dictionary.hpp"
#include "objects\WeaponSniper.hpp"
#include "objects\ThirdPersonCamera.hpp"
#include "objects\WeaponSMP.hpp"
#include "objects\Corpse.hpp"
#include "NetworkMgr/NetObjMgr.hpp"
#include "NetworkMgr/Voice/VoiceMgr.hpp"
#include "Objects\Ladders\Ladder_Field.hpp"
#include "Objects\GrenadeProjectile.hpp"
#include "Objects\GravChargeProjectile.hpp"
#include "Objects\JumpingBeanProjectile.hpp"
#include "render\LightMgr.hpp"
#include "Objects\Door.hpp"
#include "objects\Projector.hpp"
#include "objects\WeaponMutation.hpp"
#include "StateMgr\StateMgr.hpp"
#include "NetworkMgr\GameMgr.hpp"
#include "objects\HudObject.hpp"
#include "Characters\ActorEffects.hpp"
#include "Configuration/GameConfig.hpp"
#include "objects\turret.hpp"
#include "objects\WeaponShotgun.hpp"
#include "Gamelib/DebugCheats.hpp"
#include "objects\FocusObject.hpp"
#include "PerceptionMgr\PerceptionMgr.hpp"
#include "Objects\LoreObject.hpp"
#include "Objects\Camera.hpp"

#ifdef X_EDITOR
#include "../Apps/Editor/Project.hpp"
#else
#include "NetworkMgr\MsgMgr.hpp"
#include "Menu\DebugMenu2.hpp"
#endif

f32 g_CrouchUpVelocity    = 80.0f;

static f32 MIN_TIME_BETWEEN_MUTATION_CHANGES = 0.5f;

//==============================================================================
// HELPER FUNCTIONS
//==============================================================================

struct controller_scale_tweak
{
    vector2 Point0;
    vector2 Point1;
    vector2 Direction0;
    vector2 Direction1;
} ;

//---------------------------------------------------------------------------

controller_scale_tweak g_ControllerScaleTweak
    = { vector2( 0.0f, 0.0f ),      // Point0
        vector2( 1.0f, 1.0f ),      // Point1
        vector2( 1.0f, 0.1f ),      // Direction0
        vector2( 0.0f, 3.2f ) };    // Direction1

//---------------------------------------------------------------------------

static 
f32 ScaleGamepadLookAxis( f32 Value )
{
    f32 Sign = (Value < 0.0f) ? -1.0f : 1.0f;
    f32 s = x_abs( Value );
    f32 s2 = s * s;
    f32 s3 = s * s * s;
    f32 h1 = (2.0f * s3) - (3 * s2) + 1;
    f32 h2 = (-2.0f * s3) + (3 * s2);
    f32 h3 = s3 - (2.0f * s2) + s;
    f32 h4 = s3 - s2;

    f32 ScaledValue = (h1 * g_ControllerScaleTweak.Point0.Y)
                    + (h2 * g_ControllerScaleTweak.Point1.Y)
                    + (h3 * g_ControllerScaleTweak.Direction0.Y)
                    + (h4 * g_ControllerScaleTweak.Direction1.Y);

    return ScaledValue * Sign;
}

//==============================================================================

static 
f32 GetMouseLookSensitivityScale( u32 Sensitivity )
{
    static const f32 SensitivityMin = 0.10f;
    static const f32 SensitivityMax = 6.00f;
    static const f32 SettingMax     = 32.0f;

    if( Sensitivity > (u32)SettingMax )
        Sensitivity = (u32)SettingMax;

    return SensitivityMin + ((SensitivityMax - SensitivityMin) * ((f32)Sensitivity / SettingMax));
}

//==============================================================================
// FUNCTIONS
//==============================================================================

void player::UpdateUserInput(  f32 DeltaTime )
{
    if (!m_bActivePlayer)
        return;

#ifndef X_EDITOR
    if ( g_StateMgr.IsPaused() )
    {
        ClearMoveLookInput();
        g_IngamePad[m_ActivePlayerPad].ClearAllActions();
        return;
    }
#endif

    if ( m_ViewCinematicPlaying )
    {
        //Play the view cinematic
        UpdateViewCinematic( DeltaTime );
    }
    else if ( !m_Cinema.m_bCinemaOn && (!m_bDead || !m_bCanDie) )
    {
        if( m_ActivePlayerPad != -1 )
        {
            //handles button press events
            OnButtonInput();
            UpdateMoveLookInput();
        }
    }
    else if ( m_Cinema.m_bCinemaOn )
    {
        ClearMoveLookInput();
        m_fMoveValue            = 0;
        m_fStrafeValue          = 0;
        m_fRawControllerYaw     = 0;
        m_fRawControllerPitch   = 0;
        m_fPreviousYawValue     = 0;
        m_fPreviousPitchValue   = 0;
        g_IngamePad[m_ActivePlayerPad].ClearAllActions();
    }
}
//==============================================================================

void player::ClearMoveLookInput( void )
{
    m_fMoveValue            = 0.0f;
    m_fStrafeValue          = 0.0f;
    m_fRawControllerPitch   = 0.0f;
    m_fRawControllerYaw     = 0.0f;
    m_fPitchValue           = 0.0f;
    m_fYawValue             = 0.0f;
    m_LookInputMode         = LOOK_INPUT_NONE;
    m_YawLookInputMode      = LOOK_INPUT_NONE;
    m_PitchLookInputMode    = LOOK_INPUT_NONE;
    m_fPreviousPitchValue   = 0.0f;
    m_fPreviousYawValue     = 0.0f;
}

//===========================================================================

void player::ScaleGamepadLookInput( void )
{
    if( m_YawLookInputMode == LOOK_INPUT_GAMEPAD )
        m_fYawValue = ScaleGamepadLookAxis( m_fRawControllerYaw );

    if( m_PitchLookInputMode == LOOK_INPUT_GAMEPAD )
        m_fPitchValue = ScaleGamepadLookAxis( m_fRawControllerPitch );

#ifndef X_EDITOR
    // do input sensitivity
    {
        player_profile& p = g_StateMgr.GetActiveProfile(g_StateMgr.GetProfileListIndex(m_LocalSlot));

        u32 sensitivity_H = p.GetSensitivity(SM_X_SENSITIVITY);
        u32 sensitivity_V = p.GetSensitivity(SM_Y_SENSITIVITY);

        // Sensitivity setting range
        // |___________I__________|
        // 0           50        100
        // 
        // 0   = -50%
        // 50  = normal
        // 100 = +50%
        //

        f32 Scalar_H = (f32)((sensitivity_H - 50.0f)/100.0f);
        f32 Scalar_V = (f32)((sensitivity_V - 50.0f)/100.0f);

        if( m_YawLookInputMode == LOOK_INPUT_GAMEPAD )
            m_fYawValue = m_fYawValue + (m_fYawValue * Scalar_H);

        if( m_PitchLookInputMode == LOOK_INPUT_GAMEPAD )
            m_fPitchValue = m_fPitchValue + (m_fPitchValue * Scalar_V);
    }
#endif
}

//==============================================================================

void player::ScaleMouseLookInput( void )
{
#if !defined(X_EDITOR)
    player_profile& p = g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex( m_LocalSlot ) );

    u32 sensitivity_H = p.GetSensitivity( SM_X_SENSITIVITY );
    u32 sensitivity_V = p.GetSensitivity( SM_Y_SENSITIVITY );
#else
    u32 sensitivity_H = 16; //HACK HACK HACK!!!
    u32 sensitivity_V = 16; //HACK HACK HACK!!!
#endif

    f32 scaleH = GetMouseLookSensitivityScale( sensitivity_H );
    f32 scaleV = GetMouseLookSensitivityScale( sensitivity_V );

    if( m_YawLookInputMode == LOOK_INPUT_MOUSE )
        m_fYawValue = m_fRawControllerYaw * scaleH;

    if( m_PitchLookInputMode == LOOK_INPUT_MOUSE )
        m_fPitchValue = m_fRawControllerPitch * scaleV;
}

//==============================================================================

void player::UpdateMoveLookInput(void)
{
    ASSERT( m_ActivePlayerPad != -1 );

    m_fPreviousYawValue   = m_fYawValue;
    m_fPreviousPitchValue = m_fPitchValue;
    m_LookInputMode       = LOOK_INPUT_NONE;
    m_YawLookInputMode    = LOOK_INPUT_NONE;
    m_PitchLookInputMode  = LOOK_INPUT_NONE;
    m_fYawValue           = 0.0f;
    m_fPitchValue         = 0.0f;

    const ingame_pad::logical& LookHorizontal = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::LOOK_HORIZONTAL );
    const ingame_pad::logical& LookVertical   = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::LOOK_VERTICAL   );

    m_fRawControllerYaw   = -LookHorizontal.GetIsValue();
    m_fRawControllerPitch =  LookVertical.GetIsValue();

#if !defined(X_EDITOR)
    player_profile& p = g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex( m_LocalSlot ) );

    //MAB: removed invert Y global var - only check profile now
    if( p.m_bInvertY )
    {
        m_fRawControllerPitch = -m_fRawControllerPitch;
    }
#else
    // invert Y
    extern xbool g_EditorInvertY;
    if( g_EditorInvertY )
    {
        m_fRawControllerPitch = -m_fRawControllerPitch;
    }

    // Mirror weapon?
    extern xbool g_MirrorWeapon;
    if( g_MirrorWeapon )
    {
        // Turn on mirror for hands
        m_AnimPlayer.SetMirrorBone( 0 );

        // Turn on mirror for weapon
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if( pWeapon )
            pWeapon->GetCurrentAnimPlayer().SetMirrorBone( 0 );
    }
    else
    {
        // Turn off mirror for hands
        m_AnimPlayer.SetMirrorBone( -1 );

        // Turn off mirror for weapon
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if( pWeapon )
            pWeapon->GetCurrentAnimPlayer().SetMirrorBone( -1 );
    }
#endif

    if ( !m_bInTurret )
    {
        f32 MoveForward  = x_max( 0.0f, g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::MOVE_FORWARD  ).GetIsValue() );
        f32 MoveBackward = x_max( 0.0f, g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::MOVE_BACKWARD ).GetIsValue() );
        f32 StrafeRight  = x_max( 0.0f, g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::STRAFE_RIGHT  ).GetIsValue() );
        f32 StrafeLeft   = x_max( 0.0f, g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::STRAFE_LEFT   ).GetIsValue() );

        m_fMoveValue   =  (MoveForward  - MoveBackward);
        m_fStrafeValue = -(StrafeRight  - StrafeLeft);
    }
    else
    {
        m_fMoveValue   = 0.0f;
        m_fStrafeValue = 0.0f;
    }

    const input_action_source& YawSource   = LookHorizontal.GetIsSource();
    const input_action_source& PitchSource = LookVertical.GetIsSource();

    if( YawSource.GetDevice() == INPUT_DEVICE_GAMEPAD )
    {
        m_YawLookInputMode = LOOK_INPUT_GAMEPAD;
    }
    else if( YawSource.GetDevice() == INPUT_DEVICE_MOUSE )
    {
        m_YawLookInputMode = LOOK_INPUT_MOUSE;
    }

    if( PitchSource.GetDevice() == INPUT_DEVICE_GAMEPAD )
    {
        m_PitchLookInputMode = LOOK_INPUT_GAMEPAD;
    }
    else if( PitchSource.GetDevice() == INPUT_DEVICE_MOUSE )
    {
        m_PitchLookInputMode = LOOK_INPUT_MOUSE;
    }

    if( m_YawLookInputMode == m_PitchLookInputMode )
    {
        m_LookInputMode = m_YawLookInputMode;
    }
    else if( m_YawLookInputMode == LOOK_INPUT_NONE )
    {
        m_LookInputMode = m_PitchLookInputMode;
    }
    else if( m_PitchLookInputMode == LOOK_INPUT_NONE )
    {
        m_LookInputMode = m_YawLookInputMode;
    }
    else
    {
        m_LookInputMode = LOOK_INPUT_MIXED;
    }

    ScaleGamepadLookInput();
    ScaleMouseLookInput();
}

//===========================================================================

void player::OnButtonInput( void )
{
    // don't allow player to switch weapons, zoom in, attack, etc.
    if( m_bHidePlayerArms )
    {
        return;
    }
    // This is to make sure you don't lean or fire on the same button press as 
    // voting or respawning, respectively.
    {   
        xbool PrimaryDown = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_PRIMARY ).GetIsValue() > 0.25f;
        if( !PrimaryDown )
        {
            m_bRespawnButtonPressed = FALSE;
        }

        if( !(g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_VOTE_YES ).GetIsValue() > 0.25f) &&
            !(g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_VOTE_NO  ).GetIsValue() > 0.25f) )
        {
            m_bVoteButtonPressed = FALSE;
        }
    }

    //
    // Handle voting menu input
    //
    if( m_VoteCanCast )
    {
        if( !m_VoteMode )
        {
            // Activate vote mode / menu.
            if( g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_VOTE_MENU_ON ).GetWasValue() > 0.25f )
            {
                m_VoteMode = TRUE;
                // Activate the vote menu.
                LOG_MESSAGE( "player::OnButtonInput", "Vote menu activated." );
                return;
            }
        }
        else
        {
            // Deactivate vote mode / menu.
            if( g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_VOTE_MENU_OFF ).GetWasValue() > 0.25f )
            {
                m_VoteMode      = FALSE;
                // Deactivate the vote menu.
                LOG_MESSAGE( "player::OnButtonInput", "Vote menu deactivated." );
                return;
            }

            // Vote YES.
            if( g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_VOTE_YES ).GetWasValue() > 0.25f )
            {
                m_VoteMode      = FALSE;
                m_VoteCanCast   = FALSE;
                m_bVoteButtonPressed = TRUE;
                // Deactivate the vote menu.
                VoteCast( +1 );
                LOG_MESSAGE( "player::OnButtonInput", "Vote YES." );
                return;
            }

            // Vote NO.
            if( g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_VOTE_NO ).GetWasValue() > 0.25f )
            {
                m_VoteMode      = FALSE;
                m_VoteCanCast   = FALSE;
                m_bVoteButtonPressed = TRUE;
                // Deactivate the vote menu.
                VoteCast( -1 );
                LOG_MESSAGE( "player::OnButtonInput", "Vote NO." );
                return;
            }

            // Vote ABSTAIN.
            if( g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_VOTE_ABSTAIN ).GetWasValue() > 0.25f )
            {
                m_VoteMode      = FALSE;
                m_VoteCanCast   = FALSE;
                // Deactivate the vote menu.
                VoteCast( 0 );
                LOG_MESSAGE( "player::OnButtonInput", "Vote ABSTAIN." );
                return;
            }
        }
    }
    else 
    {
        // So that the vote key doesn't show up if the vote expires while it's showing.
        m_VoteMode = FALSE;

        if( g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_DROP_FLAG ).GetIsValue() > 0.25f )
        {
            #ifndef X_EDITOR
            m_NetDirtyBits |= DROP_ITEM_BIT;
            if( g_NetworkMgr.IsServer() )
                pGameLogic->DropFlag( m_NetSlot );
            #endif
        }
    }

    //
    // Do nothing if stunned.
    //
    if ( m_NonExclusiveStateBitFlag & NE_STATE_STUNNED )
    {
        return;
    }

    //update base class button input
    ASSERT( m_ActivePlayerPad != -1 );

    xbool bStopCrouching = FALSE;


#ifndef X_EDITOR
    if( g_NetworkMgr.IsOnline() )
    {
        if( g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_TALK_MODE_TOGGLE ).GetWasValue() > 0.25f )
        {
            g_VoiceMgr.ToggleTalkMode();
        }

        // Voice chat
        {
            g_VoiceMgr.SetTalking( FALSE );

            if( g_VoiceMgr.IsHeadsetPresent() == TRUE )
            {
                // check for voice chat
                if( g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_CHAT ).GetIsValue() > 0.25f )
                {
                    // Test to make sure the player is allowed to chat
                    if( (g_VoiceMgr.IsVoiceBanned()  == FALSE) &&
                        (g_VoiceMgr.IsVoiceEnabled() ==  TRUE) )
                         g_VoiceMgr.SetTalking( TRUE );
                }
            }
        }
    }

    player_profile& p = g_StateMgr.GetActiveProfile(g_StateMgr.GetProfileListIndex(m_LocalSlot));
    if( p.m_bCrouchOn )
    {
        xbool CrouchKeyPressed = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_CROUCH ).GetWasValue() > 0.25f;
        if( CrouchKeyPressed )
        {
            // crouch is a toggle and we're crouching so turn it off
            if( IsCrouching() )
            {
                bStopCrouching = TRUE;
            }
            else if ( !m_bInTurret )
            {
                // move the arms a little
                m_ArmsVelocity += vector3( 0.0f, -g_CrouchUpVelocity, 0.0f );

                // Start crouching
                SetIsCrouching( TRUE );
            }
        }
    }
    else
#endif
    {
        xbool CrouchKeyIsPressed = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_CROUCH ).GetIsValue() > 0.25f;
        if( CrouchKeyIsPressed )
        {
            // only do this if we weren't crouching previously
            if( !IsCrouching() && !m_bInTurret )
            {
                // move the arms a little
                m_ArmsVelocity += vector3( 0.0f, -g_CrouchUpVelocity, 0.0f );

                // Start crouching
                SetIsCrouching( TRUE );
            }
        }
        else
        {
            // Only do this if we are already crouching
            if( IsCrouching() )
            {
                bStopCrouching = TRUE;
            }
        }
    }

    // for whatever reason, we need to quit crouching
    if( bStopCrouching )
    {
        // Stop crouching
        SetIsCrouching( FALSE );

        // move the arms a little
        m_ArmsVelocity += vector3( 0.0f, g_CrouchUpVelocity, 0.0f );
    }

    // Jump button
    xbool JumpPressed = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_JUMP ).GetWasValue() > 0.25f;
    if( !m_bInTurret && JumpPressed && m_bCanJump )
    {
        Jump();

        // Stop crouching when you jump
        SetIsCrouching( FALSE );
    }

    // Look for 'game speak' buttons.
    OnGameSpeak() ;

    //
    // Toggle mutation
    //
    xbool Multiplayer = FALSE;
#ifndef X_EDITOR
    Multiplayer = GameMgr.IsGameMultiplayer();
#endif

    //
    // mreed:
    // This is a little strange, but we need more pressure to toggle mutation when we're 
    // leaning. This means it's less likely to succeed with a "WasValue" since you only get
    // one shot at it. So, when leaning, we will use "IsValue" so that we have more than
    // one chance to get the pressure needed. The side-effect of this is that we are no longer
    // debounced for mutation when leaning.
    // Luckily (?), there is a mutation frequency timer that prevents rapid mutation/demutation
    // so we'll always get through the mutation cycle before we come back. Plus, this only matters
    // when intentionally trying to mutate while leaning.
    //
    // So, this is where we get the button press values
    //
    ingame_pad::logical& Logical   = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_MUTATION   );
    ingame_pad::logical& MPLogical = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_MP_MUTATE  );
    
    const xbool IsLeaning = x_abs( m_SoftLeanAmount ) > 0.1f;
    const f32 ActionMutation   = IsLeaning ? Logical.GetIsValue()   : Logical.GetWasValue();
    const f32 ActionMPMutation = IsLeaning ? MPLogical.GetIsValue() : MPLogical.GetWasValue();

    xbool MutationPressed = Multiplayer
                          ? (ActionMutation > 0.25f)
                          : (ActionMPMutation > 0.25f);

    static f32 MinTimeSinceUseToThrowGrenade = 1.0f;
    static f32 MinTimeSinceUseToMutate = 1.0f;

    if (   !m_bInTurret 
        &&  m_bCanToggleMutation 
        && !IsChangingMutation()
        &&  MutationPressed
        && (m_MutationChangeTime > MIN_TIME_BETWEEN_MUTATION_CHANGES)
        && (m_UseTime > MinTimeSinceUseToMutate) )
    {

        //
        // If we're leaning, we need to press harder to toggle mutation
        //
        xbool HaveButtonPressureToToggleMutation = TRUE;

        f32 MutationValue = 1.0f;

        if ( IsLeaning )
        {
            MutationValue = Multiplayer ? Logical.GetIsValue() : MPLogical.GetIsValue();
        }

        ASSERTS( MutationValue > 0.0f, "Mutation input is zero when toggling mutation; check the input mapping" );

        static f32 MinValueToToggleMutationWhileLeaning = 0.3f;
        HaveButtonPressureToToggleMutation = MutationValue > MinValueToToggleMutationWhileLeaning;

        if ( HaveButtonPressureToToggleMutation )
        {
            // OK, our input is in order, see what we need to do
            if( m_Inventory2.HasItem( INVEN_WEAPON_MUTATION ) && !IsMutated() )
            {
                SetupMutationChange(TRUE);
            }
            else if( IsMutated() )
            {
                SetupMutationChange(FALSE);
            }        
        }
    }

    if ( !m_bInTurret && g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_CYCLE_RIGHT ).GetWasValue() > 0.25f )
    {
        OnWeaponSwitch2( CYCLE_RIGHT );
    }
    if ( !m_bInTurret && g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_CYCLE_LEFT ).GetWasValue() > 0.25f )
    {
        OnWeaponSwitch2( CYCLE_LEFT );
    }

    //
    // NOTE: TWEEK THIS, WE MIGHT WANT TO MAKE THE RAMP DOWN FIRST BEFORE DOING MELEE.
    //
    xbool MeleePressed = m_bIsMutated
                        ? g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_MUTANT_MELEE ).GetIsValue() > 0.25f
                        : g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_MELEE_ATTACK ).GetIsValue() > 0.25f;
    if( !m_bInTurret && MeleePressed )
    {
        animation_state MeleeState = ANIM_STATE_MELEE;

        // make sure we are completely mutated
        if( IsMutated() )
        {
            switch( m_CurrentAnimState )
            {
                // we don't want melee kicking off when we are switching to while we are mutated.
                // This will cause the meshes to get hosed
            case ANIM_STATE_SWITCH_TO:
            case ANIM_STATE_SWITCH_FROM:
                {}
                break;

                //////////////////////////////////////////////////////////////////////////
                // put any other mutation special cases here
                //////////////////////////////////////////////////////////////////////////

            default:
                {                    

                    // if we are mutated, do extreme melee attack.                
                    if( m_bMutationMeleeEnabled )
                    {
                        // if we are already attacking, return.
                        // NOTE: we can't expect SetAnimState to check if the anims are the same because we have 5 different ones here.
                        if( !m_bMeleeLunging )
                        {
                            MeleeState = SetupMutationMeleeWeapon();
                            SetMeleeState(MeleeState);
                        }
                    }                    
                }
                break;
            }
        }
        else // we aren't mutated, do normal melee stuff
        {   
            if( m_ComboCount >= MAX_COMBO_HITS )
            {
                m_ComboCount = MAX_COMBO_HITS-1;
                ASSERT(0);
                return;
            }  

            if( m_ComboCount == 0 )
            {
                // if you don't do this, when you demutate you will swing your wittle human arms and it looks dumb :)
                if( m_CurrentAnimState != ANIM_STATE_SWITCH_FROM && m_CurrentAnimState != ANIM_STATE_DISCARD )
                {
                    if( m_bCanRequestCombo && m_bLastMeleeHit )
                    {
                        // Still stage 0 and we're requesting to start a combo
                        SetMeleeState(ANIM_STATE_COMBO_BEGIN);
                    }
                    else
                    {
                        // we aren't requesting a combo yet, do initial melee
                        SetMeleeState(MeleeState);
                    }
                }
            }            

            // if you can request a combo, set the flag
            if( m_bCanRequestCombo )
            {
                m_bHitCombo = TRUE;
            }
        }
    }

    // don't throw a grenade if we're just exiting fly mode
#if defined( ENABLE_DEBUG_MENU )
    xbool GrenadePressed = (g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_THROW_GRENADE ).GetIsValue() > 0.25f) &&
                          !(g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_TALK_MODE_TOGGLE ).GetIsValue() > 0.25f);
#else
    xbool GrenadePressed = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_THROW_GRENADE ).GetIsValue() > 0.25f;
#endif


    if( GrenadePressed )
    {
        if (   (m_Inventory2.GetAmount( m_CurrentGrenadeType2 ) > 0)
            && ( !IsMutated() )
            && AllowedToFire() 
            && !m_bInTurret
            && (m_UseTime > MinTimeSinceUseToThrowGrenade) )
        {
            // Get a reference to the state that we are considering
            s32 GrenadeState = (m_CurrentGrenadeType2==INVEN_GRENADE_FRAG) ? ANIM_STATE_GRENADE : ANIM_STATE_ALT_GRENADE;

            // if we are already throwing a grenade of any type, don't any other grenade be thrown
            if( m_CurrentAnimState != ANIM_STATE_GRENADE && m_CurrentAnimState != ANIM_STATE_ALT_GRENADE )
            {
                state_anims& State = m_Anim[inventory2::ItemToWeaponIndex(m_CurrentWeaponItem)][GrenadeState];

                // Can we fire the secondary weapon?
                if( State.nPlayerAnims > 0 )
                {
                    new_weapon* pWeapon = GetCurrentWeaponPtr();
                    if( pWeapon )
                        pWeapon->ClearZoom();

                    SetAnimState( (animation_state)GrenadeState );
                }
            }
        }
    }

    // flashlight button
    xbool FlashlightPressed = Multiplayer
                             ? (g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_MP_FLASHLIGHT ).GetIsValue() > 0.25f)
                             : (g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_FLASHLIGHT ).GetWasValue() > 0.25f);

    if ( FlashlightPressed && !IsMutated() )
    {
        new_weapon* pWeapon = GetCurrentWeaponPtr();

        if( pWeapon && pWeapon->HasFlashlight() )
        {
            SetFlashlightActive( !IsFlashlightActive() );
        }
        else
        {
            // weapon is invalid?  Turn off flashlight then
            SetFlashlightActive( FALSE );
        }
    }

    // Use on a mutagen reservoir
    xbool UseKeyPressed = g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::ACTION_USE ).GetIsValue() > 0.25f;
    
    if ( UseKeyPressed && NearMutagenReservoir() )
    {
        static const f32 ChangeRate = 10.0f;
        AddMutagen( ChangeRate * m_DeltaTime );

        // play the sucking sound when refilling mutagen from a super-contagious dead body.
        if( m_SuckingMutagenLoopID == 0 )
        {
            m_SuckingMutagenLoopID = g_AudioMgr.Play( "SCDB_Suck_Mutagen_Loop", GetPosition(), GetZone1(), TRUE );
        }
    }
    else
    {
        if( m_SuckingMutagenLoopID != 0 )
        {
            // not refilling anymore, release ID
            g_AudioMgr.Release( m_SuckingMutagenLoopID, 1.0f );
            m_SuckingMutagenLoopID = 0;
        }
    }

    xbool LeanLeftPressed = (g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::LEAN_LEFT ).GetIsValue() > 0.25f) && !m_bVoteButtonPressed;
    xbool LeanRightPressed = (g_IngamePad[m_ActivePlayerPad].GetLogical( ingame_pad::LEAN_RIGHT ).GetIsValue() > 0.25f) && !m_bVoteButtonPressed;

    if ( !m_bInTurret && LeanLeftPressed )
    {
        UpdateLean( 1.0f );
    }
    else if ( !m_bInTurret && LeanRightPressed )
    {
        UpdateLean( -1.0f );
    }
    else
    {
        UpdateLean( 0.0f );
    }
}
