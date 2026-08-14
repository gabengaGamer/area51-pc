//=========================================================================
//
//  PlayerDeathCamera.cpp
//
//=========================================================================

#include "PlayerDeathCamera.hpp"

//=========================================================================
//  CONSTANTS
//=========================================================================

static constexpr f32 DeathCameraMoveSpeed = 800.0f;
static constexpr radian DeathCameraPitchSpeed = R_70;

//=========================================================================

PlayerDeathCamera::PlayerDeathCamera( void ) :
    m_OrbitPoint  ( 0.0f, 0.0f, 0.0f ),
    m_Distance    ( 0.0f ),
    m_EndDistance ( 0.0f ),
    m_Pitch       ( 0.0f ),
    m_DesiredPitch( 0.0f ),
    m_Yaw         ( 0.0f ),
    m_IsActive    ( FALSE )
{
}

//=========================================================================

s32 PlayerDeathCamera::SelectDirection( const f32* pClearDistances,
                                        s32        DistanceCount )
{
    ASSERT( pClearDistances );

    s32 const Count = MIN( MAX( DistanceCount, 0 ), DirectionCount );
    s32 BestIndex = 0;
    f32 BestDistance = -1.0f;

    for( s32 i = 0; i < Count; i++ )
    {
        if( pClearDistances[i] > BestDistance )
        {
            BestDistance = pClearDistances[i];
            BestIndex = i;
        }
    }

    return BestIndex;
}

//=========================================================================

radian PlayerDeathCamera::GetCandidateYaw( radian PreferredYaw,
                                           s32    CandidateIndex )
{
    ASSERT( IN_RANGE( 0, CandidateIndex, DirectionCount - 1 ) );
    return x_ModAngle( PreferredYaw +
                       ((R_360 / static_cast<f32>( DirectionCount )) * CandidateIndex) );
}

//=========================================================================

void PlayerDeathCamera::Start( const vector3& OrbitPoint,
                               radian         Pitch,
                               radian         Yaw,
                               f32            StartDistance,
                               f32            EndDistance,
                               f32            ClearDistance )
{
    m_OrbitPoint   = OrbitPoint;
    m_Pitch        = Pitch;
    m_DesiredPitch = Pitch;
    m_Yaw          = Yaw;
    m_Distance     = MIN( MAX( 0.0f, StartDistance ), MAX( 0.0f, ClearDistance ) );
    m_EndDistance  = MAX( 0.0f, EndDistance );
    m_IsActive     = TRUE;
}

//=========================================================================

void PlayerDeathCamera::Stop( void )
{
    m_IsActive = FALSE;
}

//=========================================================================

void PlayerDeathCamera::SetOrbitPoint( const vector3& OrbitPoint )
{
    m_OrbitPoint = OrbitPoint;
}

//=========================================================================

void PlayerDeathCamera::SetDesiredPitch( radian Pitch )
{
    m_DesiredPitch = Pitch;
}

//=========================================================================

void PlayerDeathCamera::Advance( f32 DeltaTime, f32 ClearDistance )
{
    if( !m_IsActive )
    {
        return;
    }

    DeltaTime = MAX( 0.0f, DeltaTime );
    f32 const TargetDistance = MIN( m_EndDistance, MAX( 0.0f, ClearDistance ) );

    if( TargetDistance < m_Distance )
    {
        m_Distance = MAX( TargetDistance,
                          m_Distance - (DeathCameraMoveSpeed * DeltaTime) );
    }
    else
    {
        m_Distance = MIN( TargetDistance,
                          m_Distance + (DeathCameraMoveSpeed * DeltaTime) );
    }

    radian const PitchDifference = x_MinAngleDiff( m_DesiredPitch, m_Pitch );
    radian const MaxPitchMove = DeathCameraPitchSpeed * DeltaTime;
    m_Pitch += x_clamp( PitchDifference, -MaxPitchMove, MaxPitchMove );
}

//=========================================================================

xbool PlayerDeathCamera::IsActive( void ) const
{
    return m_IsActive;
}

//=========================================================================

f32 PlayerDeathCamera::GetDistance( void ) const
{
    return m_Distance;
}

//=========================================================================

PlayerDeathCameraPose PlayerDeathCamera::GetPose( void ) const
{
    PlayerDeathCameraPose Pose;
    Pose.Target = m_OrbitPoint;
    Pose.Position.Set( 0.0f, 0.0f, m_Distance );
    Pose.Position.RotateX( m_Pitch );
    Pose.Position.RotateY( m_Yaw );
    Pose.Position += m_OrbitPoint;
    return Pose;
}
