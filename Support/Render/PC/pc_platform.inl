//=============================================================================
//
//  PC Specific Render Routines
//
//=============================================================================

#include "../LightMgr.hpp"
#include "../platform_Render.hpp"
#include "../ProjTextureMgr.hpp"
#include "../../Decals/DecalMgr.hpp"
#include "VertexMgr.hpp"
#include "SoftVertexMgr.hpp"
#include "GeomStorage.hpp"
#include "GeomMgr/GeomMgr.hpp"
#include "DecalRenderer.hpp"
#include "ForwardRenderMgr.hpp"
#include "ProjectionAtlas.hpp"
#include "ShadowMgr.hpp"
#include "PostMgr/PostMgr.hpp"
#include "PrimitiveMgr/PrimitiveMgr.hpp"
#include "PrimitiveMgr/PrimitiveRenderer.hpp"
#include "GBufferMgr.hpp"
#include "e_Engine.hpp"

//=============================================================================
// Static data specific to the pc-implementation
//=============================================================================

static u32                     s_RigidLightingQueryCount = 0;
static u32                     s_RigidLightingSetupCount = 0;
static u32                     s_SkinLightingQueryCount  = 0;
static light_mgr::collection_stats s_LightStatsStart = {};

//=============================================================================

static void platform_ExecuteGeomPackets( void );

//=============================================================================

//=============================================================================
// Implementation
//=============================================================================

static
void platform_Init( void )
{
    g_GBufferMgr.Init();
    g_GeomMgr.Init();
    g_ShadowMgr.Init();
    g_PostMgr.Init(); 
    g_DecalRenderer.Init();
    g_PrimitiveMgr.Init();
    g_PrimitiveRenderer.Init( g_PrimitiveMgr );
    g_ForwardRenderMgr.Init( g_GeomMgr, g_PrimitiveMgr, g_DecalRenderer );
    g_RigidVertMgr.Init( sizeof( rigid_geom::vertex ) );
    g_SkinVertMgr.Init();
    g_GeomStorage.Init( g_RigidVertMgr, g_SkinVertMgr );
}

//=============================================================================

static
void platform_Kill( void )
{
    g_GeomStorage.Kill();
    g_SkinVertMgr.Kill(); 
    g_RigidVertMgr.Kill();
    g_ForwardRenderMgr.Kill();
    g_PrimitiveRenderer.Kill();
    g_PrimitiveMgr.Kill();
    g_DecalRenderer.Kill();
    g_PostMgr.Kill();
    g_ShadowMgr.Kill();
    g_GeomMgr.Kill();
    g_GBufferMgr.Kill();
}

//=============================================================================

static
void platform_BeginSession( u32 nPlayers )
{
    (void)nPlayers;
    // TODO:
}

//=============================================================================

static
void platform_EndSession( void )
{
    // TODO:
}

//=============================================================================

static
xhandle platform_RegisterRigidGeom( const rigid_geom& Geom )
{
    return g_GeomStorage.AddRigid( Geom );
}

//=============================================================================

static
void platform_UnregisterRigidGeom( xhandle hGeom )
{
    g_GeomStorage.RemoveRigid( hGeom );
}

//=============================================================================

static
xhandle platform_RegisterSkinGeom( const skin_geom& Geom )
{
    return g_GeomStorage.AddSkin( Geom );
}

//=============================================================================

static
void platform_UnregisterSkinGeom( xhandle hGeom )
{
    g_GeomStorage.RemoveSkin( hGeom );
}

//-----------------------------------------------------------------------------
//
// Forward primitive entry points. PrimitiveRenderer owns modern submission.
//
//-----------------------------------------------------------------------------

static
xbool platform_SubmitPrimitives( render::primitive_draw_desc const& Desc,
                                 matrix4 const&                     LocalToWorld,
                                 render::primitive_vertex const*    pVertices,
                                 s32                                nVertices,
                                 u16 const*                         pIndices,
                                 s32                                nIndices )
{
    return g_PrimitiveRenderer.SubmitPrimitives( Desc, LocalToWorld, pVertices, nVertices, pIndices, nIndices );
}

//=============================================================================

static
xbool platform_SetDepthRect( irect const& Rect, f32 Depth )
{
    return g_PrimitiveRenderer.SubmitDepthRect( Rect, Depth );
}

//=============================================================================

static
xbool platform_BeginPrimitiveRender( void )
{
    if( !g_PrimitiveRenderer.BeginSubmission() )
    {
        x_DebugMsg( "PCRender: failed to begin primitive submission\n" );
        return FALSE;
    }

    return TRUE;
}

//=============================================================================

static
void platform_EndPrimitiveRender( void )
{
    if( g_PrimitiveRenderer.IsSubmissionActive() &&
        !g_PrimitiveRenderer.EndSubmission() )
    {
        x_DebugMsg( "PCRender: failed to finish primitive submission\n" );
    }
}

//=============================================================================

static
xbool platform_SubmitDecalBatch( render::decal_draw_desc const& Desc,
                                 cubemap const*                  pCubeMap,
                                 render::decal_vertex const*     pVertices,
                                 s32                             nVertices,
                                 u16 const*                      pIndices,
                                 s32                             nIndices )
{
    return g_DecalRenderer.Submit( Desc, pCubeMap, pVertices, nVertices, pIndices, nIndices );
}

