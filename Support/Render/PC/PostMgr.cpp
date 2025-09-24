//==============================================================================
//  
//  PostMgr.cpp
//  
//  Post-processing Manager for PC platform
//
//==============================================================================

//------------------------------------------------------------------------------
//
// GLOBAL TODO:
//
// Oh my god, this code looks like a complete mess right now. 
// Need to do something about it.
//
// At the very least, should split the post-processing into a separate file (sub-module for PostMgr)
// it's taking up a lot of space.
//
//------------------------------------------------------------------------------

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "PostMgr.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

struct cb_post_glow
{
    vector4 Params0;
    vector4 Params1;
};

static const f32 kGlowIntensityScale = 1.0f;

static void GlowStage_OnBeginFrame( void );
static void GlowStage_OnBeforePresent( void );

static const eng_frame_stage s_GlowFrameStage =
{
    GlowStage_OnBeginFrame,
    GlowStage_OnBeforePresent
};

//#define POST_VERBOSE_MODE

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dContext;

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

post_mgr g_PostMgr;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void post_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    x_DebugMsg( "PostMgr: Initializing post-processing manager\n" );

    // Initialize member variables
    m_bInPost = FALSE;
    m_PostViewL = m_PostViewT = m_PostViewR = m_PostViewB = 0;
    m_PostNearZ = 1.0f;
    m_PostFarZ = 2.0f;
    m_pMipTexture = NULL;

    // Initialize fog validity flags
    for( s32 i = 0; i < 3; i++ )
    {
        m_bFogValid[i] = FALSE;
    }

    // Clear all current effects
    m_Flags = post_effect_flags();
    m_MotionBlur = post_motion_blur_params();
    m_Glow = post_glow_params();
    m_MultScreen = post_mult_screen_params();
    m_RadialBlur = post_radial_blur_params();
    m_FogFilter = post_fog_filter_params();
    m_MipFilter = post_mip_filter_params();
    m_Simple = post_simple_params();

    m_GlowDownsample = rtarget();
    m_GlowBlur[0]    = rtarget();
    m_GlowBlur[1]    = rtarget();
    m_GlowComposite  = rtarget();
    m_GlowHistory    = rtarget();
    m_pActiveGlowResult   = NULL;
    m_GlowBufferWidth     = 0;
    m_GlowBufferHeight    = 0;
    m_bGlowResourcesValid = FALSE;
    m_bGlowPendingComposite = FALSE;
    m_bGlowStageRegistered  = FALSE;
    m_pGlowDownsamplePS   = NULL;
    m_pGlowBlurHPS        = NULL;
    m_pGlowBlurVPS        = NULL;
    m_pGlowCombinePS      = NULL;
    m_pGlowCompositePS    = NULL;
    m_pGlowConstantBuffer = NULL;

    if( g_pd3dDevice )
    {
        char shaderPath[256];
        x_sprintf( shaderPath, "%spost_glow.hlsl", SHADER_PATH );

        m_pGlowDownsamplePS = shader_CompilePixelFromFile( shaderPath, "PS_Downsample", "ps_5_0" );
        m_pGlowBlurHPS      = shader_CompilePixelFromFile( shaderPath, "PS_BlurHorizontal", "ps_5_0" );
        m_pGlowBlurVPS      = shader_CompilePixelFromFile( shaderPath, "PS_BlurVertical", "ps_5_0" );
        m_pGlowCombinePS    = shader_CompilePixelFromFile( shaderPath, "PS_Combine", "ps_5_0" );
        m_pGlowCompositePS  = shader_CompilePixelFromFile( shaderPath, "PS_Composite", "ps_5_0" );
        m_pGlowConstantBuffer = shader_CreateConstantBuffer( sizeof(cb_post_glow) );

        if( !m_pGlowDownsamplePS || !m_pGlowBlurHPS || !m_pGlowBlurVPS || !m_pGlowCombinePS ||
            !m_pGlowCompositePS || !m_pGlowConstantBuffer )
        {
            x_DebugMsg( "PostMgr: WARNING - Failed to initialize glow shaders\n" );
        }
    }

    d3deng_RegisterFrameStage( s_GlowFrameStage );
    m_bGlowStageRegistered = TRUE;

    m_bInitialized = TRUE;
    x_DebugMsg( "PostMgr: Post-processing manager initialized successfully\n" );
}

