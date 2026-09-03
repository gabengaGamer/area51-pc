//==============================================================================
//
//  sdleng_composite.cpp
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"

#include "e_Composite.hpp"

//==============================================================================
//  TYPES
//==============================================================================

struct composite_params
{
    vector4 BlendColor;
    s32     BlendMode;
    f32     Padding[3];
};

static_assert( sizeof(composite_params) == 32, "CompositeParams layout must match HLSL" );

//------------------------------------------------------------------------------

struct composite_pipeline_key
{
    const shader*         pPixelShader;
    const shader_backend* pPixelBackend;
    composite_blend_mode  BlendMode;
    rtarget_format        ColorFormat;
    rtarget_format        DepthFormat;
    u32                   SampleCount;

    composite_pipeline_key( void ) :
        pPixelShader ( NULL ),
        pPixelBackend( NULL ),
        BlendMode    ( COMPOSITE_BLEND_ALPHA ),
        ColorFormat  ( RTARGET_FORMAT_COUNT ),
        DepthFormat  ( RTARGET_FORMAT_COUNT ),
        SampleCount  ( 1 )
    {
    }
};

//------------------------------------------------------------------------------

struct composite_pipeline_entry
{
    composite_pipeline_key Key;
    render_pipeline        Pipeline;
};

//------------------------------------------------------------------------------

static struct composite_locals
{
    composite_locals( void ) :
        bInitialized( FALSE ),
        VertexShader (),
        PixelShader  (),
        Pipelines    ()
    {
    }

    xbool                           bInitialized;
    shader                          VertexShader;
    shader                          PixelShader;
    rstate_sampler                  Samplers[RSTATE_SAMPLER_PRESET_COUNT];
    xarray<composite_pipeline_entry*> Pipelines;
} s;

//==============================================================================
//  HELPERS
//==============================================================================

static composite_blend_mode composite_NormalizeBlendMode( composite_blend_mode BlendMode )
{
    const s32 Mode = (s32)BlendMode;
    if( (Mode < 0) || (Mode >= COMPOSITE_BLEND_COUNT) )
        return COMPOSITE_BLEND_ALPHA;

    return BlendMode;
}

//==============================================================================

static rstate_blend_preset composite_GetBlendPreset( composite_blend_mode BlendMode )
{
    switch( BlendMode )
    {
        case COMPOSITE_BLEND_ALPHA:    return RSTATE_BLEND_PRESET_ALPHA;
        case COMPOSITE_BLEND_ADDITIVE: return RSTATE_BLEND_PRESET_ADD;
        case COMPOSITE_BLEND_MULTIPLY: return RSTATE_BLEND_PRESET_MULTIPLY;
        case COMPOSITE_BLEND_OVERLAY:  return RSTATE_BLEND_PRESET_PREMULT_ALPHA;
        case COMPOSITE_BLEND_COPY:     return RSTATE_BLEND_PRESET_NONE;
        default:                       return RSTATE_BLEND_PRESET_ALPHA;
    }
}

//==============================================================================

static xbool composite_KeyEquals( const composite_pipeline_key& A,
                                  const composite_pipeline_key& B )
{
    return (A.pPixelShader  == B.pPixelShader)  &&
           (A.pPixelBackend == B.pPixelBackend) &&
           (A.BlendMode     == B.BlendMode)     &&
           (A.ColorFormat   == B.ColorFormat)   &&
           (A.DepthFormat   == B.DepthFormat)   &&
           (A.SampleCount   == B.SampleCount);
}

//==============================================================================

static void composite_DestroyPipelines( void )
{
    for( s32 i = 0; i < s.Pipelines.GetCount(); i++ )
    {
        composite_pipeline_entry* pEntry = s.Pipelines[i];
        if( pEntry )
        {
            render_DestroyPipeline( pEntry->Pipeline );
            delete pEntry;
        }
    }

    s.Pipelines.Clear();
}

//==============================================================================

static void composite_ReleaseResources( void )
{
    composite_DestroyPipelines();

    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
        rstate_DestroySampler( s.Samplers[i] );

    shader_Destroy( s.PixelShader );
    shader_Destroy( s.VertexShader );
}

//==============================================================================

