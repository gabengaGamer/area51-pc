//=========================================================================
//
//  PlayerPhysics.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "Configuration/GameConfig.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"

//=========================================================================
//  DEFINES
//=========================================================================

f32 HumanSpeedFactor  = 1.000f;
f32 MutantSpeedFactor = 1.000f;

tweak_handle MP_RunSpeedFactor_NormalTweak    ( "MP_RunSpeedFactor_Normal" );
tweak_handle MP_RunSpeedFactor_MutantTweak    ( "MP_RunSpeedFactor_Mutant" );

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

static
void ScaleVelocityComponent( const vector3& PlaneNormal, 
                             f32            PerpScale, 
                             f32            ParaScale, 
                             vector3&       Velocity, 
                             f32&           Speed )
{
    // Compute current speed squared, and exit if not moving
    f32 PrevSpeedSqr = Velocity.LengthSquared();
    if( PrevSpeedSqr < 0.0001f )
        return;

    // Compute components into and along collision plane
    vector3 PerpVel = PlaneNormal * v3_Dot( PlaneNormal, Velocity );
    vector3 ParaVel = Velocity - PerpVel;

    // Compute new velocity, taking scaling into account
    Velocity = ( PerpScale * PerpVel ) + ( ParaScale * ParaVel);

    // Scale speed in proportion with velocity change
    f32 CurrSpeedSqr = Velocity.LengthSquared();
    if( CurrSpeedSqr >= 0.0001f )
    {
        f32 PrevSpeed = x_sqrt( PrevSpeedSqr );
        f32 CurrSpeed = x_sqrt( CurrSpeedSqr );
        Speed *= CurrSpeed / PrevSpeed;
    }    
    else
    {
        Speed = 0.0f;    
    }        
}

//=========================================================================

void player::ScaleVelocity( const vector3& PlaneNormal, f32 PerpScale, f32 ParaScale )
{
    // Apply to forward and side components
    ScaleVelocityComponent( PlaneNormal, PerpScale, ParaScale, m_ForwardVelocity, m_fForwardSpeed );
    ScaleVelocityComponent( PlaneNormal, PerpScale, ParaScale, m_StrafeVelocity,  m_fStrafeSpeed );
}

//==============================================================================

void player::EvaluateMovementSpeeds( f32 DeltaTime )
{
    PlayerMovementSettings Settings;
    Settings.MaxSpeed           = m_MaxFowardVelocity;
    Settings.Acceleration       = m_fForwardAccel;
    Settings.DecelerationFactor = m_fDecelerationFactor;

    PlayerMovementSpeeds Speeds;
    m_Movement.Evaluate( m_MoveInput, Settings, DeltaTime, Speeds );
    m_fForwardSpeed = Speeds.Forward;
    m_fStrafeSpeed  = Speeds.Strafe;
}

//==============================================================================

void player::ApplyMovementSpeedFactors( void )
{
    f32 SpeedFactor = g_PerceptionMgr.GetForwardSpeedFactor();

    if( g_MPTweaks.Active )
    {
        if( IsMutated() )
        {
            SpeedFactor *= MP_RunSpeedFactor_MutantTweak.GetF32();
            SpeedFactor *= MutantSpeedFactor;
        }
        else
        {
            SpeedFactor *= MP_RunSpeedFactor_NormalTweak.GetF32();
            SpeedFactor *= HumanSpeedFactor;
        }
    }

    m_ForwardVelocity *= SpeedFactor;
    m_StrafeVelocity  *= SpeedFactor;
}

//==============================================================================

void player::CalculateForwardVelocity( const vector3& ViewZ )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "player::CalculateForwardVelocity");

    m_ForwardVelocity = ViewZ * m_fForwardSpeed;
}

//==============================================================================

void player::CalculateStrafeVelocity( const vector3& ViewX )
{
    m_StrafeVelocity = ViewX * m_fStrafeSpeed;
}

//==============================================================================

f32 player::GetSpeed( void )
{
    return m_fStrafeSpeed + m_fForwardSpeed;
}

//==============================================================================

f32 player::GetCurrentVelocity( void )
{
    //return highest of these two
    return MAX(x_abs(m_fStrafeSpeed), x_abs(m_fForwardSpeed));
}

//==============================================================================

f32 player::GetMaxVelocity( void )
{ 
    return m_MaxFowardVelocity;
}
