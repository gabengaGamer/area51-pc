//==============================================================================
//
//  CarObject.cpp
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================

#include "CarObject.hpp"
#include "Entropy.hpp"
#include "Obj_mgr\obj_mgr.hpp"
#include "CollisionMgr\CollisionMgr.hpp"
#include "GameLib\RigidGeomCollision.hpp"
#include "CollisionMgr\PolyCache.hpp"
#include "Debris\debris_mgr.hpp"
#include "Render\Render.hpp"
#include "EventMgr\EventMgr.hpp"
#include "AudioMgr\AudioMgr.hpp"

#include "render\LightMgr.hpp"
#include "Objects\ProjectileBullett.hpp"
#include "TemplateMgr\TemplateMgr.hpp"
#include "Objects\ProjectileAlienTurret.hpp"
#include "Objects\ProjectileEnergy.hpp"

//==============================================================================
// STATIC DATA
//==============================================================================

//TODO: Push this values to Editor.
static const f32 k_Gravity                      = -2000.0f;
static const f32 k_ShockLength                  = 35.0f;
static const f32 k_ShockSpeedStartT             = 0.3f;
static const f32 k_ShockSpeedFullyCompressed    = 2500.0f;
static const f32 k_ShockStiffness               = 0.2f;
static const f32 k_WheelSideFriction            = 5.0f;
static const f32 k_WheelRadius                  = 35.0f;
static const f32 k_TopSpeedForward              = 50.0f;
static const f32 k_TopSpeedReverse              = 20.0f;
static const f32 k_ZeroToTopSpeedTime           = 3.0f;
static const f32 k_TopSpeedToZeroBrakingTime    = 1.0f;
static const f32 k_TopSpeedToZeroCoastingTime   = 2.0f;
static const f32 k_MaxSteeringAngle             = 0.8f;
static const f32 k_WheelForwardTractionSpeedDiff = 500.0f;
static const f32 k_WheelForwardTractionMin      = 0.25f;
static const f32 k_WheelForwardTractionMax      = 0.5f;
static const f32 k_WheelSideTractionSpeedDiff   = 250.0f;
static const f32 k_WheelSideTractionMin         = 0.01f;
static const f32 k_WheelSideTractionMax         = 1.0f;
static const f32 k_WheelVerticalLimit           = 0.5f;
static const f32 k_WheelTurnRate                = R_360;

//==============================================================================
// STATIC FUNCTIONS
//==============================================================================

static void SolveSphereCollisions( guid IgnoreGuid, 
                                  s32 nSpheres, 
                                  vector3* pSphereOffset, 
                                  vector3* pSpherePos, 
                                  f32* pSphereRadius )
{
    s32 i, j;

    // Build full bbox and gather clusters
    {
        bbox FullBBox;
        FullBBox.Clear();
        for( i = 0; i < nSpheres; i++ )
        {
            FullBBox += bbox( pSpherePos[i], pSphereRadius[i] );
        }
        FullBBox.Inflate(100, 100, 100);

        // Gather factored out list of clusters in dynamic area
        g_PolyCache.BuildClusterList( FullBBox, object::TYPE_ALL_TYPES, object::ATTR_COLLIDABLE, object::ATTR_COLLISION_PERMEABLE );
    }

    s32 nIterations = 4;
    while( nIterations-- )
    {
        // Run constraints
        {
            s32 MaxLoops = 4;
            f32 CorrectionFactor = 1.0f / (f32)MaxLoops;
            s32 nLoops = MaxLoops;
            while( nLoops-- )
            {
                xbool bCorrected = FALSE;

                for( i = 0; i < nSpheres; i++ )
                {
                    vector3 SphereAPos = pSpherePos[i];
                    vector3 SphereAOffset = pSphereOffset[i];

                    for( j = 1; j < nSpheres; j++ )
                    {
                        s32 J = (i + j) % nSpheres;
                        vector3 SphereBPos = pSpherePos[J];
                        vector3 SphereBOffset = pSphereOffset[J];

                        vector3 OffsetDelta = SphereBOffset - SphereAOffset;
                        f32 ConstLength = OffsetDelta.Length();
                        vector3 Delta = SphereBPos - SphereAPos;
                        f32 Length = Delta.Length();
                        f32 LenDiff = ConstLength - Length;

                        if( x_abs(LenDiff) > 1.0f )
                        {
                            vector3 CorrDelta = (Delta / Length) * LenDiff;
                            pSpherePos[i] -= CorrDelta * CorrectionFactor;
                            pSpherePos[J] += CorrDelta * CorrectionFactor;
                            bCorrected = TRUE;
                        }
                    }
                }

                if( !bCorrected )
                    break;
            }
        }

        // Push each sphere away from any penetration
        {
            for( i = 0; i < nSpheres; i++ )
            {
                s32 MaxLoops = 2;
                s32 nLoops;

                for( nLoops = 0; nLoops < MaxLoops; nLoops++ )
                {
                    s32 nPenetrations = 0;
                    vector3 AccumMoveDelta;
                    AccumMoveDelta.Zero();

                    // Build sphere info
                    f32 SphereRadius = pSphereRadius[i];
                    vector3 SphereCenter = pSpherePos[i];
                    bbox SphereBBox = bbox( SphereCenter, SphereRadius );

                    // Process clusters
                    if( g_PolyCache.m_nClusters )
                    {
                        // Loop through the clusters and process the triangles
                        for( s32 iCL = 0; iCL < g_PolyCache.m_nClusters; iCL++ )
                        {
                            poly_cache::cluster& CL = *g_PolyCache.m_ClusterList[iCL];

                            if( CL.Guid == IgnoreGuid )
                                continue;

                            if( !CL.BBox.Intersect(SphereBBox) )
                                continue;

                            // TODO: Add check collision with cluster triangles
                        }
                    }
                    // No collisions found so bail
                    if( nPenetrations == 0 )
                        break;
                }
            }
        }
    }
}

