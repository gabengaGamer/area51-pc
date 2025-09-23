//==============================================================================
//  
//  PostMgr.hpp
//  
//  Post-processing Manager for PC platform
//
//==============================================================================

#ifndef POST_MANAGER_HPP
#define POST_MANAGER_HPP

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

#include "../Texture.hpp"
#include "../render.hpp"
#include "e_engine.hpp"

#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"
#include "Entropy/D3DEngine/d3deng_shader.hpp"
#include "Entropy/D3DEngine/d3deng_composite.hpp"
#include "GBufferMgr.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define ZBUFFER_BITS            19
#define MAX_POST_MIPS           3

//==============================================================================
//  POST-PROCESSING PARAMETER STRUCTURES
//==============================================================================

struct post_motion_blur_params
{
    f32     Intensity;
    
    post_motion_blur_params() : Intensity(0.0f) {}
};

struct post_glow_params
{
    f32     MotionBlurIntensity;
    s32     Cutoff;
    
    post_glow_params() : MotionBlurIntensity(0.0f), Cutoff(0) {}
};

struct post_mult_screen_params
{
    xcolor                      Color;
    render::post_screen_blend   Blend;
    
    post_mult_screen_params() : Color(255,255,255,255), Blend((render::post_screen_blend)0) {}
};

struct post_radial_blur_params
{
    f32     Zoom;
    radian  Angle;
    f32     AlphaSub;
    f32     AlphaScale;
    
    post_radial_blur_params() : Zoom(1.0f), Angle(0.0f), AlphaSub(0.0f), AlphaScale(1.0f) {}
};

struct post_fog_filter_params
{
    render::post_falloff_fn     Fn[5];
    xcolor                      Color[5];
    f32                         Param1[5];
    f32                         Param2[5];
    s32                         PaletteIndex;
    
    post_fog_filter_params() : PaletteIndex(-1)
    {
        for( s32 i = 0; i < 5; i++ )
        {
            Fn[i] = (render::post_falloff_fn)0;
            Color[i] = xcolor(255,255,255,255);
            Param1[i] = 0.0f;
            Param2[i] = 0.0f;
        }
    }
};

struct post_mip_filter_params
{
    render::post_falloff_fn     Fn[4];
    xcolor                      Color[4];
    f32                         Param1[4];
    f32                         Param2[4];
    s32                         Count[4];
    f32                         Offset[4];
    s32                         PaletteIndex;
    
    post_mip_filter_params() : PaletteIndex(-1)
    {
        for( s32 i = 0; i < 4; i++ )
        {
            Fn[i] = (render::post_falloff_fn)0;
            Color[i] = xcolor(255,255,255,255);
            Param1[i] = 0.0f;
            Param2[i] = 0.0f;
            Count[i] = 0;
            Offset[i] = 0.0f;
        }
    }
};

struct post_simple_params
{
    xcolor  NoiseColor;
    xcolor  FadeColor;
    
    post_simple_params() : NoiseColor(255,255,255,255), FadeColor(0,0,0,0) {}
};

struct post_effect_flags
{
    // Debug override flag
    xbool   Override        : 1;    // Set this to play around with values in the debugger
    
    // Effect enable flags
    xbool   DoMotionBlur    : 1;
    xbool   DoSelfIllumGlow : 1;
    xbool   DoMultScreen    : 1;
    xbool   DoRadialBlur    : 1;
    xbool   DoZFogFn        : 1;
    xbool   DoZFogCustom    : 1;
    xbool   DoMipFn         : 1;
    xbool   DoMipCustom     : 1;
    xbool   DoNoise         : 1;
    xbool   DoScreenFade    : 1;
    
    post_effect_flags() { x_memset( this, 0, sizeof(post_effect_flags) ); }
};

//==============================================================================
//  POST-PROCESSING MANAGER CLASS
//==============================================================================

class post_mgr
{
public:

    void        Init                        ( void );
    void        Kill                        ( void );

    // Main post-processing pipeline
    void        BeginPostEffects            ( void );
    void        EndPostEffects              ( void );