//=============================================================================

static
void platform_ReserveDecalSubmissionCapacity( s32 nVertices, s32 nIndices, s32 nDraws )
{
    if ( !g_DecalRenderer.Reserve( nVertices, nIndices, nDraws ) )
    {
        x_DebugMsg( "PCRender: failed to reserve decal submission capacity\n" );
    }
}

//=============================================================================

static
void platform_ExecuteForwardRender( render::forward_render_stage Stage )
{
    frame_render_targets Targets;
    if( !g_GBufferMgr.GetFrameTargets( Targets ) )
    {
        g_ForwardRenderMgr.DiscardQueuedDraws();
        x_DebugMsg( "PCRender: forward frame targets are unavailable\n" );
        return;
    }

    xbool Result = FALSE;
    switch( Stage )
    {
        case render::FORWARD_RENDER_PRE_EFFECTS:
            Result = g_ForwardRenderMgr.ExecutePreEffects( Targets );
            break;

        case render::FORWARD_RENDER_POST_EFFECTS:
            Result = g_ForwardRenderMgr.ExecutePostEffects( Targets );
            break;

        case render::FORWARD_RENDER_ALL:
            Result = g_ForwardRenderMgr.Execute( Targets );
            break;

        default:
            g_ForwardRenderMgr.DiscardQueuedDraws();
            break;
    }

    if( !Result )
        x_DebugMsg( "PCRender: failed to execute forward draw queue stage=%d\n", static_cast<s32>( Stage ) );
}

//=============================================================================

static
void platform_ResetAfterException( void )
{
    g_PrimitiveRenderer.DiscardSubmission();
    g_ForwardRenderMgr.DiscardQueuedDraws();
    g_GeomMgr.ClearDistortionState();
}

//=============================================================================

static
void* platform_CalculateRigidLighting( const matrix4&   L2W,
                                       const bbox&      WorldBBox )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "platform_CalculateRigidLighting" );
    s_RigidLightingQueryCount++;
    
    void* pResult = NULL;
    
    // Grab lights
    s32 NLights = g_LightMgr.CollectLights( WorldBBox, MAX_GEOM_LIGHTS );
    
    if( NLights )
    {
        // Try allocate
        cb_geom_lighting* pLighting = (cb_geom_lighting*)smem_BufferAlloc( sizeof(cb_geom_lighting) );
        x_memset( pLighting, 0, sizeof(cb_geom_lighting) );
        pLighting->LightCount = NLights;
      
        for( s32 i = 0; i < NLights; i++ )
        {
            vector3 Pos;
            f32     Radius;
            xcolor  Col;
            f32     Falloff;
            s32     Shape;
            vector3 Direction;
            f32     InnerConeCos;
            f32     OuterConeCos;
            s32     CookieIndex;
            vector3 CookieU;
            vector3 CookieV;
            
            g_LightMgr.GetCollectedLightInfo( i,
                                             Pos,
                                             Radius,
                                             Col,
                                             Falloff,
                                             Shape,
                                             Direction,
                                             InnerConeCos,
                                             OuterConeCos );
            g_LightMgr.GetCollectedLightCookie( i,
                                                CookieIndex,
                                                CookieU,
                                                CookieV );
            
            // Setup rigid lights
            pLighting->LightVec[i].Set( Pos.GetX(),
                                        Pos.GetY(),
                                        Pos.GetZ(),
                                        Radius );
            
            pLighting->LightCol[i].Set( (f32)Col.R / 255.0f,
                                        (f32)Col.G / 255.0f,
                                        (f32)Col.B / 255.0f,
                                        Falloff );

            pLighting->LightDir[i].Set( Direction.GetX(),
                                        Direction.GetY(),
                                        Direction.GetZ(),
                                        (Shape == light_mgr::LIGHT_SHAPE_SPOT) ? 1.0f : 0.0f );

            pLighting->LightCone[i].Set( InnerConeCos,
                                         OuterConeCos,
                                         0.0f,
                                         0.0f );

            pLighting->LightCookieU[i].Set( CookieU.GetX(),
                                            CookieU.GetY(),
                                            CookieU.GetZ(),
                                            (CookieIndex >= 0) ? (f32)(CookieIndex + 1) : 0.0f );
            pLighting->LightCookieV[i].Set( CookieV.GetX(),
                                             CookieV.GetY(),
                                             CookieV.GetZ(),
                                             0.0f );
            pLighting->DynamicLightIndex[i] = g_LightMgr.GetCollectedDynamicLightIndex( i );
        }
        
        pResult = pLighting;
        s_RigidLightingSetupCount++;
    }
    
    // Store in render instance
    return pResult;
}

//=============================================================================