//==============================================================================

void post_mgr::Kill( void )
{
    if( !m_bInitialized )
        return;

    x_DebugMsg( "PostMgr: Shutting down post-processing manager\n" );

    if( m_bGlowStageRegistered )
    {
        d3deng_UnregisterFrameStage( s_GlowFrameStage );
        m_bGlowStageRegistered = FALSE;
    }

    ReleaseGlowTargets();

    if( m_pGlowConstantBuffer )
    {
        m_pGlowConstantBuffer->Release();
        m_pGlowConstantBuffer = NULL;
    }

    if( m_pGlowDownsamplePS )
    {
        m_pGlowDownsamplePS->Release();
        m_pGlowDownsamplePS = NULL;
    }

    if( m_pGlowBlurHPS )
    {
        m_pGlowBlurHPS->Release();
        m_pGlowBlurHPS = NULL;
    }

    if( m_pGlowBlurVPS )
    {
        m_pGlowBlurVPS->Release();
        m_pGlowBlurVPS = NULL;
    }

    if( m_pGlowCombinePS )
    {
        m_pGlowCombinePS->Release();
        m_pGlowCombinePS = NULL;
    }

    if( m_pGlowCompositePS )
    {
        m_pGlowCompositePS->Release();
        m_pGlowCompositePS = NULL;
    }

    m_bInitialized = FALSE;
    x_DebugMsg( "PostMgr: Post-processing manager shutdown complete\n" );
}

//==============================================================================
//  MAIN POST-PROCESSING PIPELINE
//==============================================================================

void post_mgr::BeginPostEffects( void )
{
    if( !m_bInitialized )
    {
        x_DebugMsg( "PostMgr: ERROR - Not initialized\n" );
        return;
    }

    ASSERT( !m_bInPost );
    m_bInPost = TRUE;
    
    if( m_Flags.Override )
        return;

    // Reset all effect flags (like original)
    m_Flags.DoMotionBlur    = FALSE;
    m_Flags.DoSelfIllumGlow = FALSE;
    m_Flags.DoMultScreen    = FALSE;
    m_Flags.DoRadialBlur    = FALSE;
    m_Flags.DoZFogFn        = FALSE;
    m_Flags.DoZFogCustom    = FALSE;
    m_Flags.DoMipFn         = FALSE;
    m_Flags.DoMipCustom     = FALSE;
    m_Flags.DoNoise         = FALSE;
    m_Flags.DoScreenFade    = FALSE;

    // Reset screen warps
    m_pMipTexture = NULL;

    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Post-effects pipeline started\n" );
    #endif
}

//==============================================================================

void post_mgr::EndPostEffects( void )
{
    if( !m_bInitialized || !m_bInPost )
        return;

    ASSERT( m_bInPost );
    m_bInPost = FALSE;

    // Get common engine/screen information (like original)
    const view* pView = eng_GetView();
    if( pView )
    {
        pView->GetViewport( m_PostViewL, m_PostViewT, m_PostViewR, m_PostViewB );
        pView->GetZLimits( m_PostNearZ, m_PostFarZ );
    }

    // Process effects in the same order as original pc_post.inl

    // Handle fog
    if( m_Flags.DoZFogCustom || m_Flags.DoZFogFn )
    {
        pc_ZFogFilter();
    }
    
    // Handle the mip filter
    if( m_Flags.DoMipCustom || m_Flags.DoMipFn )
    {
        pc_MipFilter();
    }

    // Handle motion-blur
    if( m_Flags.DoMotionBlur )
    {
        pc_MotionBlur();
    }

    // Handle self-illum
    if( m_Flags.DoSelfIllumGlow )
    {
        pc_ApplySelfIllumGlows();
    }

    // Radial blurs and post-mults need a copy of the back-buffer
    if( m_Flags.DoMultScreen || m_Flags.DoRadialBlur )
    {
        pc_CopyBackBuffer();
    }

    // Handle the multscreen filter
    if( m_Flags.DoMultScreen )
    {
        pc_MultScreen();
    }

    // Handle the radial blur filter
    if( m_Flags.DoRadialBlur )
    {
        pc_RadialBlur();
    }

    // Handle the noise filter
    if( m_Flags.DoNoise )
    {
        pc_NoiseFilter();
    }

    // Handle the fade filter
    if( m_Flags.DoScreenFade )
    {
        pc_ScreenFade();
    }

    // Reset render states (like original)
    if( g_pd3dContext )
    {
        state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_NORMAL );
        state_SetState( STATE_TYPE_BLEND, STATE_BLEND_NONE );
        state_SetState( STATE_TYPE_RASTERIZER, STATE_RASTER_SOLID );
    }

    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Post-effects pipeline completed\n" );
    #endif
}

