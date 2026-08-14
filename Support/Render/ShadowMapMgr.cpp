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

#include "../Objects/object.hpp"
#include "../Objects/Player/Player.hpp"
#include "../Obj_mgr/obj_mgr.hpp"
#include "../PlaySurfaceMgr/PlaySurfaceMgr.hpp"
#include "../ZoneMgr/ZoneMgr.hpp"

#include "LightMgr.hpp"
#include "Render.hpp"

#include "Entropy.hpp"

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

ShadowMapMgr g_ShadowMapMgr;

//=========================================================================
//  FILE-LOCAL TYPES
//=========================================================================

namespace
{
struct ShadowPointFace
{
    radian Pitch;
    radian Yaw;
};

//---------------------------------------------------------------------

struct ShadowLight
{
    s32     DynamicLightIndex;
    s32     Shape;
    vector3 Pos;
    vector3 Direction;
    f32     Radius;
    f32     Falloff;
    radian  FOV;
    s32     ShadowMapResolution;
    f32     PointFaceCoverageCos;
    s32     ShadowPriority;
    s32     SourceCount;
    s32     RelevantCasterStart;
    s32     RelevantCasterCount;
    f32     Score;
};

//---------------------------------------------------------------------

ShadowPointFace const kPointShadowFaces[POINT_SHADOW_FACE_COUNT] = {
    { -R_90, R_0 }, { R_90, R_0 }, { R_0, R_0 }, { R_0, R_180 }, { R_0, R_90 }, { R_0, -R_90 },
};

//---------------------------------------------------------------------

radian const kPointShadowFOV = R_90;
radian const kMinSpotShadowFOV = DEG_TO_RAD( 1.0f );
s32 const    kMinShadowPriority = 0;
s32 const    kMaxShadowPriority = 4;
f32 const    kPointShadowProjectionOverlapTexels = 4.0f;
f32 const    kShadowCasterScoreWeight = 4096.0f;
f32 const    kPlayerShadowScoreBonus = 8192.0f;
f32 const    kShadowRadiusScorePenalty = 0.25f;
f32 const    kShadowSourceTypeSpot = 0.0f;
f32 const    kShadowSourceTypePointFace = 1.0f;

//---------------------------------------------------------------------

struct ShadowConeTest
{
    vector3 Apex;
    vector3 Axis;
    f32     CosOuter;
    f32     SinOuter;
    f32     TanOuter;
    f32     Length;
};

//---------------------------------------------------------------------

struct DynamicShadowLight
{
    vector3 Pos;
    vector3 Direction;
    xcolor  Color;
    f32     Radius;
    f32     Falloff;
    f32     OuterAngle;
    s32     Shape;
    s32     ShadowMapResolution;
};

//---------------------------------------------------------------------

struct ShadowCasterRecord
{
    enum CasterType
    {
        CASTER_OBJECT,
        CASTER_PLAY_SURFACE,
    };

