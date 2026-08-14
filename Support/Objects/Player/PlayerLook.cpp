//=========================================================================
//
//  PlayerLook.cpp
//
//  Thin player facade for look/view orchestration. Device-specific math
//  lives in PlayerLookPolicy.cpp.
//
//=========================================================================

#include "Player.hpp"
#include "StateMgr/StateMgr.hpp"

//=========================================================================

static void ApplyMouseZoomModifiers( PlayerLookSample& Sample,
                                     f32               HorizontalFovRatio,
                                     f32               MovementModifier )
{
    Sample.MouseYaw   /= HorizontalFovRatio;
    Sample.MouseYaw   *= MovementModifier;
    Sample.MousePitch *= MovementModifier;
}

//=========================================================================

static void ApplyGamepadZoomModifiers( PlayerLookSample& Sample,
                                       f32               HorizontalFovRatio,
                                       f32               MovementModifier )
{
    Sample.GamepadYaw   /= HorizontalFovRatio;
    Sample.GamepadPitch /= HorizontalFovRatio;
    Sample.GamepadYaw   *= MovementModifier;
    Sample.GamepadPitch *= MovementModifier;
}

//=========================================================================

void player::BuildLookInputSample( const PlayerLookInput& Input, PlayerLookSample& Sample )
{
    PlayerLookSettings Settings;

#if !defined(X_EDITOR)
    player_profile& Profile = g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex( m_LocalSlot ) );

    Settings.MouseInvertYaw          = Profile.IsAxisInverted( profile_control_device::Mouse,   profile_control_axis::X );
    Settings.GamepadInvertYaw        = Profile.IsAxisInverted( profile_control_device::Gamepad, profile_control_axis::X );
    Settings.MouseInvertPitch        = Profile.IsAxisInverted( profile_control_device::Mouse,   profile_control_axis::Y );
    Settings.GamepadInvertPitch      = Profile.IsAxisInverted( profile_control_device::Gamepad, profile_control_axis::Y );
    Settings.GamepadSensitivityYaw   = Profile.GetSensitivity( profile_control_device::Gamepad, profile_control_axis::X );
    Settings.GamepadSensitivityPitch = Profile.GetSensitivity( profile_control_device::Gamepad, profile_control_axis::Y );
    Settings.MouseSensitivityYaw     = Profile.GetSensitivity( profile_control_device::Mouse,   profile_control_axis::X );
    Settings.MouseSensitivityPitch   = Profile.GetSensitivity( profile_control_device::Mouse,   profile_control_axis::Y );
#else
    extern xbool g_EditorInvertY;

    Settings.MouseInvertYaw          = FALSE;
    Settings.GamepadInvertYaw        = FALSE;
    Settings.MouseInvertPitch        = g_EditorInvertY;
    Settings.GamepadInvertPitch      = g_EditorInvertY;
    Settings.GamepadSensitivityYaw   = 50;
    Settings.GamepadSensitivityPitch = 50;
    Settings.MouseSensitivityYaw     = 16;
    Settings.MouseSensitivityPitch   = 16;
#endif

    m_Look.BuildSample( Input, Settings, Sample );
}

//=========================================================================

void player::ApplyZoomLookModifiers( PlayerLookSample& Sample )
{
    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( !pWeapon || !pWeapon->IsZoomEnabled() )
    {
        return;
    }

    f32 const HorizontalFovRatio = m_OriginalViewInfo.XFOV / pWeapon->GetXFOV();
    f32 const MovementModifier = pWeapon->GetZoomMovementMod();

    ApplyMouseZoomModifiers( Sample,
                             HorizontalFovRatio,
                             MovementModifier );
    ApplyGamepadZoomModifiers( Sample,
                               HorizontalFovRatio,
                               MovementModifier );
}

//=========================================================================