//==============================================================================
//  INDIVIDUAL POST EFFECTS
//==============================================================================

void post_mgr::ApplySelfIllumGlows( f32 MotionBlurIntensity, s32 GlowCutoff )
{
    if( !m_bInitialized || m_Flags.Override )
        return;

    ASSERT( m_bInPost );
    m_Flags.DoSelfIllumGlow = TRUE;
    m_Glow.MotionBlurIntensity = MotionBlurIntensity;
    m_Glow.Cutoff = GlowCutoff;
}

//==============================================================================

void post_mgr::MotionBlur( f32 Intensity )
{
    if( !m_bInitialized || m_Flags.Override )
        return;

    ASSERT( m_bInPost );
    m_Flags.DoMotionBlur = TRUE;
    m_MotionBlur.Intensity = Intensity;
}

//==============================================================================

void post_mgr::ZFogFilter( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    if( !m_bInitialized || m_Flags.Override )
        return;
   
    ASSERT( m_bInPost );
    ASSERT( Fn != render::FALLOFF_CUSTOM );
   
    // Adjust our fog palette (will also save the fog parameters)
    pc_CreateFogPalette( Fn, Color, Param1, Param2 );
    m_Flags.DoZFogFn = TRUE;
    m_FogFilter.PaletteIndex = 2;
}

//==============================================================================

void post_mgr::ZFogFilter( render::post_falloff_fn Fn, s32 PaletteIndex )
{
    if( !m_bInitialized || m_Flags.Override )
        return;
    
    ASSERT( (PaletteIndex >= 0) && (PaletteIndex <= 4) );
    if( !m_bFogValid[PaletteIndex] )
        return;
   
    ASSERT( m_bInPost );
    ASSERT( Fn == render::FALLOFF_CUSTOM );
    m_Flags.DoZFogCustom = TRUE;
    m_FogFilter.Fn[PaletteIndex] = Fn;
    m_FogFilter.PaletteIndex = PaletteIndex;
}

//==============================================================================

void post_mgr::MipFilter( s32 nFilters, f32 Offset, render::post_falloff_fn Fn, 
                         xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex )
{
    if( !m_bInitialized || m_Flags.Override )
        return;

    ASSERT( m_bInPost );
    ASSERT( Fn != render::FALLOFF_CUSTOM );
    ASSERT( (PaletteIndex >= 0) && (PaletteIndex < 4) );

    pc_CreateMipPalette( Fn, Color, Param1, Param2, PaletteIndex );
    m_Flags.DoMipFn = TRUE;
    m_MipFilter.Fn[PaletteIndex] = Fn;
    m_MipFilter.Count[PaletteIndex] = nFilters;
    m_MipFilter.Offset[PaletteIndex] = Offset;
    m_MipFilter.PaletteIndex = PaletteIndex;
}

//==============================================================================

void post_mgr::MipFilter( s32 nFilters, f32 Offset, render::post_falloff_fn Fn,
                         const texture::handle& Texture, s32 PaletteIndex )
{
    if( !m_bInitialized || m_Flags.Override )
        return;

    ASSERT( m_bInPost );
    ASSERT( Fn == render::FALLOFF_CUSTOM );
    ASSERT( (PaletteIndex >= 0) && (PaletteIndex < 4) );

    // Safety check
    if( Texture.GetPointer() == NULL )
        return;

    m_Flags.DoMipCustom = TRUE;
    m_pMipTexture = &Texture.GetPointer()->m_Bitmap;
    m_MipFilter.Fn[PaletteIndex] = Fn;
    m_MipFilter.Count[PaletteIndex] = nFilters;
    m_MipFilter.Offset[PaletteIndex] = Offset;
    m_MipFilter.PaletteIndex = PaletteIndex;
    ASSERT( m_pMipTexture );
}

