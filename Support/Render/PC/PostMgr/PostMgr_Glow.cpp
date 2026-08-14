//==============================================================================
//
//  PostMgr_Glow.cpp
//
//  Glow post-processing module for the PC platform.
//
//==============================================================================

//==============================================================================
//  BASE INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "PostMgr.hpp"

//==============================================================================
//  FILE-LOCAL TYPES AND HELPERS
//==============================================================================

namespace
{
static const f32 kGlowPulseMin = 0.6800f;
static const f32 kGlowPulseMax = 0.7500f;
static const f32 kGlowPulseRate = 0.1500f;
static const f32 kGlowReferenceFrameRate = 30.0f;

// Constant buffer layout
struct PostGlowConstants
{
    vector4 Params0;
    vector4 Params1;
};

static f32 const s_ClearColorTransparent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

// Helper functions
static void SetClearColor( f32 pDst[4], f32 const pSrc[4] )
{
    pDst[0] = pSrc[0];
    pDst[1] = pSrc[1];
    pDst[2] = pSrc[2];
    pDst[3] = pSrc[3];
}

static xbool IsGlowTargetValid( rtarget const& target, u32 width, u32 height, rtarget_format format )
{
    return rtarget_HasRenderTarget( target ) && rtarget_HasShaderResource( target ) && ( target.Desc.Width == width ) &&
           ( target.Desc.Height == height ) && ( target.Desc.Format == format );
}

static xbool CreateGlowTarget( rtarget& target, u32 width, u32 height, rtarget_format format, char const* pDebugName )
{
    if ( IsGlowTargetValid( target, width, height, format ) )
    {
        return TRUE;
    }

    rtarget_EndPass();
    rtarget_Destroy( target );

    rtarget_desc desc;
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.SampleCount = 1;
    desc.SampleQuality = 0;
    desc.bBindAsTexture = TRUE;
    desc.pDebugName = pDebugName;
    SetClearColor( desc.ClearColor, s_ClearColorTransparent );

    return rtarget_Create( target, desc );
}

static void ReleaseGlowTarget( rtarget& target )
{
    rtarget_EndPass();
    rtarget_Destroy( target );
    target = rtarget();
}

static xbool BeginGlowTargetPass( rtarget const& target, rtarget_load_op loadOp )
{
    if ( !rtarget_HasRenderTarget( target ) )
    {
        return FALSE;
    }

    rtarget_color_attachment_desc color;
    color.pTarget = &target;
    color.LoadOp = loadOp;
    color.StoreOp = RTARGET_STORE_STORE;
    SetClearColor( color.ClearColor, s_ClearColorTransparent );

    rtarget_EndPass();
    return rtarget_BeginPass( &color, 1, NULL );
}

static xbool ClearGlowTarget( rtarget const& target )
{
    if ( !BeginGlowTargetPass( target, RTARGET_LOAD_CLEAR ) )
    {
        return FALSE;
    }

    rtarget_EndPass();
    return TRUE;
}

static xbool BindGlowConstants( shader const& shader, f32 cutoff, f32 historyRetention, f32 stepX, f32 stepY,
                                f32 currentWeight = 1.0f )
{
    PostGlowConstants constants;
    constants.Params0.Set( cutoff, historyRetention, currentWeight, 0.0f );
    constants.Params1.Set( stepX, stepY, 0.0f, 0.0f );

    return shader_PushUniformData( shader, SHADER_STAGE_PIXEL, "GlowParams", &constants, sizeof( constants ) );
}

static xbool BindGlowAuxTexture( shader const& shader, shader_resource const* pResource, rstate_sampler const& sampler )
{
    return shader_BindSampler( shader, SHADER_STAGE_PIXEL, "GlowAux", pResource, &sampler );
}
} // namespace

//==============================================================================
//  GLOW RESOURCE MANAGEMENT
//==============================================================================