    CasterType                Type;
    object*                   pObject;
    playsurface_mgr::surface* pPlaySurface;
    vector3                   WorldBBoxCenter;
    f32                       WorldBBoxRadius;
};

//---------------------------------------------------------------------

struct ShadowCasterMaskRecord
{
    ShadowCasterRecord Caster;
    u64                SourceMask;
};

//---------------------------------------------------------------------

f32 ComputeBBoxBoundingSphereRadius( bbox const& worldBBox );

//---------------------------------------------------------------------

void SetShadowCasterBounds( ShadowCasterRecord& caster, bbox const& worldBBox )
{
    caster.WorldBBoxCenter = worldBBox.GetCenter();
    caster.WorldBBoxRadius = ComputeBBoxBoundingSphereRadius( worldBBox );
}

//---------------------------------------------------------------------

ShadowCasterRecord MakeObjectShadowCaster( object* pCaster, bbox const& worldBBox )
{
    ShadowCasterRecord caster;
    caster.Type = ShadowCasterRecord::CASTER_OBJECT;
    caster.pObject = pCaster;
    caster.pPlaySurface = NULL;
    SetShadowCasterBounds( caster, worldBBox );
    return caster;
}

//---------------------------------------------------------------------

ShadowCasterRecord MakePlaySurfaceShadowCaster( playsurface_mgr::surface* pSurface, bbox const& worldBBox )
{
    ShadowCasterRecord caster;
    caster.Type = ShadowCasterRecord::CASTER_PLAY_SURFACE;
    caster.pObject = NULL;
    caster.pPlaySurface = pSurface;
    SetShadowCasterBounds( caster, worldBBox );
    return caster;
}

//---------------------------------------------------------------------

xbool SameShadowCaster( ShadowCasterRecord const& a, ShadowCasterRecord const& b )
{
    if ( a.Type != b.Type )
    {
        return FALSE;
    }

    if ( a.Type == ShadowCasterRecord::CASTER_OBJECT )
    {
        return ( a.pObject == b.pObject );
    }

    return ( a.pPlaySurface == b.pPlaySurface );
}

//---------------------------------------------------------------------

uaddr GetShadowCasterSortKey( ShadowCasterRecord const& caster )
{
    if ( caster.Type == ShadowCasterRecord::CASTER_OBJECT )
    {
        return reinterpret_cast<uaddr>( caster.pObject );
    }

    return reinterpret_cast<uaddr>( caster.pPlaySurface );
}

//---------------------------------------------------------------------

s32 ShadowCasterMaskCompareFn( void const* pA, void const* pB )
{
    ShadowCasterMaskRecord const& a = *( (ShadowCasterMaskRecord const*)pA );
    ShadowCasterMaskRecord const& b = *( (ShadowCasterMaskRecord const*)pB );

    if ( a.Caster.Type != b.Caster.Type )
    {
        return ( a.Caster.Type < b.Caster.Type ) ? -1 : 1;
    }

    uaddr const aKey = GetShadowCasterSortKey( a.Caster );
    uaddr const bKey = GetShadowCasterSortKey( b.Caster );

    if ( aKey < bKey )
    {
        return -1;
    }
    if ( aKey > bKey )
    {
        return 1;
    }

    return 0;
}

//---------------------------------------------------------------------

radian GetShadowLightFOV( DynamicShadowLight const& dynamicLight )
{
    if ( dynamicLight.Shape == light_mgr::LIGHT_SHAPE_OMNI )
    {
        return kPointShadowFOV;
    }

    return MAX( kMinSpotShadowFOV, DEG_TO_RAD( dynamicLight.OuterAngle ) );
}

//---------------------------------------------------------------------

s32 GetShadowLightSourceCount( s32 lightShape )
{
    if ( lightShape == light_mgr::LIGHT_SHAPE_OMNI )
    {
        return POINT_SHADOW_FACE_COUNT;
    }

    return 1;
}

//---------------------------------------------------------------------

s32 ClampShadowMapResolution( s32 shadowMapResolution )
{
    if ( shadowMapResolution >= 4096 )
    {
        return 4096;
    }
    if ( shadowMapResolution >= 2048 )
    {
        return 2048;
    }
    if ( shadowMapResolution >= 1024 )
    {
        return 1024;
    }
    if ( shadowMapResolution >= 512 )
    {
        return 512;
    }
    return 256;
}

//---------------------------------------------------------------------

s32 ClampShadowPriority( s32 shadowPriority )
{
    if ( shadowPriority < kMinShadowPriority )
    {
        return kMinShadowPriority;
    }
    if ( shadowPriority > kMaxShadowPriority )
    {
        return kMaxShadowPriority;
    }
    return shadowPriority;
}

//---------------------------------------------------------------------

f32 ComputePointFaceCoverageCos( s32 faceResolutionTexels )
{
    // Takes an actual texel count now, not a resolution to bucket-clamp.
    f32 const coverageScale = static_cast<f32>( faceResolutionTexels ) /
                              ( static_cast<f32>( faceResolutionTexels ) + kPointShadowProjectionOverlapTexels );
    f32 const safeScale = MAX( coverageScale, 0.0001f );
    f32 const tanHalfFov = x_tan( kPointShadowFOV * 0.5f ) / safeScale;
    f32 const cornerLengthSq = 1.0f + ( 2.0f * tanHalfFov * tanHalfFov );
    return 1.0f / x_sqrt( cornerLengthSq );
}

//---------------------------------------------------------------------

f32 ComputePointShadowNearZ( f32 lightRadius )
{
    return MAX( 0.5f, lightRadius * 0.0025f );
}

//---------------------------------------------------------------------

f32 ComputePointShadowReceiveNearZ( f32 lightRadius )
{
    return MAX( 0.05f, lightRadius * 0.00025f );
}

//---------------------------------------------------------------------

f32 ComputeSpotShadowReceiveNearZ( f32 lightRadius )
{
    return MAX( 0.05f, lightRadius * 0.0005f );
}

//---------------------------------------------------------------------

s32 ReduceShadowMapResolution( s32 shadowMapResolution )
{
    if ( shadowMapResolution > 2048 )
    {
        return 2048;
    }
    if ( shadowMapResolution > 1024 )
    {
        return 1024;
    }
    if ( shadowMapResolution > 512 )
    {
        return 512;
    }
    if ( shadowMapResolution > 256 )
    {
        return 256;
    }
    return 256;
}

//---------------------------------------------------------------------

struct ShadowAtlasEntry
{
    s32 SourceIndex;
    s32 TileSize;
    s32 ShadowPriority;
    f32 ShadowScore;
    s32 AtlasTileX;
    s32 AtlasTileY;
};

//---------------------------------------------------------------------

xbool IsPowerOfTwo( s32 value )
{
    return ( value > 0 ) && ( ( value & ( value - 1 ) ) == 0 );
}

//---------------------------------------------------------------------

struct ShadowAtlasFreeNode
{
    s32 AtlasTileX;
    s32 AtlasTileY;
    s32 TileSize;
};

//---------------------------------------------------------------------

s32 GetShadowAtlasSplitCount( s32 atlasSize, s32 minTileSize )
{
    ASSERT( atlasSize > 0 );
    ASSERT( minTileSize > 0 );
    ASSERT( IsPowerOfTwo( atlasSize ) );
    ASSERT( IsPowerOfTwo( minTileSize ) );
    ASSERT( atlasSize >= minTileSize );

    s32 splitCount = 0;
    while ( atlasSize > minTileSize )
    {
        atlasSize /= 2;
        splitCount++;
    }

    return splitCount;
}

//---------------------------------------------------------------------

s32 GetMaxShadowAtlasFreeNodeCount( s32 atlasSize, s32 nEntries )
{
    s32 const splitCount = GetShadowAtlasSplitCount( atlasSize, 256 );
    return 1 + ( MAX( nEntries, 0 ) * splitCount * 3 );
}

//---------------------------------------------------------------------

void AppendShadowAtlasFreeNode( xarray<ShadowAtlasFreeNode>& freeNodes, s32 atlasTileX, s32 atlasTileY, s32 tileSize )
{
    ShadowAtlasFreeNode& node = freeNodes.Append();
    node.AtlasTileX = atlasTileX;
    node.AtlasTileY = atlasTileY;
    node.TileSize = tileSize;
}

//---------------------------------------------------------------------

void SortShadowAtlasEntries( ShadowAtlasEntry* pEntries, s32 nEntries )
{
    for ( s32 i = 1; i < nEntries; i++ )
    {
        ShadowAtlasEntry const entry = pEntries[i];
        s32                    j = i - 1;
        while ( j >= 0 )
        {
            if ( pEntries[j].TileSize > entry.TileSize )
            {
                break;
            }

            if ( ( pEntries[j].TileSize == entry.TileSize ) && ( pEntries[j].ShadowPriority > entry.ShadowPriority ) )
            {
                break;
            }

            if ( ( pEntries[j].TileSize == entry.TileSize ) && ( pEntries[j].ShadowPriority == entry.ShadowPriority ) &&
                 ( pEntries[j].ShadowScore > entry.ShadowScore ) )
            {
                break;
            }

            if ( ( pEntries[j].TileSize == entry.TileSize ) && ( pEntries[j].ShadowPriority == entry.ShadowPriority ) &&
                 ( pEntries[j].ShadowScore == entry.ShadowScore ) && ( pEntries[j].SourceIndex < entry.SourceIndex ) )
            {
                break;
            }

            pEntries[j + 1] = pEntries[j];
            j--;
        }

        pEntries[j + 1] = entry;
    }
}

//---------------------------------------------------------------------

xbool TryPackShadowAtlasEntries( ShadowAtlasEntry* pEntries, s32 nEntries, s32 atlasSize,
                                 xarray<ShadowAtlasFreeNode>& freeNodes )
{
    SortShadowAtlasEntries( pEntries, nEntries );

    ASSERT( IsPowerOfTwo( atlasSize ) );
    if ( !IsPowerOfTwo( atlasSize ) )
    {
        return FALSE;
    }

    freeNodes.SetCount( 0 );
    s32 const requiredCapacity = GetMaxShadowAtlasFreeNodeCount( atlasSize, nEntries );
    if ( freeNodes.GetCapacity() < requiredCapacity )
    {
        freeNodes.SetCapacity( requiredCapacity );
    }
    AppendShadowAtlasFreeNode( freeNodes, 0, 0, atlasSize );

    for ( s32 i = 0; i < nEntries; i++ )
    {
        s32 const tileSize = pEntries[i].TileSize;
        ASSERT( IsPowerOfTwo( tileSize ) );
        if ( !IsPowerOfTwo( tileSize ) )
        {
            return FALSE;
        }
        if ( ( tileSize <= 0 ) || ( tileSize > atlasSize ) )
        {
            return FALSE;
        }

        s32 iBestNode = -1;
        for ( s32 iNode = 0; iNode < freeNodes.GetCount(); iNode++ )
        {
            ShadowAtlasFreeNode const& node = freeNodes[iNode];
            if ( node.TileSize < tileSize )
            {
                continue;
            }

            if ( iBestNode < 0 )
            {
                iBestNode = iNode;
                continue;
            }

            ShadowAtlasFreeNode const& bestNode = freeNodes[iBestNode];
            if ( node.TileSize < bestNode.TileSize )
            {
                iBestNode = iNode;
                continue;
            }

            if ( ( node.TileSize == bestNode.TileSize ) && ( node.AtlasTileY < bestNode.AtlasTileY ) )
            {
                iBestNode = iNode;
                continue;
            }

            if ( ( node.TileSize == bestNode.TileSize ) && ( node.AtlasTileY == bestNode.AtlasTileY ) &&
                 ( node.AtlasTileX < bestNode.AtlasTileX ) )
            {
                iBestNode = iNode;
            }
        }

        if ( iBestNode < 0 )
        {
            return FALSE;
        }

        ShadowAtlasFreeNode node = freeNodes[iBestNode];
        freeNodes[iBestNode] = freeNodes[freeNodes.GetCount() - 1];
        freeNodes.SetCount( freeNodes.GetCount() - 1 );

        while ( node.TileSize > tileSize )
        {
            s32 const childTileSize = node.TileSize / 2;

            if ( ( childTileSize <= 0 ) || ( childTileSize < tileSize ) )
            {
                return FALSE;
            }

            AppendShadowAtlasFreeNode( freeNodes, node.AtlasTileX + childTileSize, node.AtlasTileY, childTileSize );
            AppendShadowAtlasFreeNode( freeNodes, node.AtlasTileX, node.AtlasTileY + childTileSize, childTileSize );
            AppendShadowAtlasFreeNode( freeNodes, node.AtlasTileX + childTileSize, node.AtlasTileY + childTileSize,
                                       childTileSize );

            node.TileSize = childTileSize;
        }

        pEntries[i].AtlasTileX = node.AtlasTileX;
        pEntries[i].AtlasTileY = node.AtlasTileY;
    }

    return TRUE;
}

//---------------------------------------------------------------------

s32 GetShadowAtlasSizeForEntries( ShadowAtlasEntry* pEntries, s32 nEntries, xarray<ShadowAtlasFreeNode>& freeNodes )
{
    for ( s32 atlasSize = 256; atlasSize <= MAX_SHADOW_ATLAS_SIZE; atlasSize *= 2 )
    {
        if ( TryPackShadowAtlasEntries( pEntries, nEntries, atlasSize, freeNodes ) )
        {
            return atlasSize;
        }
    }

    return 0;
}

//---------------------------------------------------------------------

xbool SortsBeforeShadowLight( ShadowLight const& a, ShadowLight const& b )
{
    if ( a.ShadowPriority != b.ShadowPriority )
    {
        return ( a.ShadowPriority > b.ShadowPriority );
    }

    if ( a.Score != b.Score )
    {
        return ( a.Score > b.Score );
    }

    if ( a.ShadowMapResolution != b.ShadowMapResolution )
    {
        return ( a.ShadowMapResolution > b.ShadowMapResolution );
    }

    if ( a.SourceCount != b.SourceCount )
    {
        return ( a.SourceCount < b.SourceCount );
    }

    return FALSE;
}

//---------------------------------------------------------------------

f32 ComputeBBoxBoundingSphereRadius( bbox const& worldBBox )
{
    vector3 const extents = ( worldBBox.Max - worldBBox.Min ) * 0.5f;
    return x_sqrt( extents.Dot( extents ) );
}

//---------------------------------------------------------------------

void SetupShadowConeTest( ShadowConeTest& cone, vector3 const& apex, vector3 const& direction, f32 cosOuter,
                          f32 length )
{
    cone.Apex = apex;
    cone.Axis = direction;
    if ( !cone.Axis.SafeNormalize() )
    {
        cone.Axis.Set( 0.0f, 0.0f, 1.0f );
    }

    cone.CosOuter = MAX( cosOuter, 0.0001f );
    cone.SinOuter = x_sqrt( MAX( 1.0f - ( cone.CosOuter * cone.CosOuter ), 0.0f ) );
    cone.TanOuter = cone.SinOuter / cone.CosOuter;
    cone.Length = length;
}

//---------------------------------------------------------------------

xbool SphereIntersectsFiniteCone( vector3 const& sphereCenter, f32 sphereRadius, ShadowConeTest const& cone )
{
    vector3 const toCenter = sphereCenter - cone.Apex;
    f32 const     centerDistSq = toCenter.Dot( toCenter );
    f32 const     axialDistance = cone.Axis.Dot( toCenter );
    f32 const     sphereRadiusSq = sphereRadius * sphereRadius;

    if ( axialDistance < -sphereRadius )
    {
        return FALSE;
    }

    if ( axialDistance > ( cone.Length + sphereRadius ) )
    {
        return FALSE;
    }

    if ( axialDistance <= 0.0f )
    {
        return ( centerDistSq <= sphereRadiusSq );
    }

    f32 const radialDistanceSq = MAX( centerDistSq - ( axialDistance * axialDistance ), 0.0f );
    f32 const radialDistance = x_sqrt( radialDistanceSq );

    if ( axialDistance < cone.Length )
    {
        f32 const sideDistance = ( radialDistance * cone.CosOuter ) - ( axialDistance * cone.SinOuter );
        if ( sideDistance <= sphereRadius )
        {
            return TRUE;
        }
    }

    f32 const coneCapRadius = cone.Length * cone.TanOuter;
    f32 const capAxialDelta = MAX( axialDistance - cone.Length, 0.0f );
    f32 const capRadialDelta = MAX( radialDistance - coneCapRadius, 0.0f );
    return ( ( capAxialDelta * capAxialDelta ) + ( capRadialDelta * capRadialDelta ) ) <= sphereRadiusSq;
}

//---------------------------------------------------------------------

xbool SourceConeAffectsBBox( ShadowConeTest const& cone, bbox const& worldBBox )
{
    return SphereIntersectsFiniteCone( worldBBox.GetCenter(), ComputeBBoxBoundingSphereRadius( worldBBox ), cone );
}

//---------------------------------------------------------------------

xbool ShadowCasterBoundsAreRelevant( bbox const& casterBBox, u8 zone1, u8 zone2, vector3 const& lightPos,
                                     f32 lightRadius, ShadowConeTest const* pConeTest = NULL )
{
    // Deep and raw
    // if( !g_ZoneMgr.IsBBoxVisible( CasterBBox,
    //                              Zone1,
    //                              Zone2 ) )
    //{
    //    return FALSE;
    //}

    if ( !g_ZoneMgr.IsZoneVisible( zone1 ) && !g_ZoneMgr.IsZoneVisible( zone2 ) )
    {
        return FALSE;
    }

    if ( !casterBBox.Intersect( lightPos, lightRadius ) )
    {
        return FALSE;
    }

    if ( pConeTest )
    {
        return SourceConeAffectsBBox( *pConeTest, casterBBox );
    }

    return TRUE;
}

//---------------------------------------------------------------------

f32 GetShadowLightScore( DynamicShadowLight const& dynamicLight, s32 localCasterCount, player* pActivePlayer )
{
    f32 lightScore = static_cast<f32>( dynamicLight.Color.R + dynamicLight.Color.G + dynamicLight.Color.B );
    lightScore += static_cast<f32>( localCasterCount ) * kShadowCasterScoreWeight;
    lightScore -= dynamicLight.Radius * kShadowRadiusScorePenalty;

    if ( pActivePlayer && pActivePlayer->GetBBox().Intersect( dynamicLight.Pos, dynamicLight.Radius ) )
    {
        lightScore += kPlayerShadowScoreBonus;
    }

    return lightScore;
}

//---------------------------------------------------------------------

xbool BuildShadowLight( ShadowLight& shadowLight, s32 lightIndex, player* pActivePlayer,
                        xarray<ShadowCasterRecord>& relevantCasters )
{
    DynamicShadowLight dynamicLight;
    xbool              castShadows;
    f32                unusedInnerRadius;
    f32                unusedInnerAngle;
    s32                shadowMapResolution;
    s32                shadowPriority;

    g_LightMgr.GetDynamicLight( lightIndex, dynamicLight.Pos, dynamicLight.Radius, dynamicLight.Color,
                                dynamicLight.Shape, castShadows, unusedInnerRadius, dynamicLight.Direction,
                                dynamicLight.Falloff, unusedInnerAngle, dynamicLight.OuterAngle, shadowMapResolution,
                                shadowPriority );

    static_cast<void>( unusedInnerRadius );
    static_cast<void>( unusedInnerAngle );

    if ( !castShadows || ( dynamicLight.Radius <= 0.0f ) )
    {
        return FALSE;
    }

    dynamicLight.ShadowMapResolution = ClampShadowMapResolution( shadowMapResolution );
    shadowLight.DynamicLightIndex = lightIndex;

    ShadowConeTest        lightConeTest;
    ShadowConeTest const* pLightConeTest = NULL;
    if ( dynamicLight.Shape == light_mgr::LIGHT_SHAPE_SPOT )
    {
        f32 const lightConeOuterCos = x_cos( DEG_TO_RAD( dynamicLight.OuterAngle ) * 0.5f );
        if ( lightConeOuterCos > 0.0f )
        {
            SetupShadowConeTest( lightConeTest, dynamicLight.Pos, dynamicLight.Direction, lightConeOuterCos,
                                 dynamicLight.Radius );
            pLightConeTest = &lightConeTest;
        }
    }

    s32 const relevantCasterStart = relevantCasters.GetCount();
    s32       localCasterCount = 0;
    bbox      lightBBox = bbox( dynamicLight.Pos, dynamicLight.Radius );

    // TODO: GS: Dont forget unlock me, after patching ALL level files.
    // g_ObjMgr.SelectBBox( object::ATTR_CAST_SHADOWS,
    //                     LightBBox,
    //                     object::TYPE_ALL_TYPES );
    g_ObjMgr.SelectBBox( object::ATTR_ALL, lightBBox, object::TYPE_ALL_TYPES );

    for ( slot_id slotId = g_ObjMgr.StartLoop(); slotId != SLOT_NULL; slotId = g_ObjMgr.GetNextResult( slotId ) )
    {
        object* pCandidate = g_ObjMgr.GetObjectBySlot( slotId );
        if ( !pCandidate )
        {
            continue;
        }

#ifdef X_EDITOR
        if ( pCandidate->IsHidden() )
        {
            continue;
        }
#endif

        if ( pCandidate->GetType() == object::TYPE_PLAY_SURFACE )
        {
            continue;
        }

        bbox const& candidateBBox = pCandidate->GetBBox();
        if ( !ShadowCasterBoundsAreRelevant( candidateBBox, static_cast<u8>( pCandidate->GetZone1() ),
                                             static_cast<u8>( pCandidate->GetZone2() ), dynamicLight.Pos,
                                             dynamicLight.Radius, pLightConeTest ) )
        {
            continue;
        }

        relevantCasters.Append( MakeObjectShadowCaster( pCandidate, candidateBBox ) );
        localCasterCount++;
    }
    g_ObjMgr.EndLoop();

    // TODO: GS: Dont forget unlock me, after patching ALL level files.
    // g_PlaySurfaceMgr.CollectSurfaces( LightBBox,
    //                                  object::ATTR_CAST_SHADOWS,
    //                                  0 );
    g_PlaySurfaceMgr.CollectSurfaces( lightBBox, object::ATTR_ALL, 0 );

    playsurface_mgr::surface* pSurface = g_PlaySurfaceMgr.GetNextSurface();
    while ( pSurface )
    {
        if ( !pSurface->RenderInst.IsNull() &&
             ShadowCasterBoundsAreRelevant( pSurface->WorldBBox, static_cast<u8>( pSurface->ZoneInfo & 0xff ),
                                            static_cast<u8>( pSurface->ZoneInfo >> 8 ), dynamicLight.Pos,
                                            dynamicLight.Radius, pLightConeTest ) )
        {
            relevantCasters.Append( MakePlaySurfaceShadowCaster( pSurface, pSurface->WorldBBox ) );
            localCasterCount++;
        }

        pSurface = g_PlaySurfaceMgr.GetNextSurface();
    }

    if ( localCasterCount <= 0 )
    {
        return FALSE;
    }

    shadowLight.Shape = dynamicLight.Shape;
    shadowLight.Pos = dynamicLight.Pos;
    shadowLight.Direction = dynamicLight.Direction;
    shadowLight.Radius = dynamicLight.Radius;
    shadowLight.Falloff = dynamicLight.Falloff;
    shadowLight.FOV = GetShadowLightFOV( dynamicLight );
    shadowLight.ShadowMapResolution = dynamicLight.ShadowMapResolution;
    shadowLight.PointFaceCoverageCos = ( dynamicLight.Shape == light_mgr::LIGHT_SHAPE_OMNI )
                                           ? ComputePointFaceCoverageCos( dynamicLight.ShadowMapResolution )
                                           : 0.0f;
    shadowLight.ShadowPriority = ClampShadowPriority( shadowPriority );
    shadowLight.SourceCount = GetShadowLightSourceCount( dynamicLight.Shape );
    shadowLight.RelevantCasterStart = relevantCasterStart;
    shadowLight.RelevantCasterCount = localCasterCount;
    shadowLight.Score = GetShadowLightScore( dynamicLight, localCasterCount, pActivePlayer );
    return TRUE;
}

//---------------------------------------------------------------------

vector3 const& GetPointShadowFaceDirection( s32 iFace )
{
    ASSERT( iFace >= 0 );
    ASSERT( iFace < POINT_SHADOW_FACE_COUNT );

    static xbool   sInitialized = FALSE;
    static vector3 sFaceDirections[POINT_SHADOW_FACE_COUNT];

    if ( !sInitialized )
    {
        for ( s32 i = 0; i < POINT_SHADOW_FACE_COUNT; i++ )
        {
            matrix4 faceL2W;
            faceL2W.Setup( vector3( 1.0f, 1.0f, 1.0f ),
                           radian3( kPointShadowFaces[i].Pitch, kPointShadowFaces[i].Yaw, R_0 ),
                           vector3( 0.0f, 0.0f, 0.0f ) );

            sFaceDirections[i] = faceL2W.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
            if ( !sFaceDirections[i].SafeNormalize() )
            {
                sFaceDirections[i].Set( 0.0f, 0.0f, 1.0f );
            }
        }

        sInitialized = TRUE;
    }

    return sFaceDirections[iFace];
}

//---------------------------------------------------------------------

xbool PointShadowFaceAffectsCaster( ShadowConeTest const& cone, ShadowCasterRecord const& caster )
{
    return SphereIntersectsFiniteCone( caster.WorldBBoxCenter, caster.WorldBBoxRadius, cone );
}

//---------------------------------------------------------------------

u64 BuildFullShadowLightSourceMask( ShadowLight const& shadowLight, s32 sourceBase )
{
    u64 sourceMask = 0;

    for ( s32 iSource = 0; iSource < shadowLight.SourceCount; iSource++ )
    {
        sourceMask |= ( u64{ 1 } << ( sourceBase + iSource ) );
    }

    return sourceMask;
}

//---------------------------------------------------------------------

u64 BuildShadowCasterSourceMask( ShadowLight const& shadowLight, s32 sourceBase, ShadowCasterRecord const& caster,
                                 u64 lightSourceMask, ShadowConeTest const* pPointFaceCones )
{
    if ( shadowLight.SourceCount <= 0 )
    {
        return 0;
    }

    if ( shadowLight.Shape != light_mgr::LIGHT_SHAPE_OMNI )
    {
        return lightSourceMask;
    }

    s32 const nFaces =
        ( shadowLight.SourceCount < POINT_SHADOW_FACE_COUNT ) ? shadowLight.SourceCount : POINT_SHADOW_FACE_COUNT;
    u64 sourceMask = 0;

    ASSERT( pPointFaceCones );
    for ( s32 iFace = 0; iFace < nFaces; iFace++ )
    {
        if ( PointShadowFaceAffectsCaster( pPointFaceCones[iFace], caster ) )
        {
            sourceMask |= ( u64{ 1 } << ( sourceBase + iFace ) );
        }
    }

    return sourceMask;
}

//---------------------------------------------------------------------

void AddShadowCastersInLight( xarray<ShadowCasterRecord> const& relevantCasters, ShadowLight const& shadowLight,
                              xarray<ShadowCasterMaskRecord>& pendingCasterMasks, s32 sourceBase )
{
    s32 const             endRelevantCaster = shadowLight.RelevantCasterStart + shadowLight.RelevantCasterCount;
    u64 const             lightSourceMask = BuildFullShadowLightSourceMask( shadowLight, sourceBase );
    ShadowConeTest        pointFaceCones[POINT_SHADOW_FACE_COUNT];
    ShadowConeTest const* pPointFaceCones = NULL;

    if ( shadowLight.Shape == light_mgr::LIGHT_SHAPE_OMNI )
    {
        s32 const nFaces =
            ( shadowLight.SourceCount < POINT_SHADOW_FACE_COUNT ) ? shadowLight.SourceCount : POINT_SHADOW_FACE_COUNT;

        for ( s32 iFace = 0; iFace < nFaces; iFace++ )
        {
            SetupShadowConeTest( pointFaceCones[iFace], shadowLight.Pos, GetPointShadowFaceDirection( iFace ),
                                 shadowLight.PointFaceCoverageCos, shadowLight.Radius );
        }

        pPointFaceCones = pointFaceCones;
    }

    for ( s32 iRelevantCaster = shadowLight.RelevantCasterStart; iRelevantCaster < endRelevantCaster;
          iRelevantCaster++ )
    {
        ShadowCasterRecord const& caster = relevantCasters[iRelevantCaster];
        u64 const                 sourceMask =
            BuildShadowCasterSourceMask( shadowLight, sourceBase, caster, lightSourceMask, pPointFaceCones );

        if ( sourceMask == 0 )
        {
            continue;
        }

        ShadowCasterMaskRecord& record = pendingCasterMasks.Append();
        record.Caster = caster;
        record.SourceMask = sourceMask;
    }
}

//---------------------------------------------------------------------

void MergeShadowCasterMasks( xarray<ShadowCasterMaskRecord>& pendingCasterMasks, xarray<ShadowCasterRecord>& ppCasters,
                             xarray<u64>& casterMasks )
{
    if ( pendingCasterMasks.GetCount() <= 0 )
    {
        return;
    }

    x_qsort( pendingCasterMasks.GetPtr(), pendingCasterMasks.GetCount(), sizeof( ShadowCasterMaskRecord ),
             ShadowCasterMaskCompareFn );

    for ( s32 i = 0; i < pendingCasterMasks.GetCount(); i++ )
    {
        ShadowCasterMaskRecord const& record = pendingCasterMasks[i];
        s32 const                     iLast = ppCasters.GetCount() - 1;

        if ( ( iLast < 0 ) || !SameShadowCaster( ppCasters[iLast], record.Caster ) )
        {
            ppCasters.Append( record.Caster );
            casterMasks.Append( record.SourceMask );
            continue;
        }

        casterMasks[iLast] |= record.SourceMask;
    }
}

//---------------------------------------------------------------------

void CollectShadowCastersForLights( xarray<ShadowCasterRecord> const& relevantCasters, ShadowLight const* pShadowLights,
                                    s32 nShadowLights, xarray<ShadowCasterRecord>& ppCasters, xarray<u64>& casterMasks,
                                    xarray<ShadowCasterMaskRecord>& pendingCasterMasks )
{
    s32 sourceBase = 0;
    pendingCasterMasks.SetCount( 0 );
    pendingCasterMasks.SetGrowAmount( MAX( 128, relevantCasters.GetCount() ) );

    for ( s32 iLight = 0; iLight < nShadowLights; iLight++ )
    {
        ShadowLight const& shadowLight = pShadowLights[iLight];

        AddShadowCastersInLight( relevantCasters, shadowLight, pendingCasterMasks, sourceBase );
        sourceBase += shadowLight.SourceCount;
    }

    MergeShadowCasterMasks( pendingCasterMasks, ppCasters, casterMasks );
}

void AddPointShadowSources( vector3 const& lightPos, f32 lightRadius, f32 lightFalloff, s32 shadowMapResolution,
                            s32 shadowPriority, f32 shadowScore, s32 dynamicLightIndex )
{
    for ( s32 iFace = 0; iFace < ARRAYSIZE( kPointShadowFaces ); iFace++ )
    {
        matrix4 faceL2W;
        faceL2W.Setup( vector3( 1.0f, 1.0f, 1.0f ),
                       radian3( kPointShadowFaces[iFace].Pitch, kPointShadowFaces[iFace].Yaw, R_0 ), lightPos );

        render::AddPointShadowMapSource( faceL2W, kPointShadowFOV, lightRadius, lightFalloff, shadowMapResolution,
                                         shadowPriority, shadowScore, dynamicLightIndex );
    }
}

//---------------------------------------------------------------------

void AddSpotShadowSource( vector3 const& lightPos, vector3 const& lightDirection, radian shadowFov, f32 lightRadius,
                          f32 lightFalloff, s32 shadowMapResolution, s32 shadowPriority, f32 shadowScore,
                          s32 dynamicLightIndex )
{
    matrix4 spotL2W;
    spotL2W.Setup( vector3( 1.0f, 1.0f, 1.0f ), radian3( lightDirection.GetPitch(), lightDirection.GetYaw(), R_0 ),
                   lightPos );

    render::AddSpotShadowMapSource( spotL2W, shadowFov, lightRadius, lightFalloff, shadowMapResolution, shadowPriority,
                                    shadowScore, dynamicLightIndex );
}

//---------------------------------------------------------------------

void AddShadowSources( ShadowLight const* pShadowLights, s32 nShadowLights )
{
    for ( s32 iLight = 0; iLight < nShadowLights; iLight++ )
    {
        ShadowLight const& shadowLight = pShadowLights[iLight];

        if ( shadowLight.Shape == light_mgr::LIGHT_SHAPE_OMNI )
        {
            AddPointShadowSources( shadowLight.Pos, shadowLight.Radius, shadowLight.Falloff,
                                   shadowLight.ShadowMapResolution, shadowLight.ShadowPriority, shadowLight.Score,
                                   shadowLight.DynamicLightIndex );
        }
        else
        {
            AddSpotShadowSource( shadowLight.Pos, shadowLight.Direction, shadowLight.FOV, shadowLight.Radius,
                                 shadowLight.Falloff, shadowLight.ShadowMapResolution, shadowLight.ShadowPriority,
                                 shadowLight.Score, shadowLight.DynamicLightIndex );
        }
    }
}

//---------------------------------------------------------------------

void InsertShadowLight( ShadowLight* pLights, s32& nLights, ShadowLight const& light )
{
    if ( nLights >= light_mgr::MAX_DYNAMIC_LIGHTS )
    {
        if ( !SortsBeforeShadowLight( light, pLights[nLights - 1] ) )
        {
            return;
        }

        nLights--;
    }

    s32 insertAt = nLights;
    while ( insertAt > 0 )
    {
        if ( !SortsBeforeShadowLight( light, pLights[insertAt - 1] ) )
        {
            break;
        }

        pLights[insertAt] = pLights[insertAt - 1];
        insertAt--;
    }

    pLights[insertAt] = light;
    nLights++;
}

//---------------------------------------------------------------------

s32 SelectShadowLights( ShadowLight const* pShadowLightCandidates, s32 nCandidateShadowLights,
                        ShadowLight* pShadowLights )
{
    s32 nPointShadowLights = 0;
    s32 nShadowLights = 0;
    s32 nShadowSources = 0;

    for ( s32 iLight = 0; iLight < nCandidateShadowLights; iLight++ )
    {
        ShadowLight const& candidate = pShadowLightCandidates[iLight];

        if ( ( candidate.Shape == light_mgr::LIGHT_SHAPE_OMNI ) && ( nPointShadowLights >= MAX_SHADOW_LIGHTS ) )
        {
            continue;
        }

        if ( ( nShadowSources + candidate.SourceCount ) > MAX_SHADOW_SOURCES )
        {
            continue;
        }

        pShadowLights[nShadowLights++] = candidate;
        nShadowSources += candidate.SourceCount;

        if ( candidate.Shape == light_mgr::LIGHT_SHAPE_OMNI )
        {
            nPointShadowLights++;
        }
    }

    return nShadowLights;
}

//---------------------------------------------------------------------

xbool AtlasFitsShadowLights( ShadowLight const* pShadowLights, s32 nShadowLights,
                             xarray<ShadowAtlasFreeNode>& freeNodes )
{
    ShadowAtlasEntry entries[MAX_SHADOW_SOURCES];
    s32              nEntries = 0;

    for ( s32 iLight = 0; iLight < nShadowLights; iLight++ )
    {
        ShadowLight const& shadowLight = pShadowLights[iLight];
        for ( s32 iSource = 0; iSource < shadowLight.SourceCount; iSource++ )
        {
            ASSERT( nEntries < MAX_SHADOW_SOURCES );
            entries[nEntries].SourceIndex = nEntries;
            entries[nEntries].TileSize = ClampShadowMapResolution( shadowLight.ShadowMapResolution );
            entries[nEntries].ShadowPriority = shadowLight.ShadowPriority;
            entries[nEntries].ShadowScore = shadowLight.Score;
            entries[nEntries].AtlasTileX = 0;
            entries[nEntries].AtlasTileY = 0;
            nEntries++;
        }
    }

    if ( nEntries <= 0 )
    {
        return TRUE;
    }

    return ( GetShadowAtlasSizeForEntries( entries, nEntries, freeNodes ) > 0 );
}

//---------------------------------------------------------------------

s32 FindShadowLightForResolutionDrop( ShadowLight const* pShadowLights, s32 nShadowLights, s32 shadowPriority )
{
    s32 iBestLight = -1;

    for ( s32 iLight = 0; iLight < nShadowLights; iLight++ )
    {
        ShadowLight const& shadowLight = pShadowLights[iLight];
        if ( shadowLight.ShadowPriority != shadowPriority )
        {
            continue;
        }
        if ( shadowLight.ShadowMapResolution <= 256 )
        {
            continue;
        }

        if ( iBestLight < 0 )
        {
            iBestLight = iLight;
            continue;
        }

        ShadowLight const& bestLight = pShadowLights[iBestLight];
        if ( shadowLight.Score < bestLight.Score )
        {
            iBestLight = iLight;
            continue;
        }

        if ( ( shadowLight.Score == bestLight.Score ) &&
             ( shadowLight.ShadowMapResolution > bestLight.ShadowMapResolution ) )
        {
            iBestLight = iLight;
        }
    }

    return iBestLight;
}

//---------------------------------------------------------------------

s32 FindShadowLightForRemoval( ShadowLight const* pShadowLights, s32 nShadowLights, s32 shadowPriority )
{
    s32 iBestLight = -1;

    for ( s32 iLight = 0; iLight < nShadowLights; iLight++ )
    {
        ShadowLight const& shadowLight = pShadowLights[iLight];
        if ( shadowLight.ShadowPriority != shadowPriority )
        {
            continue;
        }

        if ( iBestLight < 0 )
        {
            iBestLight = iLight;
            continue;
        }

        ShadowLight const& bestLight = pShadowLights[iBestLight];
        if ( shadowLight.Score < bestLight.Score )
        {
            iBestLight = iLight;
            continue;
        }

        if ( ( shadowLight.Score == bestLight.Score ) &&
             ( shadowLight.ShadowMapResolution > bestLight.ShadowMapResolution ) )
        {
            iBestLight = iLight;
        }
    }

    return iBestLight;
}

//---------------------------------------------------------------------

void RemoveShadowLight( ShadowLight* pShadowLights, s32& nShadowLights, s32 removeIndex )
{
    ASSERT( ( removeIndex >= 0 ) && ( removeIndex < nShadowLights ) );

    for ( s32 iLight = removeIndex + 1; iLight < nShadowLights; iLight++ )
    {
        pShadowLights[iLight - 1] = pShadowLights[iLight];
    }

    nShadowLights--;
}

//---------------------------------------------------------------------

void ResolveAtlasBudget( ShadowLight* pShadowLights, s32& nShadowLights, xarray<ShadowAtlasFreeNode>& freeNodes )
{
    for ( s32 shadowPriority = kMinShadowPriority; shadowPriority <= kMaxShadowPriority; shadowPriority++ )
    {
        while ( !AtlasFitsShadowLights( pShadowLights, nShadowLights, freeNodes ) )
        {
            s32 const iLightToReduce = FindShadowLightForResolutionDrop( pShadowLights, nShadowLights, shadowPriority );
            if ( iLightToReduce >= 0 )
            {
                pShadowLights[iLightToReduce].ShadowMapResolution =
                    ReduceShadowMapResolution( pShadowLights[iLightToReduce].ShadowMapResolution );
                continue;
            }

            s32 const iLightToRemove = FindShadowLightForRemoval( pShadowLights, nShadowLights, shadowPriority );
            if ( iLightToRemove >= 0 )
            {
                RemoveShadowLight( pShadowLights, nShadowLights, iLightToRemove );
                continue;
            }

            break;
        }

        if ( AtlasFitsShadowLights( pShadowLights, nShadowLights, freeNodes ) )
        {
            return;
        }
    }
}

//---------------------------------------------------------------------

s32 GetAtlasGuardTexels( f32 filterRadius )
{
    s32 const guardTexels = static_cast<s32>( x_ceil( filterRadius ) ) + 1;
    return ( guardTexels > 0 ) ? guardTexels : 1;
}
} // namespace

