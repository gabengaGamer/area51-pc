//=========================================================================
//
//  PlayerLookPolicy.cpp
//
//=========================================================================

#include "PlayerLook.hpp"
#include "StateMgr/PlayerProfile.hpp"

//=========================================================================

static f32 const s_BaseYawRateFraction    = 2.0f / 5.5f;
static f32 const s_OuterYawThreshold      = 0.95f;
static f32 const s_OuterYawRampUpTime     = 0.25f;
static f32 const s_OuterYawRampDownTime   = 0.10f;
static f32 const s_MouseYawRadiansPerUnit = DEG_TO_RAD( 0.022f );
static f32 const s_MousePitchRadiansPerUnit
                                             = DEG_TO_RAD( 0.022f );

struct ControllerScaleTweak
{
    vector2 Point0;
    vector2 Point1;
    vector2 Direction0;
    vector2 Direction1;
};

ControllerScaleTweak g_ControllerScaleTweak
    = { vector2( 0.0f, 0.0f ),
        vector2( 1.0f, 1.0f ),
        vector2( 1.0f, 0.1f ),
        vector2( 0.0f, 3.2f ) };

//=========================================================================

PlayerLookSample::PlayerLookSample( void ) :
    MouseYaw     ( 0.0f ),
    MousePitch   ( 0.0f ),
    GamepadYaw      ( 0.0f ),
    GamepadPitch    ( 0.0f ),
    GamepadOuterYaw ( 0.0f )
{
}

//=========================================================================

xbool PlayerLookSample::HasGamepadInput( void ) const
{
    return( (x_abs( GamepadYaw   ) >= F32_MIN) ||
            (x_abs( GamepadPitch ) >= F32_MIN) );
}

//=========================================================================

PlayerLookRotation::PlayerLookRotation( void ) :
    PitchDelta( 0.0f ),
    YawDelta  ( 0.0f )
{
}

//=========================================================================

PlayerLook::PlayerLook( void ) :
    m_YawStickSensitivity           ( 5.5f ),
    m_PitchStickSensitivity         ( 1.5f ),
    m_OriginalYawStickSensitivity   ( 5.5f ),
    m_OriginalPitchStickSensitivity ( 1.5f ),
    m_OuterYawBoost                 ( 0.0f )
{
}

//=========================================================================

void PlayerLook::Clear( void )
{
    m_OuterYawBoost = 0.0f;
}

//=========================================================================

f32 PlayerLook::ScaleGamepadLookMagnitude( f32 Magnitude )
{
    f32 const S    = x_clamp( Magnitude, 0.0f, 1.0f );
    f32 const S2   = S * S;
    f32 const S3   = S * S * S;
    f32 const H1   = (2.0f * S3) - (3.0f * S2) + 1.0f;
    f32 const H2   = (-2.0f * S3) + (3.0f * S2);
    f32 const H3   = S3 - (2.0f * S2) + S;
    f32 const H4   = S3 - S2;

    f32 const ScaledValue = (H1 * g_ControllerScaleTweak.Point0.Y)
                          + (H2 * g_ControllerScaleTweak.Point1.Y)
                          + (H3 * g_ControllerScaleTweak.Direction0.Y)
                          + (H4 * g_ControllerScaleTweak.Direction1.Y);

    return x_clamp( ScaledValue, 0.0f, 1.0f );
}

//=========================================================================

void PlayerLook::ScaleGamepadLookVector( vector2& Look )
{
    f32 Magnitude = Look.Length();
    if( Magnitude <= F32_MIN )
    {
        Look.Zero();
        return;
    }

    if( Magnitude > 1.0f )
    {
        Look /= Magnitude;
        Magnitude = 1.0f;
    }

    Look *= ScaleGamepadLookMagnitude( Magnitude ) / Magnitude;
}

//=========================================================================

f32 PlayerLook::GetMouseLookSensitivityScale( u32 Sensitivity ) const
{
    static const f32 SensitivityMin = 0.10f;
    static const f32 SensitivityMax = 12.00f;
    static const f32 SettingMax     = static_cast<f32>( MOUSE_SENSITIVITY_MAX );

    Sensitivity = MIN( Sensitivity, MOUSE_SENSITIVITY_MAX );
    return SensitivityMin + ((SensitivityMax - SensitivityMin) *
                             (static_cast<f32>( Sensitivity ) / SettingMax));
}

//=========================================================================

void PlayerLook::BuildMouseSample( PlayerLookInput const&    Input,
                                   PlayerLookSettings const& Settings,
                                   PlayerLookSample&         Sample ) const
{
    Sample.MouseYaw     = Input.MouseYaw;
    Sample.MousePitch   = Input.MousePitch;

    if( Settings.MouseInvertYaw )
    {
        Sample.MouseYaw = -Sample.MouseYaw;
    }
    if( Settings.MouseInvertPitch )
    {
        Sample.MousePitch = -Sample.MousePitch;
    }

    Sample.MouseYaw   *= GetMouseLookSensitivityScale( Settings.MouseSensitivityYaw );
    Sample.MousePitch *= GetMouseLookSensitivityScale( Settings.MouseSensitivityPitch );
}

//=========================================================================

