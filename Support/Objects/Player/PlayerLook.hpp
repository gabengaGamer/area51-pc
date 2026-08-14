//=========================================================================
//
//  PlayerLook.hpp
//
//=========================================================================

#ifndef PLAYER_LOOK_HPP
#define PLAYER_LOOK_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"
#include "PlayerInput.hpp"

//=========================================================================
//  DATA CONTRACTS
//=========================================================================

struct PlayerLookSample
{
                    PlayerLookSample                ( void );

    xbool           HasGamepadInput                 ( void ) const;

    f32             MouseYaw;
    f32             MousePitch;
    f32             GamepadYaw;
    f32             GamepadPitch;
    f32             GamepadOuterYaw;
};

//-------------------------------------------------------------------------

struct PlayerLookRotation
{
                    PlayerLookRotation              ( void );

    radian          PitchDelta;
    radian          YawDelta;
};

//-------------------------------------------------------------------------

// Per-axis invert/sensitivity, resolved by the caller from the player profile.
struct PlayerLookSettings
{
    xbool           MouseInvertYaw;
    xbool           MouseInvertPitch;
    xbool           GamepadInvertYaw;
    xbool           GamepadInvertPitch;
    u32             GamepadSensitivityYaw;
    u32             GamepadSensitivityPitch;
    u32             MouseSensitivityYaw;
    u32             MouseSensitivityPitch;
};

//=========================================================================
//  PLAYER LOOK
//=========================================================================

class PlayerLook
{
public:

                    PlayerLook                      ( void );

            void    Clear                           ( void );
            f32     GetMouseLookSensitivityScale    ( u32 Sensitivity ) const;

            void    BuildSample                     ( PlayerLookInput const& Input,
                                                       PlayerLookSettings const& Settings,
                                                       PlayerLookSample& Sample ) const;

            void    EvaluateRotation                ( PlayerLookSample const& Sample,
                                                       f32 DeltaTime,
                                                       f32 GamepadYawAimModifier,
                                                       f32 GamepadPitchAimModifier,
                                                       PlayerLookRotation& Rotation );

            f32     GetYawStickSensitivity          ( void ) const { return m_YawStickSensitivity; }
            f32     GetPitchStickSensitivity        ( void ) const { return m_PitchStickSensitivity; }
            void    SetBaseStickSensitivity         ( f32 YawSensitivity, f32 PitchSensitivity );
            void    ScaleStickSensitivity           ( f32 Multiplier );
            void    ResetStickSensitivity           ( void );

private:

            void    BuildMouseSample                ( PlayerLookInput const& Input,
                                                       PlayerLookSettings const& Settings,
                                                       PlayerLookSample& Sample ) const;
            void    BuildGamepadSample              ( PlayerLookInput const& Input,
                                                       PlayerLookSettings const& Settings,
                                                       PlayerLookSample& Sample ) const;
    static  f32     ScaleGamepadLookMagnitude       ( f32 Magnitude );
    static  void    ScaleGamepadLookVector          ( vector2& Look );
    static  void    EvaluateMouseRotation           ( PlayerLookSample const& Sample,
                                                       PlayerLookRotation& Rotation );
            void    EvaluateGamepadRotation         ( PlayerLookSample const& Sample,
                                                       f32 DeltaTime,
                                                       f32 GamepadYawAimModifier,
                                                       f32 GamepadPitchAimModifier,
                                                       PlayerLookRotation& Rotation );
            void    AdvanceOuterYawBoost            ( f32 Target, f32 DeltaTime );

    f32             m_YawStickSensitivity;
    f32             m_PitchStickSensitivity;
    f32             m_OriginalYawStickSensitivity;
    f32             m_OriginalPitchStickSensitivity;
    f32             m_OuterYawBoost;
};

//=========================================================================
#endif // PLAYER_LOOK_HPP