static xbool composite_CreateResources( void )
{
    if( !shader_LoadFromEcs( s.VertexShader, "composite_vs.vs.ecs" ) )
    {
        x_DebugMsg( "CompositeMgr: failed to load composite vertex shader\n" );
        return FALSE;
    }

    if( !shader_LoadFromEcs( s.PixelShader, "composite_ps.ps.ecs" ) )
    {
        x_DebugMsg( "CompositeMgr: failed to load composite pixel shader\n" );
        return FALSE;
    }

    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        if( !rstate_CreateSampler( s.Samplers[i], (rstate_sampler_preset)i, "CompositeSampler" ) )
        {
            x_DebugMsg( "CompositeMgr: failed to create sampler preset %d\n", i );
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

static xbool composite_SetTargetViewport( const rtarget& Target )
{
    if( (Target.Desc.Width == 0) || (Target.Desc.Height == 0) )
        return FALSE;

    rdraw_viewport Viewport;
    Viewport.TopLeftX = 0.0f;
    Viewport.TopLeftY = 0.0f;
    Viewport.Width    = (f32)Target.Desc.Width;
    Viewport.Height   = (f32)Target.Desc.Height;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    return rdraw_SetViewport( Viewport );
}

//==============================================================================

static xbool composite_SetPipelineKey( composite_pipeline_key& Key,
                                       composite_blend_mode    BlendMode,
                                       const shader&           PixelShader,
                                       rtarget_format          ColorFormat,
                                       rtarget_format          DepthFormat,
                                       u32                     SampleCount )
{
    if( !PixelShader || (PixelShader.Stage != SHADER_STAGE_PIXEL) || !PixelShader.pBackend )
        return FALSE;

    if( ((u32)ColorFormat >= (u32)RTARGET_FORMAT_COUNT) ||
        (ColorFormat == RTARGET_FORMAT_DEPTH_STENCIL) ||
        (ColorFormat == RTARGET_FORMAT_DEPTH32F) )
    {
        return FALSE;
    }

    if( (DepthFormat != RTARGET_FORMAT_COUNT) &&
        (DepthFormat != RTARGET_FORMAT_DEPTH_STENCIL) &&
        (DepthFormat != RTARGET_FORMAT_DEPTH32F) )
    {
        return FALSE;
    }

    Key.pPixelShader  = &PixelShader;
    Key.pPixelBackend = PixelShader.pBackend;
    Key.BlendMode     = BlendMode;
    Key.ColorFormat   = ColorFormat;
    Key.DepthFormat   = DepthFormat;
    Key.SampleCount   = SampleCount ? SampleCount : 1;
    return TRUE;
}

//==============================================================================

static xbool composite_BuildPipelineKey( composite_pipeline_key& Key,
                                         composite_blend_mode    BlendMode,
                                         const shader&           PixelShader )
{
    if( rtarget_GetCurrentCount() != 1 )
    {
        x_DebugMsg( "CompositeMgr: composite pass requires exactly one color target\n" );
        return FALSE;
    }

    const rtarget* pColorTarget = rtarget_GetCurrentTarget( 0 );
    if( !pColorTarget )
        return FALSE;

    const rtarget* pDepthTarget = rtarget_GetCurrentDepth();
    return composite_SetPipelineKey( Key,
                                     BlendMode,
                                     PixelShader,
                                     pColorTarget->Desc.Format,
                                     pDepthTarget ? pDepthTarget->Desc.Format : RTARGET_FORMAT_COUNT,
                                     pColorTarget->Desc.SampleCount );
}

//==============================================================================

static xbool composite_CreatePipelineForKey( render_pipeline&              Pipeline,
                                             const composite_pipeline_key& Key )
{
    render_color_target_desc ColorTarget;
    ColorTarget.Format = Key.ColorFormat;
    ColorTarget.Blend  = rstate_GetBlendDesc( composite_GetBlendPreset( Key.BlendMode ) );

    render_pipeline_desc Desc;
    Desc.Shader.pVertexShader = &s.VertexShader;
    Desc.Shader.pPixelShader  = Key.pPixelShader;
    Desc.Shader.Topology      = SHADER_TOPOLOGY_TRIANGLE_LIST;
    Desc.Raster               = rstate_GetRasterDesc( RSTATE_RASTER_PRESET_SOLID_NO_CULL );
    Desc.Depth                = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE );
    Desc.ColorTargets[0]      = ColorTarget;
    Desc.ColorCount           = 1;
    Desc.DepthFormat          = Key.DepthFormat;
    Desc.SampleCount          = Key.SampleCount;
    Desc.pDebugName           = "CompositePipeline";

    return render_CreatePipeline( Pipeline, Desc );
}

//==============================================================================

static render_pipeline* composite_GetPipeline( const composite_pipeline_key& Key )
{
    for( s32 i = 0; i < s.Pipelines.GetCount(); i++ )
    {
        composite_pipeline_entry* pEntry = s.Pipelines[i];
        if( pEntry && composite_KeyEquals( pEntry->Key, Key ) )
            return &pEntry->Pipeline;
    }

    composite_pipeline_entry* pEntry = new composite_pipeline_entry;
    if( !pEntry )
        return NULL;

    pEntry->Key = Key;

    if( !composite_CreatePipelineForKey( pEntry->Pipeline, Key ) )
    {
        delete pEntry;
        return NULL;
    }

    s.Pipelines.Append( pEntry );
    return &pEntry->Pipeline;
}

//==============================================================================

static xbool composite_BindSource( const shader&          PixelShader,
                                   const char*            pSourceBindingName,
                                   const shader_resource* pSourceResource,
                                   rstate_sampler_preset  SamplerMode )
{
    if( !pSourceBindingName || !pSourceBindingName[0] )
        return TRUE;

    if( (SamplerMode < 0) || (SamplerMode >= RSTATE_SAMPLER_PRESET_COUNT) )
        SamplerMode = RSTATE_SAMPLER_PRESET_POINT_CLAMP;

    return shader_BindSampler( PixelShader,
                               SHADER_STAGE_PIXEL,
                               pSourceBindingName,
                               pSourceResource,
                               &s.Samplers[SamplerMode] );
}

//==============================================================================

static xbool composite_BindDefaultParams( const shader& PixelShader, composite_blend_mode BlendMode, f32 Alpha )
{
    composite_params Params;
    Params.BlendColor = vector4( 1.0f, 1.0f, 1.0f, Alpha );
    Params.BlendMode  = (s32)BlendMode;
    Params.Padding[0] = 0.0f;
    Params.Padding[1] = 0.0f;
    Params.Padding[2] = 0.0f;

    return shader_PushUniformData( PixelShader,
                                   SHADER_STAGE_PIXEL,
                                   "CompositeParams",
                                   &Params,
                                   sizeof(Params) );
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

void composite_Init( void )
{
    if( s.bInitialized )
        return;

    if( !composite_CreateResources() )
    {
        composite_ReleaseResources();
        return;
    }

    s.bInitialized = TRUE;
}

//==============================================================================

void composite_Kill( void )
{
    if( !s.bInitialized )
        return;

    composite_ReleaseResources();
    s.bInitialized = FALSE;
}

//==============================================================================

xbool composite_PrewarmPipeline( composite_blend_mode BlendMode,
                                 rtarget_format       ColorFormat,
                                 rtarget_format       DepthFormat,
                                 u32                  SampleCount,
                                 const shader*        pCustomShader )
{
    if( !s.bInitialized )
        return FALSE;

    BlendMode = composite_NormalizeBlendMode( BlendMode );
    const shader* pPixelShader = pCustomShader ? pCustomShader : &s.PixelShader;

    composite_pipeline_key Key;
    if( !composite_SetPipelineKey( Key,
                                   BlendMode,
                                   *pPixelShader,
                                   ColorFormat,
                                   DepthFormat,
                                   SampleCount ) )
    {
        return FALSE;
    }

    return composite_GetPipeline( Key ) != NULL;
}

//==============================================================================

void composite_Blit( const rtarget&        Source,
                     composite_blend_mode  BlendMode,
                     f32                   Alpha,
                     const shader*         pCustomShader,
                     rstate_sampler_preset SamplerMode,
                     const char*           pSourceBindingName )
{
    if( !s.bInitialized )
        return;

    if( !sdleng_InRenderPass() )
        return;

    BlendMode = composite_NormalizeBlendMode( BlendMode );

    const shader_resource* pSourceResource = rtarget_GetShaderResource( Source );
    if( !pSourceResource )
        return;

    const rtarget* pCurrentTarget = rtarget_GetCurrentTarget( 0 );
    if( !pCurrentTarget || !composite_SetTargetViewport( *pCurrentTarget ) )
        return;

    const shader* pPixelShader = pCustomShader ? pCustomShader : &s.PixelShader;

    composite_pipeline_key Key;
    if( !composite_BuildPipelineKey( Key, BlendMode, *pPixelShader ) )
        return;

    render_pipeline* pPipeline = composite_GetPipeline( Key );
    if( !pPipeline )
        return;

    if( !render_BindPipeline( *pPipeline ) )
        return;

    const char* pResolvedSourceBindingName = pCustomShader ? pSourceBindingName : "CompositeSource";
    if( !composite_BindSource( *pPixelShader, pResolvedSourceBindingName, pSourceResource, SamplerMode ) )
        return;

    if( !pCustomShader && !composite_BindDefaultParams( *pPixelShader, BlendMode, Alpha ) )
        return;

    rdraw_Draw( 3 );
}

//==============================================================================
#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