//=========================================================================
//  FUNCTIONS
//=========================================================================

struct ShadowMapMgr::ScratchData
{
    xarray<ShadowCasterRecord>     RelevantCasters;
    xarray<ShadowCasterRecord>     ShadowCasters;
    xarray<u64>                    CasterMasks;
    xarray<ShadowCasterMaskRecord> PendingCasterMasks;
    xarray<ShadowAtlasFreeNode>    AtlasFreeNodes;
};

//=========================================================================

ShadowMapMgr::ShadowMapMgr( void )
  : m_pScratch( NULL ), m_sourceCount( 0 ), m_pointFaceCount( 0 ), m_pointLightCount( 0 ), m_atlasSourceCount( 0 ),
      m_atlasSize( 0 ), m_atlasSizeFloor( 0 ), m_atlasLayoutDirty( FALSE ), m_enabled( TRUE ),
      m_shadowFilterType( ShadowFilterType::Evsm )
{
    x_memset( m_sources, 0, sizeof( m_sources ) );
}

//=========================================================================

ShadowMapMgr::~ShadowMapMgr( void )
{
    ClearSources();
    delete m_pScratch;
    m_pScratch = NULL;
}

//=========================================================================

ShadowMapMgr::ScratchData& ShadowMapMgr::GetScratch( void )
{
    if ( !m_pScratch )
    {
        m_pScratch = new ScratchData;
    }

    return *m_pScratch;
}

