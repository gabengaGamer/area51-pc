//=========================================================================
//
//  ShadowMapMgr.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ShadowMapMgr.hpp"

#include "x_array.hpp"

#include "..\Objects\Object.hpp"
#include "..\Objects\player.hpp"
#include "..\ZoneMgr\ZoneMgr.hpp"

#include "LightMgr.hpp"
#include "Render.hpp"

#include "Entropy.hpp"

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

shadow_map_mgr g_ShadowMapMgr;

//=========================================================================
//  FILE-LOCAL TYPES
//=========================================================================

namespace
{
    struct shadow_point_face
    {
        radian Pitch;
        radian Yaw;
    };

    //---------------------------------------------------------------------

    struct shadow_light
    {
        s32     Shape;
        vector3 Pos;
        vector3 Direction;
        f32     Radius;
        f32     Falloff;
        radian  FOV;
        s32     SourceCount;
        s32     RelevantCasterStart;
        s32     RelevantCasterCount;
        f32     Score;
    };

    //---------------------------------------------------------------------

    const shadow_point_face kPointShadowFaces[POINT_SHADOW_FACE_COUNT] =
    {
        { -R_90,  R_0    },
        {  R_90,  R_0    },
        {  R_0,   R_0    },
        {  R_0,   R_180  },
        {  R_0,   R_90   },
        {  R_0,  -R_90   },
    };

    //---------------------------------------------------------------------

    const radian kPointShadowFOV           = R_90;
    const radian kMinSpotShadowFOV         = DEG_TO_RAD( 1.0f );
    const f32    kPointFaceCoverageCos     = 0.57735026919f; // 1/sqrt(3), covers the face corners of a 90-degree cube face.
    const f32    kShadowAtlasFilterRadius  = 2.0f;
    const f32    kShadowCasterScoreWeight  = 4096.0f;
    const f32    kPlayerShadowScoreBonus   = 8192.0f;
    const f32    kShadowRadiusScorePenalty = 0.25f;

    //---------------------------------------------------------------------

    struct dynamic_shadow_light
    {
        vector3 Pos;
        vector3 Direction;
        xcolor  Color;
        f32     Radius;
        f32     Falloff;
        f32     OuterAngle;
        s32     Shape;
    };

    //---------------------------------------------------------------------

    radian GetShadowLightFOV( const dynamic_shadow_light& DynamicLight )
    {
        if( DynamicLight.Shape == light_mgr::LIGHT_SHAPE_OMNI )
            return kPointShadowFOV;

        return MAX( kMinSpotShadowFOV, DEG_TO_RAD( DynamicLight.OuterAngle ) );
    }

    //---------------------------------------------------------------------

    s32 GetShadowLightSourceCount( s32 LightShape )
    {
        if( LightShape == light_mgr::LIGHT_SHAPE_OMNI )
            return POINT_SHADOW_FACE_COUNT;

        return 1;
    }

    //---------------------------------------------------------------------

    f32 ComputeSourceConeCoverage( const vector3& LightPos,
                                   const vector3& LightDirection,
                                   f32            CosOuter,
                                   const vector3& TargetPos )
    {
        if( CosOuter <= -1.0f )
            return 1.0f;

        vector3 SourceDir = LightDirection;
        if( !SourceDir.SafeNormalize() )
            SourceDir.Set( 0.0f, 0.0f, 1.0f );

        vector3 ToTarget = TargetPos - LightPos;
        if( !ToTarget.SafeNormalize() )
            return 1.0f;

        const f32 CosRange = MAX( 1.0f - CosOuter, 0.0001f );
        const f32 SourceCos = SourceDir.Dot( ToTarget );
        return MINMAX( 0.0f, ( SourceCos - CosOuter ) / CosRange, 1.0f );
    }

    //---------------------------------------------------------------------

    xbool SourceConeAffectsBBox( const vector3& LightPos,
                                 const vector3& LightDirection,
                                 f32            CosOuter,
                                 const bbox&    WorldBBox )
    {
        if( CosOuter <= -1.0f )
            return TRUE;

        const vector3 Center = WorldBBox.GetCenter();
        if( ComputeSourceConeCoverage( LightPos,
                                       LightDirection,
                                       CosOuter,
                                       Center ) > 0.0f )
        {
            return TRUE;
        }

        const vector3 Min = WorldBBox.Min;
        const vector3 Max = WorldBBox.Max;
        vector3 TestPoint;

        for( s32 i = 0; i < 8; i++ )
        {
            TestPoint.Set( (i & 1) ? Max.GetX() : Min.GetX(),
                           (i & 2) ? Max.GetY() : Min.GetY(),
                           (i & 4) ? Max.GetZ() : Min.GetZ() );

            if( ComputeSourceConeCoverage( LightPos,
                                           LightDirection,
                                           CosOuter,
                                           TestPoint ) > 0.0f )
            {
                return TRUE;
            }
        }

        return FALSE;
    }

