//=========================================================================
//
//  PlayerCinemaPolicy.cpp
//
//=========================================================================

#include "PlayerCinema.hpp"

//=========================================================================

xbool PlayerCinema::BlendLookAt( const vector3& CurrentDirection,
                                  const vector3& EyePosition,
                                  const vector3& TargetPosition,
                                  f32            BlendT,
                                  radian&        Pitch,
                                  radian&        Yaw )
{
    vector3 DesiredDirection = TargetPosition - EyePosition;
    if( DesiredDirection.LengthSquared() <= F32_MIN )
    {
        return FALSE;
    }

    DesiredDirection.Normalize();

    vector3 BlendedDirection = CurrentDirection;
    if( BlendedDirection.LengthSquared() <= F32_MIN )
    {
        BlendedDirection.Set( 0.0f, 0.0f, 1.0f );
    }
    else
    {
        BlendedDirection.Normalize();
    }

    radian const Angle = v3_AngleBetween( BlendedDirection, DesiredDirection );
    vector3 Axis = BlendedDirection.Cross( DesiredDirection );
    if( Angle > R_179 )
    {
        Axis.Set( 0.0f, 1.0f, 0.0f );
    }

    if( Axis.LengthSquared() > F32_MIN )
    {
        Axis.Normalize();
        quaternion const Rotation( Axis, x_clamp( BlendT, 0.0f, 1.0f ) * Angle );
        BlendedDirection = Rotation * BlendedDirection;
    }

    BlendedDirection.GetPitchYaw( Pitch, Yaw );
    return TRUE;
}

//=========================================================================

PlayerCinemaSettings::PlayerCinemaSettings( void ) :
    CameraGuid        ( 0 ),
    LookAtTargetGuid  ( 0 ),
    LookAtBlendTime   ( 3.0f ),
    UseViewCorrection ( FALSE )
{
}

//=========================================================================

PlayerCinema::PlayerCinema( void ) :
    m_FinishReason      ( PlayerCinemaFinishReason::Completed ),
    m_BoundCinemaGuid   ( 0 ),
    m_LookAtElapsedTime ( 0.0f ),
    m_IsActive          ( FALSE )
{
}

//=========================================================================

void PlayerCinema::Begin( const PlayerCinemaSettings& Settings )
{
    m_Settings          = Settings;
    m_FinishReason      = PlayerCinemaFinishReason::Completed;
    m_LookAtElapsedTime = 0.0f;
    m_IsActive          = TRUE;
}

//=========================================================================

void PlayerCinema::End( PlayerCinemaFinishReason Reason )
{
    m_FinishReason      = Reason;
    m_LookAtElapsedTime = 0.0f;
    m_IsActive          = FALSE;
}

//=========================================================================

void PlayerCinema::UpdateSettings( const PlayerCinemaSettings& Settings )
{
    if( Settings.LookAtTargetGuid != m_Settings.LookAtTargetGuid )
    {
        m_LookAtElapsedTime = 0.0f;
    }

    m_Settings = Settings;
}

//=========================================================================

void PlayerCinema::Advance( f32 DeltaTime )
{
    if( !m_IsActive )
    {
        return;
    }

    m_LookAtElapsedTime += MAX( 0.0f, DeltaTime );
}

//=========================================================================

void PlayerCinema::BindCinemaObject( guid CinemaGuid )
{
    m_BoundCinemaGuid = CinemaGuid;
}

//=========================================================================

void PlayerCinema::UnbindCinemaObject( guid CinemaGuid )
{
    if( m_BoundCinemaGuid == CinemaGuid )
    {
        m_BoundCinemaGuid = 0;
    }
}

//=========================================================================

xbool PlayerCinema::IsActive( void ) const
{
    return m_IsActive;
}

//=========================================================================

f32 PlayerCinema::GetLookAtBlend( void ) const
{
    if( m_Settings.LookAtBlendTime <= F32_MIN )
    {
        return 1.0f;
    }

    return x_clamp( m_LookAtElapsedTime / m_Settings.LookAtBlendTime,
                    0.0f,
                    1.0f );
}

//=========================================================================

guid PlayerCinema::GetBoundCinemaGuid( void ) const
{
    return m_BoundCinemaGuid;
}

//=========================================================================

PlayerCinemaFinishReason PlayerCinema::GetFinishReason( void ) const
{
    return m_FinishReason;
}

//=========================================================================

const PlayerCinemaSettings& PlayerCinema::GetSettings( void ) const
{
    return m_Settings;
}