    // Individual post effects
    void        ApplySelfIllumGlows         ( f32 MotionBlurIntensity, s32 GlowCutoff );
    void        MotionBlur                  ( f32 Intensity );
    void        ZFogFilter                  ( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 );
    void        ZFogFilter                  ( render::post_falloff_fn Fn, s32 PaletteIndex );
    void        MipFilter                   ( s32 nFilters, f32 Offset, render::post_falloff_fn Fn, 
                                             xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex );
    void        MipFilter                   ( s32 nFilters, f32 Offset, render::post_falloff_fn Fn,
                                             const texture::handle& Texture, s32 PaletteIndex );
    void        NoiseFilter                 ( xcolor Color );
    void        ScreenFade                  ( xcolor Color );
    void        MultScreen                  ( xcolor MultColor, render::post_screen_blend FinalBlend );
    void        RadialBlur                  ( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale );

    // Utility functions
    xcolor      GetFogValue                 ( const vector3& WorldPos, s32 PaletteIndex );
    void        OnGlowStageBeginFrame       ( void );
    void        OnGlowStageBeforePresent    ( void );

protected:

    // Internal effect processing functions
    void        pc_MotionBlur               ( void );
    void        pc_ApplySelfIllumGlows      ( void );
    void        pc_MultScreen               ( void );
    void        pc_RadialBlur               ( void );
    void        pc_ZFogFilter               ( void );
    void        pc_MipFilter                ( void );
    void        pc_NoiseFilter              ( void );
    void        pc_ScreenFade               ( void );

    void        UpdateGlowStageBegin        ( void );
    void        CompositePendingGlow        ( void );
    xbool       EnsureGlowTargets           ( u32 SourceWidth, u32 SourceHeight );
    void        ReleaseGlowTargets          ( void );
    void        UpdateGlowConstants         ( f32 Cutoff, f32 IntensityScale, f32 MotionBlend, f32 StepX, f32 StepY );
    

    // Helper functions
    void        pc_CreateFogPalette         ( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 );
    void        pc_CreateMipPalette         ( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex );
    void        pc_CopyBackBuffer           ( void );
    void        pc_BuildScreenMips          ( s32 nMips, xbool UseAlpha );

protected:

    // Initialization tracking
    xbool       m_bInitialized;

    // Current post-processing state
    xbool       m_bInPost;
    s32         m_PostViewL, m_PostViewT, m_PostViewR, m_PostViewB;
    f32         m_PostNearZ, m_PostFarZ;

    // Effect flags and parameters (organized by effect type)
    post_effect_flags           m_Flags;
    post_motion_blur_params     m_MotionBlur;
    post_glow_params            m_Glow;
    post_mult_screen_params     m_MultScreen;
    post_radial_blur_params     m_RadialBlur;
    post_fog_filter_params      m_FogFilter;
    post_mip_filter_params      m_MipFilter;
    post_simple_params          m_Simple;

    // Fog data
    xbool       m_bFogValid[3];

    // Mip filter data  
    const xbitmap*  m_pMipTexture;
    
    // Glow rendering resources
    rtarget                 m_GlowDownsample;
    rtarget                 m_GlowBlur[2];
    rtarget                 m_GlowComposite;
    rtarget                 m_GlowHistory;
    const rtarget*          m_pActiveGlowResult;
    u32                     m_GlowBufferWidth;
    u32                     m_GlowBufferHeight;
    xbool                   m_bGlowResourcesValid;
    xbool                   m_bGlowPendingComposite;
    xbool                   m_bGlowStageRegistered;
    ID3D11PixelShader*      m_pGlowDownsamplePS;
    ID3D11PixelShader*      m_pGlowBlurHPS;
    ID3D11PixelShader*      m_pGlowBlurVPS;
    ID3D11PixelShader*      m_pGlowCombinePS;
    ID3D11PixelShader*      m_pGlowCompositePS;
    ID3D11Buffer*           m_pGlowConstantBuffer;    
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern post_mgr g_PostMgr;

//==============================================================================
//  END
//==============================================================================
#endif