    //---------------------------------------------------------------------

    xbool ShadowCasterIsRelevant( object*       pCaster,
                                  const vector3& LightPos,
                                  f32            LightRadius,
                                  const vector3* pLightDirection = NULL,
                                  f32            LightConeOuterCos = -1.0f )
    {
        if( !pCaster )
            return FALSE;

#ifdef X_EDITOR
        if( pCaster->IsHidden() )
            return FALSE;
#endif

        const bbox& CasterBBox = pCaster->GetBBox();
        // Deep and raw
        //if( !g_ZoneMgr.IsBBoxVisible( CasterBBox,
        //                              (u8)pCaster->GetZone1(),
        //                              (u8)pCaster->GetZone2() ) )
        //{
        //    return FALSE;
        //}

        if( !CasterBBox.Intersect( LightPos, LightRadius ) )
            return FALSE;

        if( pLightDirection )
        {
            return SourceConeAffectsBBox( LightPos,
                                          *pLightDirection,
                                          LightConeOuterCos,
                                          CasterBBox );
        }

        return TRUE;
    }

    //---------------------------------------------------------------------

    f32 GetShadowLightScore( const dynamic_shadow_light& DynamicLight,
                             s32                         LocalCasterCount,
                             player*                     pActivePlayer )
    {
        f32 LightScore = (f32)( DynamicLight.Color.R + DynamicLight.Color.G + DynamicLight.Color.B );
        LightScore += (f32)LocalCasterCount * kShadowCasterScoreWeight;
        LightScore -= DynamicLight.Radius * kShadowRadiusScorePenalty;

        if( pActivePlayer && pActivePlayer->GetBBox().Intersect( DynamicLight.Pos, DynamicLight.Radius ) )
            LightScore += kPlayerShadowScoreBonus;

        return LightScore;
    }

    //---------------------------------------------------------------------

    xbool BuildShadowLight( shadow_light&         ShadowLight,
                            s32                   LightIndex,
                            object* const*        ppCasterCandidates,
                            s32                   NCasterCandidates,
                            player*               pActivePlayer,
                            xarray<s32>&          RelevantCasterIndices )
    {
        dynamic_shadow_light DynamicLight;
        xbool                CastShadows;
        f32                  UnusedInnerRadius;
        f32                  UnusedInnerAngle;
        s32                  UnusedShadowMapResolution;
        s32                  UnusedShadowPriority;

        g_LightMgr.GetDynamicLight( LightIndex,
                                    DynamicLight.Pos,
                                    DynamicLight.Radius,
                                    DynamicLight.Color,
                                    DynamicLight.Shape,
                                    CastShadows,
                                    UnusedInnerRadius,
                                    DynamicLight.Direction,
                                    DynamicLight.Falloff,
                                    UnusedInnerAngle,
                                    DynamicLight.OuterAngle,
                                    UnusedShadowMapResolution,
                                    UnusedShadowPriority );

        (void)UnusedInnerRadius;
        (void)UnusedInnerAngle;
        (void)UnusedShadowMapResolution;
        (void)UnusedShadowPriority;

        if( !CastShadows || ( DynamicLight.Radius <= 0.0f ) )
            return FALSE;

        f32 LightConeOuterCos = -1.0f;
        if( DynamicLight.Shape == light_mgr::LIGHT_SHAPE_SPOT )
            LightConeOuterCos = x_cos( DEG_TO_RAD( DynamicLight.OuterAngle ) * 0.5f );

        const s32 RelevantCasterStart = RelevantCasterIndices.GetCount();
        s32       LocalCasterCount    = 0;

        for( s32 iCandidate = 0; iCandidate < NCasterCandidates; iCandidate++ )
        {
            if( !ShadowCasterIsRelevant( ppCasterCandidates[iCandidate],
                                         DynamicLight.Pos,
                                         DynamicLight.Radius,
                                         ( DynamicLight.Shape == light_mgr::LIGHT_SHAPE_SPOT ) ? &DynamicLight.Direction : NULL,
                                         LightConeOuterCos ) )
            {
                continue;
            }

            RelevantCasterIndices.Append( iCandidate );
            LocalCasterCount++;
        }

        if( LocalCasterCount <= 0 )
            return FALSE;

        ShadowLight.Shape       = DynamicLight.Shape;
        ShadowLight.Pos         = DynamicLight.Pos;
        ShadowLight.Direction   = DynamicLight.Direction;
        ShadowLight.Radius      = DynamicLight.Radius;
        ShadowLight.Falloff     = DynamicLight.Falloff;
        ShadowLight.FOV         = GetShadowLightFOV( DynamicLight );
        ShadowLight.SourceCount = GetShadowLightSourceCount( DynamicLight.Shape );
        ShadowLight.RelevantCasterStart = RelevantCasterStart;
        ShadowLight.RelevantCasterCount = LocalCasterCount;
        ShadowLight.Score       = GetShadowLightScore( DynamicLight, LocalCasterCount, pActivePlayer );
        return TRUE;
    }