//=========================================================================

void ShadowMapMgr::ClearSources( void )
{
    m_sourceCount = 0;
    m_pointFaceCount = 0;
    m_pointLightCount = 0;
    m_atlasSourceCount = 0;
    m_atlasSize = 0;
    m_atlasLayoutDirty = FALSE;
}

//=========================================================================

void ShadowMapMgr::FinalizeSources( void )
{
    if ( !m_atlasLayoutDirty )
    {
        return;
    }

    UpdateAtlasLayout();
    m_atlasLayoutDirty = FALSE;
}

//=========================================================================

void ShadowMapMgr::SetEnabled( xbool Enabled )
{
    m_enabled = Enabled;
    if( !m_enabled )
    {
        ClearSources();
    }
}

//=========================================================================

void ShadowMapMgr::SetShadowFilterType( ShadowFilterType Type )
{
    if( m_shadowFilterType == Type )
    {
        return;
    }

    m_shadowFilterType = Type;
    m_atlasLayoutDirty = ( m_sourceCount > 0 );
}

//=========================================================================

void ShadowMapMgr::ComputePerspectiveSource( ShadowSource& dest, s32 sourceType, matrix4 const& l2W, radian fov,
                                             f32 lightRadius, f32 lightFalloff, s32 shadowMapResolution ) const
{
    shadowMapResolution = ClampShadowMapResolution( shadowMapResolution );

    f32 receiveNearZ = ComputeSpotShadowReceiveNearZ( lightRadius );
    f32 nearZ = MAX( 0.5f, lightRadius * 0.01f );

    if ( sourceType == SHADOW_SOURCE_POINT_FACE )
    {
        receiveNearZ = ComputePointShadowReceiveNearZ( lightRadius );
        nearZ = ComputePointShadowNearZ( lightRadius );
    }

    nearZ = MAX( 0.01f, nearZ );

    f32 farZ = MAX( nearZ + 1.0f, lightRadius );

    matrix4 w2Proj = l2W;
    w2Proj.InvertRT();

    f32 const tanHalfFov = x_tan( fov * 0.5f );
    f32       scale = ( tanHalfFov > 0.0f ) ? ( 1.0f / tanHalfFov ) : 1.0f;

    if ( sourceType == SHADOW_SOURCE_POINT_FACE )
    {
        f32 const coverageScale = static_cast<f32>( shadowMapResolution ) /
                                  ( static_cast<f32>( shadowMapResolution ) + kPointShadowProjectionOverlapTexels );
        scale *= coverageScale;
    }

    matrix4 proj2Clip;
    proj2Clip.Zero();

    if ( sourceType == SHADOW_SOURCE_POINT_FACE )
    {
        proj2Clip( 0, 0 ) = scale;
    }
    else
    {
        proj2Clip( 0, 0 ) = -scale;
    }

    proj2Clip( 1, 1 ) = scale;
    proj2Clip( 2, 2 ) = farZ / ( farZ - nearZ );
    proj2Clip( 3, 2 ) = -( nearZ * farZ ) / ( farZ - nearZ );
    proj2Clip( 2, 3 ) = 1.0f;

    dest.Type = sourceType;
    dest.DynamicLightIndex = -1;
    dest.PointLightIndex = -1;
    dest.FaceIndex = -1;
    dest.RequestedResolution = shadowMapResolution;
    dest.ShadowPriority = kMinShadowPriority;
    dest.ShadowScore = 0.0f;
    dest.WorldToClip = proj2Clip * w2Proj;
    dest.WorldToAtlas = dest.WorldToClip;
    dest.FaceLightDirFalloff.Zero();
    dest.FaceLightData.Zero();
    dest.LightFalloff = lightFalloff;
    dest.NearZ = nearZ;
    dest.ReceiveNearZ = receiveNearZ;
    dest.FarZ = farZ;
    dest.AtlasX = 0;
    dest.AtlasY = 0;
    dest.AtlasWidth = shadowMapResolution;
    dest.AtlasHeight = shadowMapResolution;
    dest.AtlasTileX = 0;
    dest.AtlasTileY = 0;
    dest.AtlasTileWidth = shadowMapResolution;
    dest.AtlasTileHeight = shadowMapResolution;

    vector3 const lightPos = l2W.GetTranslation();
    vector3 const extent( farZ, farZ, farZ );
    dest.LightPosRadius = vector4( lightPos.GetX(), lightPos.GetY(), lightPos.GetZ(), lightRadius );
    dest.WorldBBox.Set( lightPos - extent, lightPos + extent );
}

