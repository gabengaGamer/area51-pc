//==============================================================================
//
//  ConstraintPoint.cpp
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "PhysicsMgr.hpp"
#include "Constraint.hpp"
#include "Entropy.hpp"


//==============================================================================
// CONSTRAINT FUNCTIONS
//==============================================================================

#ifdef ENABLE_PHYSICS_DEBUG

constraint::constraint() :
    m_BodyPos0( 0.0f, 0.0f, 0.0f ),
    m_BodyPos1( 0.0f, 0.0f, 0.0f ),
    m_MaxDist ( 0.0f ),
    m_pBody0( NULL ),
    m_pBody1( NULL )
{
    m_DebugColor = XCOLOR_RED;    // Color of constraint for debugging
    PHYSICS_DEBUG_DYNAMIC_MEM_ALLOC( sizeof( constraint ) );
}

//==============================================================================

constraint::~constraint()
{
    PHYSICS_DEBUG_DYNAMIC_MEM_FREE( sizeof( constraint ) );
}

#endif  //#ifndef ENABLE_PHYSICS_DEBUG


// Initialization with world position
void constraint::Init( rigid_body*      pBody0,
                       rigid_body*      pBody1,
                       const vector3&   WorldPos,
                       f32              MaxDist,
                       u32              Flags,
                       xcolor           DebugColor )
{
    ASSERT( pBody0 );
    ASSERT( pBody1 );

    m_pBody0     = pBody0;
    m_pBody1     = pBody1;
    m_BodyPos0   = pBody0->GetW2L() * WorldPos;
    m_BodyPos1   = pBody1->GetW2L() * WorldPos;
    m_MaxDist    = MaxDist;
    m_Flags      = Flags;

#ifdef ENABLE_PHYSICS_DEBUG
    m_DebugColor = DebugColor;
#else
    (void)DebugColor;
#endif
}

//==============================================================================

// Initialization with local position for each body
void constraint::Init( rigid_body*      pBody0,
                       rigid_body*      pBody1,
                       const vector3&   Body0Pos,
                       const vector3&   Body1Pos,
                       f32              MaxDist,
                       u32              Flags,
                       xcolor           DebugColor )
{
    ASSERT( pBody0 );
    ASSERT( pBody1 );

    m_pBody0     = pBody0;
    m_pBody1     = pBody1;
    m_BodyPos0   = Body0Pos;
    m_BodyPos1   = Body1Pos;
    m_MaxDist    = MaxDist;
    m_Flags      = Flags;

#ifdef ENABLE_PHYSICS_DEBUG
    m_DebugColor = DebugColor;
#else
    (void)DebugColor;
#endif
}

//==============================================================================

xbool constraint::PreApply( f32 DeltaTime, active_constraint& Active )
{
    // TO DO: Use prediction for max constraint?
    
    // Lookup bodies
    rigid_body* pBody0 = m_pBody0 ;
    rigid_body* pBody1 = m_pBody1 ;
    ASSERT( pBody0 );
    ASSERT( pBody1 );

    // Compute world space positions
    vector3 WorldPos0 = pBody0->GetL2W() * m_BodyPos0 ;
    vector3 WorldPos1 = pBody1->GetL2W() * m_BodyPos1 ;

    // Compute world delta
    vector3 Delta   = WorldPos0 - WorldPos1 ;
    f32     DistSqr = Delta.LengthSquared();

    // Constraint already satisfied?
    // NOTE: Always keep pin (zero dist) constraints active so that limbs don't jerk
    //       (fixes the punchbag dummy from jittering)
    f32 MaxDist    = m_MaxDist;
    f32 MaxDistSqr = x_sqr( MaxDist );
    if( ( m_MaxDist > 0.0f ) && ( DistSqr <= MaxDistSqr ) )
    {
        return FALSE;
    }

    // Compute mid pos and relative mid positions
    vector3 WorldMidPos = ( WorldPos0 + WorldPos1 ) * 0.5f;
    Active.m_RelMidPos0 = WorldMidPos - pBody0->GetPosition();
    Active.m_RelMidPos1 = WorldMidPos - pBody1->GetPosition();

    // Compute relative positions
    Active.m_RelPos0 = WorldPos0 - pBody0->GetPosition();
    Active.m_RelPos1 = WorldPos1 - pBody1->GetPosition();

    // Compute deviation distance between points?
    if( DistSqr > 0.0001f )
    {
        // Normalize direction between constraints
        f32 Dist = x_sqrt( DistSqr );
        Delta *= 1.0f / Dist;
        
        // Compute deviation dist from constraint limit
        if( Dist > MaxDist )
            Dist -= MaxDist;

        const f32 CorrectionSpeed = x_min( Dist / DeltaTime,
                                           g_PhysicsMgr.m_Settings.m_MaxConstraintCorrectionSpeed );
        Active.m_CorrectionVel = Delta * CorrectionSpeed;
    }
    else
    {
        Active.m_CorrectionVel.Zero();
    }
    
    // Needs solving
    return TRUE;
}

