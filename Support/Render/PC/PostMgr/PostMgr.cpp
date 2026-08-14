//==============================================================================
//
//  PostMgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "PostMgr.hpp"

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

PostMgr g_PostMgr;

static eng_frame_stage const s_PostFrameStage = { PostMgr::PostStage_BeginFrameThunk,
                                                  PostMgr::PostStage_BeforePresentThunk, 0 };
// The UI execute stage is registered at order 1000, so the fade must run later.
static eng_frame_stage const s_PostFrameStageAfterUI = { NULL,
                                                         PostMgr::PostStage_AfterUIThunk, 2000 };

//==============================================================================
//  MANAGER LIFETIME
//==============================================================================

void PostMgr::Init( void )
{
    if ( m_isInitialized )
    {
        return;
    }

    x_DebugMsg( "PostMgr: Initializing post-processing manager\n" );

    m_isInPost    = FALSE;
    m_postViewL   = m_postViewT = m_postViewR = m_postViewB = 0;
    m_postNearZ   = 1.0f;
    m_postFarZ    = 2.0f;
    m_pMipTexture = NULL;
    m_frameDeltaTime = 0.0f;

    for ( s32 i = 0; i < 5; i++ )
    {
        m_isFogValid[i] = FALSE;
        x_memset( m_fogSourcePalette[i], 0, sizeof( m_fogSourcePalette[i] ) );
        x_memset( m_fogPalette[i], 0, sizeof( m_fogPalette[i] ) );
        m_fogColor[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
        m_fogConst[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
        m_fogStart[i] = 0.0f;
    }

    m_Flags      = PostEffectFlags();
    m_motionBlur = PostMotionBlurParams();
    m_glow       = PostGlowParams();
    m_radialBlur = PostRadialBlurParams();
    m_ScreenWarp = PostScreenWarpParams();
    m_fogFilter  = PostFogFilterParams();
    m_mipFilter  = PostMipFilterParams();
    m_simple     = PostSimpleParams();

    m_isPostStageRegistered = FALSE;
    m_fogResources = FogResources();
    m_fogResources.Initialize();
    m_glowResources = GlowResources();
    m_glowResources.Initialize();
    m_FilterResources = FilterResources();
    m_FilterResources.Initialize();

    if ( !PrewarmPipelines() )
    {
        x_DebugMsg( "PostMgr: WARNING - Failed to prewarm one or more graphics pipelines\n" );
    }

    eng_RegisterFrameStage( s_PostFrameStage );
    eng_RegisterFrameStage( s_PostFrameStageAfterUI );
    m_isPostStageRegistered = TRUE;

    m_isInitialized = TRUE;
    x_DebugMsg( "PostMgr: Post-processing manager initialized successfully\n" );
}

//==============================================================================

xbool PostMgr::PrewarmPipelines( void )
{
    struct PrewarmDesc
    {
        composite_blend_mode Blend;
        rtarget_format       ColorFormat;
        rtarget_format       DepthFormat;
        shader const*        pPixelShader;
    };

    PrewarmDesc const descs[] = {
        // Default composite shader: post-chain copies and the final scene resolve.
        { COMPOSITE_BLEND_COPY, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_COUNT, NULL },
        { COMPOSITE_BLEND_COPY, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8, NULL },
        { COMPOSITE_BLEND_COPY, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_COUNT,
          &m_FilterResources.MipDownsamplePS },
        { COMPOSITE_BLEND_COPY, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_COUNT, &m_FilterResources.PainBlurPS },
        { COMPOSITE_BLEND_ADDITIVE, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8, NULL },
        { COMPOSITE_BLEND_ADDITIVE, RTARGET_FORMAT_RGBA16F, RTARGET_FORMAT_COUNT, NULL },

        // Fog and filters rendering directly into the final scene target.
        { COMPOSITE_BLEND_ALPHA, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8, &m_fogResources.CompositePS },
        { COMPOSITE_BLEND_ALPHA, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8, &m_fogResources.PolynomialPS },
        { COMPOSITE_BLEND_ALPHA, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8,
          &m_FilterResources.MotionBlurPS },
        { COMPOSITE_BLEND_ALPHA, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8,
          &m_FilterResources.MipCompositePS },
        { COMPOSITE_BLEND_ALPHA, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8,
          &m_FilterResources.MipCompositeCustomPS },
        { COMPOSITE_BLEND_ALPHA, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8, &m_FilterResources.NoisePS },
        { COMPOSITE_BLEND_ALPHA, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8,
          &m_FilterResources.ScreenFadePS },
        { COMPOSITE_BLEND_ALPHA, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_COUNT, &m_FilterResources.ScreenFadePS },

        // Filter ping-pong targets have no depth attachment.
        { COMPOSITE_BLEND_ALPHA, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_COUNT, &m_FilterResources.RadialBlurPS },
        { COMPOSITE_BLEND_COPY, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_COUNT, &m_FilterResources.ScreenWarpPS },

        // All internal glow targets are single-sample RGBA16F without depth.
        { COMPOSITE_BLEND_COPY, RTARGET_FORMAT_RGBA16F, RTARGET_FORMAT_COUNT, &m_glowResources.DownsamplePS },
        { COMPOSITE_BLEND_COPY, RTARGET_FORMAT_RGBA16F, RTARGET_FORMAT_COUNT, &m_glowResources.BlurHPS },
        { COMPOSITE_BLEND_COPY, RTARGET_FORMAT_RGBA16F, RTARGET_FORMAT_COUNT, &m_glowResources.BlurVPS },
        { COMPOSITE_BLEND_COPY, RTARGET_FORMAT_RGBA16F, RTARGET_FORMAT_COUNT, &m_glowResources.CombinePS },
        { COMPOSITE_BLEND_ADDITIVE, RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_DEPTH24_STENCIL8,
          &m_glowResources.CompositePS } };

    xbool bSuccess = TRUE;
    u32   pipelineCount = 0;
    for ( u32 i = 0; i < ARRAYSIZE( descs ); ++i )
    {
        PrewarmDesc const& desc = descs[i];
        if ( desc.pPixelShader && !*desc.pPixelShader )
        {
            continue;
        }

        if ( !composite_PrewarmPipeline( desc.Blend, desc.ColorFormat, desc.DepthFormat, 1, desc.pPixelShader ) )
        {
            bSuccess = FALSE;
        }
        else
        {
            pipelineCount++;
        }
    }

    // Swapchain format is selected by the SDL GPU backend and can differ from
    // the RGBA8 scene format used by the renderer.
    rtarget const* pBackBuffer = rtarget_GetBackBuffer();
    if ( pBackBuffer )
    {
        if ( ( pBackBuffer->Desc.Format != RTARGET_FORMAT_RGBA8 ) &&
             composite_PrewarmPipeline( COMPOSITE_BLEND_COPY, pBackBuffer->Desc.Format, RTARGET_FORMAT_COUNT,
                                        pBackBuffer->Desc.SampleCount, NULL ) )
        {
            pipelineCount++;
        }
        else if ( pBackBuffer->Desc.Format != RTARGET_FORMAT_RGBA8 )
        {
            bSuccess = FALSE;
        }

        if ( ( pBackBuffer->Desc.Format != RTARGET_FORMAT_RGBA8 || pBackBuffer->Desc.SampleCount != 1 ) &&
             composite_PrewarmPipeline( COMPOSITE_BLEND_ALPHA, pBackBuffer->Desc.Format, RTARGET_FORMAT_COUNT,
                                        pBackBuffer->Desc.SampleCount, &m_FilterResources.ScreenFadePS ) )
        {
            pipelineCount++;
        }
        else if ( pBackBuffer->Desc.Format != RTARGET_FORMAT_RGBA8 || pBackBuffer->Desc.SampleCount != 1 )
        {
            bSuccess = FALSE;
        }
    }

    x_DebugMsg( "PostMgr: prewarmed %u known graphics pipeline configurations\n", pipelineCount );
    return bSuccess;
}

//==============================================================================

void PostMgr::Kill( void )
{
    if ( !m_isInitialized )
    {
        return;
    }

    x_DebugMsg( "PostMgr: Shutting down post-processing manager\n" );

    if ( m_isPostStageRegistered )
    {
        eng_UnregisterFrameStage( s_PostFrameStageAfterUI );
        eng_UnregisterFrameStage( s_PostFrameStage );
        m_isPostStageRegistered = FALSE;
    }

    m_fogResources.Shutdown();
    m_glowResources.Shutdown();
    m_FilterResources.Shutdown();

    m_isInitialized = FALSE;
    x_DebugMsg( "PostMgr: Post-processing manager shutdown complete\n" );
}

//==============================================================================

void PostMgr::Update( f32 deltaTime )
{
    m_frameDeltaTime = x_clamp( deltaTime, 0.0f, 0.1f );
}

//==============================================================================
//  POST-PROCESSING PIPELINE
//==============================================================================

void PostMgr::BeginPostEffects( void )
{
    if ( !m_isInitialized )
    {
        x_DebugMsg( "PostMgr: ERROR - Not initialized\n" );
        return;
    }

    ASSERT( !m_isInPost );
    m_isInPost = TRUE;
    m_FilterResources.ResetPostChain();

    if ( m_Flags.Override )
    {
        return;
    }

    m_Flags.DoMotionBlur    = FALSE;
    m_Flags.DoSelfIllumGlow = FALSE;
    m_Flags.DoRadialBlur    = FALSE;
    m_Flags.DoZFogFn        = FALSE;
    m_Flags.DoZFogCustom    = FALSE;
    m_Flags.DoMipFn         = FALSE;
    m_Flags.DoMipCustom     = FALSE;
    m_Flags.DoNoise         = FALSE;
    m_Flags.DoScreenFade    = FALSE;

    m_ScreenWarp.Count = 0;
    m_pMipTexture = NULL;
}

//==============================================================================

void PostMgr::EndPostEffects( void )
{
    if ( !m_isInitialized || !m_isInPost )
    {
        return;
    }

    ASSERT( m_isInPost );
    m_isInPost = FALSE;

    view const* pView = eng_GetView();
    if ( pView )
    {
        pView->GetViewport( m_postViewL, m_postViewT, m_postViewR, m_postViewB );
        pView->GetZLimits( m_postNearZ, m_postFarZ );
    }

    if ( m_Flags.DoZFogCustom || m_Flags.DoZFogFn )
    {
        ExecuteZFogFilter();
    }

    if ( m_Flags.DoMipCustom || m_Flags.DoMipFn )
    {
        ExecuteMipFilter();
    }

    if ( m_Flags.DoMotionBlur )
    {
        ExecuteMotionBlur();
    }

    if ( m_Flags.DoSelfIllumGlow )
    {
        ExecuteSelfIllumGlow();
        CompositePendingGlow();
    }

    if ( m_Flags.DoRadialBlur || ( m_ScreenWarp.Count > 0 ) )
    {
        CopyBackBuffer();
    }

    if ( m_Flags.DoRadialBlur )
    {
        ExecuteRadialBlur();
    }

    if ( m_ScreenWarp.Count > 0 )
    {
        ExecuteScreenWarps();
    }

    if ( m_FilterResources.IsPostChainActive() )
    {
        g_GBufferMgr.SetFinalColorTarget();
        m_FilterResources.ResolvePostChain();
    }

    if ( m_Flags.DoNoise )
    {
        ExecuteNoiseFilter();
    }

    // Screen fade is applied by the late frame stage so it covers the UI.
}

//==============================================================================
//  EFFECT REQUEST RECORDING
//==============================================================================

void PostMgr::ApplySelfIllumGlows( f32 motionBlurIntensity, s32 glowCutoff )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    m_Flags.DoSelfIllumGlow = TRUE;
    m_glow.MotionBlurIntensity = x_clamp( motionBlurIntensity, 0.0f, 1.0f );
    m_glow.Cutoff = x_clamp( glowCutoff, 0, 255 );
}

//==============================================================================

void PostMgr::AddScreenWarp( vector3 const& worldPos, f32 radius, f32 warpAmount )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    if ( m_ScreenWarp.Count >= MAX_POST_SCREEN_WARPS )
    {
        return;
    }

    s32 const index = m_ScreenWarp.Count;
    m_ScreenWarp.WorldPos[index] = worldPos;
    m_ScreenWarp.Radius[index] = radius;
    m_ScreenWarp.Amount[index] = warpAmount;
    m_ScreenWarp.Count++;
}