    //---------------------------------------------------------------------

    void AddShadowCastersInLight( object* const*  ppCasterCandidates,
                                  const s32*      pRelevantCasterIndices,
                                  const shadow_light& ShadowLight,
                                  object**        ppCasters,
                                  u64*            pCasterMasks,
                                  s32&            NCasters,
                                  u64             SourceMask )
    {
        const s32 EndRelevantCaster = ShadowLight.RelevantCasterStart + ShadowLight.RelevantCasterCount;

        for( s32 iRelevantCaster = ShadowLight.RelevantCasterStart;
             iRelevantCaster < EndRelevantCaster;
             iRelevantCaster++ )
        {
            object* pCaster = ppCasterCandidates[pRelevantCasterIndices[iRelevantCaster]];

            s32 iExisting = 0;
            for( ; iExisting < NCasters; iExisting++ )
            {
                if( ppCasters[iExisting] == pCaster )
                    break;
            }

            if( iExisting == NCasters )
            {
                if( NCasters >= render::MAX_SHADOW_CASTERS )
                    break;

                ppCasters[NCasters]    = pCaster;
                pCasterMasks[NCasters] = 0;
                iExisting              = NCasters;
                NCasters++;
            }

            pCasterMasks[iExisting] |= SourceMask;
        }
    }

    //---------------------------------------------------------------------

    s32 CollectShadowCastersForLights( object* const*       ppCasterCandidates,
                                       const s32*           pRelevantCasterIndices,
                                       const shadow_light*  pShadowLights,
                                       s32                  NShadowLights,
                                       object**             ppCasters,
                                       u64*                 pCasterMasks )
    {
        s32 NCasters   = 0;
        s32 SourceBase = 0;

        for( s32 iLight = 0; iLight < NShadowLights; iLight++ )
        {
            const shadow_light& ShadowLight = pShadowLights[iLight];
            const u64           LightMask   = ( ( (u64)1 << ShadowLight.SourceCount ) - 1 ) << SourceBase;

            AddShadowCastersInLight( ppCasterCandidates,
                                     pRelevantCasterIndices,
                                     ShadowLight,
                                     ppCasters,
                                     pCasterMasks,
                                     NCasters,
                                     LightMask );
            SourceBase += ShadowLight.SourceCount;
        }

        return NCasters;
    }
	
    //---------------------------------------------------------------------	

    void AddPointShadowSources( const vector3& LightPos,
                                f32            LightRadius,
                                f32            LightFalloff )
    {
        for( s32 iFace = 0; iFace < ARRAYSIZE(kPointShadowFaces); iFace++ )
        {
            matrix4 FaceL2W;
            FaceL2W.Setup( vector3( 1.0f, 1.0f, 1.0f ),
                           radian3( kPointShadowFaces[iFace].Pitch,
                                    kPointShadowFaces[iFace].Yaw,
                                    R_0 ),
                           LightPos );

            render::AddPointShadowMapSource( FaceL2W,
                                             kPointShadowFOV,
                                             LightRadius,
                                             LightFalloff );
        }
    }

    //---------------------------------------------------------------------

    void AddSpotShadowSource( const vector3& LightPos,
                              const vector3& LightDirection,
                              radian         ShadowFOV,
                              f32            LightRadius,
                              f32            LightFalloff )
    {
        matrix4 SpotL2W;
        SpotL2W.Setup( vector3( 1.0f, 1.0f, 1.0f ),
                       radian3( LightDirection.GetPitch(), LightDirection.GetYaw(), R_0 ),
                       LightPos );

        render::AddSpotShadowMapSource( SpotL2W,
                                        ShadowFOV,
                                        LightRadius,
                                        LightFalloff );
    }

    //---------------------------------------------------------------------

    void AddShadowSources( const shadow_light* pShadowLights,
                           s32                 NShadowLights )
    {
        for( s32 iLight = 0; iLight < NShadowLights; iLight++ )
        {
            const shadow_light& ShadowLight = pShadowLights[iLight];

            if( ShadowLight.Shape == light_mgr::LIGHT_SHAPE_OMNI )
            {
                AddPointShadowSources( ShadowLight.Pos,
                                       ShadowLight.Radius,
                                       ShadowLight.Falloff );
            }
            else
            {
                AddSpotShadowSource( ShadowLight.Pos,
                                     ShadowLight.Direction,
                                     ShadowLight.FOV,
                                     ShadowLight.Radius,
                                     ShadowLight.Falloff );
            }
        }
    }