//=========================================================================
// OBJECT DESCRIPTION
//=========================================================================

static struct car_object_desc : public object_desc
{
    car_object_desc( void ) : object_desc( 
        object::TYPE_VEHICLE, 
        "Car",
        "VEHICLE", 
        object::ATTR_NEEDS_LOGIC_TIME       |
        object::ATTR_COLLIDABLE             | 
        object::ATTR_BLOCKS_ALL_PROJECTILES |
        object::ATTR_BLOCKS_ALL_ACTORS      |
        object::ATTR_BLOCKS_RAGDOLL         |
        object::ATTR_BLOCKS_CHARACTER_LOS   |
        object::ATTR_BLOCKS_PLAYER_LOS      |
        object::ATTR_BLOCKS_PAIN_LOS        |
        object::ATTR_BLOCKS_SMALL_DEBRIS    |
        object::ATTR_RENDERABLE             | 
        object::ATTR_SPACIAL_ENTRY          |
        object::ATTR_DAMAGEABLE             |
        object::ATTR_LIVING,
        FLAGS_GENERIC_EDITOR_CREATE         | 
        FLAGS_IS_DYNAMIC                    |
        FLAGS_NO_ICON                       |
        FLAGS_BURN_VERTEX_LIGHTING  ) {}

    //-------------------------------------------------------------------------
    
    virtual object* Create( void ) { return new car_object; }

    //-------------------------------------------------------------------------
    
    virtual const char* QuickResourceName( void ) const
    {
        return "RigidGeom";
    }
    
    //-------------------------------------------------------------------------
    
    virtual const char* QuickResourcePropertyName( void ) const 
    {
        return "RenderInst\\File";
    }

    //-------------------------------------------------------------------------

#ifdef X_EDITOR

    virtual s32 OnEditorRender( object& Object ) const
    {
        object_desc::OnEditorRender( Object );
        return -1;
    }

#endif // X_EDITOR

} s_car_object_Desc;

//=============================================================================

const object_desc& car_object::GetTypeDesc( void ) const
{
    return s_car_object_Desc;
}

//=============================================================================

const object_desc& car_object::GetObjectType( void )
{
    return s_car_object_Desc;
}

//==============================================================================
// WHEEL CLASS IMPLEMENTATION
//==============================================================================

wheel::wheel()
{
    m_iBone                 = -1;
    m_Offset.Zero();
    m_Steer                 = 0;
    m_Spin                  = 0;
    m_SpinSpeed             = 0;
    m_Velocity.Zero();
    m_Position.Zero();
    m_OldPosition.Zero();
    m_ObjectGuid            = 0;
    m_LastShockLength       = 0;
    m_CurrentShockLength    = 0;
    m_bPassive              = FALSE;
    m_bHasTraction          = FALSE;
    m_DebrisTimer           = 0;
    m_nLogicLoops           = 0;
}

//==============================================================================

void wheel::Init( s32 iBone, 
                  const matrix4& VehicleL2W, 
                  const vector3& ShockOffset, 
                  guid ObjectGuid,
                  xbool bPassive)
{
    m_iBone                 = iBone;
    m_Offset                = ShockOffset;
    m_Steer                 = 0;
    m_Spin                  = 0;
    m_SpinSpeed             = 0;
    m_ObjectGuid            = ObjectGuid;
    m_bHasTraction          = FALSE;
    m_Position              = VehicleL2W * m_Offset;
    m_OldPosition           = m_Position;
    m_CurrentShockLength    = 0;
    m_LastShockLength       = 0;
    m_Velocity.Zero();
    m_bPassive              = bPassive;
    m_DebrisTimer           = 0;
    m_nLogicLoops           = 0;
    
    x_memset(m_History, 0, sizeof(m_History));
}

//==============================================================================

void wheel::Reset( const matrix4& VehicleL2W )
{
    m_Steer                 = 0;
    m_Spin                  = 0;
    m_SpinSpeed             = 0;
    m_Position              = VehicleL2W * m_Offset;
    m_OldPosition           = m_Position;
    m_Velocity.Zero();
    m_CurrentShockLength    = 0;
    m_LastShockLength       = 0;
    m_bHasTraction          = FALSE;
    m_DebrisTimer           = 0;
    m_nLogicLoops           = 0;
    
    x_memset(m_History, 0, sizeof(m_History));
}

//==============================================================================