//==============================================================================

void post_mgr::NoiseFilter( xcolor Color )
{
    if( !m_bInitialized || m_Flags.Override )
        return;

    ASSERT( m_bInPost );
    m_Flags.DoNoise = TRUE;
    m_Simple.NoiseColor = Color;
}

//==============================================================================

void post_mgr::ScreenFade( xcolor Color )
{
    if( !m_bInitialized || m_Flags.Override )
        return;

    ASSERT( m_bInPost );
    m_Simple.FadeColor = Color;
    m_Flags.DoScreenFade = TRUE;
}

//==============================================================================

void post_mgr::MultScreen( xcolor MultColor, render::post_screen_blend FinalBlend )
{
    if( !m_bInitialized || m_Flags.Override )
        return;

    ASSERT( m_bInPost );
    m_Flags.DoMultScreen = TRUE;
    m_MultScreen.Color = MultColor;
    m_MultScreen.Blend = FinalBlend;
}

//==============================================================================

void post_mgr::RadialBlur( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale )
{
    if( !m_bInitialized || m_Flags.Override )
        return;

    ASSERT( m_bInPost );
    m_Flags.DoRadialBlur = TRUE;
    m_RadialBlur.Zoom = Zoom;
    m_RadialBlur.Angle = Angle;
    m_RadialBlur.AlphaSub = AlphaSub;
    m_RadialBlur.AlphaScale = AlphaScale;
}

//==============================================================================
//  INTERNAL EFFECT PROCESSING FUNCTIONS (STUBS FOR NOW)
//==============================================================================

void post_mgr::pc_MotionBlur( void )
{
    // TODO: Implement DirectX 11 motion blur
    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Motion blur (intensity: %f)\n", m_MotionBlur.Intensity );
    #endif
}

//==============================================================================