    //---------------------------------------------------------------------

    void InsertShadowLight( shadow_light*            pLights,
                            s32&                     NLights,
                            const shadow_light&      Light )
    {
        if( NLights >= light_mgr::MAX_DYNAMIC_LIGHTS )
        {
            if( Light.Score <= pLights[NLights - 1].Score )
                return;

            NLights--;
        }

        s32 InsertAt = NLights;
        while( InsertAt > 0 )
        {
            if( pLights[InsertAt - 1].Score >= Light.Score )
                break;

            pLights[InsertAt] = pLights[InsertAt - 1];
            InsertAt--;
        }

        pLights[InsertAt] = Light;
        NLights++;
    }

    //---------------------------------------------------------------------

    s32 SelectShadowLights( const shadow_light* pShadowLightCandidates,
                            s32                 NCandidateShadowLights,
                            shadow_light*       pShadowLights )
    {
        s32 NPointShadowLights = 0;
        s32 NShadowLights      = 0;
        s32 NShadowSources     = 0;

        for( s32 iLight = 0; iLight < NCandidateShadowLights; iLight++ )
        {
            const shadow_light& Candidate = pShadowLightCandidates[iLight];

            if( ( Candidate.Shape == light_mgr::LIGHT_SHAPE_OMNI ) &&
                ( NPointShadowLights >= MAX_SHADOW_LIGHTS ) )
            {
                continue;
            }

            if( ( NShadowSources + Candidate.SourceCount ) > MAX_SHADOW_SOURCES )
                continue;

            pShadowLights[NShadowLights++] = Candidate;
            NShadowSources += Candidate.SourceCount;

            if( Candidate.Shape == light_mgr::LIGHT_SHAPE_OMNI )
                NPointShadowLights++;
        }

        return NShadowLights;
    }

    //---------------------------------------------------------------------

    s32 GetAtlasGuardTexels( f32 FilterRadius )
    {
        const s32 GuardTexels = (s32)x_ceil( ( FilterRadius * 4.0f ) + 2.0f );
        return ( GuardTexels > 0 ) ? GuardTexels : 1;
    }

    //---------------------------------------------------------------------

    xbool ShadowSourceAffectsBBox( const shadow_map_mgr::shadow_source& Source,
                                   const bbox&                          WorldBBox )
    {
        if( !Source.WorldBBox.Intersect( WorldBBox ) )
            return FALSE;

        const vector3 LightPos( Source.LightPosRadius.GetX(),
                                Source.LightPosRadius.GetY(),
                                Source.LightPosRadius.GetZ() );
        const vector3 LightDirection( Source.FaceLightDirFalloff.GetX(),
                                      Source.FaceLightDirFalloff.GetY(),
                                      Source.FaceLightDirFalloff.GetZ() );
        return SourceConeAffectsBBox( LightPos,
                                      LightDirection,
                                      Source.FaceLightData.GetX(),
                                      WorldBBox );
    }

    //---------------------------------------------------------------------

    f32 ComputeShadowSourceScore( const shadow_map_mgr::shadow_source& Source,
                                  const bbox&                          WorldBBox )
    {
        const vector3 LightPos( Source.LightPosRadius.GetX(),
                                Source.LightPosRadius.GetY(),
                                Source.LightPosRadius.GetZ() );
        const vector3 LightDirection( Source.FaceLightDirFalloff.GetX(),
                                      Source.FaceLightDirFalloff.GetY(),
                                      Source.FaceLightDirFalloff.GetZ() );
        const vector3 BBoxCenter = WorldBBox.GetCenter();
        const vector3 ToCenter   = BBoxCenter - LightPos;
        const f32     DistanceSq = ToCenter.Dot( ToCenter );
        const f32     Radius     = Source.LightPosRadius.GetW();

        if( Radius <= 0.0f )
            return 0.0f;

        f32 Distance = 0.0f;
        if( DistanceSq > 1e-8f )
            Distance = x_sqrt( DistanceSq );

        const f32 Falloff     = MINMAX( 0.0f, Source.LightFalloff, 1.0f );
        const f32 InnerRadius = Radius * ( 1.0f - Falloff );
        const f32 RadialScore = 1.0f - MINMAX( 0.0f,
                                               ( Distance - InnerRadius ) / MAX( Radius - InnerRadius, 0.0001f ),
                                               1.0f );
        const f32 ConeScore   = ComputeSourceConeCoverage( LightPos,
                                                           LightDirection,
                                                           Source.FaceLightData.GetX(),
                                                           BBoxCenter );

        return MAX( 0.0001f, RadialScore * ConeScore );
    }

