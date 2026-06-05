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
#include "..\Obj_mgr\Obj_Mgr.hpp"
#include "..\PlaySurfaceMgr\PlaySurfaceMgr.hpp"
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
        s32     ShadowMapResolution;
        f32     PointFaceCoverageCos;
        s32     ShadowPriority;
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

    const radian kPointShadowFOV                     = R_90;
    const radian kMinSpotShadowFOV                   = DEG_TO_RAD( 1.0f );
    const s32    kMinShadowPriority                  = 0;
    const s32    kMaxShadowPriority                  = 4;
    const f32    kPointShadowProjectionOverlapTexels = 0.0f;
    const f32    kShadowAtlasFilterRadius            = 2.0f;
    const f32    kShadowCasterScoreWeight            = 4096.0f;
    const f32    kPlayerShadowScoreBonus             = 8192.0f;
    const f32    kShadowRadiusScorePenalty           = 0.25f;
    const f32    kShadowSourceTypeSpot               = 0.0f;
    const f32    kShadowSourceTypePointFace          = 1.0f;

    //---------------------------------------------------------------------

    struct shadow_cone_test
    {
        vector3 Apex;
        vector3 Axis;
        f32     CosOuter;
        f32     SinOuter;
        f32     TanOuter;
        f32     Length;
    };

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
        s32     ShadowMapResolution;
    };

    //---------------------------------------------------------------------

    struct shadow_caster_record
    {
        enum caster_type
        {
            CASTER_OBJECT,
            CASTER_PLAY_SURFACE,
        };

        caster_type              Type;
        object*                  pObject;
        playsurface_mgr::surface* pPlaySurface;
        vector3                  WorldBBoxCenter;
        f32                      WorldBBoxRadius;
    };

    //---------------------------------------------------------------------

    struct shadow_caster_mask_record
    {
        shadow_caster_record Caster;
        u64                  SourceMask;
    };

    //---------------------------------------------------------------------

    f32 ComputeBBoxBoundingSphereRadius( const bbox& WorldBBox );

    //---------------------------------------------------------------------

    void SetShadowCasterBounds( shadow_caster_record& Caster,
                                const bbox&           WorldBBox )
    {
        Caster.WorldBBoxCenter = WorldBBox.GetCenter();
        Caster.WorldBBoxRadius = ComputeBBoxBoundingSphereRadius( WorldBBox );
    }

    //---------------------------------------------------------------------

    shadow_caster_record MakeObjectShadowCaster( object*      pCaster,
                                                 const bbox&  WorldBBox )
    {
        shadow_caster_record Caster;
        Caster.Type         = shadow_caster_record::CASTER_OBJECT;
        Caster.pObject      = pCaster;
        Caster.pPlaySurface = NULL;
        SetShadowCasterBounds( Caster, WorldBBox );
        return Caster;
    }

    //---------------------------------------------------------------------

    shadow_caster_record MakePlaySurfaceShadowCaster( playsurface_mgr::surface* pSurface,
                                                      const bbox&               WorldBBox )
    {
        shadow_caster_record Caster;
        Caster.Type         = shadow_caster_record::CASTER_PLAY_SURFACE;
        Caster.pObject      = NULL;
        Caster.pPlaySurface = pSurface;
        SetShadowCasterBounds( Caster, WorldBBox );
        return Caster;
    }

    //---------------------------------------------------------------------

    xbool SameShadowCaster( const shadow_caster_record& A,
                            const shadow_caster_record& B )
    {
        if( A.Type != B.Type )
            return FALSE;

        if( A.Type == shadow_caster_record::CASTER_OBJECT )
            return ( A.pObject == B.pObject );

        return ( A.pPlaySurface == B.pPlaySurface );
    }

    //---------------------------------------------------------------------

    uaddr GetShadowCasterSortKey( const shadow_caster_record& Caster )
    {
        if( Caster.Type == shadow_caster_record::CASTER_OBJECT )
            return (uaddr)Caster.pObject;

        return (uaddr)Caster.pPlaySurface;
    }

    //---------------------------------------------------------------------

    s32 ShadowCasterMaskCompareFn( const void* pA, const void* pB )
    {
        const shadow_caster_mask_record& A = *((const shadow_caster_mask_record*)pA);
        const shadow_caster_mask_record& B = *((const shadow_caster_mask_record*)pB);

        if( A.Caster.Type != B.Caster.Type )
            return ( A.Caster.Type < B.Caster.Type ) ? -1 : 1;

        const uaddr AKey = GetShadowCasterSortKey( A.Caster );
        const uaddr BKey = GetShadowCasterSortKey( B.Caster );

        if( AKey < BKey )
            return -1;
        if( AKey > BKey )
            return 1;

        return 0;
    }

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

    s32 ClampShadowMapResolution( s32 ShadowMapResolution )
    {
        if( ShadowMapResolution >= 4096 )
            return 4096;		
        if( ShadowMapResolution >= 2048 )
            return 2048;
        if( ShadowMapResolution >= 1024 )
            return 1024;
        if( ShadowMapResolution >= 512 )
            return 512;
        return 256;
    }

    //---------------------------------------------------------------------

    s32 ClampShadowPriority( s32 ShadowPriority )
    {
        if( ShadowPriority < kMinShadowPriority )
            return kMinShadowPriority;
        if( ShadowPriority > kMaxShadowPriority )
            return kMaxShadowPriority;
        return ShadowPriority;
    }

    //---------------------------------------------------------------------

    f32 ComputePointFaceCoverageCos( s32 ShadowMapResolution )
    {
        const s32 ClampedResolution = ClampShadowMapResolution( ShadowMapResolution );
        const f32 CoverageScale     = (f32)ClampedResolution /
                                      ( (f32)ClampedResolution + kPointShadowProjectionOverlapTexels );
        const f32 SafeScale         = MAX( CoverageScale, 0.0001f );
        const f32 TanHalfFOV        = x_tan( kPointShadowFOV * 0.5f ) / SafeScale;
        const f32 CornerLengthSq    = 1.0f + ( 2.0f * TanHalfFOV * TanHalfFOV );
        return 1.0f / x_sqrt( CornerLengthSq );
    }

    //---------------------------------------------------------------------

    f32 ComputePointShadowNearZ( f32 LightRadius )
    {
        return MAX( 0.5f, LightRadius * 0.0025f );
    }

    //---------------------------------------------------------------------

    f32 ComputePointShadowReceiveNearZ( f32 LightRadius )
    {
        return MAX( 0.05f, LightRadius * 0.00025f );
    }

    //---------------------------------------------------------------------

    f32 ComputeSpotShadowReceiveNearZ( f32 LightRadius )
    {
        return MAX( 0.05f, LightRadius * 0.0005f );
    }

    //---------------------------------------------------------------------

    s32 ReduceShadowMapResolution( s32 ShadowMapResolution )
    {
        if( ShadowMapResolution > 2048 )
            return 2048;		
        if( ShadowMapResolution > 1024 )
            return 1024;
        if( ShadowMapResolution > 512 )
            return 512;
        if( ShadowMapResolution > 256 )
            return 256;
        return 256;
    }

    //---------------------------------------------------------------------

    struct shadow_atlas_entry
    {
        s32 SourceIndex;
        s32 TileSize;
        s32 ShadowPriority;
        f32 ShadowScore;
        s32 AtlasTileX;
        s32 AtlasTileY;
    };

    //---------------------------------------------------------------------

    xbool IsPowerOfTwo( s32 Value )
    {
        return ( Value > 0 ) && ( ( Value & ( Value - 1 ) ) == 0 );
    }

    //---------------------------------------------------------------------

    struct shadow_atlas_free_node
    {
        s32 AtlasTileX;
        s32 AtlasTileY;
        s32 TileSize;
    };

    //---------------------------------------------------------------------

    s32 GetShadowAtlasSplitCount( s32 AtlasSize, s32 MinTileSize )
    {
        ASSERT( AtlasSize > 0 );
        ASSERT( MinTileSize > 0 );
        ASSERT( IsPowerOfTwo( AtlasSize ) );
        ASSERT( IsPowerOfTwo( MinTileSize ) );
        ASSERT( AtlasSize >= MinTileSize );

        s32 SplitCount = 0;
        while( AtlasSize > MinTileSize )
        {
            AtlasSize /= 2;
            SplitCount++;
        }

        return SplitCount;
    }

    //---------------------------------------------------------------------

    s32 GetMaxShadowAtlasFreeNodeCount( s32 AtlasSize, s32 NEntries )
    {
        const s32 SplitCount = GetShadowAtlasSplitCount( AtlasSize, 256 );
        return 1 + ( MAX( NEntries, 0 ) * SplitCount * 3 );
    }

    //---------------------------------------------------------------------

    void AppendShadowAtlasFreeNode( xarray<shadow_atlas_free_node>& FreeNodes,
                                    s32                             AtlasTileX,
                                    s32                             AtlasTileY,
                                    s32                             TileSize )
    {
        shadow_atlas_free_node& Node = FreeNodes.Append();
        Node.AtlasTileX = AtlasTileX;
        Node.AtlasTileY = AtlasTileY;
        Node.TileSize   = TileSize;
    }

    //---------------------------------------------------------------------

    void SortShadowAtlasEntries( shadow_atlas_entry* pEntries, s32 NEntries )
    {
        for( s32 i = 1; i < NEntries; i++ )
        {
            const shadow_atlas_entry Entry = pEntries[i];
            s32 j = i - 1;
            while( j >= 0 )
            {
                if( pEntries[j].TileSize > Entry.TileSize )
                    break;

                if( ( pEntries[j].TileSize == Entry.TileSize ) &&
                    ( pEntries[j].ShadowPriority > Entry.ShadowPriority ) )
                {
                    break;
                }

                if( ( pEntries[j].TileSize == Entry.TileSize ) &&
                    ( pEntries[j].ShadowPriority == Entry.ShadowPriority ) &&
                    ( pEntries[j].ShadowScore > Entry.ShadowScore ) )
                {
                    break;
                }

                if( ( pEntries[j].TileSize == Entry.TileSize ) &&
                    ( pEntries[j].ShadowPriority == Entry.ShadowPriority ) &&
                    ( pEntries[j].ShadowScore == Entry.ShadowScore ) &&
                    ( pEntries[j].SourceIndex < Entry.SourceIndex ) )
                {
                    break;
                }

                pEntries[j + 1] = pEntries[j];
                j--;
            }

            pEntries[j + 1] = Entry;
        }
    }

    //---------------------------------------------------------------------

    xbool TryPackShadowAtlasEntries( shadow_atlas_entry* pEntries,
                                     s32                 NEntries,
                                     s32                 AtlasSize )
    {
        SortShadowAtlasEntries( pEntries, NEntries );

        ASSERT( IsPowerOfTwo( AtlasSize ) );
        if( !IsPowerOfTwo( AtlasSize ) )
            return FALSE;

        xarray<shadow_atlas_free_node> FreeNodes;
        FreeNodes.SetCapacity( GetMaxShadowAtlasFreeNodeCount( AtlasSize, NEntries ) );
        AppendShadowAtlasFreeNode( FreeNodes, 0, 0, AtlasSize );

        for( s32 i = 0; i < NEntries; i++ )
        {
            const s32 TileSize = pEntries[i].TileSize;
            ASSERT( IsPowerOfTwo( TileSize ) );
            if( !IsPowerOfTwo( TileSize ) )
                return FALSE;
            if( ( TileSize <= 0 ) || ( TileSize > AtlasSize ) )
                return FALSE;

            s32 iBestNode = -1;
            for( s32 iNode = 0; iNode < FreeNodes.GetCount(); iNode++ )
            {
                const shadow_atlas_free_node& Node = FreeNodes[iNode];
                if( Node.TileSize < TileSize )
                    continue;

                if( iBestNode < 0 )
                {
                    iBestNode = iNode;
                    continue;
                }

                const shadow_atlas_free_node& BestNode = FreeNodes[iBestNode];
                if( Node.TileSize < BestNode.TileSize )
                {
                    iBestNode = iNode;
                    continue;
                }

                if( ( Node.TileSize == BestNode.TileSize ) &&
                    ( Node.AtlasTileY < BestNode.AtlasTileY ) )
                {
                    iBestNode = iNode;
                    continue;
                }

                if( ( Node.TileSize == BestNode.TileSize ) &&
                    ( Node.AtlasTileY == BestNode.AtlasTileY ) &&
                    ( Node.AtlasTileX < BestNode.AtlasTileX ) )
                {
                    iBestNode = iNode;
                }
            }

            if( iBestNode < 0 )
                return FALSE;

            shadow_atlas_free_node Node = FreeNodes[iBestNode];
            FreeNodes[iBestNode] = FreeNodes[FreeNodes.GetCount() - 1];
            FreeNodes.SetCount( FreeNodes.GetCount() - 1 );

            while( Node.TileSize > TileSize )
            {
                const s32 ChildTileSize = Node.TileSize / 2;

                if( ( ChildTileSize <= 0 ) || ( ChildTileSize < TileSize ) )
                {
                    return FALSE;
                }

                AppendShadowAtlasFreeNode( FreeNodes,
                                           Node.AtlasTileX + ChildTileSize,
                                           Node.AtlasTileY,
                                           ChildTileSize );
                AppendShadowAtlasFreeNode( FreeNodes,
                                           Node.AtlasTileX,
                                           Node.AtlasTileY + ChildTileSize,
                                           ChildTileSize );
                AppendShadowAtlasFreeNode( FreeNodes,
                                           Node.AtlasTileX + ChildTileSize,
                                           Node.AtlasTileY + ChildTileSize,
                                           ChildTileSize );

                Node.TileSize = ChildTileSize;
            }

            pEntries[i].AtlasTileX = Node.AtlasTileX;
            pEntries[i].AtlasTileY = Node.AtlasTileY;
        }

        return TRUE;
    }

    //---------------------------------------------------------------------

    s32 GetShadowAtlasSizeForEntries( shadow_atlas_entry* pEntries, s32 NEntries )
    {
        for( s32 AtlasSize = 256; AtlasSize <= MAX_SHADOW_ATLAS_SIZE; AtlasSize *= 2 )
        {
            if( TryPackShadowAtlasEntries( pEntries, NEntries, AtlasSize ) )
                return AtlasSize;
        }

        return 0;
    }

    //---------------------------------------------------------------------

    xbool SortsBeforeShadowLight( const shadow_light& A,
                                  const shadow_light& B )
    {
        if( A.ShadowPriority != B.ShadowPriority )
            return ( A.ShadowPriority > B.ShadowPriority );

        if( A.Score != B.Score )
            return ( A.Score > B.Score );

        if( A.ShadowMapResolution != B.ShadowMapResolution )
            return ( A.ShadowMapResolution > B.ShadowMapResolution );

        if( A.SourceCount != B.SourceCount )
            return ( A.SourceCount < B.SourceCount );

        return FALSE;
    }

    //---------------------------------------------------------------------

    f32 ComputeBBoxBoundingSphereRadius( const bbox& WorldBBox )
    {
        const vector3 Extents = ( WorldBBox.Max - WorldBBox.Min ) * 0.5f;
        return x_sqrt( Extents.Dot( Extents ) );
    }

    //---------------------------------------------------------------------

    void SetupShadowConeTest( shadow_cone_test& Cone,
                              const vector3&   Apex,
                              const vector3&   Direction,
                              f32              CosOuter,
                              f32              Length )
    {
        Cone.Apex = Apex;
        Cone.Axis = Direction;
        if( !Cone.Axis.SafeNormalize() )
            Cone.Axis.Set( 0.0f, 0.0f, 1.0f );

        Cone.CosOuter = MAX( CosOuter, 0.0001f );
        Cone.SinOuter = x_sqrt( MAX( 1.0f - ( Cone.CosOuter * Cone.CosOuter ), 0.0f ) );
        Cone.TanOuter = Cone.SinOuter / Cone.CosOuter;
        Cone.Length   = Length;
    }

    //---------------------------------------------------------------------

    xbool SphereIntersectsFiniteCone( const vector3& SphereCenter,
                                      f32            SphereRadius,
                                      const shadow_cone_test& Cone )
    {
        const vector3 ToCenter      = SphereCenter - Cone.Apex;
        const f32     CenterDistSq  = ToCenter.Dot( ToCenter );
        const f32     AxialDistance = Cone.Axis.Dot( ToCenter );
        const f32     SphereRadiusSq= SphereRadius * SphereRadius;

        if( AxialDistance < -SphereRadius )
            return FALSE;

        if( AxialDistance > ( Cone.Length + SphereRadius ) )
            return FALSE;

        if( AxialDistance <= 0.0f )
            return ( CenterDistSq <= SphereRadiusSq );

        const f32 RadialDistanceSq = MAX( CenterDistSq - ( AxialDistance * AxialDistance ), 0.0f );
        const f32 RadialDistance   = x_sqrt( RadialDistanceSq );

        if( AxialDistance < Cone.Length )
        {
            const f32 SideDistance = ( RadialDistance * Cone.CosOuter ) - ( AxialDistance * Cone.SinOuter );
            if( SideDistance <= SphereRadius )
                return TRUE;
        }

        const f32 ConeCapRadius  = Cone.Length * Cone.TanOuter;
        const f32 CapAxialDelta  = MAX( AxialDistance - Cone.Length, 0.0f );
        const f32 CapRadialDelta = MAX( RadialDistance - ConeCapRadius, 0.0f );
        return ( ( CapAxialDelta * CapAxialDelta ) + ( CapRadialDelta * CapRadialDelta ) ) <= SphereRadiusSq;
    }

    //---------------------------------------------------------------------

    xbool SourceConeAffectsBBox( const shadow_cone_test& Cone,
                                 const bbox&             WorldBBox )
    {
        return SphereIntersectsFiniteCone( WorldBBox.GetCenter(),
                                           ComputeBBoxBoundingSphereRadius( WorldBBox ),
                                           Cone );
    }

    //---------------------------------------------------------------------

    xbool ShadowCasterBoundsAreRelevant( const bbox&    CasterBBox,
                                         u8             Zone1,
                                         u8             Zone2,
                                         const vector3& LightPos,
                                         f32            LightRadius,
                                         const shadow_cone_test* pConeTest = NULL )
    {
        // Deep and raw
        //if( !g_ZoneMgr.IsBBoxVisible( CasterBBox,
        //                              Zone1,
        //                              Zone2 ) )
        //{
        //    return FALSE;
        //}

        if( !g_ZoneMgr.IsZoneVisible( Zone1 ) &&
            !g_ZoneMgr.IsZoneVisible( Zone2 ) )
        {
            return FALSE;
        }

        if( !CasterBBox.Intersect( LightPos, LightRadius ) )
            return FALSE;

        if( pConeTest )
        {
            return SourceConeAffectsBBox( *pConeTest, CasterBBox );
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
                            player*               pActivePlayer,
                            xarray<shadow_caster_record>& RelevantCasters )
    {
        dynamic_shadow_light DynamicLight;
        xbool                CastShadows;
        f32                  UnusedInnerRadius;
        f32                  UnusedInnerAngle;
        s32                  ShadowMapResolution;
        s32                  ShadowPriority;

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
                                    ShadowMapResolution,
                                    ShadowPriority );

        (void)UnusedInnerRadius;
        (void)UnusedInnerAngle;

        if( !CastShadows || ( DynamicLight.Radius <= 0.0f ) )
            return FALSE;

        DynamicLight.ShadowMapResolution = ClampShadowMapResolution( ShadowMapResolution );

        shadow_cone_test        LightConeTest;
        const shadow_cone_test* pLightConeTest = NULL;
        if( DynamicLight.Shape == light_mgr::LIGHT_SHAPE_SPOT )
        {
            const f32 LightConeOuterCos = x_cos( DEG_TO_RAD( DynamicLight.OuterAngle ) * 0.5f );
            if( LightConeOuterCos > 0.0f )
            {
                SetupShadowConeTest( LightConeTest,
                                     DynamicLight.Pos,
                                     DynamicLight.Direction,
                                     LightConeOuterCos,
                                     DynamicLight.Radius );
                pLightConeTest = &LightConeTest;
            }
        }

        const s32 RelevantCasterStart = RelevantCasters.GetCount();
        s32       LocalCasterCount    = 0;
        bbox      LightBBox           = bbox( DynamicLight.Pos, DynamicLight.Radius );

        // TODO: GS: Dont forget unlock me, after patching ALL level files.
        //g_ObjMgr.SelectBBox( object::ATTR_CAST_SHADOWS,
        //                     LightBBox,
        //                     object::TYPE_ALL_TYPES );
        g_ObjMgr.SelectBBox( object::ATTR_ALL,
                             LightBBox,
                             object::TYPE_ALL_TYPES );

        for( slot_id SlotID = g_ObjMgr.StartLoop(); SlotID != SLOT_NULL; SlotID = g_ObjMgr.GetNextResult( SlotID ) )
        {
            object* pCandidate = g_ObjMgr.GetObjectBySlot( SlotID );
            if( !pCandidate )
                continue;

#ifdef X_EDITOR
            if( pCandidate->IsHidden() )
                continue;
#endif

            if( pCandidate->GetType() == object::TYPE_PLAY_SURFACE )
                continue;

            const bbox& CandidateBBox = pCandidate->GetBBox();
            if( !ShadowCasterBoundsAreRelevant( CandidateBBox,
                                                (u8)pCandidate->GetZone1(),
                                                (u8)pCandidate->GetZone2(),
                                                DynamicLight.Pos,
                                                DynamicLight.Radius,
                                                pLightConeTest ) )
            {
                continue;
            }

            RelevantCasters.Append( MakeObjectShadowCaster( pCandidate, CandidateBBox ) );
            LocalCasterCount++;
        }
        g_ObjMgr.EndLoop();

        // TODO: GS: Dont forget unlock me, after patching ALL level files.
        //g_PlaySurfaceMgr.CollectSurfaces( LightBBox,
        //                                  object::ATTR_CAST_SHADOWS,
        //                                  0 );
        g_PlaySurfaceMgr.CollectSurfaces( LightBBox,
                                          object::ATTR_ALL,
                                          0 );

        playsurface_mgr::surface* pSurface = g_PlaySurfaceMgr.GetNextSurface();
        while( pSurface )
        {
            if( !pSurface->RenderInst.IsNull() &&
                ShadowCasterBoundsAreRelevant( pSurface->WorldBBox,
                                               (u8)( pSurface->ZoneInfo & 0xff ),
                                               (u8)( pSurface->ZoneInfo >> 8 ),
                                               DynamicLight.Pos,
                                               DynamicLight.Radius,
                                               pLightConeTest ) )
            {
                RelevantCasters.Append( MakePlaySurfaceShadowCaster( pSurface, pSurface->WorldBBox ) );
                LocalCasterCount++;
            }

            pSurface = g_PlaySurfaceMgr.GetNextSurface();
        }

        if( LocalCasterCount <= 0 )
            return FALSE;

        ShadowLight.Shape               = DynamicLight.Shape;
        ShadowLight.Pos                 = DynamicLight.Pos;
        ShadowLight.Direction           = DynamicLight.Direction;
        ShadowLight.Radius              = DynamicLight.Radius;
        ShadowLight.Falloff             = DynamicLight.Falloff;
        ShadowLight.FOV                 = GetShadowLightFOV( DynamicLight );
        ShadowLight.ShadowMapResolution = DynamicLight.ShadowMapResolution;
        ShadowLight.PointFaceCoverageCos = ( DynamicLight.Shape == light_mgr::LIGHT_SHAPE_OMNI ) ?
                                           ComputePointFaceCoverageCos( DynamicLight.ShadowMapResolution ) :
                                           0.0f;
        ShadowLight.ShadowPriority      = ClampShadowPriority( ShadowPriority );
        ShadowLight.SourceCount         = GetShadowLightSourceCount( DynamicLight.Shape );
        ShadowLight.RelevantCasterStart = RelevantCasterStart;
        ShadowLight.RelevantCasterCount = LocalCasterCount;
        ShadowLight.Score               = GetShadowLightScore( DynamicLight,
                                                               LocalCasterCount,
                                                               pActivePlayer );
        return TRUE;
    }

    //---------------------------------------------------------------------

    const vector3& GetPointShadowFaceDirection( s32 iFace )
    {
        ASSERT( iFace >= 0 );
        ASSERT( iFace < POINT_SHADOW_FACE_COUNT );

        static xbool   s_Initialized = FALSE;
        static vector3 s_FaceDirections[POINT_SHADOW_FACE_COUNT];

        if( !s_Initialized )
        {
            for( s32 i = 0; i < POINT_SHADOW_FACE_COUNT; i++ )
            {
                matrix4 FaceL2W;
                FaceL2W.Setup( vector3( 1.0f, 1.0f, 1.0f ),
                               radian3( kPointShadowFaces[i].Pitch,
                                        kPointShadowFaces[i].Yaw,
                                        R_0 ),
                               vector3( 0.0f, 0.0f, 0.0f ) );

                s_FaceDirections[i] = FaceL2W.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
                if( !s_FaceDirections[i].SafeNormalize() )
                    s_FaceDirections[i].Set( 0.0f, 0.0f, 1.0f );
            }

            s_Initialized = TRUE;
        }

        return s_FaceDirections[iFace];
    }

    //---------------------------------------------------------------------

    xbool PointShadowFaceAffectsCaster( const shadow_cone_test&     Cone,
                                        const shadow_caster_record& Caster )
    {
        return SphereIntersectsFiniteCone( Caster.WorldBBoxCenter,
                                           Caster.WorldBBoxRadius,
                                           Cone );
    }

    //---------------------------------------------------------------------

    u64 BuildFullShadowLightSourceMask( const shadow_light& ShadowLight,
                                        s32                 SourceBase )
    {
        u64 SourceMask = 0;

        for( s32 iSource = 0; iSource < ShadowLight.SourceCount; iSource++ )
            SourceMask |= ( (u64)1 << ( SourceBase + iSource ) );

        return SourceMask;
    }

    //---------------------------------------------------------------------

    u64 BuildShadowCasterSourceMask( const shadow_light&         ShadowLight,
                                     s32                         SourceBase,
                                     const shadow_caster_record& Caster,
                                     u64                         LightSourceMask,
                                     const shadow_cone_test*     pPointFaceCones )
    {
        if( ShadowLight.SourceCount <= 0 )
            return 0;

        if( ShadowLight.Shape != light_mgr::LIGHT_SHAPE_OMNI )
            return LightSourceMask;

        const s32 NFaces = ( ShadowLight.SourceCount < POINT_SHADOW_FACE_COUNT ) ?
                           ShadowLight.SourceCount :
                           POINT_SHADOW_FACE_COUNT;
        u64       SourceMask = 0;

        ASSERT( pPointFaceCones );
        for( s32 iFace = 0; iFace < NFaces; iFace++ )
        {
            if( PointShadowFaceAffectsCaster( pPointFaceCones[iFace], Caster ) )
            {
                SourceMask |= ( (u64)1 << ( SourceBase + iFace ) );
            }
        }

        return SourceMask;
    }

    //---------------------------------------------------------------------

    void AddShadowCastersInLight( const xarray<shadow_caster_record>& RelevantCasters,
                                  const shadow_light&                 ShadowLight,
                                  xarray<shadow_caster_mask_record>&  PendingCasterMasks,
                                  s32                                 SourceBase )
    {
        const s32 EndRelevantCaster = ShadowLight.RelevantCasterStart + ShadowLight.RelevantCasterCount;
        const u64 LightSourceMask   = BuildFullShadowLightSourceMask( ShadowLight, SourceBase );
        shadow_cone_test PointFaceCones[POINT_SHADOW_FACE_COUNT];
        const shadow_cone_test* pPointFaceCones = NULL;

        if( ShadowLight.Shape == light_mgr::LIGHT_SHAPE_OMNI )
        {
            const s32 NFaces = ( ShadowLight.SourceCount < POINT_SHADOW_FACE_COUNT ) ?
                               ShadowLight.SourceCount :
                               POINT_SHADOW_FACE_COUNT;

            for( s32 iFace = 0; iFace < NFaces; iFace++ )
            {
                SetupShadowConeTest( PointFaceCones[iFace],
                                     ShadowLight.Pos,
                                     GetPointShadowFaceDirection( iFace ),
                                     ShadowLight.PointFaceCoverageCos,
                                     ShadowLight.Radius );
            }

            pPointFaceCones = PointFaceCones;
        }

        for( s32 iRelevantCaster = ShadowLight.RelevantCasterStart;
             iRelevantCaster < EndRelevantCaster;
             iRelevantCaster++ )
        {
            const shadow_caster_record& Caster = RelevantCasters[iRelevantCaster];
            const u64                   SourceMask = BuildShadowCasterSourceMask( ShadowLight,
                                                                                  SourceBase,
                                                                                  Caster,
                                                                                  LightSourceMask,
                                                                                  pPointFaceCones );

            if( SourceMask == 0 )
                continue;

            shadow_caster_mask_record& Record = PendingCasterMasks.Append();
            Record.Caster     = Caster;
            Record.SourceMask = SourceMask;
        }
    }

    //---------------------------------------------------------------------

    void MergeShadowCasterMasks( xarray<shadow_caster_mask_record>& PendingCasterMasks,
                                 xarray<shadow_caster_record>&      ppCasters,
                                 xarray<u64>&                       CasterMasks )
    {
        if( PendingCasterMasks.GetCount() <= 0 )
            return;

        x_qsort( PendingCasterMasks.GetPtr(),
                 PendingCasterMasks.GetCount(),
                 sizeof(shadow_caster_mask_record),
                 ShadowCasterMaskCompareFn );

        for( s32 i = 0; i < PendingCasterMasks.GetCount(); i++ )
        {
            const shadow_caster_mask_record& Record = PendingCasterMasks[i];
            const s32                        iLast  = ppCasters.GetCount() - 1;

            if( ( iLast < 0 ) || !SameShadowCaster( ppCasters[iLast], Record.Caster ) )
            {
                ppCasters.Append( Record.Caster );
                CasterMasks.Append( Record.SourceMask );
                continue;
            }

            CasterMasks[iLast] |= Record.SourceMask;
        }
    }

    //---------------------------------------------------------------------

    void CollectShadowCastersForLights( const xarray<shadow_caster_record>& RelevantCasters,
                                        const shadow_light*                 pShadowLights,
                                        s32                                 NShadowLights,
                                        xarray<shadow_caster_record>&       ppCasters,
                                        xarray<u64>&                        CasterMasks )
    {
        s32 SourceBase = 0;
        xarray<shadow_caster_mask_record> PendingCasterMasks;
        PendingCasterMasks.SetGrowAmount( MAX( 128, RelevantCasters.GetCount() ) );

        for( s32 iLight = 0; iLight < NShadowLights; iLight++ )
        {
            const shadow_light& ShadowLight = pShadowLights[iLight];

            AddShadowCastersInLight( RelevantCasters,
                                     ShadowLight,
                                     PendingCasterMasks,
                                     SourceBase );
            SourceBase += ShadowLight.SourceCount;
        }

        MergeShadowCasterMasks( PendingCasterMasks,
                                ppCasters,
                                CasterMasks );
    }

    void AddPointShadowSources( const vector3& LightPos,
                                f32            LightRadius,
                                f32            LightFalloff,
                                s32            ShadowMapResolution,
                                s32            ShadowPriority,
                                f32            ShadowScore )
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
                                             LightFalloff,
                                             ShadowMapResolution,
                                             ShadowPriority,
                                             ShadowScore );
        }
    }

    //---------------------------------------------------------------------

    void AddSpotShadowSource( const vector3& LightPos,
                              const vector3& LightDirection,
                              radian         ShadowFOV,
                              f32            LightRadius,
                              f32            LightFalloff,
                              s32            ShadowMapResolution,
                              s32            ShadowPriority,
                              f32            ShadowScore )
    {
        matrix4 SpotL2W;
        SpotL2W.Setup( vector3( 1.0f, 1.0f, 1.0f ),
                       radian3( LightDirection.GetPitch(), LightDirection.GetYaw(), R_0 ),
                       LightPos );

        render::AddSpotShadowMapSource( SpotL2W,
                                        ShadowFOV,
                                        LightRadius,
                                        LightFalloff,
                                        ShadowMapResolution,
                                        ShadowPriority,
                                        ShadowScore );
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
                                       ShadowLight.Falloff,
                                       ShadowLight.ShadowMapResolution,
                                       ShadowLight.ShadowPriority,
                                       ShadowLight.Score );
            }
            else
            {
                AddSpotShadowSource( ShadowLight.Pos,
                                     ShadowLight.Direction,
                                     ShadowLight.FOV,
                                     ShadowLight.Radius,
                                     ShadowLight.Falloff,
                                     ShadowLight.ShadowMapResolution,
                                     ShadowLight.ShadowPriority,
                                     ShadowLight.Score );
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
            if( !SortsBeforeShadowLight( Light, pLights[NLights - 1] ) )
                return;

            NLights--;
        }

        s32 InsertAt = NLights;
        while( InsertAt > 0 )
        {
            if( !SortsBeforeShadowLight( Light, pLights[InsertAt - 1] ) )
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

    xbool AtlasFitsShadowLights( const shadow_light* pShadowLights,
                                 s32                 NShadowLights )
    {
        shadow_atlas_entry Entries[MAX_SHADOW_SOURCES];
        s32              NEntries = 0;

        for( s32 iLight = 0; iLight < NShadowLights; iLight++ )
        {
            const shadow_light& ShadowLight = pShadowLights[iLight];
            for( s32 iSource = 0; iSource < ShadowLight.SourceCount; iSource++ )
            {
                ASSERT( NEntries < MAX_SHADOW_SOURCES );
                Entries[NEntries].SourceIndex    = NEntries;
                Entries[NEntries].TileSize       = ClampShadowMapResolution( ShadowLight.ShadowMapResolution );
                Entries[NEntries].ShadowPriority = ShadowLight.ShadowPriority;
                Entries[NEntries].ShadowScore    = ShadowLight.Score;
                Entries[NEntries].AtlasTileX     = 0;
                Entries[NEntries].AtlasTileY     = 0;
                NEntries++;
            }
        }

        if( NEntries <= 0 )
            return TRUE;

        return ( GetShadowAtlasSizeForEntries( Entries, NEntries ) > 0 );
    }

    //---------------------------------------------------------------------

    s32 FindShadowLightForResolutionDrop( const shadow_light* pShadowLights,
                                          s32                 NShadowLights,
                                          s32                 ShadowPriority )
    {
        s32 iBestLight = -1;

        for( s32 iLight = 0; iLight < NShadowLights; iLight++ )
        {
            const shadow_light& ShadowLight = pShadowLights[iLight];
            if( ShadowLight.ShadowPriority != ShadowPriority )
                continue;
            if( ShadowLight.ShadowMapResolution <= 256 )
                continue;

            if( iBestLight < 0 )
            {
                iBestLight = iLight;
                continue;
            }

            const shadow_light& BestLight = pShadowLights[iBestLight];
            if( ShadowLight.Score < BestLight.Score )
            {
                iBestLight = iLight;
                continue;
            }

            if( ( ShadowLight.Score == BestLight.Score ) &&
                ( ShadowLight.ShadowMapResolution > BestLight.ShadowMapResolution ) )
            {
                iBestLight = iLight;
            }
        }

        return iBestLight;
    }

    //---------------------------------------------------------------------

    s32 FindShadowLightForRemoval( const shadow_light* pShadowLights,
                                   s32                 NShadowLights,
                                   s32                 ShadowPriority )
    {
        s32 iBestLight = -1;

        for( s32 iLight = 0; iLight < NShadowLights; iLight++ )
        {
            const shadow_light& ShadowLight = pShadowLights[iLight];
            if( ShadowLight.ShadowPriority != ShadowPriority )
                continue;

            if( iBestLight < 0 )
            {
                iBestLight = iLight;
                continue;
            }

            const shadow_light& BestLight = pShadowLights[iBestLight];
            if( ShadowLight.Score < BestLight.Score )
            {
                iBestLight = iLight;
                continue;
            }

            if( ( ShadowLight.Score == BestLight.Score ) &&
                ( ShadowLight.ShadowMapResolution > BestLight.ShadowMapResolution ) )
            {
                iBestLight = iLight;
            }
        }

        return iBestLight;
    }

    //---------------------------------------------------------------------

    void RemoveShadowLight( shadow_light* pShadowLights,
                            s32&          NShadowLights,
                            s32           RemoveIndex )
    {
        ASSERT( ( RemoveIndex >= 0 ) && ( RemoveIndex < NShadowLights ) );

        for( s32 iLight = RemoveIndex + 1; iLight < NShadowLights; iLight++ )
            pShadowLights[iLight - 1] = pShadowLights[iLight];

        NShadowLights--;
    }

    //---------------------------------------------------------------------

    void ResolveAtlasBudget( shadow_light* pShadowLights,
                             s32&          NShadowLights )
    {
        for( s32 ShadowPriority = kMinShadowPriority;
             ShadowPriority <= kMaxShadowPriority;
             ShadowPriority++ )
        {
            while( !AtlasFitsShadowLights( pShadowLights, NShadowLights ) )
            {
                const s32 iLightToReduce = FindShadowLightForResolutionDrop( pShadowLights,
                                                                              NShadowLights,
                                                                              ShadowPriority );
                if( iLightToReduce >= 0 )
                {
                    pShadowLights[iLightToReduce].ShadowMapResolution =
                        ReduceShadowMapResolution( pShadowLights[iLightToReduce].ShadowMapResolution );
                    continue;
                }

                const s32 iLightToRemove = FindShadowLightForRemoval( pShadowLights,
                                                                      NShadowLights,
                                                                      ShadowPriority );
                if( iLightToRemove >= 0 )
                {
                    RemoveShadowLight( pShadowLights, NShadowLights, iLightToRemove );
                    continue;
                }

                break;
            }

            if( AtlasFitsShadowLights( pShadowLights, NShadowLights ) )
                return;
        }
    }

    //---------------------------------------------------------------------

    s32 GetAtlasGuardTexels( f32 FilterRadius )
    {
        const s32 GuardTexels = (s32)x_ceil( ( FilterRadius * 4.0f ) + 2.0f );
        return ( GuardTexels > 0 ) ? GuardTexels : 1;
    }
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

shadow_map_mgr::shadow_map_mgr( void ) :
    m_SourceCount      ( 0 ),
    m_PointFaceCount   ( 0 ),
    m_PointLightCount  ( 0 ),
    m_AtlasSourceCount ( 0 ),
    m_AtlasSize        ( 0 ),
    m_AtlasLayoutDirty ( FALSE )
{
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
    m_AtlasSourceCount  = 0;
    m_AtlasSize         = 0;
    m_AtlasLayoutDirty  = FALSE;
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
                                               f32             LightFalloff,
                                               s32             ShadowMapResolution ) const
{
    ShadowMapResolution = ClampShadowMapResolution( ShadowMapResolution );

    f32       ReceiveNearZ = ComputeSpotShadowReceiveNearZ( LightRadius );
    f32       NearZ        = MAX( 0.5f, LightRadius * 0.01f );

    if( SourceType == SHADOW_SOURCE_POINT_FACE )
    {
        ReceiveNearZ = ComputePointShadowReceiveNearZ( LightRadius );
        NearZ        = ComputePointShadowNearZ( LightRadius );
    }

    NearZ = MAX( 0.01f, NearZ );

    f32 FarZ  = MAX( NearZ + 1.0f, LightRadius );

    matrix4 W2Proj = L2W;
    W2Proj.InvertRT();

    const f32 TanHalfFOV = x_tan( FOV * 0.5f );
    f32       Scale      = ( TanHalfFOV > 0.0f ) ? ( 1.0f / TanHalfFOV ) : 1.0f;

    if( SourceType == SHADOW_SOURCE_POINT_FACE )
    {
        const f32 CoverageScale = (f32)ShadowMapResolution /
                                  ( (f32)ShadowMapResolution + kPointShadowProjectionOverlapTexels );
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

    Dest.Type                = SourceType;
    Dest.PointLightIndex     = -1;
    Dest.FaceIndex           = -1;
    Dest.RequestedResolution = ShadowMapResolution;
    Dest.ShadowPriority      = kMinShadowPriority;
    Dest.ShadowScore         = 0.0f;
    Dest.WorldToClip         = Proj2Clip * W2Proj;
    Dest.WorldToAtlas        = Dest.WorldToClip;
    Dest.FaceLightDirFalloff.Zero();
    Dest.FaceLightData.Zero();
    Dest.LightFalloff        = LightFalloff;
    Dest.NearZ               = NearZ;
    Dest.ReceiveNearZ        = ReceiveNearZ;
    Dest.FarZ                = FarZ;
    Dest.AtlasX              = 0;
    Dest.AtlasY              = 0;
    Dest.AtlasWidth          = ShadowMapResolution;
    Dest.AtlasHeight         = ShadowMapResolution;

    const vector3 LightPos = L2W.GetTranslation();
    const vector3 Extent( FarZ, FarZ, FarZ );
    Dest.LightPosRadius = vector4( LightPos.GetX(), LightPos.GetY(), LightPos.GetZ(), LightRadius );
    Dest.WorldBBox.Set( LightPos - Extent,
                        LightPos + Extent );
}

//=========================================================================

void shadow_map_mgr::UpdateAtlasLayout( void )
{
    m_AtlasSize = 0;

    if( m_AtlasSourceCount <= 0 )
        return;

    const s32 GuardTexels = ( m_AtlasSourceCount > 1 ) ? GetAtlasGuardTexels( kShadowAtlasFilterRadius ) : 0;
    shadow_atlas_entry Entries[MAX_SHADOW_SOURCES];
    s32             NEntries = 0;

    for( s32 i = 0; i < m_SourceCount; i++ )
    {
        const shadow_source& Source = m_Sources[i];
        ASSERT( NEntries < MAX_SHADOW_SOURCES );
        Entries[NEntries].SourceIndex    = i;
        Entries[NEntries].TileSize       = ClampShadowMapResolution( Source.RequestedResolution );
        Entries[NEntries].ShadowPriority = Source.ShadowPriority;
        Entries[NEntries].ShadowScore    = Source.ShadowScore;
        Entries[NEntries].AtlasTileX     = 0;
        Entries[NEntries].AtlasTileY     = 0;
        NEntries++;
    }

    s32 AtlasSize = GetShadowAtlasSizeForEntries( Entries, NEntries );
    while( AtlasSize <= 0 )
    {
        s32 iLargestEntry = -1;
        for( s32 i = 0; i < NEntries; i++ )
        {
            if( Entries[i].TileSize <= 256 )
                continue;

            if( iLargestEntry < 0 )
            {
                iLargestEntry = i;
                continue;
            }

            if( Entries[i].ShadowPriority < Entries[iLargestEntry].ShadowPriority )
            {
                iLargestEntry = i;
                continue;
            }

            if( ( Entries[i].ShadowPriority == Entries[iLargestEntry].ShadowPriority ) &&
                ( Entries[i].ShadowScore < Entries[iLargestEntry].ShadowScore ) )
            {
                iLargestEntry = i;
                continue;
            }

            if( ( Entries[i].ShadowPriority == Entries[iLargestEntry].ShadowPriority ) &&
                ( Entries[i].ShadowScore == Entries[iLargestEntry].ShadowScore ) &&
                ( Entries[i].TileSize > Entries[iLargestEntry].TileSize ) )
            {
                iLargestEntry = i;
            }
        }

        if( iLargestEntry < 0 )
        {
            ASSERT( FALSE );
            break;
        }

        Entries[iLargestEntry].TileSize = ReduceShadowMapResolution( Entries[iLargestEntry].TileSize );
        AtlasSize = GetShadowAtlasSizeForEntries( Entries, NEntries );
    }

    m_AtlasSize = AtlasSize;

    for( s32 i = 0; i < NEntries; i++ )
    {
        shadow_source& Source = m_Sources[Entries[i].SourceIndex];
        const s32      TileSize = Entries[i].TileSize;

        Source.AtlasX      = Entries[i].AtlasTileX;
        Source.AtlasY      = Entries[i].AtlasTileY;
        Source.AtlasWidth  = TileSize;
        Source.AtlasHeight = TileSize;

        if( GuardTexels > 0 )
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

        if( Source.Type == SHADOW_SOURCE_POINT_FACE )
        {
            Source.FaceLightData.GetX() = ComputePointFaceCoverageCos( Source.AtlasWidth );
        }

        Source.WorldToAtlas = Source.WorldToClip;
        Source.WorldToAtlas.Scale( vector3( 0.5f, -0.5f, 1.0f ) );
        Source.WorldToAtlas.Translate( vector3( 0.5f, 0.5f, 0.0f ) );
        Source.WorldToAtlas.Scale( vector3( (f32)Source.AtlasWidth  / (f32)m_AtlasSize,
                                            (f32)Source.AtlasHeight / (f32)m_AtlasSize,
                                            1.0f ) );
        Source.WorldToAtlas.Translate( vector3( (f32)Source.AtlasX / (f32)m_AtlasSize,
                                                (f32)Source.AtlasY / (f32)m_AtlasSize,
                                                0.0f ) );
    }
}

//=========================================================================

xbool shadow_map_mgr::AddPointSource( const matrix4& L2W,
                                      radian         FOV,
                                      f32            LightRadius,
                                      f32            LightFalloff,
                                      s32            ShadowMapResolution,
                                      s32            ShadowPriority,
                                      f32            ShadowScore )
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
                              LightFalloff,
                              ShadowMapResolution );

    vector3 FaceDirection = L2W.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
    if( !FaceDirection.SafeNormalize() )
        FaceDirection.Set( 0.0f, 0.0f, 1.0f );

    Source.FaceLightDirFalloff.Set( FaceDirection.GetX(),
                                    FaceDirection.GetY(),
                                    FaceDirection.GetZ(),
                                    LightFalloff );
    Source.FaceLightData.Set( ComputePointFaceCoverageCos( ShadowMapResolution ),
                              Source.NearZ,
                              Source.FarZ,
                              kShadowSourceTypePointFace );
    Source.ShadowPriority = ClampShadowPriority( ShadowPriority );
    Source.ShadowScore    = ShadowScore;
    Source.PointLightIndex = m_PointFaceCount / POINT_SHADOW_FACE_COUNT;
    Source.FaceIndex       = m_PointFaceCount % POINT_SHADOW_FACE_COUNT;
    m_SourceCount++;
    m_PointFaceCount++;
    m_AtlasSourceCount++;
    m_AtlasLayoutDirty = TRUE;    

    if( Source.FaceIndex == 0 )
        m_PointLightCount++;

    return TRUE;
}