static
void* platform_CalculateSkinLighting( u32            Flags,
                                      const matrix4& L2W,
                                      const bbox&    BBox,
                                      xcolor         Ambient )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "platform_CalculateSkinLighting" );
    s_SkinLightingQueryCount++;
    
    void* pResult = NULL;

    // Try allocate
    cb_geom_lighting* pLighting = (cb_geom_lighting*)smem_BufferAlloc( sizeof(cb_geom_lighting) );
    x_memset( pLighting, 0, sizeof(cb_geom_lighting) );
    
    // Setup ambient
    pLighting->AmbCol.Set( (f32)Ambient.R / 255.0f,
                           (f32)Ambient.G / 255.0f,
                           (f32)Ambient.B / 255.0f,
                           1.0f );

    // Mutant vision uses the ambient color as forced glow without scene lighting.
    if( Flags & render::GLOWING )
    {
        return pLighting;
    }

    bbox WorldBBox = BBox;
    WorldBBox.Transform( L2W );

    s32 NSceneLights = g_LightMgr.CollectLights( WorldBBox, MAX_GEOM_LIGHTS );
    s32 LightIndex   = 0;

    for( s32 i = 0; ( i < NSceneLights ) && ( LightIndex < MAX_GEOM_LIGHTS ); i++, LightIndex++ )
    {
        vector3 Pos;
        f32     Radius;
        xcolor  Col;
        f32     Falloff;
        s32     Shape;
        vector3 Direction;
        f32     InnerConeCos;
        f32     OuterConeCos;
        s32     CookieIndex;
        vector3 CookieU;
        vector3 CookieV;

        g_LightMgr.GetCollectedLightInfo( i,
                                          Pos,
                                          Radius,
                                          Col,
                                          Falloff,
                                          Shape,
                                          Direction,
                                          InnerConeCos,
                                          OuterConeCos );
        g_LightMgr.GetCollectedLightCookie( i,
                                            CookieIndex,
                                            CookieU,
                                            CookieV );

        pLighting->LightVec[LightIndex].Set( Pos.GetX(),
                                             Pos.GetY(),
                                             Pos.GetZ(),
                                             Radius );

        pLighting->LightCol[LightIndex].Set( (f32)Col.R / 255.0f,
                                             (f32)Col.G / 255.0f,
                                             (f32)Col.B / 255.0f,
                                             Falloff );

        pLighting->LightDir[LightIndex].Set( Direction.GetX(),
                                             Direction.GetY(),
                                             Direction.GetZ(),
                                             (Shape == light_mgr::LIGHT_SHAPE_SPOT) ? 1.0f : 0.0f );

        pLighting->LightCone[LightIndex].Set( InnerConeCos,
                                              OuterConeCos,
                                              0.0f,
                                              0.0f );

        pLighting->LightCookieU[LightIndex].Set( CookieU.GetX(),
                                                 CookieU.GetY(),
                                                 CookieU.GetZ(),
                                                 (CookieIndex >= 0) ? (f32)(CookieIndex + 1) : 0.0f );
        pLighting->LightCookieV[LightIndex].Set( CookieV.GetX(),
                                                  CookieV.GetY(),
                                                  CookieV.GetZ(),
                                                  0.0f );
        pLighting->DynamicLightIndex[LightIndex] = g_LightMgr.GetCollectedDynamicLightIndex( i );
    }

    s32 NCharLights = g_LightMgr.CollectCharLightsOnly( L2W, BBox, MAX_GEOM_LIGHTS );
    for( s32 i = 0; ( i < NCharLights ) && ( LightIndex < MAX_GEOM_LIGHTS ); i++, LightIndex++ )
    {
        vector3 Dir;
        xcolor  Col;

        g_LightMgr.GetCollectedCharLight( i, Dir, Col );

        pLighting->LightVec[LightIndex].Set( Dir.GetX(),
                                             Dir.GetY(),
                                             Dir.GetZ(),
                                             0.0f );

        pLighting->LightCol[LightIndex].Set( (f32)Col.R / 255.0f,
                                             (f32)Col.G / 255.0f,
                                             (f32)Col.B / 255.0f,
                                             0.0f );

        pLighting->LightDir[LightIndex].Set( 0.0f,
                                              0.0f,
                                              0.0f,
                                              2.0f );
        pLighting->DynamicLightIndex[LightIndex] = -1;
    }

    pLighting->LightCount = LightIndex;
    
    pResult = pLighting;
    
    // Store in render instance
    return pResult;
}