    //---------------------------------------------------------------------

    void InsertCollectedSourceByScore( s32   SourceIndex,
                                       f32   Score,
                                       s32   MaxSourceCount,
                                       s32*  pCollectedSources,
                                       f32*  pCollectedScores,
                                       s32&  NCollectedSources )
    {
        ASSERT( MaxSourceCount > 0 );

        if( NCollectedSources >= MaxSourceCount )
        {
            if( Score <= pCollectedScores[NCollectedSources - 1] )
                return;
        }

        s32 InsertAt = NCollectedSources;
        if( NCollectedSources < MaxSourceCount )
        {
            NCollectedSources++;
        }
        else
        {
            InsertAt = MaxSourceCount - 1;
        }

        while( InsertAt > 0 )
        {
            if( pCollectedScores[InsertAt - 1] >= Score )
                break;

            if( InsertAt < MaxSourceCount )
            {
                pCollectedScores[InsertAt]  = pCollectedScores[InsertAt - 1];
                pCollectedSources[InsertAt] = pCollectedSources[InsertAt - 1];
            }
            InsertAt--;
        }

        pCollectedScores [InsertAt] = Score;
        pCollectedSources[InsertAt] = SourceIndex;
    }
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

shadow_map_mgr::shadow_map_mgr( void ) :
    m_SourceCount      ( 0 ),
    m_PointFaceCount   ( 0 ),
    m_PointLightCount  ( 0 ),
    m_SpotSourceCount  ( 0 ),
    m_AtlasLayoutDirty ( FALSE ),
    m_NCollectedSources( 0 )
{
    x_memset( m_CollectedSources, 0, sizeof(m_CollectedSources) );
    x_memset( m_Sources, 0, sizeof(m_Sources) );
}

//=========================================================================

shadow_map_mgr::~shadow_map_mgr( void )
{
    ClearSources();
}

//=========================================================================

void shadow_map_mgr::ClearSources( void )
{
    m_SourceCount       = 0;
    m_PointFaceCount    = 0;
    m_PointLightCount   = 0;
    m_SpotSourceCount   = 0;
    m_AtlasLayoutDirty  = FALSE;
    m_NCollectedSources = 0;
}

//=========================================================================

void shadow_map_mgr::FinalizeSources( void )
{
    if( !m_AtlasLayoutDirty )
        return;

    UpdateAtlasLayout();
    m_AtlasLayoutDirty = FALSE;
}

//=========================================================================

void shadow_map_mgr::ComputePerspectiveSource( shadow_source&  Dest,
                                               s32             SourceType,
                                               const matrix4&  L2W,
                                               radian          FOV,
                                               f32             LightRadius,
                                               f32             LightFalloff ) const
{
    f32 NearZ = MAX( 0.5f, LightRadius * 0.01f );
    f32 FarZ  = MAX( NearZ + 1.0f, LightRadius );
    // TODO: Fix this double clamp 
    NearZ = MAX( 0.01f, NearZ );

    matrix4 W2Proj = L2W;
    W2Proj.InvertRT();

    const f32 TanHalfFOV = x_tan( FOV * 0.5f );
    f32       Scale      = ( TanHalfFOV > 0.0f ) ? ( 1.0f / TanHalfFOV ) : 1.0f;

    if( SourceType == SHADOW_SOURCE_POINT_FACE )
    {
        const f32 CoverageScale = (f32)POINT_SHADOW_FACE_SIZE /
                                  ( (f32)POINT_SHADOW_FACE_SIZE + 1.0f );
        Scale *= CoverageScale;
    }

    matrix4 Proj2Clip;
    Proj2Clip.Zero();

    if( SourceType == SHADOW_SOURCE_POINT_FACE )
        Proj2Clip(0,0) =  Scale;
    else
        Proj2Clip(0,0) = -Scale;

    Proj2Clip(1,1) =  Scale;
    Proj2Clip(2,2) =  FarZ / ( FarZ - NearZ );
    Proj2Clip(3,2) = -( NearZ * FarZ ) / ( FarZ - NearZ );
    Proj2Clip(2,3) =  1.0f;

    Dest.Type           = SourceType;
    Dest.PointLightIndex= -1;
    Dest.FaceIndex      = -1;
    Dest.WorldToClip    = Proj2Clip * W2Proj;
    Dest.WorldToAtlas   = Dest.WorldToClip;
    Dest.FaceLightDirFalloff.Zero();
    Dest.FaceLightData.Zero();
    Dest.LightFalloff   = LightFalloff;
    Dest.NearZ          = NearZ;
    Dest.FarZ           = FarZ;
    Dest.AtlasX         = 0;
    Dest.AtlasY         = 0;
    Dest.AtlasWidth     = SHADOW_ATLAS_SIZE;
    Dest.AtlasHeight    = SHADOW_ATLAS_SIZE;

    const vector3 LightPos = L2W.GetTranslation();
    const vector3 Extent( FarZ, FarZ, FarZ );
    Dest.LightPosRadius = vector4( LightPos.GetX(), LightPos.GetY(), LightPos.GetZ(), LightRadius );
    Dest.WorldBBox.Set( LightPos - Extent,
                        LightPos + Extent );
}

//=========================================================================

void shadow_map_mgr::UpdateAtlasLayout( void )
{
    if( m_SpotSourceCount <= 0 )
        return;

    s32 GridCols = 1;
    while( ( GridCols * GridCols ) < m_SpotSourceCount )
        GridCols++;

    s32 GridRows = ( m_SpotSourceCount + GridCols - 1 ) / GridCols;

    const s32 TileWidth   = SHADOW_ATLAS_SIZE / GridCols;
    const s32 TileHeight  = SHADOW_ATLAS_SIZE / GridRows;
    const s32 GuardTexels = ( m_SpotSourceCount > 1 ) ? GetAtlasGuardTexels( kShadowAtlasFilterRadius ) : 0;
    s32 SpotIndex = 0;
    for( s32 i = 0; i < m_SourceCount; i++ )
    {
        shadow_source& Source = m_Sources[i];
        if( Source.Type != SHADOW_SOURCE_SPOT )
            continue;

        const s32 AtlasTileX = SpotIndex % GridCols;
        const s32 AtlasTileY = SpotIndex / GridCols;

        Source.AtlasX      = AtlasTileX * TileWidth;
        Source.AtlasY      = AtlasTileY * TileHeight;
        Source.AtlasWidth  = TileWidth;
        Source.AtlasHeight = TileHeight;

        if( m_SpotSourceCount > 1 )
        {
            const s32 MaxGuardX   = ( Source.AtlasWidth  > 2 ) ? ( ( Source.AtlasWidth  - 1 ) / 2 ) : 0;
            const s32 MaxGuardY   = ( Source.AtlasHeight > 2 ) ? ( ( Source.AtlasHeight - 1 ) / 2 ) : 0;
            const s32 GuardX      = ( GuardTexels < MaxGuardX ) ? GuardTexels : MaxGuardX;
            const s32 GuardY      = ( GuardTexels < MaxGuardY ) ? GuardTexels : MaxGuardY;

            Source.AtlasX      += GuardX;
            Source.AtlasY      += GuardY;
            Source.AtlasWidth  -= GuardX * 2;
            Source.AtlasHeight -= GuardY * 2;
        }

        Source.WorldToAtlas = Source.WorldToClip;
        Source.WorldToAtlas.Scale( vector3( 0.5f, -0.5f, 1.0f ) );
        Source.WorldToAtlas.Translate( vector3( 0.5f, 0.5f, 0.0f ) );
        Source.WorldToAtlas.Scale( vector3( (f32)Source.AtlasWidth  / (f32)SHADOW_ATLAS_SIZE,
                                            (f32)Source.AtlasHeight / (f32)SHADOW_ATLAS_SIZE,
                                            1.0f ) );
        Source.WorldToAtlas.Translate( vector3( (f32)Source.AtlasX / (f32)SHADOW_ATLAS_SIZE,
                                                (f32)Source.AtlasY / (f32)SHADOW_ATLAS_SIZE,
                                                0.0f ) );
        SpotIndex++;
    }
}

//=========================================================================

xbool shadow_map_mgr::AddPointSource( const matrix4& L2W,
                                      radian         FOV,
                                      f32            LightRadius,
                                      f32            LightFalloff )
{
    if( m_SourceCount >= MAX_SHADOW_SOURCES )
        return FALSE;
    if( m_PointLightCount >= MAX_SHADOW_LIGHTS &&
        ( m_PointFaceCount % POINT_SHADOW_FACE_COUNT ) == 0 )
    {
        return FALSE;
    }

    shadow_source& Source = m_Sources[m_SourceCount];
    ComputePerspectiveSource( Source,
                              SHADOW_SOURCE_POINT_FACE,
                              L2W,
                              FOV,
                              LightRadius,
                              LightFalloff );

    vector3 FaceDirection = L2W.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
    if( !FaceDirection.SafeNormalize() )
        FaceDirection.Set( 0.0f, 0.0f, 1.0f );

    Source.FaceLightDirFalloff.Set( FaceDirection.GetX(),
                                    FaceDirection.GetY(),
                                    FaceDirection.GetZ(),
                                    LightFalloff );
    Source.FaceLightData.Set( kPointFaceCoverageCos,
                              Source.NearZ,
                              Source.FarZ,
                              0.0f );
    Source.PointLightIndex = m_PointFaceCount / POINT_SHADOW_FACE_COUNT;
    Source.FaceIndex       = m_PointFaceCount % POINT_SHADOW_FACE_COUNT;
    m_SourceCount++;
    m_PointFaceCount++;
    m_AtlasLayoutDirty = TRUE;	

    if( Source.FaceIndex == 0 )
        m_PointLightCount++;

    return TRUE;
}

//=========================================================================

xbool shadow_map_mgr::AddSpotSource( const matrix4& L2W,
                                     radian         FOV,
                                     f32            LightRadius,
                                     f32            LightFalloff )
{
    if( m_SourceCount >= MAX_SHADOW_SOURCES )
        return FALSE;

    shadow_source& Source = m_Sources[m_SourceCount];
    ComputePerspectiveSource( Source,
                              SHADOW_SOURCE_SPOT,
                              L2W,
                              FOV,
                              LightRadius,
                              LightFalloff );

    vector3 SpotDirection = L2W.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
    if( !SpotDirection.SafeNormalize() )
        SpotDirection.Set( 0.0f, 0.0f, 1.0f );

    Source.FaceLightDirFalloff.Set( SpotDirection.GetX(),
                                    SpotDirection.GetY(),
                                    SpotDirection.GetZ(),
                                    LightFalloff );
    Source.FaceLightData.Set( x_cos( FOV * 0.5f ),
                              Source.NearZ,
                              Source.FarZ,
                              0.0f );
    m_SourceCount++;
    m_SpotSourceCount++;
    m_AtlasLayoutDirty = TRUE;
    return TRUE;
}

//=========================================================================

s32 shadow_map_mgr::CollectSources( const matrix4& L2W, const bbox& B, s32 MaxSourceCount )
{
    ASSERT( MaxSourceCount >= 0 );

    FinalizeSources();
    m_NCollectedSources = 0;

    if( MaxSourceCount <= 0 )
        return 0;

    if( MaxSourceCount > MAX_SHADOW_SOURCES )
        MaxSourceCount = MAX_SHADOW_SOURCES;

    bbox WorldBBox = B;
    WorldBBox.Transform( L2W );

    f32 CollectedScores[MAX_SHADOW_SOURCES];
    s32 BestPointSources[MAX_SHADOW_LIGHTS];
    f32 BestPointScores[MAX_SHADOW_LIGHTS];

    for( s32 iPointLight = 0; iPointLight < MAX_SHADOW_LIGHTS; iPointLight++ )
    {
        BestPointSources[iPointLight] = -1;
        BestPointScores [iPointLight] = -1.0f;
    }

    for( s32 i = 0; i < m_SourceCount; i++ )
    {
        const shadow_source& Source = m_Sources[i];
        if( !ShadowSourceAffectsBBox( Source, WorldBBox ) )
            continue;

        const f32 Score = ComputeShadowSourceScore( Source, WorldBBox );
        if( Source.Type == SHADOW_SOURCE_POINT_FACE )
        {
            const s32 PointLightIndex = Source.PointLightIndex;
            if( ( PointLightIndex < 0 ) || ( PointLightIndex >= MAX_SHADOW_LIGHTS ) )
                continue;

            if( Score > BestPointScores[PointLightIndex] )
            {
                BestPointScores [PointLightIndex] = Score;
                BestPointSources[PointLightIndex] = i;
            }
        }
        else
        {
            InsertCollectedSourceByScore( i,
                                          Score,
                                          MaxSourceCount,
                                          m_CollectedSources,
                                          CollectedScores,
                                          m_NCollectedSources );
        }
    }

    for( s32 iPointLight = 0; iPointLight < MAX_SHADOW_LIGHTS; iPointLight++ )
    {
        if( BestPointSources[iPointLight] < 0 )
            continue;

        InsertCollectedSourceByScore( BestPointSources[iPointLight],
                                      BestPointScores[iPointLight],
                                      MaxSourceCount,
                                      m_CollectedSources,
                                      CollectedScores,
                                      m_NCollectedSources );
    }

    return m_NCollectedSources;
}

//=========================================================================

void shadow_map_mgr::GetCollectedSource( s32      CollectedIndex,
                                         s32&     SourceIndex,
                                         s32&     Type,
                                         matrix4& ShadowMatrix,
                                         vector4& LightPosRadius,
                                         f32&     Falloff,
                                         f32&     NearZ,
                                         f32&     FarZ,
                                         s32&     PointLightIndex,
                                         s32&     FaceIndex ) const
{
    ASSERT( ( CollectedIndex >= 0 ) && ( CollectedIndex < m_NCollectedSources ) );
    ASSERT( !m_AtlasLayoutDirty );
    SourceIndex      = m_CollectedSources[CollectedIndex];
    const shadow_source& Source = m_Sources[SourceIndex];
    Type            = Source.Type;
    ShadowMatrix    = Source.WorldToAtlas;
    LightPosRadius  = Source.LightPosRadius;
    Falloff         = Source.LightFalloff;
    NearZ           = Source.NearZ;
    FarZ            = Source.FarZ;
    PointLightIndex = Source.PointLightIndex;
    FaceIndex       = Source.FaceIndex;
}

//=========================================================================

const shadow_map_mgr::shadow_source& shadow_map_mgr::GetSource( s32 SourceIndex ) const
{
    ASSERT( ( SourceIndex >= 0 ) && ( SourceIndex < m_SourceCount ) );
    ASSERT( !m_AtlasLayoutDirty );
    return m_Sources[SourceIndex];
}

//=========================================================================

s32 shadow_map_mgr::GetSourceCount( void ) const
{
    return m_SourceCount;
}

//=========================================================================

s32 shadow_map_mgr::GetPointLightCount( void ) const
{
    return m_PointLightCount;
}

//=========================================================================

s32 shadow_map_mgr::GetSpotSourceCount( void ) const
{
    return m_SpotSourceCount;
}

//=========================================================================

xbool shadow_map_mgr::HasActiveSources( void ) const
{
    return ( m_SourceCount > 0 );
}

//=========================================================================

xbool shadow_map_mgr::CanReceiveShadowMap( material_type Type,
                                           u16           MaterialFlags ) const
{
    if( IsAlphaMaterial( Type ) &&
        !( MaterialFlags & geom::material::FLAG_FORCE_ZFILL ) )
    {
        return FALSE;
    }

    if( MaterialFlags & geom::material::FLAG_IS_PUNCH_THRU )
        return FALSE;

    return TRUE;
}

//=========================================================================

xbool shadow_map_mgr::CanReceiveShadowMap( const geom::material& Mat ) const
{
    return CanReceiveShadowMap( (material_type)Mat.Type, Mat.Flags );
}

//=========================================================================

xbool shadow_map_mgr::CanReceiveShadowMap( const material& Mat ) const
{
    return CanReceiveShadowMap( (material_type)Mat.m_Type, Mat.m_Flags );
}

//=========================================================================

void shadow_map_mgr::CreateShadowMap( object* const* ppCasterCandidates,
                                      s32            NCasterCandidates )
{
    ClearSources();

    if( !ppCasterCandidates || ( NCasterCandidates <= 0 ) )
        return;

    object*      ppCasters[render::MAX_SHADOW_CASTERS];
    u64          CasterMasks[render::MAX_SHADOW_CASTERS];
    s32          NCasters               = 0;
    shadow_light ShadowLightCandidates[light_mgr::MAX_DYNAMIC_LIGHTS];
    shadow_light ShadowLights[light_mgr::MAX_DYNAMIC_LIGHTS];
    s32          NCandidateShadowLights = 0;
    player*      pActivePlayer          = SMP_UTIL_GetActivePlayer();
    xarray<s32>  RelevantCasterIndices;
    RelevantCasterIndices.SetGrowAmount( MAX( 128, NCasterCandidates ) );

    for( s32 iLight = 0; iLight < g_LightMgr.GetNDynamicLights(); iLight++ )
    {
        shadow_light ShadowLight;
        if( !BuildShadowLight( ShadowLight,
                               iLight,
                               ppCasterCandidates,
                               NCasterCandidates,
                               pActivePlayer,
                               RelevantCasterIndices ) )
        {
            continue;
        }

        InsertShadowLight( ShadowLightCandidates,
                           NCandidateShadowLights,
                           ShadowLight );
    }

    const s32 NShadowLights = SelectShadowLights( ShadowLightCandidates,
                                                  NCandidateShadowLights,
                                                  ShadowLights );

    if( NShadowLights <= 0 )
        return;

    NCasters = CollectShadowCastersForLights( ppCasterCandidates,
                                              RelevantCasterIndices.GetPtr(),
                                              ShadowLights,
                                              NShadowLights,
                                              ppCasters,
                                              CasterMasks );

    if( NCasters <= 0 )
        return;

    if( eng_Begin( "Shadow Map" ) )
    {
        render::BeginShadowCreation();
        AddShadowSources( ShadowLights, NShadowLights );
        FinalizeSources();

        for( s32 i = 0; i < NCasters; i++ )
            ppCasters[i]->OnRenderShadowCast( CasterMasks[i] );

        render::EndShadowCreation();
        eng_End();
    }
}