//=========================================================================

xbool shadow_map_mgr::AddSpotSource( const matrix4& L2W,
                                     radian         FOV,
                                     f32            LightRadius,
                                     f32            LightFalloff,
                                     s32            ShadowMapResolution,
                                     s32            ShadowPriority,
                                     f32            ShadowScore )
{
    if( m_SourceCount >= MAX_SHADOW_SOURCES )
        return FALSE;

    shadow_source& Source = m_Sources[m_SourceCount];
    ComputePerspectiveSource( Source,
                              SHADOW_SOURCE_SPOT,
                              L2W,
                              FOV,
                              LightRadius,
                              LightFalloff,
                              ShadowMapResolution );

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
                              kShadowSourceTypeSpot );
    Source.ShadowPriority = ClampShadowPriority( ShadowPriority );
    Source.ShadowScore    = ShadowScore;
    m_SourceCount++;
    m_AtlasSourceCount++;
    m_AtlasLayoutDirty = TRUE;
    return TRUE;
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

s32 shadow_map_mgr::GetAtlasSize( void ) const
{
    return ( m_AtlasSize > 0 ) ? m_AtlasSize : MAX_SHADOW_ATLAS_SIZE;
}

//=========================================================================

s32 shadow_map_mgr::GetAtlasSourceCount( void ) const
{
    return m_AtlasSourceCount;
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

    (void)ppCasterCandidates;

    shadow_light ShadowLightCandidates[light_mgr::MAX_DYNAMIC_LIGHTS];
    shadow_light ShadowLights[light_mgr::MAX_DYNAMIC_LIGHTS];
    s32          NCandidateShadowLights = 0;
    player*      pActivePlayer          = SMP_UTIL_GetActivePlayer();
    xarray<shadow_caster_record> RelevantCasters;
    xarray<shadow_caster_record> ShadowCasters;
    xarray<u64>                  CasterMasks;
    RelevantCasters.SetGrowAmount( MAX( 128, NCasterCandidates ) );
    ShadowCasters.SetGrowAmount( MAX( 128, NCasterCandidates ) );
    CasterMasks.SetGrowAmount( MAX( 128, NCasterCandidates ) );

    for( s32 iLight = 0; iLight < g_LightMgr.GetNDynamicLights(); iLight++ )
    {
        shadow_light ShadowLight;
        if( !BuildShadowLight( ShadowLight,
                               iLight,
                               pActivePlayer,
                               RelevantCasters ) )
        {
            continue;
        }

        InsertShadowLight( ShadowLightCandidates,
                           NCandidateShadowLights,
                           ShadowLight );
    }

    s32 NShadowLights = SelectShadowLights( ShadowLightCandidates,
                                            NCandidateShadowLights,
                                            ShadowLights );

    ResolveAtlasBudget( ShadowLights, NShadowLights );

    if( NShadowLights <= 0 )
        return;

    CollectShadowCastersForLights( RelevantCasters,
                                   ShadowLights,
                                   NShadowLights,
                                   ShadowCasters,
                                   CasterMasks );

    if( eng_Begin( "Shadow Map" ) )
    {
        render::BeginShadowCreation();
        AddShadowSources( ShadowLights, NShadowLights );
        FinalizeSources();

        for( s32 i = 0; i < ShadowCasters.GetCount(); i++ )
        {
            const shadow_caster_record& Caster = ShadowCasters[i];

            if( Caster.Type == shadow_caster_record::CASTER_OBJECT )
            {
                ASSERT( Caster.pObject );
                Caster.pObject->OnRenderShadowCast( CasterMasks[i] );
            }
            else
            {
                ASSERT( Caster.pPlaySurface );
                if( !Caster.pPlaySurface->RenderInst.IsNull() )
                {
                    render::AddRigidCasterSimple( Caster.pPlaySurface->RenderInst,
                                                  &Caster.pPlaySurface->L2W,
                                                  CasterMasks[i] );
                }
            }
        }

        render::EndShadowCreation();
        eng_End();
    }
}