static
void platform_ExecuteGeomPackets( void )
{
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/PrepareProjectionAtlas" );
        g_GBufferMgr.EndPass();

        if( !g_DecalRenderer.RequestProjectionTextures() ||
            !g_GeomMgr.PrepareProjectionAtlas() )
        {
            x_DebugMsg( "PCRender: failed to prepare projection atlas\n" );
            g_ForwardRenderMgr.DiscardQueuedDraws();
            return;
        }
    }

    static xprofile_counter GeomPackets =
        x_GetProfiler().RegisterCounter( "GeomPackets", "RenderCounter" );
    static xprofile_counter GeomRigidInstances =
        x_GetProfiler().RegisterCounter( "GeomRigidInstances", "RenderCounter" );
    static xprofile_counter GeomSkinInstances =
        x_GetProfiler().RegisterCounter( "GeomSkinInstances", "RenderCounter" );
    static xprofile_counter GeomLitInstances =
        x_GetProfiler().RegisterCounter( "GeomLitInstances", "RenderCounter" );
    static xprofile_counter GeomInstanceLights =
        x_GetProfiler().RegisterCounter( "GeomInstanceLights", "RenderCounter" );
    static xprofile_counter GeomLightingRecords =
        x_GetProfiler().RegisterCounter( "GeomLightingRecords", "RenderCounter" );
    static xprofile_counter GeomGBufferPackets =
        x_GetProfiler().RegisterCounter( "GeomGBufferPackets", "RenderCounter" );
    static xprofile_counter GeomGBufferGpuDraws =
        x_GetProfiler().RegisterCounter( "GeomGBufferGpuDraws", "RenderCounter" );
    static xprofile_counter GeomGBufferRigidIndirectRuns =
        x_GetProfiler().RegisterCounter( "GeomGBufferRigidIndirectRuns", "RenderCounter" );
    static xprofile_counter GeomGBufferRigidIndirectCommands =
        x_GetProfiler().RegisterCounter( "GeomGBufferRigidIndirectCommands", "RenderCounter" );
    static xprofile_counter GeomGBufferSkinIndirectRuns =
        x_GetProfiler().RegisterCounter( "GeomGBufferSkinIndirectRuns", "RenderCounter" );
    static xprofile_counter GeomGBufferSkinIndirectCommands =
        x_GetProfiler().RegisterCounter( "GeomGBufferSkinIndirectCommands", "RenderCounter" );
    static xprofile_counter GeomGBufferInstances =
        x_GetProfiler().RegisterCounter( "GeomGBufferInstances", "RenderCounter" );
    static xprofile_counter GeomGBufferSkinSectionDraws =
        x_GetProfiler().RegisterCounter( "GeomGBufferSkinSectionDraws", "RenderCounter" );
    static xprofile_counter GeomGBufferSubmittedIndices =
        x_GetProfiler().RegisterCounter( "GeomGBufferSubmittedIndices", "RenderCounter" );
    static xprofile_gauge GeomGBufferInstancesPerPacket =
        x_GetProfiler().RegisterGauge( "GeomGBufferInstancesPerPacket",
                                       XPROFILE_UNIT_COUNT,
                                       "RenderGauge" );
    static xprofile_gauge GeomGBufferDrawsPerPacket =
        x_GetProfiler().RegisterGauge( "GeomGBufferDrawsPerPacket",
                                       XPROFILE_UNIT_COUNT,
                                       "RenderGauge" );
    static xprofile_gauge GeomGBufferIndicesPerDraw =
        x_GetProfiler().RegisterGauge( "GeomGBufferIndicesPerDraw",
                                       XPROFILE_UNIT_COUNT,
                                       "RenderGauge" );
    static xprofile_gauge GeomInstancesPerPacket =
        x_GetProfiler().RegisterGauge( "GeomInstancesPerPacket",
                                       XPROFILE_UNIT_COUNT,
                                       "RenderGauge" );
    static xprofile_gauge GeomLightsPerLitInstance =
        x_GetProfiler().RegisterGauge( "GeomLightsPerLitInstance",
                                       XPROFILE_UNIT_COUNT,
                                       "RenderGauge" );
    static xprofile_gauge GeomLightingRecordReuse =
        x_GetProfiler().RegisterGauge( "GeomLightingRecordReuse",
                                       XPROFILE_UNIT_COUNT,
                                       "RenderGauge" );

    GeomPackets.Add( g_GeomMgr.GetPacketCount() );
    GeomRigidInstances.Add( g_GeomMgr.GetRigidInstanceCount() );
    GeomSkinInstances.Add( g_GeomMgr.GetSkinInstanceCount() );
    GeomLitInstances.Add( g_GeomMgr.GetLitInstanceCount() );
    GeomInstanceLights.Add( g_GeomMgr.GetInstanceLightCount() );
    const s32 GeomInstanceCount = g_GeomMgr.GetRigidInstanceCount() +
                                  g_GeomMgr.GetSkinInstanceCount();
    GeomInstancesPerPacket.Set( g_GeomMgr.GetPacketCount()
                              ? (f64)GeomInstanceCount / g_GeomMgr.GetPacketCount()
                              : 0.0 );
    GeomLightsPerLitInstance.Set( g_GeomMgr.GetLitInstanceCount()
                                ? (f64)g_GeomMgr.GetInstanceLightCount() /
                                  g_GeomMgr.GetLitInstanceCount()
                                : 0.0 );

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/Upload" );
        if( !g_GeomMgr.PrepareSharedShadowData() )
        {
            x_DebugMsg( "PCRender: failed to prepare shared shadow data\n" );
            g_ForwardRenderMgr.DiscardQueuedDraws();
            return;
        }
        if( !g_GeomMgr.UploadPackets() )
        {
            x_DebugMsg( "PCRender: failed to upload geometry packets\n" );
            g_ForwardRenderMgr.DiscardQueuedDraws();
            return;
        }
        if( !g_DecalRenderer.UploadQueuedDraws( g_GeomMgr ) )
        {
            x_DebugMsg( "PCRender: failed to upload decal batches\n" );
            g_ForwardRenderMgr.DiscardQueuedDraws();
            return;
        }
    }

    GeomLightingRecords.Add( g_GeomMgr.GetLightingRecordCount() );
    GeomLightingRecordReuse.Set( g_GeomMgr.GetLightingRecordCount()
                               ? (f64)g_GeomMgr.GetLitInstanceCount() /
                                 g_GeomMgr.GetLightingRecordCount()
                               : 0.0 );

    frame_render_targets Targets;
    if( !g_GBufferMgr.GetFrameTargets( Targets ) )
    {
        x_DebugMsg( "PCRender: geometry frame targets are unavailable\n" );
        g_ForwardRenderMgr.DiscardQueuedDraws();
        return;
    }

    geom_pass_desc GeomPass;
    GeomPass.TargetWidth  = Targets.pSceneColor->Desc.Width;
    GeomPass.TargetHeight = Targets.pSceneColor->Desc.Height;

    xbool Result = TRUE;
    u32 GBufferPacketCount = 0;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/ExecuteGBuffer" );
        for( s32 i = 0; i < g_GeomMgr.GetPacketCount(); ++i )
        {
            if( g_GeomMgr.GetPacketPass( i ) == GEOMETRY_PASS_GBUFFER )
                ++GBufferPacketCount;
        }

        if( GBufferPacketCount > 0 )
        {
            Result = g_GBufferMgr.SetGBufferTargets();
            if( !Result )
                x_DebugMsg( "PCRender: failed to bind G-buffer targets\n" );
            else
            {
                Result = g_GeomMgr.ExecuteGBuffer( GeomPass );
                if( !Result )
                    x_DebugMsg( "PCRender: failed to execute G-buffer packets\n" );
            }
        }
    }

    const u32 GBufferGpuDrawCount = g_GeomMgr.GetGBufferGpuDrawCount();
    const u32 GBufferInstanceCount = g_GeomMgr.GetGBufferInstanceCount();
    const u64 GBufferSubmittedIndexCount = g_GeomMgr.GetGBufferSubmittedIndexCount();
    GeomGBufferPackets.Add( GBufferPacketCount );
    GeomGBufferGpuDraws.Add( GBufferGpuDrawCount );
    GeomGBufferRigidIndirectRuns.Add( g_GeomMgr.GetGBufferRigidIndirectRunCount() );
    GeomGBufferRigidIndirectCommands.Add( g_GeomMgr.GetGBufferRigidIndirectCommandCount() );
    GeomGBufferSkinIndirectRuns.Add( g_GeomMgr.GetGBufferSkinIndirectRunCount() );
    GeomGBufferSkinIndirectCommands.Add( g_GeomMgr.GetGBufferSkinIndirectCommandCount() );
    GeomGBufferInstances.Add( GBufferInstanceCount );
    GeomGBufferSkinSectionDraws.Add( g_GeomMgr.GetGBufferSkinSectionDrawCount() );
    GeomGBufferSubmittedIndices.Add( (f64)GBufferSubmittedIndexCount );
    GeomGBufferInstancesPerPacket.Set( GBufferPacketCount
                                     ? (f64)GBufferInstanceCount / GBufferPacketCount
                                     : 0.0 );
    GeomGBufferDrawsPerPacket.Set( GBufferPacketCount
                                 ? (f64)GBufferGpuDrawCount / GBufferPacketCount
                                 : 0.0 );
    GeomGBufferIndicesPerDraw.Set( GBufferGpuDrawCount
                                  ? (f64)GBufferSubmittedIndexCount / GBufferGpuDrawCount
                                  : 0.0 );

    if( !Result )
    {
        g_ForwardRenderMgr.DiscardQueuedDraws();
        return;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/QueueForward" );
        for( s32 i = 0; Result && (i < g_GeomMgr.GetPacketCount()); ++i )
        {
            const geometry_render_pass Pass = g_GeomMgr.GetPacketPass( i );
            if( Pass == GEOMETRY_PASS_GBUFFER )
                continue;

            forward_render_phase Phase;
            switch( Pass )
            {
                case GEOMETRY_PASS_TRANSPARENT: Phase = FORWARD_PHASE_TRANSPARENT; break;
                case GEOMETRY_PASS_ADDITIVE:    Phase = FORWARD_PHASE_ADDITIVE;    break;
                case GEOMETRY_PASS_FORCE_LAST:  Phase = FORWARD_PHASE_FORCE_LAST;  break;
                case GEOMETRY_PASS_ZPRIME:      Phase = FORWARD_PHASE_ZPRIME;      break;
                case GEOMETRY_PASS_FADING:      Phase = FORWARD_PHASE_FADING;      break;
                case GEOMETRY_PASS_DISTORTION:  Phase = FORWARD_PHASE_DISTORTION;  break;
                default:
                    Result = FALSE;
                    continue;
            }

            Result = g_ForwardRenderMgr.QueueGeometry( i,
                                                       Phase,
                                                       g_GeomMgr.GetPacketSortDepth( i ),
                                                       g_GeomMgr.GetPacketSequence( i ) );
        }
    }

    if( !Result )
    {
        x_DebugMsg( "PCRender: failed to collect normal geometry packets\n" );
        g_ForwardRenderMgr.DiscardQueuedDraws();
    }
}