//==============================================================================

void PostMgr::MotionBlur( f32 intensity )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    m_Flags.DoMotionBlur = TRUE;
    m_motionBlur.Intensity = intensity;
}

//==============================================================================

void PostMgr::ZFogFilter( render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2 )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    ASSERT( fn != render::FALLOFF_CUSTOM );

    BuildFogPalette( fn, color, param1, param2 );
    m_Flags.DoZFogFn = TRUE;
}

//==============================================================================

void PostMgr::ZFogFilter( render::post_falloff_fn fn, s32 paletteIndex )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( ( paletteIndex >= 0 ) && ( paletteIndex <= 4 ) );
    if ( !m_isFogValid[paletteIndex] )
    {
        return;
    }

    ASSERT( m_isInPost );
    ASSERT( fn == render::FALLOFF_CUSTOM );
    m_Flags.DoZFogCustom = TRUE;
    m_fogFilter.Fn[paletteIndex] = fn;
    m_fogFilter.PaletteIndex = paletteIndex;
}

//==============================================================================

void PostMgr::MipFilter( s32 nFilters, f32 offset, render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2,
                         s32 paletteIndex )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    ASSERT( fn != render::FALLOFF_CUSTOM );
    ASSERT( ( paletteIndex >= 0 ) && ( paletteIndex < 4 ) );

    BuildMipPalette( fn, color, param1, param2, paletteIndex );
    m_Flags.DoMipFn = TRUE;
    m_mipFilter.Fn[paletteIndex] = fn;
    m_mipFilter.Count[paletteIndex] = nFilters;
    m_mipFilter.Offset[paletteIndex] = offset;
    m_mipFilter.PaletteIndex = paletteIndex;

}