void wheel::ApplyTraction( f32 DeltaTime, const matrix4& VehicleL2W )
{
    // Back up shock length
    m_LastShockLength = m_CurrentShockLength;

    // Remember if we had traction last frame
    xbool bHadTraction = m_bHasTraction;
    m_bHasTraction = FALSE;

    // Get direction vectors for wheel
    vector3 WheelLeftDir;
    vector3 WheelUpDir(0,1,0);
    vector3 WheelForwardDir(0,0,1);
    WheelForwardDir.RotateY( m_Steer );
    WheelForwardDir = (VehicleL2W * WheelForwardDir) - VehicleL2W.GetTranslation();
    WheelUpDir      = (VehicleL2W * WheelUpDir) - VehicleL2W.GetTranslation();
    WheelLeftDir    = WheelUpDir.Cross(WheelForwardDir);

    // If we are upside down then no traction
    if( WheelUpDir.GetY() <= 0 )
    {
        m_Velocity += vector3(0, k_Gravity * 1.5f, 0) * DeltaTime;
        return;
    }

    // Compute sphere check to find ground
    f32     GroundCheckDistAbove = 100.0f;
    f32     GroundCheckDistBelow = 400.0f;
    vector3 S = m_Position + WheelUpDir * vector3(0, GroundCheckDistAbove, 0);
    vector3 E = m_Position + WheelUpDir * vector3(0, -GroundCheckDistBelow, 0);

    // Collect ground information
    plane   GroundPlane;
    plane   GroundSlipPlane;
    vector3 GroundPoint;
    f32     GroundT;
    f32     MaxTractionGroundT = (GroundCheckDistAbove + k_ShockLength) / (GroundCheckDistAbove + GroundCheckDistBelow);
    f32     MinTractionGroundT = GroundCheckDistAbove / (GroundCheckDistAbove + GroundCheckDistBelow);

    // Prepare for collision
    g_CollisionMgr.SphereSetup( m_ObjectGuid, S, E, k_WheelRadius );
    g_CollisionMgr.UseLowPoly();

    // Perform collision and determine if we have traction
    f32 CollT = 500.0f;
    if( g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, object::ATTR_COLLIDABLE, object::ATTR_COLLISION_PERMEABLE ) )
    {
        if( g_CollisionMgr.m_Collisions[0].Plane.Normal.GetY() >= k_WheelVerticalLimit )
            CollT = g_CollisionMgr.m_Collisions[0].T;
    }

    if( CollT <= MaxTractionGroundT )
    {
        collision_mgr::collision& Coll = g_CollisionMgr.m_Collisions[0];
        GroundPlane = Coll.Plane;
        GroundPoint = Coll.Point;
        GroundSlipPlane = Coll.SlipPlane;
        GroundT     = Coll.T;

        if( GroundT < MinTractionGroundT )
        {
            // We are penetrating into the ground
            m_bHasTraction = TRUE;
            vector3 SphereCenterAtImpact = S + GroundT * (E - S);
            m_Position = SphereCenterAtImpact;
            m_CurrentShockLength = 0;
        }
        else
        {
            // We are within the shock's reach
            m_bHasTraction = TRUE;
            m_CurrentShockLength = k_ShockLength * (GroundT - MinTractionGroundT) / (MaxTractionGroundT - MinTractionGroundT);
        }
    }
    else
    {
        m_bHasTraction = FALSE;
    }

    // Apply gravity
    m_Velocity += vector3(0, k_Gravity, 0) * DeltaTime;

    // Apply friction and acceleration if we have traction
    if( m_bHasTraction )
    {
        // Get tire forward direction parallel to ground plane
        vector3 WheelForwardParallelDir;
        vector3 WheelSideParallelDir;
        {
            vector3 WheelPar, WheelPerp;

            GroundSlipPlane.GetComponents( WheelForwardDir, WheelPar, WheelPerp );
            WheelForwardParallelDir = WheelPar;
            WheelForwardParallelDir.Normalize();

            GroundSlipPlane.GetComponents( WheelLeftDir, WheelPar, WheelPerp );
            WheelSideParallelDir = WheelPar;
            WheelSideParallelDir.Normalize();
        }

        // Compute tire surface speed and ground speed
        f32 TireSurfaceSpeed = m_SpinSpeed * k_WheelRadius;
        f32 GroundSpeed = WheelForwardParallelDir.Dot(m_Velocity);

        // Determine traction
        f32 ForwardTractionT = x_abs(TireSurfaceSpeed - GroundSpeed) / k_WheelForwardTractionSpeedDiff;
        ForwardTractionT = MINMAX( 0.0f, ForwardTractionT, 1.0f );
        ForwardTractionT = k_WheelForwardTractionMax + ForwardTractionT * (k_WheelForwardTractionMin - k_WheelForwardTractionMax);

        f32 SideTractionT = x_abs(TireSurfaceSpeed - GroundSpeed) / k_WheelSideTractionSpeedDiff;
        SideTractionT = MINMAX( 0.0f, SideTractionT, 1.0f );
        SideTractionT = k_WheelSideTractionMax + SideTractionT * (k_WheelSideTractionMin - k_WheelSideTractionMax);

        // Vertical traction based on slope
        f32 VerticalTractionT = (GroundSlipPlane.Normal.GetY() - k_WheelVerticalLimit) / (1.0f - k_WheelVerticalLimit);
        VerticalTractionT = MINMAX( 0.0f, VerticalTractionT, 1.0f );
        VerticalTractionT = x_sqrt(VerticalTractionT);

        // Dampen side velocity
        f32 SideSpeed = WheelSideParallelDir.Dot(m_Velocity);
        f32 SpeedChange = SideSpeed * k_WheelSideFriction * SideTractionT;

        if( x_abs(SpeedChange) > x_abs(SideSpeed) )
            SpeedChange = SideSpeed;

        m_Velocity -= WheelSideParallelDir * SpeedChange;

        // Adjust velocity along surface of ground
        if( !m_bPassive )
        {
            vector3 WheelPar, WheelPerp;
            GroundSlipPlane.GetComponents( WheelForwardDir, WheelPar, WheelPerp );
            WheelPar.Normalize();

            m_Velocity += WheelPar * (TireSurfaceSpeed - GroundSpeed) * VerticalTractionT * ForwardTractionT;
        }
    }

    // Adjust shock
    if( m_bHasTraction )
    {
        // Compute Shock Compression parametric
        f32 SCT = 1 - (m_CurrentShockLength / k_ShockLength);
        ASSERT( SCT >= 0.0f );
        ASSERT( SCT <= 1.0f );

        // Dampen any velocity into the shock
        f32 SpeedInDirectionOfTheShock = WheelUpDir.Dot( m_Velocity );
        if( SpeedInDirectionOfTheShock < 0 )
        {
            ASSERT((k_ShockStiffness >= 0) && (k_ShockStiffness <= 1));
            f32 DampenT = k_ShockStiffness + SCT * (1.0f - k_ShockStiffness);
            DampenT = x_sqrt(DampenT);
            m_Velocity += -WheelUpDir * SpeedInDirectionOfTheShock * DampenT;
        }

        // Compute Current shock speed
        f32 CurrentShockSpeed = WheelUpDir.Dot( m_Velocity );

        ASSERT( k_ShockSpeedStartT < 1.0f );
        f32 SpeedT = (SCT - k_ShockSpeedStartT) / (1.0f - k_ShockSpeedStartT);
        SpeedT = MINMAX( 0.0f, SpeedT, 1.0f );
        
        f32 DesiredShockSpeed = SpeedT * k_ShockSpeedFullyCompressed;
        f32 ShockAccel = DesiredShockSpeed - CurrentShockSpeed;
        m_Velocity += WheelUpDir * ShockAccel * DeltaTime;
    }
    else
    {
        // Since we don't have traction, extend shock completely
        m_CurrentShockLength = k_ShockLength;
        m_LastShockLength = m_CurrentShockLength;
    }

    // Check if we just landed
    if( (bHadTraction == FALSE) && (m_bHasTraction == TRUE) )
    {
        m_DebrisTimer += 0.5f;
    }
}