//=============================================================================

static
void platform_SubmitGeometry( const xarray<geometry_draw_item>&    Draws,
                              const xarray<dynamic_geometry_draw>& DynamicDraws,
                              const cubemap*                       pCubeMap )
{
    if( !g_GeomMgr.BuildPackets( Draws, DynamicDraws, pCubeMap ) )
    {
        x_DebugMsg( "PCRender: failed to build geometry packets\n" );
        g_ForwardRenderMgr.DiscardQueuedDraws();
        return;
    }

    platform_ExecuteGeomPackets();
}

static
void platform_SetCustomFogPalette( const texture::handle& Texture, xbool ImmediateSwitch, s32 PaletteIndex )
{
    g_PostMgr.SetCustomFogPalette( Texture, ImmediateSwitch, PaletteIndex );
}

//=============================================================================

static
xcolor platform_GetFogValue( const vector3& WorldPos, s32 PaletteIndex )
{
    return g_PostMgr.GetFogValue( WorldPos, PaletteIndex );
}

//=============================================================================

static
void platform_BeginPostEffects( void )
{
    g_PostMgr.BeginPostEffects();
}

//=============================================================================

static
void platform_AddScreenWarp( const vector3& WorldPos, f32 Radius, f32 WarpAmount )
{
    g_PostMgr.AddScreenWarp( WorldPos, Radius, WarpAmount );
}