//=========================================================================

void ShadowMapMgr::UpdateAtlasLayout( void )
{
    m_atlasSize = 0;

    if ( m_atlasSourceCount <= 0 )
    {
        return;
    }

    ScratchData& scratch = GetScratch();

    ShadowAtlasEntry entries[MAX_SHADOW_SOURCES];
    s32              nEntries = 0;

    for ( s32 i = 0; i < m_sourceCount; i++ )
    {
        ShadowSource const& source = m_sources[i];
        ASSERT( nEntries < MAX_SHADOW_SOURCES );
        entries[nEntries].SourceIndex = i;
        entries[nEntries].TileSize = ClampShadowMapResolution( source.RequestedResolution );
        entries[nEntries].ShadowPriority = source.ShadowPriority;
        entries[nEntries].ShadowScore = source.ShadowScore;
        entries[nEntries].AtlasTileX = 0;
        entries[nEntries].AtlasTileY = 0;
        nEntries++;
    }

    s32 atlasSize = GetShadowAtlasSizeForEntries( entries, nEntries, scratch.AtlasFreeNodes );
    while ( atlasSize <= 0 )
    {
        s32 iLargestEntry = -1;
        for ( s32 i = 0; i < nEntries; i++ )
        {
            if ( entries[i].TileSize <= 256 )
            {
                continue;
            }

            if ( iLargestEntry < 0 )
            {
                iLargestEntry = i;
                continue;
            }

            if ( entries[i].ShadowPriority < entries[iLargestEntry].ShadowPriority )
            {
                iLargestEntry = i;
                continue;
            }

            if ( ( entries[i].ShadowPriority == entries[iLargestEntry].ShadowPriority ) &&
                 ( entries[i].ShadowScore < entries[iLargestEntry].ShadowScore ) )
            {
                iLargestEntry = i;
                continue;
            }

            if ( ( entries[i].ShadowPriority == entries[iLargestEntry].ShadowPriority ) &&
                 ( entries[i].ShadowScore == entries[iLargestEntry].ShadowScore ) &&
                 ( entries[i].TileSize > entries[iLargestEntry].TileSize ) )
            {
                iLargestEntry = i;
            }
        }

        if ( iLargestEntry < 0 )
        {
            ASSERT( FALSE );
            break;
        }

        ShadowSource const& selectedSource = m_sources[entries[iLargestEntry].SourceIndex];
        s32 const reducedTileSize = ReduceShadowMapResolution( entries[iLargestEntry].TileSize );
        if ( selectedSource.Type == SHADOW_SOURCE_POINT_FACE )
        {
            for ( s32 i = 0; i < nEntries; i++ )
            {
                ShadowSource const& source = m_sources[entries[i].SourceIndex];
                if ( ( source.Type == SHADOW_SOURCE_POINT_FACE ) &&
                     ( source.PointLightIndex == selectedSource.PointLightIndex ) )
                {
                    entries[i].TileSize = reducedTileSize;
                }
            }
        }
        else
        {
            entries[iLargestEntry].TileSize = reducedTileSize;
        }
        atlasSize = GetShadowAtlasSizeForEntries( entries, nEntries, scratch.AtlasFreeNodes );
    }

    s32 const kAtlasFloorCap = 4096;
    if ( atlasSize < m_atlasSizeFloor )
    {
        atlasSize = m_atlasSizeFloor;
    }
    if ( atlasSize <= kAtlasFloorCap && atlasSize > m_atlasSizeFloor )
    {
        m_atlasSizeFloor = atlasSize;
    }

    m_atlasSize = atlasSize;

    f32 depthFilterRadius = 0.0f;
    if( m_shadowFilterType == ShadowFilterType::Evsm )
    {
        // Conversion preserves normalized atlas coordinates. Reserve enough
        // depth texels for the scaled moment-space kernel at the selected
        // atlas ratio.
        s32 const momentAtlasSize = MIN( MAX( atlasSize / 2, 1 ), MAX_SHADOW_MOMENT_ATLAS_SIZE );
        f32 const depthToMomentScale = static_cast<f32>( atlasSize ) / static_cast<f32>( momentAtlasSize );
        depthFilterRadius =
            static_cast<f32>( SHADOW_EVSM_BLUR_RADIUS ) * SHADOW_EVSM_BLUR_SCALE * depthToMomentScale;
    }
    s32 const guardTexels = GetAtlasGuardTexels( depthFilterRadius );

    for ( s32 i = 0; i < nEntries; i++ )
    {
        ShadowSource& source = m_sources[entries[i].SourceIndex];
        s32 const     tileSize = entries[i].TileSize;

        source.AtlasTileX = entries[i].AtlasTileX;
        source.AtlasTileY = entries[i].AtlasTileY;
        source.AtlasTileWidth = tileSize;
        source.AtlasTileHeight = tileSize;
        source.AtlasX = source.AtlasTileX;
        source.AtlasY = source.AtlasTileY;
        source.AtlasWidth = source.AtlasTileWidth;
        source.AtlasHeight = source.AtlasTileHeight;

        if ( guardTexels > 0 )
        {
            s32 const maxGuardX = ( source.AtlasWidth > 2 ) ? ( ( source.AtlasWidth - 1 ) / 2 ) : 0;
            s32 const maxGuardY = ( source.AtlasHeight > 2 ) ? ( ( source.AtlasHeight - 1 ) / 2 ) : 0;
            s32 const guardX = ( guardTexels < maxGuardX ) ? guardTexels : maxGuardX;
            s32 const guardY = ( guardTexels < maxGuardY ) ? guardTexels : maxGuardY;

            source.AtlasX += guardX;
            source.AtlasY += guardY;
            source.AtlasWidth -= guardX * 2;
            source.AtlasHeight -= guardY * 2;
        }

        if ( source.Type == SHADOW_SOURCE_POINT_FACE )
        {
            f32 const requestedResolution = static_cast<f32>( ClampShadowMapResolution( source.RequestedResolution ) );
            f32 const requestedCoverage =
                requestedResolution / ( requestedResolution + kPointShadowProjectionOverlapTexels );
            f32 const actualResolution = static_cast<f32>( source.AtlasWidth );
            f32 const actualCoverage = actualResolution / ( actualResolution + kPointShadowProjectionOverlapTexels );
            f32 const coverageCorrection = actualCoverage / requestedCoverage;
            source.WorldToClip.Scale( vector3( coverageCorrection, coverageCorrection, 1.0f ) );
            source.FaceLightData.GetX() = ComputePointFaceCoverageCos( source.AtlasWidth );
        }

        source.WorldToAtlas = source.WorldToClip;
        source.WorldToAtlas.Scale( vector3( 0.5f, -0.5f, 1.0f ) );
        source.WorldToAtlas.Translate( vector3( 0.5f, 0.5f, 0.0f ) );
        source.WorldToAtlas.Scale( vector3( static_cast<f32>( source.AtlasWidth ) / static_cast<f32>( m_atlasSize ),
                                            static_cast<f32>( source.AtlasHeight ) / static_cast<f32>( m_atlasSize ),
                                            1.0f ) );
        source.WorldToAtlas.Translate( vector3( static_cast<f32>( source.AtlasX ) / static_cast<f32>( m_atlasSize ),
                                                static_cast<f32>( source.AtlasY ) / static_cast<f32>( m_atlasSize ),
                                                0.0f ) );
    }
}

