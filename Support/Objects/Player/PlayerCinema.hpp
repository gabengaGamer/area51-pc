//=========================================================================
//
//  PlayerCinema.hpp
//
//=========================================================================

#ifndef PLAYER_CINEMA_HPP
#define PLAYER_CINEMA_HPP

#include "x_math.hpp"

//=========================================================================
//  TYPES
//=========================================================================

enum class PlayerCinemaFinishReason
{
    Completed,
    Deactivated,
    Error
};

//-------------------------------------------------------------------------

struct PlayerCinemaSettings
{
                    PlayerCinemaSettings        ( void );

    guid            CameraGuid;
    guid            LookAtTargetGuid;
    f32             LookAtBlendTime;
    xbool           UseViewCorrection;
};

//=========================================================================
//  PLAYER CINEMA
//=========================================================================

class PlayerCinema
{
public:

    static xbool    BlendLookAt                 ( const vector3& CurrentDirection,
                                                   const vector3& EyePosition,
                                                   const vector3& TargetPosition,
                                                   f32 BlendT,
                                                   radian& Pitch,
                                                   radian& Yaw );

                    PlayerCinema                ( void );

    void            Begin                       ( const PlayerCinemaSettings& Settings );
    void            End                         ( PlayerCinemaFinishReason Reason );
    void            UpdateSettings              ( const PlayerCinemaSettings& Settings );
    void            Advance                     ( f32 DeltaTime );

    void            BindCinemaObject            ( guid CinemaGuid );
    void            UnbindCinemaObject          ( guid CinemaGuid );

    xbool           IsActive                    ( void ) const;
    f32             GetLookAtBlend              ( void ) const;
    guid            GetBoundCinemaGuid          ( void ) const;
    PlayerCinemaFinishReason GetFinishReason    ( void ) const;
    const PlayerCinemaSettings& GetSettings     ( void ) const;

private:

    PlayerCinemaSettings     m_Settings;
    PlayerCinemaFinishReason m_FinishReason;
    guid                     m_BoundCinemaGuid;
    f32                      m_LookAtElapsedTime;
    xbool                    m_IsActive;
};

//=========================================================================
#endif // PLAYER_CINEMA_HPP
