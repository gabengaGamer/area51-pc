//==============================================================================
//  
//  PostMgr.cpp
//  
//  Post-processing Manager for PC platform
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
#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

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

    m_bInitialized = TRUE;
    x_DebugMsg( "PostMgr: Post-processing manager initialized successfully\n" );
}

//==============================================================================

void post_mgr::Kill( void )
{
    if( !m_bInitialized )
        return;

    x_DebugMsg( "PostMgr: Shutting down post-processing manager\n" );

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

    x_DebugMsg( "PostMgr: Post-effects pipeline started\n" );
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

    x_DebugMsg( "PostMgr: Post-effects pipeline completed\n" );
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
    x_DebugMsg( "PostMgr: Motion blur (intensity: %f)\n", m_MotionBlur.Intensity );
}

//==============================================================================

void post_mgr::pc_ApplySelfIllumGlows( void )
{
    // TODO: Implement DirectX 11 self-illum glow
    x_DebugMsg( "PostMgr: Self-illum glow (intensity: %f, cutoff: %d)\n", 
               m_Glow.MotionBlurIntensity, m_Glow.Cutoff );
}

//==============================================================================

void post_mgr::pc_MultScreen( void )
{
    // TODO: Implement DirectX 11 mult screen
    x_DebugMsg( "PostMgr: Mult screen (color: %d,%d,%d,%d)\n", 
               m_MultScreen.Color.R, m_MultScreen.Color.G, 
               m_MultScreen.Color.B, m_MultScreen.Color.A );
}

//==============================================================================

void post_mgr::pc_RadialBlur( void )
{
    // TODO: Implement DirectX 11 radial blur
    x_DebugMsg( "PostMgr: Radial blur (zoom: %f, angle: %f)\n", 
               m_RadialBlur.Zoom, m_RadialBlur.Angle );
}

//==============================================================================

void post_mgr::pc_ZFogFilter( void )
{
    // TODO: Implement DirectX 11 Z fog filter
    x_DebugMsg( "PostMgr: Z fog filter (palette: %d)\n", m_FogFilter.PaletteIndex );
}

//==============================================================================

void post_mgr::pc_MipFilter( void )
{
    // TODO: Implement DirectX 11 mip filter
    x_DebugMsg( "PostMgr: Mip filter (palette: %d, count: %d)\n", 
               m_MipFilter.PaletteIndex, m_MipFilter.Count[m_MipFilter.PaletteIndex] );
}

//==============================================================================

void post_mgr::pc_NoiseFilter( void )
{
    // TODO: Implement DirectX 11 noise filter
    x_DebugMsg( "PostMgr: Noise filter (color: %d,%d,%d,%d)\n", 
               m_Simple.NoiseColor.R, m_Simple.NoiseColor.G, 
               m_Simple.NoiseColor.B, m_Simple.NoiseColor.A );
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
    
    x_DebugMsg( "PostMgr: Screen fade (color: %d,%d,%d,%d)\n", 
               m_Simple.FadeColor.R, m_Simple.FadeColor.G, 
               m_Simple.FadeColor.B, m_Simple.FadeColor.A );
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
    x_DebugMsg( "PostMgr: Building screen mips (%d levels)\n", nMips );
    (void)UseAlpha;
}

//==============================================================================

xcolor post_mgr::GetFogValue( const vector3& WorldPos, s32 PaletteIndex )
{
    // TODO: Implement fog value calculation
    (void)WorldPos; (void)PaletteIndex;
    return xcolor(255, 255, 255, 0);
}