//=========================================================================

xbool ShadowMapMgr::AddPointSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff,
                                    s32 shadowMapResolution, s32 shadowPriority, f32 shadowScore,
                                    s32 dynamicLightIndex )
{
    if ( !m_enabled )
    {
        return FALSE;
    }

    if ( m_sourceCount >= MAX_SHADOW_SOURCES )
    {
        return FALSE;
    }
    if ( m_pointLightCount >= MAX_SHADOW_LIGHTS && ( m_pointFaceCount % POINT_SHADOW_FACE_COUNT ) == 0 )
    {
        return FALSE;
    }

    ShadowSource& source = m_sources[m_sourceCount];
    ComputePerspectiveSource( source, SHADOW_SOURCE_POINT_FACE, l2W, fov, lightRadius, lightFalloff,
                              shadowMapResolution );

    vector3 faceDirection = l2W.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
    if ( !faceDirection.SafeNormalize() )
    {
        faceDirection.Set( 0.0f, 0.0f, 1.0f );
    }

    source.FaceLightDirFalloff.Set( faceDirection.GetX(), faceDirection.GetY(), faceDirection.GetZ(), lightFalloff );
    source.FaceLightData.Set( ComputePointFaceCoverageCos( shadowMapResolution ), source.ReceiveNearZ, source.FarZ,
                              kShadowSourceTypePointFace );
    source.ShadowPriority = ClampShadowPriority( shadowPriority );
    source.ShadowScore = shadowScore;
    source.DynamicLightIndex = dynamicLightIndex;
    source.PointLightIndex = m_pointFaceCount / POINT_SHADOW_FACE_COUNT;
    source.FaceIndex = m_pointFaceCount % POINT_SHADOW_FACE_COUNT;
    m_sourceCount++;
    m_pointFaceCount++;
    m_atlasSourceCount++;
    m_atlasLayoutDirty = TRUE;

    if ( source.FaceIndex == 0 )
    {
        m_pointLightCount++;
    }

    return TRUE;
}

