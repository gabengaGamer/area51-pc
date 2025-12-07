//==============================================================================
// 
//  PostMgr.cpp
// 
//  Post-processing manager entry points for PC platform
// 
//==============================================================================

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

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

post_mgr g_PostMgr;

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

static const eng_frame_stage s_PostFrameStage =
{
    post_mgr::PostStage_BeginFrameThunk,
    post_mgr::PostStage_BeforePresentThunk
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

void post_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    x_DebugMsg( "PostMgr: Initializing post-processing manager\n" );

    m_bInPost = FALSE;
    m_PostViewL = m_PostViewT = m_PostViewR = m_PostViewB = 0;
    m_PostNearZ = 1.0f;
    m_PostFarZ = 2.0f;
    m_pMipTexture = NULL;

    for( s32 i = 0; i < 3; i++ )
    {
        m_bFogValid[i] = FALSE;
    }

    m_Flags = post_effect_flags();
    m_MotionBlur = post_motion_blur_params();
    m_Glow = post_glow_params();
    m_MultScreen = post_mult_screen_params();
    m_RadialBlur = post_radial_blur_params();
    m_FogFilter = post_fog_filter_params();
    m_MipFilter = post_mip_filter_params();
    m_Simple = post_simple_params();

    m_bPostStageRegistered = FALSE;
    m_GlowResources = glow_resources();
    m_GlowResources.Initialize();

    d3deng_RegisterFrameStage( s_PostFrameStage );
    m_bPostStageRegistered = TRUE;

    m_bInitialized = TRUE;
    x_DebugMsg( "PostMgr: Post-processing manager initialized successfully\n" );
}

//==============================================================================

void post_mgr::Kill( void )
{
    if( !m_bInitialized )
        return;

    x_DebugMsg( "PostMgr: Shutting down post-processing manager\n" );

    if( m_bPostStageRegistered )
    {
        d3deng_UnregisterFrameStage( s_PostFrameStage );
        m_bPostStageRegistered = FALSE;
    }

    m_GlowResources.Shutdown();

    m_bInitialized = FALSE;
    x_DebugMsg( "PostMgr: Post-processing manager shutdown complete\n" );
}

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

    m_pMipTexture = NULL;
}

//==============================================================================

void post_mgr::EndPostEffects( void )
{
    if( !m_bInitialized || !m_bInPost )
        return;

    ASSERT( m_bInPost );
    m_bInPost = FALSE;

    const view* pView = eng_GetView();
    if( pView )
    {
        pView->GetViewport( m_PostViewL, m_PostViewT, m_PostViewR, m_PostViewB );
        pView->GetZLimits( m_PostNearZ, m_PostFarZ );
    }

    if( m_Flags.DoZFogCustom || m_Flags.DoZFogFn )
    {
        ExecuteZFogFilter();
    }

    if( m_Flags.DoMipCustom || m_Flags.DoMipFn )
    {
        ExecuteMipFilter();
    }

    if( m_Flags.DoMotionBlur )
    {
        ExecuteMotionBlur();
    }

    if( m_Flags.DoSelfIllumGlow )
    {
        ExecuteSelfIllumGlow();
    }

    if( m_Flags.DoMultScreen || m_Flags.DoRadialBlur )
    {
        CopyBackBuffer();
    }

    if( m_Flags.DoMultScreen )
    {
        ExecuteMultScreen();
    }

    if( m_Flags.DoRadialBlur )
    {
        ExecuteRadialBlur();
    }

    if( m_Flags.DoNoise )
    {
        ExecuteNoiseFilter();
    }

    if( m_Flags.DoScreenFade )
    {
        ExecuteScreenFade();
    }

    RestoreDefaultState();
}

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

    BuildFogPalette( Fn, Color, Param1, Param2 );
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

    BuildMipPalette( Fn, Color, Param1, Param2, PaletteIndex );
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

xcolor post_mgr::GetFogValue( const vector3& WorldPos, s32 PaletteIndex )
{
    (void)WorldPos;
    (void)PaletteIndex;
    return xcolor(255, 255, 255, 0);
}

//==============================================================================

void post_mgr::PrepareFullscreenQuad( void ) const
{
    state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_DISABLED );
    state_SetState( STATE_TYPE_RASTERIZER, STATE_RASTER_SOLID );
}

//==============================================================================

void post_mgr::RestoreDefaultState( void ) const
{
    state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_NORMAL );
    state_SetState( STATE_TYPE_BLEND, STATE_BLEND_NONE );
    state_SetState( STATE_TYPE_RASTERIZER, STATE_RASTER_SOLID );
}

//==============================================================================
//  FRAME STAGE THUNKS
//==============================================================================

void post_mgr::PostStage_BeginFrameThunk( void )
{
    if( !g_PostMgr.m_bInitialized )
        return;

    // GS: NOTE: Dispatch additional post-processing stages here (glow, blur, etc.)
    g_PostMgr.UpdateGlowStageBegin();
}

//==============================================================================

void post_mgr::PostStage_BeforePresentThunk( void )
{
    if( !g_PostMgr.m_bInitialized )
        return;

    // GS: NOTE: Dispatch additional post-processing stages here (glow, blur, etc.)
    g_PostMgr.CompositePendingGlow();
}