//=============================================================================

static
void platform_ApplySelfIllumGlows( f32 MotionBlurIntensity, s32 GlowCutoff )
{
    g_PostMgr.ApplySelfIllumGlows( MotionBlurIntensity, GlowCutoff );
}

//=============================================================================

static
void platform_UpdatePostEffects( f32 DeltaTime )
{
    g_PostMgr.Update( DeltaTime );
}

//=============================================================================

static
void platform_MotionBlur( f32 Intensity )
{
    g_PostMgr.MotionBlur( Intensity );
}

//=============================================================================

static
void platform_ZFogFilter( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    g_PostMgr.ZFogFilter( Fn, Color, Param1, Param2 );
}

//=============================================================================

static
void platform_ZFogFilter( render::post_falloff_fn Fn, s32 PaletteIndex )
{
    g_PostMgr.ZFogFilter( Fn, PaletteIndex );
}

//=============================================================================

static
void platform_MipFilter( s32                        nFilters,
                         f32                        Offset,
                         render::post_falloff_fn    Fn,
                         xcolor                     Color,
                         f32                        Param1,
                         f32                        Param2,
                         s32                        PaletteIndex )
{
    g_PostMgr.MipFilter( nFilters, Offset, Fn, Color, Param1, Param2, PaletteIndex );
}

//=============================================================================

static
void platform_MipFilter( s32                        nFilters,
                         f32                        Offset,
                         render::post_falloff_fn    Fn,
                         const texture::handle&     Texture,
                         s32                        PaletteIndex )
{
    g_PostMgr.MipFilter( nFilters, Offset, Fn, Texture, PaletteIndex );
}

//=============================================================================

static
void platform_NoiseFilter( xcolor Color )
{
    g_PostMgr.NoiseFilter( Color );
}

//=============================================================================

static
void platform_ScreenFade( xcolor Color )
{
    g_PostMgr.ScreenFade( Color );
}

//=============================================================================

static
void platform_MultScreen( xcolor MultColor, render::post_screen_blend FinalBlend )
{
    g_PostMgr.MultScreen( MultColor, FinalBlend );
}

//=============================================================================

void platform_RadialBlur( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale  )
{
    g_PostMgr.RadialBlur( Zoom, Angle, AlphaSub, AlphaScale );
}

//=============================================================================

static
void platform_EndPostEffects( void )
{
    g_PostMgr.EndPostEffects();
}

//=============================================================================

static
void platform_BeginShadowShaders( void )
{
    g_ShadowMgr.BeginShadowShaders();
}

//=============================================================================

static
void platform_EndShadowShaders( void )
{
    g_ShadowMgr.EndShadowShaders();
}

//=============================================================================

static
void platform_ClearShadowSourceList( void )
{
    g_ShadowMapMgr.ClearSources();
}

//=============================================================================

static
void platform_FinalizeShadowSourceList( void )
{
    g_ShadowMapMgr.FinalizeSources();
}

//=============================================================================

static
void platform_AddPointShadowMapSource( const matrix4& L2W,
                                       radian         FOV,
                                       f32            LightRadius,
                                       f32            LightFalloff,
                                       s32            ShadowMapResolution,
                                       s32            ShadowPriority,
                                       f32            ShadowScore,
                                       s32            DynamicLightIndex )
{
    g_ShadowMapMgr.AddPointSource( L2W,
                                   FOV,
                                   LightRadius,
                                   LightFalloff,
                                   ShadowMapResolution,
                                   ShadowPriority,
                                   ShadowScore,
                                   DynamicLightIndex );
}

//=============================================================================