//=========================================================================

xbool ShadowMapMgr::AddSpotSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff,
                                   s32 shadowMapResolution, s32 shadowPriority, f32 shadowScore, s32 dynamicLightIndex )
{
    if ( !m_enabled )
    {
        return FALSE;
    }

    if ( m_sourceCount >= MAX_SHADOW_SOURCES )
    {
        return FALSE;
    }

    ShadowSource& source = m_sources[m_sourceCount];
    ComputePerspectiveSource( source, SHADOW_SOURCE_SPOT, l2W, fov, lightRadius, lightFalloff, shadowMapResolution );

    vector3 spotDirection = l2W.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
    if ( !spotDirection.SafeNormalize() )
    {
        spotDirection.Set( 0.0f, 0.0f, 1.0f );
    }

    source.FaceLightDirFalloff.Set( spotDirection.GetX(), spotDirection.GetY(), spotDirection.GetZ(), lightFalloff );
    source.FaceLightData.Set( x_cos( fov * 0.5f ), source.ReceiveNearZ, source.FarZ, kShadowSourceTypeSpot );
    source.ShadowPriority = ClampShadowPriority( shadowPriority );
    source.ShadowScore = shadowScore;
    source.DynamicLightIndex = dynamicLightIndex;
    m_sourceCount++;
    m_atlasSourceCount++;
    m_atlasLayoutDirty = TRUE;
    return TRUE;
}