void PlayerLook::BuildGamepadSample( PlayerLookInput const&    Input,
                                     PlayerLookSettings const& Settings,
                                     PlayerLookSample&         Sample ) const
{
    Sample.GamepadYaw   = Input.GamepadYaw;
    Sample.GamepadPitch = Input.GamepadPitch;

    vector2 GamepadLook( Sample.GamepadYaw, Sample.GamepadPitch );
    f32 const OuterYawRange = MAX( 1.0f - s_OuterYawThreshold, F32_MIN );
    Sample.GamepadOuterYaw = x_clamp( (x_abs( GamepadLook.X ) - s_OuterYawThreshold) /
                                      OuterYawRange,
                                      0.0f,
                                      1.0f );

    if( Settings.GamepadInvertYaw )
    {
        GamepadLook.X = -GamepadLook.X;
    }
    if( Settings.GamepadInvertPitch )
    {
        GamepadLook.Y = -GamepadLook.Y;
    }

    ScaleGamepadLookVector( GamepadLook );
    Sample.GamepadYaw   = GamepadLook.X;
    Sample.GamepadPitch = GamepadLook.Y;

    Sample.GamepadYaw += Sample.GamepadYaw *
                         ((static_cast<f32>( Settings.GamepadSensitivityYaw ) - 50.0f) / 100.0f);

    Sample.GamepadPitch += Sample.GamepadPitch *
                           ((static_cast<f32>( Settings.GamepadSensitivityPitch ) - 50.0f) / 100.0f);
}

//=========================================================================

void PlayerLook::BuildSample( PlayerLookInput const&    Input,
                              PlayerLookSettings const& Settings,
                              PlayerLookSample&         Sample ) const
{
    Sample = PlayerLookSample();
    BuildMouseSample( Input, Settings, Sample );
    BuildGamepadSample( Input, Settings, Sample );
}

//=========================================================================

void PlayerLook::EvaluateMouseRotation( PlayerLookSample const& Sample,
                                        PlayerLookRotation&     Rotation )
{
    Rotation.YawDelta   += Sample.MouseYaw   * s_MouseYawRadiansPerUnit;
    Rotation.PitchDelta += Sample.MousePitch * s_MousePitchRadiansPerUnit;
}

//=========================================================================

void PlayerLook::AdvanceOuterYawBoost( f32 Target, f32 DeltaTime )
{
    Target = x_clamp( Target, 0.0f, 1.0f );
    f32 const RampTime = (Target > m_OuterYawBoost)
                       ? s_OuterYawRampUpTime
                       : s_OuterYawRampDownTime;
    f32 const Step = DeltaTime / MAX( RampTime, F32_MIN );

    if( Target > m_OuterYawBoost )
    {
        m_OuterYawBoost = MIN( Target, m_OuterYawBoost + Step );
    }
    else
    {
        m_OuterYawBoost = MAX( Target, m_OuterYawBoost - Step );
    }
}

//=========================================================================

void PlayerLook::EvaluateGamepadRotation( PlayerLookSample const& Sample,
                                          f32 DeltaTime,
                                          f32 GamepadYawAimModifier,
                                          f32 GamepadPitchAimModifier,
                                          PlayerLookRotation& Rotation )
{
    AdvanceOuterYawBoost( Sample.GamepadOuterYaw, DeltaTime );

    if( x_abs( Sample.GamepadYaw ) > 0.0f )
    {
        f32 const FastYawRate = MAX( 0.0f, m_YawStickSensitivity );
        f32 const BaseYawRate = FastYawRate * s_BaseYawRateFraction;
        f32 const YawRate = BaseYawRate +
                            ((FastYawRate - BaseYawRate) * m_OuterYawBoost);
        Rotation.YawDelta += Sample.GamepadYaw * YawRate * DeltaTime * GamepadYawAimModifier;
    }

    Rotation.PitchDelta += Sample.GamepadPitch * DeltaTime * m_PitchStickSensitivity *
                           GamepadPitchAimModifier;
}

//=========================================================================

void PlayerLook::EvaluateRotation( PlayerLookSample const& Sample,
                                   f32                     DeltaTime,
                                   f32                     GamepadYawAimModifier,
                                   f32                     GamepadPitchAimModifier,
                                   PlayerLookRotation&     Rotation )
{
    DeltaTime = MAX( 0.0f, DeltaTime );
    Rotation  = PlayerLookRotation();

    EvaluateMouseRotation( Sample, Rotation );
    EvaluateGamepadRotation( Sample,
                             DeltaTime,
                             GamepadYawAimModifier,
                             GamepadPitchAimModifier,
                             Rotation );
}

//=========================================================================

void PlayerLook::SetBaseStickSensitivity( f32 YawSensitivity, f32 PitchSensitivity )
{
    m_YawStickSensitivity           = YawSensitivity;
    m_PitchStickSensitivity         = PitchSensitivity;
    m_OriginalYawStickSensitivity   = YawSensitivity;
    m_OriginalPitchStickSensitivity = PitchSensitivity;
}

//=========================================================================

void PlayerLook::ScaleStickSensitivity( f32 Multiplier )
{
    m_YawStickSensitivity   *= Multiplier;
    m_PitchStickSensitivity *= Multiplier;
}

//=========================================================================

void PlayerLook::ResetStickSensitivity( void )
{
    m_YawStickSensitivity   = m_OriginalYawStickSensitivity;
    m_PitchStickSensitivity = m_OriginalPitchStickSensitivity;
}