static
void platform_AddSpotShadowMapSource( const matrix4& L2W,
                                      radian         FOV,
                                      f32            LightRadius,
                                      f32            LightFalloff,
                                      s32            ShadowMapResolution,
                                      s32            ShadowPriority,
                                      f32            ShadowScore,
                                      s32            DynamicLightIndex )
{
    g_ShadowMapMgr.AddSpotSource( L2W,
                                  FOV,
                                  LightRadius,
                                  LightFalloff,
                                  ShadowMapResolution,
                                  ShadowPriority,
                                  ShadowScore,
                                  DynamicLightIndex );
}

//=============================================================================

static
void platform_RenderShadowCasters( const xarray<const geometry_draw_item*>& Draws,
                                   const xarray<dynamic_geometry_shadow_draw>& DynamicDraws )
{
    g_ShadowMgr.RenderCasters( Draws, DynamicDraws );

    const shadow_mgr::caster_stats& Stats = g_ShadowMgr.GetLastCasterStats();
    static xprofile_counter ShadowCasterInputDraws =
        x_GetProfiler().RegisterCounter( "ShadowCasterInputDraws", "RenderCounter" );
    static xprofile_counter ShadowCasterPreparedDraws =
        x_GetProfiler().RegisterCounter( "ShadowCasterPreparedDraws", "RenderCounter" );
    static xprofile_counter ShadowCasterDroppedDraws =
        x_GetProfiler().RegisterCounter( "ShadowCasterDroppedDraws", "RenderCounter" );
    static xprofile_counter ShadowCasterPackets =
        x_GetProfiler().RegisterCounter( "ShadowCasterPackets", "RenderCounter" );
    static xprofile_counter ShadowCasterGpuDraws =
        x_GetProfiler().RegisterCounter( "ShadowCasterGpuDraws", "RenderCounter" );
    static xprofile_counter ShadowRigidInstances =
        x_GetProfiler().RegisterCounter( "ShadowRigidInstances", "RenderCounter" );
    static xprofile_counter ShadowSkinInstances =
        x_GetProfiler().RegisterCounter( "ShadowSkinInstances", "RenderCounter" );
    static xprofile_counter ShadowSkinPaletteMatrices =
        x_GetProfiler().RegisterCounter( "ShadowSkinPaletteMatrices", "RenderCounter" );
    static xprofile_counter ShadowRigidPackets =
        x_GetProfiler().RegisterCounter( "ShadowRigidPackets", "RenderCounter" );
    static xprofile_counter ShadowRigidIndirectRuns =
        x_GetProfiler().RegisterCounter( "ShadowRigidIndirectRuns", "RenderCounter" );
    static xprofile_counter ShadowRigidIndirectCommands =
        x_GetProfiler().RegisterCounter( "ShadowRigidIndirectCommands", "RenderCounter" );
    static xprofile_counter ShadowSkinIndirectRuns =
        x_GetProfiler().RegisterCounter( "ShadowSkinIndirectRuns", "RenderCounter" );
    static xprofile_counter ShadowSkinIndirectCommands =
        x_GetProfiler().RegisterCounter( "ShadowSkinIndirectCommands", "RenderCounter" );
    static xprofile_counter ShadowSkinPackets =
        x_GetProfiler().RegisterCounter( "ShadowSkinPackets", "RenderCounter" );
    static xprofile_counter ShadowAlphaPackets =
        x_GetProfiler().RegisterCounter( "ShadowAlphaPackets", "RenderCounter" );
    static xprofile_counter ShadowSourceStateChanges =
        x_GetProfiler().RegisterCounter( "ShadowSourceStateChanges", "RenderCounter" );
    static xprofile_counter ShadowSkinPalettes =
        x_GetProfiler().RegisterCounter( "ShadowSkinPalettes", "RenderCounter" );
    static xprofile_counter ShadowCasterUploadBytes =
        x_GetProfiler().RegisterCounter( "ShadowCasterUploadBytes", "RenderCounter" );
    static xprofile_counter ShadowCasterSubmittedIndices =
        x_GetProfiler().RegisterCounter( "ShadowCasterSubmittedIndices", "RenderCounter" );
    static xprofile_counter ShadowCasterBufferReallocs =
        x_GetProfiler().RegisterCounter( "ShadowCasterBufferReallocs", "RenderCounter" );
    static xprofile_gauge ShadowInstancesPerPacket =
        x_GetProfiler().RegisterGauge( "ShadowInstancesPerPacket",
                                       XPROFILE_UNIT_COUNT,
                                       "RenderGauge" );
    static xprofile_gauge ShadowDrawCompression =
        x_GetProfiler().RegisterGauge( "ShadowDrawCompression",
                                       XPROFILE_UNIT_PERCENT,
                                       "RenderGauge" );
    static xprofile_gauge ShadowPaletteReuse =
        x_GetProfiler().RegisterGauge( "ShadowPaletteReuse",
                                       XPROFILE_UNIT_COUNT,
                                       "RenderGauge" );
    static xprofile_gauge ShadowMaxPacketInstances =
        x_GetProfiler().RegisterGauge( "ShadowMaxPacketInstances",
                                       XPROFILE_UNIT_COUNT,
                                       "RenderGauge" );

    ShadowCasterInputDraws.Add( Stats.InputDrawCount );
    ShadowCasterPreparedDraws.Add( Stats.PreparedDrawCount );
    ShadowCasterDroppedDraws.Add( Stats.InputDrawCount - Stats.PreparedDrawCount );
    ShadowCasterPackets.Add( Stats.PacketCount );
    ShadowCasterGpuDraws.Add( Stats.GpuDrawCount );
    ShadowRigidInstances.Add( Stats.RigidInstanceCount );
    ShadowSkinInstances.Add( Stats.SkinInstanceCount );
    ShadowSkinPaletteMatrices.Add( Stats.SkinPaletteMatrixCount );
    ShadowRigidPackets.Add( Stats.RigidPacketCount );
    ShadowRigidIndirectRuns.Add( Stats.RigidIndirectRunCount );
    ShadowRigidIndirectCommands.Add( Stats.RigidIndirectCommandCount );
    ShadowSkinIndirectRuns.Add( Stats.SkinIndirectRunCount );
    ShadowSkinIndirectCommands.Add( Stats.SkinIndirectCommandCount );
    ShadowSkinPackets.Add( Stats.SkinPacketCount );
    ShadowAlphaPackets.Add( Stats.AlphaPacketCount );
    ShadowSourceStateChanges.Add( Stats.SourceStateChangeCount );
    ShadowSkinPalettes.Add( Stats.SkinPaletteCount );
    ShadowCasterUploadBytes.Add( (f64)Stats.UploadBytes );
    ShadowCasterSubmittedIndices.Add( (f64)Stats.SubmittedIndexCount );
    ShadowCasterBufferReallocs.Add( Stats.BufferReallocationCount );
    ShadowInstancesPerPacket.Set( Stats.PacketCount
                                ? (f64)Stats.PreparedDrawCount / Stats.PacketCount
                                : 0.0 );
    ShadowDrawCompression.Set( Stats.PreparedDrawCount
                             ? 100.0 * (f64)Stats.GpuDrawCount / Stats.PreparedDrawCount
                             : 0.0 );
    ShadowPaletteReuse.Set( Stats.SkinPaletteCount
                          ? (f64)Stats.SkinInstanceCount / Stats.SkinPaletteCount
                          : 0.0 );
    ShadowMaxPacketInstances.Set( Stats.MaxPacketInstanceCount );
}