//==============================================================================

void wheel::EnforceCollision( void )
{
    vector3 CurrentPos = m_OldPosition;
    vector3 DesiredPos = m_Position;
    vector3 Delta = DesiredPos - CurrentPos;
    plane LastPlane;

    s32 nLoops = 3;
    while( nLoops-- )
    {
        f32 DeltaLen = Delta.Length();
        if( DeltaLen < 0.01f )
            break;

        g_CollisionMgr.SphereSetup( m_ObjectGuid, CurrentPos, CurrentPos + Delta, k_WheelRadius + 2.0f );
        g_CollisionMgr.UseLowPoly();
        g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, object::ATTR_COLLIDABLE, object::ATTR_COLLISION_PERMEABLE );
        
        if( g_CollisionMgr.m_nCollisions == 0 )
            break;

        collision_mgr::collision& Coll = g_CollisionMgr.m_Collisions[0];

        // Pull back from collision
        f32 CollT = Coll.T - (1.0f / DeltaLen);
        CollT = MINMAX( 0.0f, CollT, 1.0f );

        // Update position with part of Delta
        CurrentPos += Delta * CollT;

        // Compute the slide delta
        f32 RemainingT = 1 - CollT;
        ASSERT( RemainingT >= 0 );
        ASSERT( RemainingT <= 1 );
        Delta *= RemainingT;
        
        vector3 SlideDelta;
        vector3 Perp;
        vector3 OldDelta = Delta;

        // Is this our 2nd collision on this move between acute planes?
        if( (nLoops == 1) && (LastPlane.Normal.Dot( Coll.SlipPlane.Normal ) <= 0) )
        {
            // Move parallel to the crease between the planes
            SlideDelta = LastPlane.Normal.Cross( Coll.SlipPlane.Normal );
            SlideDelta.Normalize();
            const f32 Dist = Delta.Dot( SlideDelta );
            SlideDelta *= Dist;

            if( SlideDelta.LengthSquared() > Delta.LengthSquared() + 0.01f )
            {
                SlideDelta = Delta;
            }

            if( SlideDelta.LengthSquared() > OldDelta.LengthSquared() + 0.01f )
            {
                SlideDelta = OldDelta;
            }
        }
        else
        {
            LastPlane = Coll.SlipPlane;
            Coll.SlipPlane.GetComponents( Delta, SlideDelta, Perp );
        }

        // Use the slide delta
        Delta = SlideDelta;
    }

    // Move whatever delta is left after the collisions
    if( Delta.LengthSquared() > 0.001f )
        CurrentPos += Delta;

    m_Position = CurrentPos;

    // Check if we fell through
    {
        vector3 RS = m_Position + vector3(0, 300.0f, 0);
        vector3 RE = RS - vector3(0, 300.0f, 0);
        g_CollisionMgr.RaySetup( m_ObjectGuid, RS, RE );
        g_CollisionMgr.UseLowPoly();
        
        if( g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, object::ATTR_COLLIDABLE, object::ATTR_COLLISION_PERMEABLE ) )
        {
            collision_mgr::collision& Coll = g_CollisionMgr.m_Collisions[0];

            if( Coll.Point.GetY() > (m_Position.GetY() - k_WheelRadius) ) 
            {
                m_Position.GetY() = Coll.Point.GetY() + k_WheelRadius + 1.0f;
            }
        }
    }
}

//==============================================================================