//=========================================================================

ShadowMapMgr::ShadowSource const& ShadowMapMgr::GetSource( s32 sourceIndex ) const
{
    ASSERT( ( sourceIndex >= 0 ) && ( sourceIndex < m_sourceCount ) );
    ASSERT( !m_atlasLayoutDirty );
    return m_sources[sourceIndex];
}

//=========================================================================

s32 ShadowMapMgr::GetSourceCount( void ) const
{
    return m_sourceCount;
}

//=========================================================================

s32 ShadowMapMgr::GetAtlasSize( void ) const
{
    return ( m_atlasSize > 0 ) ? m_atlasSize : MAX_SHADOW_ATLAS_SIZE;
}

//=========================================================================

s32 ShadowMapMgr::GetAtlasSourceCount( void ) const
{
    return m_atlasSourceCount;
}

//=========================================================================

ShadowFilterType ShadowMapMgr::GetShadowFilterType( void ) const
{
    return m_shadowFilterType;
}

//=========================================================================

xbool ShadowMapMgr::HasActiveSources( void ) const
{
    return m_enabled && ( m_sourceCount > 0 );
}

//=========================================================================

void ShadowMapMgr::CreateShadowMap( object* const* pPCasterCandidates, s32 nCasterCandidates )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/Create" );

    ClearSources();
    if( !m_enabled )
    {
        return;
    }

    static xprofile_counter shadowMapDynamicLights =
        x_GetProfiler().RegisterCounter( "ShadowMapDynamicLights", "RenderCounter" );
    static xprofile_counter shadowMapCandidateLights =
        x_GetProfiler().RegisterCounter( "ShadowMapCandidateLights", "RenderCounter" );
    static xprofile_counter shadowMapSelectedLights =
        x_GetProfiler().RegisterCounter( "ShadowMapSelectedLights", "RenderCounter" );
    static xprofile_counter shadowMapPointLights =
        x_GetProfiler().RegisterCounter( "ShadowMapPointLights", "RenderCounter" );
    static xprofile_counter shadowMapSpotLights =
        x_GetProfiler().RegisterCounter( "ShadowMapSpotLights", "RenderCounter" );
    static xprofile_counter shadowMapRelevantCasterRefs =
        x_GetProfiler().RegisterCounter( "ShadowMapRelevantCasterRefs", "RenderCounter" );
    static xprofile_counter shadowMapUniqueCasters =
        x_GetProfiler().RegisterCounter( "ShadowMapUniqueCasters", "RenderCounter" );
    static xprofile_counter shadowMapObjectCasters =
        x_GetProfiler().RegisterCounter( "ShadowMapObjectCasters", "RenderCounter" );
    static xprofile_counter shadowMapPlaySurfaceCasters =
        x_GetProfiler().RegisterCounter( "ShadowMapPlaySurfaceCasters", "RenderCounter" );
    static xprofile_counter shadowMapCasterSourceLinks =
        x_GetProfiler().RegisterCounter( "ShadowMapCasterSourceLinks", "RenderCounter" );
    static xprofile_counter shadowMapActiveSources =
        x_GetProfiler().RegisterCounter( "ShadowMapActiveSources", "RenderCounter" );
    static xprofile_counter shadowMapAtlasSources =
        x_GetProfiler().RegisterCounter( "ShadowMapAtlasSources", "RenderCounter" );

    static_cast<void>( pPCasterCandidates );

    ShadowLight                 shadowLightCandidates[light_mgr::MAX_DYNAMIC_LIGHTS];
    ShadowLight                 shadowLights[light_mgr::MAX_DYNAMIC_LIGHTS];
    s32                         nCandidateShadowLights = 0;
    player*                     pActivePlayer = SMP_UTIL_GetActivePlayer();
    ScratchData&                scratch = GetScratch();
    xarray<ShadowCasterRecord>& relevantCasters = scratch.RelevantCasters;
    xarray<ShadowCasterRecord>& shadowCasters = scratch.ShadowCasters;
    xarray<u64>&                casterMasks = scratch.CasterMasks;
    relevantCasters.SetCount( 0 );
    shadowCasters.SetCount( 0 );
    casterMasks.SetCount( 0 );
    relevantCasters.SetGrowAmount( MAX( 128, nCasterCandidates ) );
    shadowCasters.SetGrowAmount( MAX( 128, nCasterCandidates ) );
    casterMasks.SetGrowAmount( MAX( 128, nCasterCandidates ) );

    s32 const nDynamicLights = g_LightMgr.GetNDynamicLights();
    shadowMapDynamicLights.Add( nDynamicLights );
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/BuildLightCandidates" );
        for ( s32 iLight = 0; iLight < nDynamicLights; iLight++ )
        {
            ShadowLight shadowLight;
            if ( !BuildShadowLight( shadowLight, iLight, pActivePlayer, relevantCasters ) )
            {
                continue;
            }

            InsertShadowLight( shadowLightCandidates, nCandidateShadowLights, shadowLight );
        }
    }
    shadowMapCandidateLights.Add( nCandidateShadowLights );
    shadowMapRelevantCasterRefs.Add( relevantCasters.GetCount() );

    s32 nShadowLights;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/SelectAndPackLights" );
        nShadowLights = SelectShadowLights( shadowLightCandidates, nCandidateShadowLights, shadowLights );

        ResolveAtlasBudget( shadowLights, nShadowLights, scratch.AtlasFreeNodes );
    }

    s32 nPointLights = 0;
    s32 nSpotLights = 0;
    for ( s32 i = 0; i < nShadowLights; ++i )
    {
        if ( shadowLights[i].Shape == light_mgr::LIGHT_SHAPE_OMNI )
        {
            nPointLights++;
        }
        else
        {
            nSpotLights++;
        }
    }
    shadowMapSelectedLights.Add( nShadowLights );
    shadowMapPointLights.Add( nPointLights );
    shadowMapSpotLights.Add( nSpotLights );

    if ( nShadowLights <= 0 )
    {
        shadowMapUniqueCasters.Add( 0 );
        shadowMapObjectCasters.Add( 0 );
        shadowMapPlaySurfaceCasters.Add( 0 );
        shadowMapCasterSourceLinks.Add( 0 );
        shadowMapActiveSources.Add( 0 );
        shadowMapAtlasSources.Add( 0 );
        return;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/CollectCasters" );
        CollectShadowCastersForLights( relevantCasters, shadowLights, nShadowLights, shadowCasters, casterMasks,
                                       scratch.PendingCasterMasks );
    }

    s32 nObjectCasters = 0;
    s32 nPlaySurfaceCasters = 0;
    u64 nCasterSourceLinks = 0;
    for ( s32 i = 0; i < shadowCasters.GetCount(); ++i )
    {
        if ( shadowCasters[i].Type == ShadowCasterRecord::CASTER_OBJECT )
        {
            nObjectCasters++;
        }
        else
        {
            nPlaySurfaceCasters++;
        }

        u64 mask = casterMasks[i];
        while ( mask )
        {
            nCasterSourceLinks += mask & 1;
            mask >>= 1;
        }
    }
    shadowMapUniqueCasters.Add( shadowCasters.GetCount() );
    shadowMapObjectCasters.Add( nObjectCasters );
    shadowMapPlaySurfaceCasters.Add( nPlaySurfaceCasters );
    shadowMapCasterSourceLinks.Add( static_cast<f64>( nCasterSourceLinks ) );

    if ( eng_Begin( "Shadow Map" ) )
    {
        render::BeginShadowCreation();
        AddShadowSources( shadowLights, nShadowLights );
        FinalizeSources();
        shadowMapActiveSources.Add( GetSourceCount() );
        shadowMapAtlasSources.Add( GetAtlasSourceCount() );

        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/SubmitCasters" );
            for ( s32 i = 0; i < shadowCasters.GetCount(); i++ )
            {
                ShadowCasterRecord const& caster = shadowCasters[i];

                if ( caster.Type == ShadowCasterRecord::CASTER_OBJECT )
                {
                    ASSERT( caster.pObject );
                    caster.pObject->OnRenderShadowCast( casterMasks[i] );
                }
                else
                {
                    ASSERT( caster.pPlaySurface );
                    if ( !caster.pPlaySurface->RenderInst.IsNull() )
                    {
                        render::AddRigidCasterSimple( caster.pPlaySurface->RenderInst, &caster.pPlaySurface->L2W,
                                                      casterMasks[i] );
                    }
                }
            }
        }

        render::EndShadowCreation();
        eng_End();
    }
    else
    {
        shadowMapActiveSources.Add( 0 );
        shadowMapAtlasSources.Add( 0 );
    }
}