//==============================================================================

void PostMgr::MipFilter( s32 nFilters, f32 offset, render::post_falloff_fn fn, texture::handle const& texture,
                         s32 paletteIndex )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    ASSERT( fn == render::FALLOFF_CUSTOM );
    ASSERT( ( paletteIndex >= 0 ) && ( paletteIndex < 4 ) );

    if ( texture.GetPointer() == NULL )
    {
        return;
    }

    m_Flags.DoMipCustom = TRUE;
    m_pMipTexture = &texture.GetPointer()->m_bitmap;
    m_mipFilter.Fn[paletteIndex] = fn;
    m_mipFilter.Count[paletteIndex] = nFilters;
    m_mipFilter.Offset[paletteIndex] = offset;
    m_mipFilter.PaletteIndex = paletteIndex;
    ASSERT( m_pMipTexture );
}

//==============================================================================

void PostMgr::NoiseFilter( xcolor color )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    m_Flags.DoNoise = TRUE;
    m_simple.NoiseColor = color;
}

//==============================================================================

void PostMgr::ScreenFade( xcolor color )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    m_simple.FadeColor = color;
    m_Flags.DoScreenFade = TRUE;
}

//==============================================================================

void PostMgr::MultScreen( xcolor multColor, render::post_screen_blend finalBlend )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    static_cast<void>( multColor );
    static_cast<void>( finalBlend );
}