void wheel::ApplyInput( f32 DeltaTime, f32 Accel, f32 Brake )
{
    // Compute numbers from physics
    f32 CircumferenceOfTire = (k_WheelRadius * 2) * 3.14159f;
    f32 MPHToCMPSec = (12.0f * 2.54f * 5280.0f) / 3600.0f;
    f32 TopForwardRotPerSecond = +(k_TopSpeedForward * MPHToCMPSec) / CircumferenceOfTire;
    f32 TopReverseRotPerSecond = -(k_TopSpeedReverse * MPHToCMPSec) / CircumferenceOfTire;
    f32 Acceleration = TopForwardRotPerSecond / k_ZeroToTopSpeedTime;
    f32 BrakingRate = TopForwardRotPerSecond / k_TopSpeedToZeroBrakingTime;
    f32 CoastingRate = TopForwardRotPerSecond / k_TopSpeedToZeroCoastingTime;

    if( !m_bPassive )
    {
        // Apply acceleration
        if( Accel != 0 )
        {
            m_SpinSpeed += Accel * R_360 * Acceleration * DeltaTime;

            // Limit forward speed
            if( m_SpinSpeed > (R_360 * TopForwardRotPerSecond) )
                m_SpinSpeed = R_360 * TopForwardRotPerSecond;

            // Limit reverse speed
            if( m_SpinSpeed < (R_360 * TopReverseRotPerSecond) )
                m_SpinSpeed = R_360 * TopReverseRotPerSecond;
        }

        // Auto-brake when reversing direction
        if( ((Accel < 0) && (m_SpinSpeed > 0)) ||
            ((Accel > 0) && (m_SpinSpeed < 0)) )
        {
            Brake = 1;
        }
    }

    // Apply braking
    if( Brake != 0 )
    {
        if( m_SpinSpeed > 0 )
        {
            m_SpinSpeed -= Brake * R_360 * BrakingRate * DeltaTime;
            if( m_SpinSpeed < R_1 )
                m_SpinSpeed = 0;
        }
        else
        {
            m_SpinSpeed += Brake * R_360 * BrakingRate * DeltaTime;
            if( m_SpinSpeed > -R_1 )
                m_SpinSpeed = 0;
        }
    }

    // Apply natural friction
    if( (Accel == 0) && (Brake == 0) )
    {
        if( m_SpinSpeed > 0 )
        {
            m_SpinSpeed -= R_360 * CoastingRate * DeltaTime;
            if( m_SpinSpeed < R_1 )
                m_SpinSpeed = 0;
        }
        else if( m_SpinSpeed < 0 )
        {
            m_SpinSpeed += R_360 * CoastingRate * DeltaTime;
            if( m_SpinSpeed > -R_1 )
                m_SpinSpeed = 0;
        }
    }

    // Update current tire rotation
    m_Spin += m_SpinSpeed * DeltaTime;
}

//==============================================================================

void wheel::Advance( f32 DeltaTime, const matrix4& VehicleL2W, f32 Accel, f32 Brake )
{
    ApplyInput( DeltaTime, Accel, Brake );
    ApplyTraction( DeltaTime, VehicleL2W );

    // Step position
    m_Position += m_Velocity * DeltaTime;

    m_nLogicLoops++;

    // Store history
    history& H = m_History[m_nLogicLoops % WHEEL_HISTORY_LENGTH];
    H.ShockLength = m_CurrentShockLength;
    H.ShockSpeed = (m_CurrentShockLength - m_LastShockLength) / DeltaTime;
    H.bHasTraction = m_bHasTraction;

    // Update debris timer
    m_DebrisTimer -= DeltaTime;
    if( m_DebrisTimer <= 0 ) 
        m_DebrisTimer = 0;

    // Create debris effect if needed
    if( m_DebrisTimer > 0 )
    {
        vector3 VelDir = m_Velocity;
        if( VelDir.Length() > 2.0f )
        {
            VelDir.Normalize();

            vector3 WheelDownDir = VehicleL2W.RotateVector(vector3(0, -1, 0));
            vector3 WheelBottom = m_Position + WheelDownDir * (m_CurrentShockLength + k_WheelRadius);

            if( x_irand(0, 100) < 10 )  // 10% chance
            {
                VelDir += vector3(x_frand(-1, +1), x_frand(-1, +1), x_frand(-1, +1));
                // Create debris particles here if needed
            }
        }
    }
}

//==============================================================================

void wheel::FinishAdvance( f32 DeltaTime, const matrix4& VehicleL2W )
{
    (void)VehicleL2W;
    
    // Update velocity
    m_Velocity = (m_Position - m_OldPosition) / DeltaTime;
    
    // Backup current position
    m_OldPosition = m_Position;
}

//==============================================================================
// CAR_OBJECT CLASS IMPLEMENTATION
//==============================================================================

car_object::car_object()
{
    m_Health = 1000.0f;
    m_MaxHealth = 1000.0f;
    m_OldStabilizationTheta = 0;
    m_StabilizationSpeed = 0;
    m_AudioIdle = 0;
    m_AudioRev = 0;
    m_RevTurbo = 0;
    m_nBubbles = 3;
    m_bDisplayShockInfo = FALSE;
    
    // Initialize collision bubbles
    m_BubbleOffset[0].Set(0, 100, 100);
    m_BubbleOffset[1].Set(0, 100, 0);
    m_BubbleOffset[2].Set(0, 100, -100);
    m_BubbleRadius[0] = 90;
    m_BubbleRadius[1] = 90;
    m_BubbleRadius[2] = 90;
}

//==============================================================================

car_object::~car_object()
{
}

//==============================================================================

void car_object::OnInit( void )
{
    vehicle_object::OnInit();
    
    // Initialize vehicle specific data
    VehicleInit();
}

//==============================================================================