//=============================================================================

static
void platform_BeginNormalRender( void )
{
    s_RigidLightingQueryCount = 0;
    s_RigidLightingSetupCount = 0;
    s_SkinLightingQueryCount  = 0;
    s_LightStatsStart         = g_LightMgr.GetCollectionStats();

    g_PrimitiveMgr.BeginRender();
    g_DecalRenderer.BeginRender();
    g_ForwardRenderMgr.BeginFrame();
    g_ProjectionAtlas.BeginFrame();

    const view* pView = eng_GetView();
    if( !pView ) return;
        
    s32 x0, y0, x1, y1;
    pView->GetViewport( x0, y0, x1, y1 );
    
    u32 screenWidth  = (u32)(x1 - x0);
    u32 screenHeight = (u32)(y1 - y0);
    
    if( g_GBufferMgr.ResizeGBuffer( screenWidth, screenHeight ) )
    {
        g_GBufferMgr.ClearGBuffer();
        g_GBufferMgr.SetGBufferTargets();
    }
    
    g_ProjTextureMgr.ClearProjTextures();
}

//=============================================================================

static
void platform_EndNormalRender( void )
{
    const light_mgr::collection_stats& LightStats = g_LightMgr.GetCollectionStats();
    static xprofile_counter LightRigidQueries =
        x_GetProfiler().RegisterCounter( "LightRigidQueries", "RenderCounter" );
    static xprofile_counter LightRigidSetups =
        x_GetProfiler().RegisterCounter( "LightRigidSetups", "RenderCounter" );
    static xprofile_counter LightSkinQueries =
        x_GetProfiler().RegisterCounter( "LightSkinQueries", "RenderCounter" );
    static xprofile_counter LightSceneCandidates =
        x_GetProfiler().RegisterCounter( "LightSceneCandidates", "RenderCounter" );
    static xprofile_counter LightCharCandidates =
        x_GetProfiler().RegisterCounter( "LightCharCandidates", "RenderCounter" );
    static xprofile_counter LightSceneHits =
        x_GetProfiler().RegisterCounter( "LightSceneHits", "RenderCounter" );
    static xprofile_counter LightCharHits =
        x_GetProfiler().RegisterCounter( "LightCharHits", "RenderCounter" );

    LightRigidQueries.Add( s_RigidLightingQueryCount );
    LightRigidSetups.Add( s_RigidLightingSetupCount );
    LightSkinQueries.Add( s_SkinLightingQueryCount );
    LightSceneCandidates.Add( LightStats.SceneCandidates - s_LightStatsStart.SceneCandidates );
    LightCharCandidates.Add( LightStats.CharCandidates - s_LightStatsStart.CharCandidates );
    LightSceneHits.Add( LightStats.SceneHits - s_LightStatsStart.SceneHits );
    LightCharHits.Add( LightStats.CharHits - s_LightStatsStart.CharHits );

    g_GBufferMgr.SetFinalColorTarget();
}

//=============================================================================