PostMgr::GlowResources::GlowResources()
{
    Downsample[0] = rtarget();
    Downsample[1] = rtarget();
    Downsample[2] = rtarget();
    Blur[0] = rtarget();
    Blur[1] = rtarget();
    Composite = rtarget();
    History = rtarget();
    ActiveResult = NULL;
    BufferWidth = 0;
    BufferHeight = 0;
    bResourcesValid = FALSE;
    bPendingComposite = FALSE;
    bHistoryValid = FALSE;
    bStoreHistory = FALSE;
    PulseScale = 1.0f;
    PulseDirection = kGlowPulseRate;
    DownsamplePS = shader();
    BlurHPS = shader();
    BlurVPS = shader();
    CombinePS = shader();
    CompositePS = shader();
    AuxSampler = rstate_sampler();
}

//==============================================================================

void PostMgr::GlowResources::Initialize( void )
{
    Shutdown();
    ResetFrame();

    shader_LoadFromEcs( DownsamplePS, "post_glow_downsample_ps.ps.ecs" );
    shader_LoadFromEcs( BlurHPS, "post_glow_blur_horizontal_ps.ps.ecs" );
    shader_LoadFromEcs( BlurVPS, "post_glow_blur_vertical_ps.ps.ecs" );
    shader_LoadFromEcs( CombinePS, "post_glow_combine_ps.ps.ecs" );
    shader_LoadFromEcs( CompositePS, "post_glow_composite_ps.ps.ecs" );
    rstate_CreateSampler( AuxSampler, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostGlowAux" );

    if ( !DownsamplePS || !BlurHPS || !BlurVPS || !CombinePS || !CompositePS || !AuxSampler )
    {
        x_DebugMsg( "PostMgr: WARNING - Failed to initialize glow shaders\n" );
    }
}

//==============================================================================

void PostMgr::GlowResources::Shutdown( void )
{
    ReleaseGlowTarget( Downsample[0] );
    ReleaseGlowTarget( Downsample[1] );
    ReleaseGlowTarget( Downsample[2] );
    ReleaseGlowTarget( Blur[0] );
    ReleaseGlowTarget( Blur[1] );
    ReleaseGlowTarget( Composite );
    ReleaseGlowTarget( History );

    BufferWidth = 0;
    BufferHeight = 0;
    bResourcesValid = FALSE;
    bHistoryValid = FALSE;
    PulseScale = 1.0f;
    PulseDirection = kGlowPulseRate;
    ResetFrame();

    rstate_DestroySampler( AuxSampler );
    shader_Destroy( DownsamplePS );
    shader_Destroy( BlurHPS );
    shader_Destroy( BlurVPS );
    shader_Destroy( CombinePS );
    shader_Destroy( CompositePS );
}

//==============================================================================

void PostMgr::GlowResources::ResetFrame( void )
{
    ActiveResult = NULL;
    bPendingComposite = FALSE;
    bStoreHistory = FALSE;
}

//==============================================================================

void PostMgr::GlowResources::InvalidateHistory( void )
{
    ResetFrame();
    bHistoryValid = FALSE;
    PulseScale = 1.0f;
    PulseDirection = kGlowPulseRate;

    if ( !rtarget_HasRenderTarget( History ) )
    {
        return;
    }

    ClearGlowTarget( History );
}

//==============================================================================

xbool PostMgr::GlowResources::ResizeIfNeeded( u32 sourceWidth, u32 sourceHeight )
{
    if ( sourceWidth == 0 || sourceHeight == 0 )
    {
        ReleaseGlowTarget( Downsample[0] );
        ReleaseGlowTarget( Downsample[1] );
        ReleaseGlowTarget( Downsample[2] );
        ReleaseGlowTarget( Blur[0] );
        ReleaseGlowTarget( Blur[1] );
        ReleaseGlowTarget( Composite );
        ReleaseGlowTarget( History );
        BufferWidth = 0;
        BufferHeight = 0;
        bResourcesValid = FALSE;
        bHistoryValid = FALSE;
        PulseScale = 1.0f;
        PulseDirection = kGlowPulseRate;
        ResetFrame();
        return FALSE;
    }

    u32 halfW = ( sourceWidth > 1 ) ? ( sourceWidth / 2 ) : sourceWidth;
    u32 halfH = ( sourceHeight > 1 ) ? ( sourceHeight / 2 ) : sourceHeight;
    u32 qW = ( halfW > 1 ) ? ( halfW / 2 ) : halfW;
    u32 qH = ( halfH > 1 ) ? ( halfH / 2 ) : halfH;
    u32 eW = ( qW > 1 ) ? ( qW / 2 ) : qW;
    u32 eH = ( qH > 1 ) ? ( qH / 2 ) : qH;

    if ( bResourcesValid && ( BufferWidth == halfW ) && ( BufferHeight == halfH ) &&
         IsGlowTargetValid( Downsample[0], halfW, halfH, RTARGET_FORMAT_RGBA16F ) &&
         IsGlowTargetValid( Downsample[1], qW, qH, RTARGET_FORMAT_RGBA16F ) &&
         IsGlowTargetValid( Downsample[2], eW, eH, RTARGET_FORMAT_RGBA16F ) &&
         IsGlowTargetValid( Blur[0], eW, eH, RTARGET_FORMAT_RGBA16F ) &&
         IsGlowTargetValid( Blur[1], eW, eH, RTARGET_FORMAT_RGBA16F ) &&
         IsGlowTargetValid( Composite, eW, eH, RTARGET_FORMAT_RGBA16F ) &&
         IsGlowTargetValid( History, eW, eH, RTARGET_FORMAT_RGBA16F ) )
    {
        return TRUE;
    }

    if ( !CreateGlowTarget( Downsample[0], halfW, halfH, RTARGET_FORMAT_RGBA16F, "PostGlowDownsample0" ) ||
         !CreateGlowTarget( Downsample[1], qW, qH, RTARGET_FORMAT_RGBA16F, "PostGlowDownsample1" ) ||
         !CreateGlowTarget( Downsample[2], eW, eH, RTARGET_FORMAT_RGBA16F, "PostGlowDownsample2" ) ||
         !CreateGlowTarget( Blur[0], eW, eH, RTARGET_FORMAT_RGBA16F, "PostGlowBlur0" ) ||
         !CreateGlowTarget( Blur[1], eW, eH, RTARGET_FORMAT_RGBA16F, "PostGlowBlur1" ) ||
         !CreateGlowTarget( Composite, eW, eH, RTARGET_FORMAT_RGBA16F, "PostGlowComposite" ) ||
         !CreateGlowTarget( History, eW, eH, RTARGET_FORMAT_RGBA16F, "PostGlowHistory" ) )
    {
        ResizeIfNeeded( 0, 0 );
        return FALSE;
    }

    ClearGlowTarget( Downsample[0] );
    ClearGlowTarget( Downsample[1] );
    ClearGlowTarget( Downsample[2] );
    ClearGlowTarget( Blur[0] );
    ClearGlowTarget( Blur[1] );
    ClearGlowTarget( Composite );
    ClearGlowTarget( History );

    BufferWidth = halfW;
    BufferHeight = halfH;
    bResourcesValid = TRUE;
    bHistoryValid = FALSE;
    PulseScale = 1.0f;
    PulseDirection = kGlowPulseRate;
    ResetFrame();

    return TRUE;
}

//==============================================================================

rtarget const* PostMgr::GlowResources::BindForComposite( void ) const
{
    if ( !bPendingComposite || !ActiveResult )
    {
        return NULL;
    }

    if ( !rtarget_HasShaderResource( *ActiveResult ) )
    {
        return NULL;
    }

    g_GBufferMgr.SetFinalColorTarget();
    return ActiveResult;
}

//==============================================================================

void PostMgr::GlowResources::FinalizeComposite( void )
{
    if ( bStoreHistory && ActiveResult && rtarget_HasShaderResource( *ActiveResult ) &&
         rtarget_HasRenderTarget( History ) )
    {
        rtarget_EndPass();

        if ( !rtarget_Copy( History, *ActiveResult ) )
        {
            bHistoryValid = FALSE;
            x_DebugMsg( "PostMgr: failed to store glow history\n" );
            ResetFrame();
            g_GBufferMgr.SetFinalColorTarget();
            return;
        }

        g_GBufferMgr.SetFinalColorTarget();
        bHistoryValid = TRUE;
    }
    ResetFrame();
}

//==============================================================================

void PostMgr::GlowResources::SetPendingResult( rtarget const* pResult, xbool storeHistory )
{
    ActiveResult = pResult;
    bPendingComposite = ( pResult != NULL );
    bStoreHistory = bPendingComposite && storeHistory;
}

//==============================================================================
//  GLOW PROCESSING
//==============================================================================

void PostMgr::ExecuteSelfIllumGlow( void )
{
    xbool const bStoreHistory = ( m_glow.MotionBlurIntensity > 0.0f );
    f32 historyRetention = 0.0f;
    f32 currentWeight = 1.0f;

    if ( bStoreHistory )
    {
        m_glowResources.PulseScale += m_glowResources.PulseDirection * m_frameDeltaTime;

        if ( m_glowResources.PulseScale >= kGlowPulseMax )
        {
            m_glowResources.PulseScale = kGlowPulseMax;
            m_glowResources.PulseDirection = -kGlowPulseRate;
        }
        else if ( m_glowResources.PulseScale <= kGlowPulseMin )
        {
            m_glowResources.PulseScale = kGlowPulseMin;
            m_glowResources.PulseDirection = kGlowPulseRate;
        }

        f32 const referenceRetention = m_glowResources.PulseScale;
        f32 const referenceFrameCount = m_frameDeltaTime * kGlowReferenceFrameRate;
        historyRetention = x_pow( referenceRetention, referenceFrameCount );
        currentWeight = ( 1.0f - historyRetention ) / ( 1.0f - referenceRetention );
    }
    else
    {
        m_glowResources.PulseScale = 1.0f;
        m_glowResources.PulseDirection = kGlowPulseRate;
    }

    if ( !bStoreHistory && m_glowResources.bHistoryValid )
    {
        m_glowResources.InvalidateHistory();
        g_GBufferMgr.SetFinalColorTarget();
    }

    if ( !m_glowResources.DownsamplePS || !m_glowResources.BlurHPS || !m_glowResources.BlurVPS ||
         !m_glowResources.CombinePS || !m_glowResources.CompositePS )
    {
        x_DebugMsg( "PostMgr: Glow resources missing, skipping glow stage\n" );
        return;
    }

    rtarget const* pGlowSource = g_GBufferMgr.GetGBufferTarget( GBufferTarget::Glow );
    if ( !pGlowSource || !rtarget_HasShaderResource( *pGlowSource ) )
    {
        return;
    }

    u32 const sourceWidth = pGlowSource->Desc.Width;
    u32 const sourceHeight = pGlowSource->Desc.Height;
    if ( sourceWidth == 0 || sourceHeight == 0 )
    {
        return;
    }

    if ( !m_glowResources.ResizeIfNeeded( sourceWidth, sourceHeight ) )
    {
        return;
    }

    f32 const cutoff = static_cast<f32>( m_glow.Cutoff ) / 255.0f;

    // Downsample chain: 1/2, 1/4, 1/8
    if ( !BeginGlowTargetPass( m_glowResources.Downsample[0], RTARGET_LOAD_CLEAR ) )
    {
        return;
    }
    if ( !BindGlowConstants( m_glowResources.DownsamplePS, cutoff, 0.0f,
                             1.0f / static_cast<f32>( sourceWidth ), 1.0f / static_cast<f32>( sourceHeight ) ) )
    {
        return;
    }
    composite_Blit( *pGlowSource, COMPOSITE_BLEND_COPY, 1.0f, &m_glowResources.DownsamplePS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "GlowSource" );

    if ( !BeginGlowTargetPass( m_glowResources.Downsample[1], RTARGET_LOAD_CLEAR ) )
    {
        return;
    }
    if ( !BindGlowConstants( m_glowResources.DownsamplePS, 0.0f, 0.0f,
                             1.0f / static_cast<f32>( m_glowResources.Downsample[0].Desc.Width ),
                             1.0f / static_cast<f32>( m_glowResources.Downsample[0].Desc.Height ) ) )
    {
        return;
    }
    composite_Blit( m_glowResources.Downsample[0], COMPOSITE_BLEND_COPY, 1.0f, &m_glowResources.DownsamplePS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "GlowSource" );

    if ( !BeginGlowTargetPass( m_glowResources.Downsample[2], RTARGET_LOAD_CLEAR ) )
    {
        return;
    }
    if ( !BindGlowConstants( m_glowResources.DownsamplePS, 0.0f, 0.0f,
                             1.0f / static_cast<f32>( m_glowResources.Downsample[1].Desc.Width ),
                             1.0f / static_cast<f32>( m_glowResources.Downsample[1].Desc.Height ) ) )
    {
        return;
    }
    composite_Blit( m_glowResources.Downsample[1], COMPOSITE_BLEND_COPY, 1.0f, &m_glowResources.DownsamplePS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "GlowSource" );

    // Xbox glow jitter: one weighted horizontal pass followed by one weighted
    // vertical pass on the 1/8 working buffer.
    f32 const blurStepX = 1.0f / static_cast<f32>( m_glowResources.Downsample[2].Desc.Width );
    f32 const blurStepY = 1.0f / static_cast<f32>( m_glowResources.Downsample[2].Desc.Height );

    if ( !BeginGlowTargetPass( m_glowResources.Blur[0], RTARGET_LOAD_CLEAR ) )
    {
        return;
    }
    if ( !BindGlowConstants( m_glowResources.BlurHPS, 0.0f, 0.0f, blurStepX, 0.0f ) )
    {
        return;
    }
    composite_Blit( m_glowResources.Downsample[2], COMPOSITE_BLEND_COPY, 1.0f, &m_glowResources.BlurHPS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "GlowSource" );

    if ( !BeginGlowTargetPass( m_glowResources.Blur[1], RTARGET_LOAD_CLEAR ) )
    {
        return;
    }
    if ( !BindGlowConstants( m_glowResources.BlurVPS, 0.0f, 0.0f, 0.0f, blurStepY ) )
    {
        return;
    }
    composite_Blit( m_glowResources.Blur[0], COMPOSITE_BLEND_COPY, 1.0f, &m_glowResources.BlurVPS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "GlowSource" );

    if ( bStoreHistory && m_glowResources.bHistoryValid && rtarget_HasShaderResource( m_glowResources.History ) )
    {
        if ( !BeginGlowTargetPass( m_glowResources.Composite, RTARGET_LOAD_CLEAR ) )
        {
            return;
        }

        if ( !BindGlowConstants( m_glowResources.CombinePS, cutoff, historyRetention, blurStepX, blurStepY,
                                 currentWeight ) )
        {
            return;
        }

        if ( !BindGlowAuxTexture( m_glowResources.CombinePS, rtarget_GetShaderResource( m_glowResources.History ),
                                  m_glowResources.AuxSampler ) )
        {
            return;
        }

        composite_Blit( m_glowResources.Blur[1], COMPOSITE_BLEND_COPY, 1.0f, &m_glowResources.CombinePS,
                        RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "GlowSource" );

        m_glowResources.SetPendingResult( &m_glowResources.Composite, TRUE );
    }
    else
    {
        m_glowResources.SetPendingResult( &m_glowResources.Blur[1], bStoreHistory );
    }

    // Glow leaves an internal 1/8 HDR target bound; restore the final scene
    // target so any subsequent post effects in EndPostEffects render to the
    // lit scene color buffer.
    g_GBufferMgr.SetFinalColorTarget();
}

//==============================================================================

void PostMgr::UpdateGlowStageBegin( void )
{
    m_glowResources.ResetFrame();

    if ( !g_GBufferMgr.IsGBufferEnabled() )
    {
        m_glowResources.ResizeIfNeeded( 0, 0 );
        return;
    }

    u32 width = 0;
    u32 height = 0;
    g_GBufferMgr.GetGBufferSize( width, height );

    if ( ( width == 0 ) || ( height == 0 ) )
    {
        m_glowResources.ResizeIfNeeded( 0, 0 );
        return;
    }

    u32 const targetWidth = ( width > 1 ) ? ( width / 2 ) : width;
    u32 const targetHeight = ( height > 1 ) ? ( height / 2 ) : height;

    if ( m_glowResources.bResourcesValid &&
         ( ( m_glowResources.BufferWidth != targetWidth ) || ( m_glowResources.BufferHeight != targetHeight ) ) )
    {
        m_glowResources.ResizeIfNeeded( 0, 0 );
    }
}

//==============================================================================

void PostMgr::CompositePendingGlow( void )
{
    rtarget const* pResult = m_glowResources.BindForComposite();
    if ( !pResult )
    {
        return;
    }

    composite_Blit( *pResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, &m_glowResources.CompositePS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "GlowSource" );

    m_glowResources.FinalizeComposite();
}