void car_object::OnKill( void )
{
    VehicleKill();
    
    vehicle_object::OnKill();
}

//==============================================================================

void car_object::OnEnumProp( prop_enum& List )
{
    vehicle_object::OnEnumProp( List );
    
    List.PropEnumHeader( "Car", "Car specific properties", 0 );
    List.PropEnumFloat( "Car\\Health", "Health of the car", PROP_TYPE_EXPOSE );
    List.PropEnumFloat( "Car\\Max Health", "Maximum health of the car", 0 );
    List.PropEnumBool( "Car\\Show Shock Info", "Display shock debugging information", PROP_TYPE_DONT_SAVE );
}

//==============================================================================

xbool car_object::OnProperty( prop_query& I )
{
    if( vehicle_object::OnProperty( I ) )
        return TRUE;
        
    if( I.IsVar( "Car\\Health" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarFloat( m_Health );
        }
        else
        {
            m_Health = I.GetVarFloat();
        }
        return TRUE;
    }
    
    if( I.IsVar( "Car\\Max Health" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarFloat( m_MaxHealth );
        }
        else
        {
            m_MaxHealth = I.GetVarFloat();
            if( m_Health > m_MaxHealth )
                m_Health = m_MaxHealth;
        }
        return TRUE;
    }
    
    if( I.IsVar( "Car\\Show Shock Info" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarBool( m_bDisplayShockInfo );
        }
        else
        {
            m_bDisplayShockInfo = I.GetVarBool();
        }
        return TRUE;
    }
    
    return FALSE;
}

//==============================================================================

void car_object::OnPain( const pain& Pain )
{
    if( m_Health <= 0 )
        return;
        
    // Process damage
    health_handle HealthHandle(GetLogicalName());
    Pain.ComputeDamageAndForce( HealthHandle, GetGuid(), GetPosition() );
    f32 Damage = Pain.GetDamage();
    
    m_Health -= Damage;
    
    if( m_Health <= 0 )
    {
        m_Health = 0;
        
        // Create destruction effects
        particle_emitter::CreateOnPainEffect( Pain, GetBBox().GetRadius(), 
                                            particle_emitter::GRENADE_EXPLOSION, XCOLOR_WHITE ); //VEHICLE_EXPLOSION CHANGE IT!!!
      		
		// Destroy the car
       g_ObjMgr.DestroyObject( GetGuid() );
   }
}

//==============================================================================

void car_object::VehicleInit( void )
{
   s32 i;

   m_BackupL2W = GetL2W();

   rigid_geom* pGeom = m_RigidInst.GetRigidGeom();
   if( !pGeom )
       return;

   // Lookup animation pointer
   anim_group* pAnimGroup = m_hAnimGroup.GetPointer();
   if( !pAnimGroup )
       return;

   // Initialize wheels
   const char* WheelBoneNames[4] = 
   {
       "whl_lft_frnt",
       "whl_rt_frnt",
       "whl_lft_bck",
       "whl_rt_bck"
   };

   for( i = 0; i < 4; i++ )
   {
       // Lookup bone index
       s32 iBone = pAnimGroup->GetBoneIndex( WheelBoneNames[i] );
       
       // In case bone is not found
       if( iBone == -1 )
           iBone = 0;

       // Lookup offset 
       vector3 Offset = pAnimGroup->GetBone(iBone).BindTranslation;

       // Initialize wheel
       m_Wheels[i].Init( iBone,                     
                        GetL2W(),
                        Offset,                    
                        GetGuid(),
                        FALSE );    
   }

   m_OldStabilizationTheta = 0;
   m_RevTurbo = 0;

   // Initialize audio
   m_AudioIdle = 0;
   m_AudioRev = 0;
}

//==============================================================================

void car_object::VehicleKill( void )
{
   // Release audio voices
   if( m_AudioIdle )
   {
       g_AudioMgr.Release( m_AudioIdle, 0.0f );
       m_AudioIdle = 0;
   }
   
   if( m_AudioRev )
   {
       g_AudioMgr.Release( m_AudioRev, 0.0f );
       m_AudioRev = 0;
   }
}

//==============================================================================

void car_object::VehicleReset( void )
{
   s32 i;

   m_OldStabilizationTheta = 0;

   SetTransform( m_BackupL2W );

   // Reset wheels
   for( i = 0; i < 4; i++ )
       m_Wheels[i].Reset( m_BackupL2W );

   vehicle_object::VehicleReset();

   m_RevTurbo = 0;
   
   // Reset health
   m_Health = m_MaxHealth;
}

//==============================================================================

void car_object::ComputeL2W( void )
{
   s32 i;

   vector3 FLToFR = m_Wheels[1].m_Position - m_Wheels[0].m_Position;
   vector3 FLToBL = m_Wheels[2].m_Position - m_Wheels[0].m_Position;

   vector3 Up = FLToBL.Cross(FLToFR);
   vector3 Forward = -FLToBL;
   vector3 Left = Up.Cross(Forward);
   Up.Normalize();
   Forward.Normalize();
   Left.Normalize();

   matrix4 L2W;
   L2W.Identity();
   L2W.SetColumns(Left, Up, Forward);

   vector3 L2WPos(0, 0, 0);
   for( i = 0; i < 4; i++ )
   {
       vector3 Trans = m_Wheels[i].m_Position - (L2W * m_Wheels[i].m_Offset);
       L2WPos += Trans * 0.25f;
   }

   L2W.SetTranslation( L2WPos );

   SetTransform(L2W);
   OnMove( L2WPos );
}

//==============================================================================