//==============================================================================

void PostMgr::RadialBlur( f32 zoom, radian angle, f32 alphaSub, f32 alphaScale )
{
    if ( !m_isInitialized || m_Flags.Override )
    {
        return;
    }

    ASSERT( m_isInPost );
    m_Flags.DoRadialBlur = TRUE;
    m_radialBlur.Zoom = zoom;
    m_radialBlur.Angle = angle;
    m_radialBlur.AlphaSub = alphaSub;
    m_radialBlur.AlphaScale = alphaScale;
}

//==============================================================================

void PostMgr::InvalidateTemporalHistory( void )
{
    m_FilterResources.InvalidateHistory();
    m_glowResources.InvalidateHistory();
}

//==============================================================================
//  FRAME STAGE THUNKS
//==============================================================================

void PostMgr::PostStage_BeginFrameThunk( void )
{
    if ( !g_PostMgr.m_isInitialized )
    {
        return;
    }

    g_GBufferMgr.BeginFrame();
    g_PostMgr.UpdateGlowStageBegin();
}

//==============================================================================

void PostMgr::PostStage_BeforePresentThunk( void )
{
    if ( !g_PostMgr.m_isInitialized )
    {
        return;
    }

    if ( !g_GBufferMgr.WasSceneRenderedThisFrame() )
    {
        g_PostMgr.InvalidateTemporalHistory();
        return;
    }

    g_PostMgr.UpdateFilterHistoryBeforePresent();
    g_GBufferMgr.PresentFinalColor();
}

//==============================================================================

void PostMgr::PostStage_AfterUIThunk( void )
{
    if ( !g_PostMgr.m_isInitialized )
    {
        return;
    }

    if ( !g_GBufferMgr.WasSceneRenderedThisFrame() )
    {
        return;
    }

    g_PostMgr.ExecuteScreenFadeLate();
    rtarget_EndPass();
}