void post_mgr::pc_ApplySelfIllumGlows( void )
{
    if( !g_pd3dContext )
        return;

    if( !m_pGlowDownsamplePS || !m_pGlowBlurHPS || !m_pGlowBlurVPS || !m_pGlowCompositePS || !m_pGlowConstantBuffer )
        return;

    const rtarget* pGlowSource = g_GBufferMgr.GetGBufferTarget( GBUFFER_GLOW );
    if( !pGlowSource || !pGlowSource->pShaderResourceView )
        return;

    const u32 sourceWidth  = pGlowSource->Desc.Width;
    const u32 sourceHeight = pGlowSource->Desc.Height;
    if( sourceWidth == 0 || sourceHeight == 0 )
        return;

    if( !EnsureGlowTargets( sourceWidth, sourceHeight ) )
        return;

    const f32 cutoff       = (m_Glow.Cutoff >= 255) ? 0.0f : (f32)m_Glow.Cutoff / 255.0f;
    const f32 motionBlend  = x_clamp( m_Glow.MotionBlurIntensity, 0.0f, 1.0f );
    const f32 intensity    = kGlowIntensityScale;
    const f32 clearColor[4]= { 0.0f, 0.0f, 0.0f, 0.0f };

    // Downsample high-resolution glow mask to a smaller buffer
    rtarget_SetTargets( &m_GlowDownsample, 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    UpdateGlowConstants( cutoff, intensity, motionBlend, 1.0f / (f32)sourceWidth, 1.0f / (f32)sourceHeight );
    composite_Blit( *pGlowSource, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_pGlowDownsamplePS );

    // Horizontal blur
    rtarget_SetTargets( &m_GlowBlur[0], 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    UpdateGlowConstants( cutoff, intensity, motionBlend, 1.0f / (f32)m_GlowBufferWidth, 0.0f );
    composite_Blit( m_GlowDownsample, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_pGlowBlurHPS, STATE_SAMPLER_LINEAR_CLAMP );

    // Vertical blur
    rtarget_SetTargets( &m_GlowBlur[1], 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    UpdateGlowConstants( cutoff, intensity, motionBlend, 0.0f, 1.0f / (f32)m_GlowBufferHeight );
    composite_Blit( m_GlowBlur[0], COMPOSITE_BLEND_ADDITIVE, 1.0f, m_pGlowBlurVPS, STATE_SAMPLER_LINEAR_CLAMP );

    const rtarget* pBlurredResult = &m_GlowBlur[1];

    if( motionBlend > 0.0f && m_GlowHistory.pShaderResourceView )
    {
        rtarget_SetTargets( &m_GlowComposite, 1, NULL );
        rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
        UpdateGlowConstants( cutoff, intensity, motionBlend, 0.0f, 0.0f );

        ID3D11ShaderResourceView* pHistorySRV = m_GlowHistory.pShaderResourceView;
        g_pd3dContext->PSSetShaderResources( 1, 1, &pHistorySRV );

        composite_Blit( *pBlurredResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_pGlowCombinePS, STATE_SAMPLER_LINEAR_CLAMP );

        ID3D11ShaderResourceView* pNullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( 1, 1, &pNullSRV );

        pBlurredResult = &m_GlowComposite;

        if( m_GlowHistory.pTexture && pBlurredResult->pTexture )
            g_pd3dContext->CopyResource( m_GlowHistory.pTexture, pBlurredResult->pTexture );
    }
    else if( m_GlowHistory.pTexture && pBlurredResult->pTexture )
    {
        g_pd3dContext->CopyResource( m_GlowHistory.pTexture, pBlurredResult->pTexture );
    }

    m_pActiveGlowResult    = pBlurredResult;
    m_bGlowPendingComposite = TRUE;
}

//==============================================================================

void post_mgr::UpdateGlowStageBegin( void )
{
    m_bGlowPendingComposite = FALSE;
    m_pActiveGlowResult = NULL;

    if( !g_GBufferMgr.IsGBufferEnabled() )
    {
        ReleaseGlowTargets();
        return;
    }

    u32 width = 0;
    u32 height = 0;
    g_GBufferMgr.GetGBufferSize( width, height );

    if( (width == 0) || (height == 0) )
    {
        ReleaseGlowTargets();
        return;
    }

    u32 targetWidth  = (width  > 1) ? (width  / 2) : width;
    u32 targetHeight = (height > 1) ? (height / 2) : height;

    if( m_bGlowResourcesValid && ((m_GlowBufferWidth != targetWidth) || (m_GlowBufferHeight != targetHeight)) )
    {
        ReleaseGlowTargets();
    }
}

//==============================================================================

void post_mgr::CompositePendingGlow( void )
{
    if( !m_bGlowPendingComposite || !m_pActiveGlowResult )
        return;

    if( !m_pActiveGlowResult->pShaderResourceView )
        return;

    if( !g_pd3dContext )
        return;

    rtarget_SetBackBuffer();

    if( m_pGlowCompositePS && m_pGlowConstantBuffer )
    {
        UpdateGlowConstants( 0.0f, kGlowIntensityScale, 0.0f, 0.0f, 0.0f );
        composite_Blit( *m_pActiveGlowResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_pGlowCompositePS, STATE_SAMPLER_LINEAR_CLAMP );
    }
    else
    {
        composite_Blit( *m_pActiveGlowResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, NULL, STATE_SAMPLER_LINEAR_CLAMP );
    }

    m_bGlowPendingComposite = FALSE;
    m_pActiveGlowResult = NULL;
}

//==============================================================================

xbool post_mgr::EnsureGlowTargets( u32 SourceWidth, u32 SourceHeight )
{
    u32 width  = (SourceWidth  > 1) ? (SourceWidth  / 2) : SourceWidth;
    u32 height = (SourceHeight > 1) ? (SourceHeight / 2) : SourceHeight;

    if( width == 0 || height == 0 )
        return FALSE;

    if( m_bGlowResourcesValid && (m_GlowBufferWidth == width) && (m_GlowBufferHeight == height) )
        return TRUE;

    ReleaseGlowTargets();

    rtarget_desc desc;
    desc.Width = width;
    desc.Height = height;
    desc.Format = RTARGET_FORMAT_RGBA16F;
    desc.SampleCount = 1;
    desc.SampleQuality = 0;
    desc.bBindAsTexture = TRUE;

    if( !rtarget_Create( m_GlowDownsample, desc ) )
        return FALSE;

    if( !rtarget_Create( m_GlowBlur[0], desc ) )
    {
        ReleaseGlowTargets();
        return FALSE;
    }

    if( !rtarget_Create( m_GlowBlur[1], desc ) )
    {
        ReleaseGlowTargets();
        return FALSE;
    }

    if( !rtarget_Create( m_GlowComposite, desc ) )
    {
        ReleaseGlowTargets();
        return FALSE;
    }

    if( !rtarget_Create( m_GlowHistory, desc ) )
    {
        ReleaseGlowTargets();
        return FALSE;
    }

    if( g_pd3dContext )
    {
        const f32 clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dContext->ClearRenderTargetView( m_GlowDownsample.pRenderTargetView, clearColor );
        g_pd3dContext->ClearRenderTargetView( m_GlowBlur[0].pRenderTargetView, clearColor );
        g_pd3dContext->ClearRenderTargetView( m_GlowBlur[1].pRenderTargetView, clearColor );
        g_pd3dContext->ClearRenderTargetView( m_GlowComposite.pRenderTargetView, clearColor );
        g_pd3dContext->ClearRenderTargetView( m_GlowHistory.pRenderTargetView, clearColor );
    }

    m_GlowBufferWidth = width;
    m_GlowBufferHeight = height;
    m_bGlowResourcesValid = TRUE;
    m_pActiveGlowResult = NULL;

    return TRUE;
}

//==============================================================================

void post_mgr::ReleaseGlowTargets( void )
{
    rtarget_Destroy( m_GlowDownsample );
    rtarget_Destroy( m_GlowBlur[0] );
    rtarget_Destroy( m_GlowBlur[1] );
    rtarget_Destroy( m_GlowComposite );
    rtarget_Destroy( m_GlowHistory );

    m_GlowBufferWidth = 0;
    m_GlowBufferHeight = 0;
    m_bGlowResourcesValid = FALSE;
    m_pActiveGlowResult = NULL;
    m_bGlowPendingComposite = FALSE;
}

//==============================================================================

void post_mgr::UpdateGlowConstants( f32 Cutoff, f32 IntensityScale, f32 MotionBlend, f32 StepX, f32 StepY )
{
    if( !m_pGlowConstantBuffer || !g_pd3dContext )
        return;

    cb_post_glow cbData;
    cbData.Params0.Set( Cutoff, IntensityScale, MotionBlend, 0.0f );
    cbData.Params1.Set( StepX, StepY, 0.0f, 0.0f );

    shader_UpdateConstantBuffer( m_pGlowConstantBuffer, &cbData, sizeof(cb_post_glow) );
    g_pd3dContext->PSSetConstantBuffers( 4, 1, &m_pGlowConstantBuffer );
}

//==============================================================================

void post_mgr::pc_MultScreen( void )
{
    // TODO: Implement DirectX 11 mult screen
    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Mult screen (color: %d,%d,%d,%d)\n", 
               m_MultScreen.Color.R, m_MultScreen.Color.G, 
               m_MultScreen.Color.B, m_MultScreen.Color.A );
    #endif           
}

//==============================================================================

void post_mgr::pc_RadialBlur( void )
{
    // TODO: Implement DirectX 11 radial blur
    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Radial blur (zoom: %f, angle: %f)\n", 
               m_RadialBlur.Zoom, m_RadialBlur.Angle );
    #endif           
}

//==============================================================================

void post_mgr::pc_ZFogFilter( void )
{
    // TODO: Implement DirectX 11 Z fog filter
    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Z fog filter (palette: %d)\n", m_FogFilter.PaletteIndex );
    #endif
}

//==============================================================================

void post_mgr::pc_MipFilter( void )
{
    // TODO: Implement DirectX 11 mip filter
    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Mip filter (palette: %d, count: %d)\n", 
               m_MipFilter.PaletteIndex, m_MipFilter.Count[m_MipFilter.PaletteIndex] );
    #endif           
}