void car_object::AdvanceSteering( f32 DeltaTime )
{
   s32 i;

   // Get camera yaw
   radian CameraYaw = m_Camera.m_Yaw;
   radian ForwardYaw = GetL2W().GetRotation().Yaw;

   radian WheelAngle;
   if( m_Input.m_Accel >= 0 )
       WheelAngle = x_MinAngleDiff(CameraYaw, ForwardYaw);
   else
       WheelAngle = -x_MinAngleDiff(CameraYaw, ForwardYaw);

   // Limit wheel angles
   WheelAngle = MINMAX( -k_MaxSteeringAngle, WheelAngle, k_MaxSteeringAngle );
   
   radian MaxWheelRot = k_WheelTurnRate * DeltaTime;

   // Rotate front wheels
   for( i = 0; i < 2; i++ )
   {
       radian WheelDiff = (WheelAngle - m_Wheels[i].m_Steer);
       WheelDiff = MINMAX( -MaxWheelRot, WheelDiff, MaxWheelRot );
       
       m_Wheels[i].m_Steer += WheelDiff;
       m_Wheels[i].m_Steer = MINMAX( -k_MaxSteeringAngle, m_Wheels[i].m_Steer, k_MaxSteeringAngle );
   }
}

//==============================================================================

void car_object::UpdateAudio( f32 DeltaTime )
{
   (void)DeltaTime;

   // Try to gather audio
   if( !m_AudioIdle )
       m_AudioIdle = g_AudioMgr.Play( "Jeep_Loop_01", GetPosition(), GetZone1(), TRUE );

   if( !m_AudioRev )
       m_AudioRev = g_AudioMgr.Play( "Jeep_Loop_02", GetPosition(), GetZone1(), TRUE );

   // Get average wheel speed
   s32 i;
   f32 AvgWS = 0;
   for( i = 0; i < 4; i++ )
       AvgWS += x_abs(m_Wheels[i].m_SpinSpeed) * 0.25f;

   f32 CircumferenceOfTire = (k_WheelRadius * 2) * 3.14159f;
   f32 MPHToCMPSec = (12.0f * 2.54f * 5280.0f) / 3600.0f;
   f32 TopForwardRotPerSecond = +(k_TopSpeedForward * MPHToCMPSec) / CircumferenceOfTire;
   f32 SpinT = AvgWS / (TopForwardRotPerSecond * R_360);
   SpinT = MINMAX( 0.0f, SpinT, 1.0f );

   // Update audio parameters
   f32 IdleV = 0.50f + SpinT * 0.10f;
   f32 IdleP = 0.60f + SpinT * 0.60f;
   f32 RevV = SpinT * 0.80f;
   f32 RevP = 0.80f + SpinT * 0.40f;

   // Check if REV is in turbo from lost traction
   for( i = 0; i < 4; i++ )
   {
       if( m_Wheels[i].m_bHasTraction ) 
           break;
   }
   
   if( i == 4 )
   {
       // No traction - add turbo
       m_RevTurbo += DeltaTime * 1.0f;
       if( m_RevTurbo > 2.0f )
           m_RevTurbo = 2.0f;
   }
   else
   {
       // Has traction - reduce turbo
       m_RevTurbo -= DeltaTime * 4.0f;
       if( m_RevTurbo < 0 )
           m_RevTurbo = 0;
   }

   RevP += m_RevTurbo;

   // Set audio parameters
   g_AudioMgr.SetVolume( m_AudioIdle, IdleV );
   g_AudioMgr.SetVolume( m_AudioRev, RevV );
   g_AudioMgr.SetPitch( m_AudioIdle, IdleP );
   g_AudioMgr.SetPitch( m_AudioRev, RevP );
}

//==============================================================================

void car_object::VehicleUpdate( f32 DeltaTime )
{
   CONTEXT( "car_object::VehicleUpdate" );

   AdvanceSteering( DeltaTime );

   // Do air stabilization
   UpdateAirStabilization( DeltaTime );

   // Advance all wheels
   m_Wheels[0].Advance( DeltaTime, GetL2W(), m_Input.m_Accel, m_Input.m_Brake );
   m_Wheels[1].Advance( DeltaTime, GetL2W(), m_Input.m_Accel, m_Input.m_Brake );
   m_Wheels[2].Advance( DeltaTime, GetL2W(), m_Input.m_Accel, m_Input.m_Brake );
   m_Wheels[3].Advance( DeltaTime, GetL2W(), m_Input.m_Accel, m_Input.m_Brake );

   // Enforce constraints
   EnforceConstraints();

   // Allow wheels to reorient
   m_Wheels[0].FinishAdvance( DeltaTime, GetL2W() );
   m_Wheels[1].FinishAdvance( DeltaTime, GetL2W() );
   m_Wheels[2].FinishAdvance( DeltaTime, GetL2W() );
   m_Wheels[3].FinishAdvance( DeltaTime, GetL2W() );

   UpdateAudio( DeltaTime );
}

//==============================================================================

