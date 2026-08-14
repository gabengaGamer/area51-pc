//=========================================================================
//
//  PlayerDeathCamera.hpp
//
//=========================================================================

#ifndef PLAYER_DEATH_CAMERA_HPP
#define PLAYER_DEATH_CAMERA_HPP

#include "x_math.hpp"

//=========================================================================
//  TYPES
//=========================================================================

struct PlayerDeathCameraPose
{
    vector3 Position;
    vector3 Target;
};

//=========================================================================
//  PLAYER DEATH CAMERA
//=========================================================================

class PlayerDeathCamera
{
public:

    static constexpr s32 DirectionCount = 16;

                    PlayerDeathCamera           ( void );

    static s32      SelectDirection             ( const f32* pClearDistances,
                                                   s32 DistanceCount );
    static radian   GetCandidateYaw             ( radian PreferredYaw,
                                                   s32 CandidateIndex );

    void            Start                       ( const vector3& OrbitPoint,
                                                   radian Pitch,
                                                   radian Yaw,
                                                   f32 StartDistance,
                                                   f32 EndDistance,
                                                   f32 ClearDistance );
    void            Stop                        ( void );
    void            SetOrbitPoint               ( const vector3& OrbitPoint );
    void            SetDesiredPitch             ( radian Pitch );
    void            Advance                     ( f32 DeltaTime,
                                                   f32 ClearDistance );

    xbool           IsActive                    ( void ) const;
    f32             GetDistance                 ( void ) const;
    PlayerDeathCameraPose GetPose               ( void ) const;

private:

    vector3         m_OrbitPoint;
    f32             m_Distance;
    f32             m_EndDistance;
    radian          m_Pitch;
    radian          m_DesiredPitch;
    radian          m_Yaw;
    xbool           m_IsActive;
};

//=========================================================================
#endif // PLAYER_DEATH_CAMERA_HPP