//==============================================================================

void post_mgr::pc_NoiseFilter( void )
{
    // TODO: Implement DirectX 11 noise filter
    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Noise filter (color: %d,%d,%d,%d)\n", 
               m_Simple.NoiseColor.R, m_Simple.NoiseColor.G, 
               m_Simple.NoiseColor.B, m_Simple.NoiseColor.A );
    #endif           
}

//==============================================================================

void post_mgr::pc_ScreenFade( void )
{
    if( !g_pd3dContext )
        return;
        
    // Render a big transparent quad over the screen (like original)
    irect Rect;
    Rect.l = m_PostViewL;
    Rect.t = m_PostViewT;
    Rect.r = m_PostViewR;
    Rect.b = m_PostViewB;
    
    // Set blend state for fade effect
    state_SetState( STATE_TYPE_BLEND, STATE_BLEND_ALPHA );
    state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_DISABLED );
    
    // Draw the fade rect using draw system
    draw_Rect( Rect, m_Simple.FadeColor, FALSE );
    
    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Screen fade (color: %d,%d,%d,%d)\n", 
               m_Simple.FadeColor.R, m_Simple.FadeColor.G, 
               m_Simple.FadeColor.B, m_Simple.FadeColor.A );
    #endif           
}

//==============================================================================
//  HELPER FUNCTIONS (STUBS FOR NOW)
//==============================================================================