void car_object::UpdateAirStabilization( f32 DeltaTime )
{
   (void)DeltaTime;
   
   // Check if any wheels have traction
   s32 i;
   for( i = 0; i < 4; i++ )
   {
       if( m_Wheels[i].m_bHasTraction )
           break;
   }
   
   // If any wheel has traction, don't stabilize
   if( i < 4 )
       return;
   
   // Determine current UP axis and current theta
   vector3 UpDir = GetL2W().RotateVector(vector3(0, 1, 0));
   radian CurrentTheta = v3_AngleBetween( UpDir, vector3(0, 1, 0) );

   // If no wheels have traction then try and stabilize
   if( CurrentTheta > R_30 )
   {
       // Compute axis to rotate on and build quaternion
       vector3 RotAxis = UpDir.Cross(vector3(0, 1, 0));
       quaternion Q;
       Q.Setup(RotAxis, (CurrentTheta - R_30));

       // Loop through the wheels and apply stabilization
       vector3 CarPosition = GetPosition();
       for( i = 0; i < 4; i++ )
       {
           vector3 OldP = m_Wheels[i].m_Position;
           vector3 NewP = Q * (OldP - CarPosition) + CarPosition;
           m_Wheels[i].m_Position = NewP;

           vector3 Dir = NewP - OldP;
           f32 Len = Dir.Length();
           if( Len > 0.0001f )
           {
               Dir /= Len;
               f32 SpeedInDir = Dir.Dot(m_Wheels[i].m_Velocity);
               m_Wheels[i].m_Velocity -= Dir * SpeedInDir;
           }
       }
   }
}

//==============================================================================

void car_object::EnforceConstraints( void )
{
   s32 i, j;
   s32 nSpheres = 0;
   vector3 SphereOffset[16];
   vector3 SpherePos[16];
   f32 SphereRadius[16];

   // Add wheels
   for( i = 0; i < 4; i++ )
   {
       SphereOffset[nSpheres] = m_Wheels[i].m_Offset;
       SpherePos[nSpheres] = m_Wheels[i].m_Position;
       SphereRadius[nSpheres] = k_WheelRadius;
       nSpheres++;
   }

   // Solve wheel collisions
   SolveSphereCollisions( GetGuid(), 4, SphereOffset, SpherePos, SphereRadius );

   // Update car transform
   ComputeL2W();

   // Add collision bubbles
   for( i = 0; i < m_nBubbles; i++ )
   {
       SphereOffset[nSpheres] = m_BubbleOffset[i];
       SpherePos[nSpheres] = GetL2W() * m_BubbleOffset[i];
       SphereRadius[nSpheres] = m_BubbleRadius[i];
       nSpheres++;
   }

   // Solve all collisions
   SolveSphereCollisions( GetGuid(), nSpheres, SphereOffset, SpherePos, SphereRadius );

   // Update wheel positions
   nSpheres = 0;
   for( i = 0; i < 4; i++ )
   {
       m_Wheels[i].m_Position = SpherePos[nSpheres];
       nSpheres++;
   }

   // Update car transform
   ComputeL2W();
}

//==============================================================================

void car_object::RenderShockHistory( void )
{
   // TODO: Make shock history rendering code
}

//==============================================================================

void car_object::VehicleRender( void )
{
   CONTEXT( "car_object::VehicleRender" );

   s32 i;
  
   // Lookup animation pointer
   anim_group* pAnimGroup = m_hAnimGroup.GetPointer();
   if( !pAnimGroup )
       return;
   
   // Allocate matrices
   s32 NBones = pAnimGroup->GetNBones();
   matrix4* pMatrices = (matrix4*)smem_BufferAlloc(NBones * sizeof(matrix4));
   if( !pMatrices )
       return;

   // Setup matrices
   for( i = 0; i < NBones; i++ )
   {
       pMatrices[i] = GetL2W();
   }

   // Setup wheel matrices
   for( i = 0; i < 4; i++ )
   {
       wheel& Wheel = m_Wheels[i];

       matrix4 VehicleL2W = GetL2W();

       matrix4 L2W;
       L2W.Identity();
       L2W.RotateX( Wheel.m_Spin );
       L2W.RotateY( Wheel.m_Steer );
       L2W.Translate(vector3(0, -Wheel.m_CurrentShockLength, 0));
       L2W.Translate(Wheel.m_Offset);

       pMatrices[Wheel.m_iBone] = VehicleL2W * L2W * pAnimGroup->GetBoneBindInvMatrix(Wheel.m_iBone);
   }

   // Draw geometry
   u32 Flags = (GetFlagBits() & object::FLAG_CHECK_PLANES) ? render::CLIPPED : 0;
   m_RigidInst.Render(pMatrices, Flags | GetRenderMode());

#ifndef X_RETAIL
   // Draw debug info if enabled
   if( m_bDisplayShockInfo )
   {
       draw_ClearL2W();
       
       for( i = 0; i < 4; i++ )
       {
           wheel& Wheel = m_Wheels[i];

           vector3 WheelDownDir = GetL2W().RotateVector(vector3(0, -1, 0));
           vector3 WheelCenter = Wheel.m_Position + WheelDownDir * Wheel.m_CurrentShockLength;
           draw_Sphere(WheelCenter, k_WheelRadius, XCOLOR_RED);
       }

       // Draw collision bubbles
       for( i = 0; i < m_nBubbles; i++ )
       {
           vector3 P = GetL2W() * m_BubbleOffset[i];
           draw_Sphere( P, m_BubbleRadius[i], XCOLOR_WHITE );
       }

       RenderShockHistory();
   }
#endif // X_RETAIL
}

//==============================================================================

void car_object::OnPolyCacheGather( void )
{
   RigidGeom_GatherToPolyCache( GetGuid(), 
                               GetBBox(), 
                               m_RigidInst.GetLODMask(U16_MAX), 
                               &GetL2W(), 
                               m_RigidInst.GetRigidGeom() );
}

//==============================================================================

const char* car_object::GetLogicalName( void )
{
   return "CAR";
}

//==============================================================================