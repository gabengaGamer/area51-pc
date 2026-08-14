//=========================================================================
//
//  PlayerMovement.hpp
//
//=========================================================================

#ifndef PLAYER_MOVEMENT_HPP
#define PLAYER_MOVEMENT_HPP

#include "x_math.hpp"
#include "PlayerInput.hpp"

//=========================================================================
//  DATA CONTRACTS
//=========================================================================

enum class PlayerTeleportVelocityPolicy
{
    Keep,
    Clear
};

//-------------------------------------------------------------------------

struct PlayerMovementSettings
{
    f32 MaxSpeed;
    f32 Acceleration;
    f32 DecelerationFactor;
};

//-------------------------------------------------------------------------

struct PlayerMovementSpeeds
{
                    PlayerMovementSpeeds          ( void );

    f32             Forward;
    f32             Strafe;
};

//=========================================================================
//  PLAYER MOVEMENT
//=========================================================================

class PlayerMovement
{
public:

                    PlayerMovement                ( void );

    void            Clear                         ( void );
    void            BuildInput                    ( PlayerMoveInput const& Source,
                                                     PlayerMoveInput& Input ) const;
    void            Evaluate                      ( PlayerMoveInput const& Input,
                                                     PlayerMovementSettings const& Settings,
                                                     f32 DeltaTime,
                                                     PlayerMovementSpeeds& Speeds );

private:

    static void     BuildKeyboardSpeeds           ( PlayerMoveInput const& Input,
                                                     f32 MaxSpeed,
                                                     PlayerMovementSpeeds& Speeds );
            void    AdvanceGamepadSpeed           ( PlayerMoveInput const& Input,
                                                     PlayerMovementSettings const& Settings,
                                                     f32 DeltaTime,
                                                     PlayerMovementSpeeds& Speeds );
    static void     CombineDeviceSpeeds           ( PlayerMovementSpeeds const& KeyboardSpeeds,
                                                     PlayerMovementSpeeds const& GamepadSpeeds,
                                                     f32 MaxSpeed,
                                                     PlayerMovementSpeeds& Speeds );
    static void     ShapeGamepadMoveVector        ( vector2& Move );
    static void     ApproachGamepadVelocity       ( vector2 const& TargetVelocity,
                                                     f32 Acceleration,
                                                     f32 DecelerationFactor,
                                                     f32 DeltaTime,
                                                     vector2& Velocity );

private:

    vector2         m_GamepadVelocity;
};

//=========================================================================
#endif // PLAYER_MOVEMENT_HPP