void post_mgr::pc_CreateFogPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    // Force this to be fog palette 2 (like original)
    m_FogFilter.PaletteIndex = 2;
    
    // Is it necessary to rebuild the palette?
    if( (m_FogFilter.Fn[m_FogFilter.PaletteIndex] == Fn) &&
        (m_FogFilter.Param1[m_FogFilter.PaletteIndex] == Param1) &&
        (m_FogFilter.Param2[m_FogFilter.PaletteIndex] == Param2) &&
        (m_FogFilter.Color[m_FogFilter.PaletteIndex] == Color) )
    {
        return;
    }
    
    m_FogFilter.Fn[m_FogFilter.PaletteIndex] = Fn;
    m_FogFilter.Param1[m_FogFilter.PaletteIndex] = Param1;
    m_FogFilter.Param2[m_FogFilter.PaletteIndex] = Param2;
    m_FogFilter.Color[m_FogFilter.PaletteIndex] = Color;
    
    // TODO: Replace CLUT palette with DirectX 11 shader constants
    m_bFogValid[m_FogFilter.PaletteIndex] = TRUE;
}

//==============================================================================

void post_mgr::pc_CreateMipPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex )
{
    // Is it necessary to rebuild the palette?
    if( (m_MipFilter.Fn[PaletteIndex] == Fn) &&
        (m_MipFilter.Param1[PaletteIndex] == Param1) &&
        (m_MipFilter.Param2[PaletteIndex] == Param2) &&
        (m_MipFilter.Color[PaletteIndex] == Color) )
    {
        return;
    }

    m_MipFilter.Fn[PaletteIndex] = Fn;
    m_MipFilter.Param1[PaletteIndex] = Param1;
    m_MipFilter.Param2[PaletteIndex] = Param2;
    m_MipFilter.Color[PaletteIndex] = Color;
    m_MipFilter.PaletteIndex = PaletteIndex;

    // TODO: Replace CLUT palette with DirectX 11 shader constants
}

//==============================================================================

void post_mgr::pc_CopyBackBuffer( void )
{
    // TODO: Implement back buffer copy for DX11
    x_DebugMsg( "PostMgr: Copying back buffer\n" );
}

//==============================================================================

void post_mgr::pc_BuildScreenMips( s32 nMips, xbool UseAlpha )
{
    // TODO: Implement screen mips building for DX11
    #ifdef POST_VERBOSE_MODE
    x_DebugMsg( "PostMgr: Building screen mips (%d levels)\n", nMips );
    #endif
    (void)UseAlpha;
}

//==============================================================================

xcolor post_mgr::GetFogValue( const vector3& WorldPos, s32 PaletteIndex )
{
    // TODO: Implement fog value calculation
    (void)WorldPos; (void)PaletteIndex;
    return xcolor(255, 255, 255, 0);
}






//==============================================================================

void post_mgr::OnGlowStageBeginFrame( void )
{
    if( !m_bInitialized )
        return;

    UpdateGlowStageBegin();
}

//==============================================================================

void post_mgr::OnGlowStageBeforePresent( void )
{
    if( !m_bInitialized )
        return;

    CompositePendingGlow();
}

//==============================================================================

static void GlowStage_OnBeginFrame( void )
{
    g_PostMgr.OnGlowStageBeginFrame();
}

//==============================================================================

static void GlowStage_OnBeforePresent( void )
{
    g_PostMgr.OnGlowStageBeforePresent();
}