//==============================================================================

xbool constraint::Apply( active_constraint& Active )
{
    // Lookup bodies
    rigid_body* pBody0 = m_pBody0 ;
    rigid_body* pBody1 = m_pBody1 ;
    ASSERT( pBody0 );
    ASSERT( pBody1 );

    // Compute velocities of each point
    vector3 Vel0 = pBody0->GetLinearVelocity() + v3_Cross( pBody0->GetAngularVelocity(), Active.m_RelPos0   );
    vector3 Vel1 = pBody1->GetLinearVelocity() + v3_Cross( pBody1->GetAngularVelocity(), Active.m_RelPos1   );

    // Compute relative velocity
    vector3 RelVel = Vel0 - Vel1 + Active.m_CorrectionVel;

    // Compute relative speed
    f32 RelSpeedSqr = RelVel.LengthSquared();
    if( RelSpeedSqr < 0.00001f )
        return FALSE;
    f32 RelSpeed = x_sqrt( RelSpeedSqr );

    // Compute impulse to satisfy constraint
    vector3 N = RelVel / RelSpeed;
    f32 Numerator   = -RelSpeed;
    f32 Denominator = 0.0f;

    // Treat inactive bodies as immovable
    if( pBody0->IsActive() )
    {
        Denominator += pBody0->GetInvMass();
        Denominator += v3_Dot( N,
                              v3_Cross( pBody0->GetWorldInvInertia().RotateVector(
                                            v3_Cross( Active.m_RelPos0, N ) ),
                                        Active.m_RelPos0 ) );
    }
    if( pBody1->IsActive() )
    {
        Denominator += pBody1->GetInvMass();
        Denominator += v3_Dot( N,
                              v3_Cross( pBody1->GetWorldInvInertia().RotateVector(
                                            v3_Cross( Active.m_RelPos1, N ) ),
                                        Active.m_RelPos1 ) );
    }

    // Valid?    
    if ( Denominator < 0.00001f )
        return FALSE;

    // Apply impulse to active bodies only so other body is not woken up!
    vector3 NormalImpulse = Active.m_Weight * N * ( Numerator / Denominator );

    if( pBody0->IsActive() )
        pBody0->ApplyLocalImpulse(  NormalImpulse, Active.m_RelMidPos0   );

    if( pBody1->IsActive() )        
        pBody1->ApplyLocalImpulse( -NormalImpulse, Active.m_RelMidPos1   );

    return TRUE;
}

//==============================================================================

#ifdef ENABLE_PHYSICS_DEBUG

// Render functions
void constraint::DebugRender ( void )
{
    // Lookup rigid bodies
    const rigid_body* pBody0 = GetRigidBody( 0 );
    const rigid_body* pBody1 = GetRigidBody( 1 );

    // Draw info
    if( pBody0 )
    {
        render::debug::Sphere( pBody0->GetL2W() * GetBodyPos( 0 ), 2.0f, m_DebugColor );
    }

    if( pBody1 )
    {        
        render::debug::Sphere( pBody1->GetL2W() * GetBodyPos( 1 ), 2.0f, m_DebugColor );
    }

    // Draw line between 
    if( pBody0 && pBody1 )
    {        
        render::debug::Line( pBody0->GetL2W() * GetBodyPos( 0 ),
                   pBody1->GetL2W() * GetBodyPos( 1 ), 
                   m_DebugColor );
    }            
}

#endif    

//==============================================================================
