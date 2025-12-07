//==============================================================================
// 
//  PostMgr.hpp
// 
//  Post-processing manager for PC platform
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

#include "../../Texture.hpp"
#include "../../Render.hpp"

#include "../GBufferMgr.hpp"

#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"
#include "Entropy/D3DEngine/d3deng_shader.hpp"
#include "Entropy/D3DEngine/d3deng_composite.hpp"

#include "e_engine.hpp"

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

//------------------------------------------------------------------------------

struct post_glow_params
{
    f32     MotionBlurIntensity;
    s32     Cutoff;

    post_glow_params() : MotionBlurIntensity(0.0f), Cutoff(0) {}
};

//------------------------------------------------------------------------------

struct post_mult_screen_params
{
    xcolor                      Color;
    render::post_screen_blend   Blend;

    post_mult_screen_params() : Color(255,255,255,255), Blend((render::post_screen_blend)0) {}
};

//------------------------------------------------------------------------------

struct post_radial_blur_params
{
    f32     Zoom;
    radian  Angle;
    f32     AlphaSub;
    f32     AlphaScale;

    post_radial_blur_params() : Zoom(1.0f), Angle(0.0f), AlphaSub(0.0f), AlphaScale(1.0f) {}
};

//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------

struct post_simple_params
{
    xcolor  NoiseColor;
    xcolor  FadeColor;

    post_simple_params() : NoiseColor(255,255,255,255), FadeColor(0,0,0,0) {}
};

//------------------------------------------------------------------------------

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
	
    static void PostStage_BeginFrameThunk   ( void );
    static void PostStage_BeforePresentThunk( void );

protected:

    // Internal effect processing functions
    void        ExecuteMotionBlur           ( void );
    void        ExecuteSelfIllumGlow        ( void );
    void        ExecuteMultScreen           ( void );
    void        ExecuteRadialBlur           ( void );
    void        ExecuteZFogFilter           ( void );
    void        ExecuteMipFilter            ( void );
    void        ExecuteNoiseFilter          ( void );
    void        ExecuteScreenFade           ( void );

    void        UpdateGlowStageBegin        ( void );
    void        CompositePendingGlow        ( void );

    // Helper functions
    void        BuildFogPalette             ( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 );
    void        BuildMipPalette             ( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex );
    void        CopyBackBuffer              ( void );
    void        BuildScreenMips             ( s32 nMips, xbool UseAlpha );
    void        PrepareFullscreenQuad       ( void ) const;
    void        RestoreDefaultState         ( void ) const;

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
    struct glow_resources
    {
        glow_resources();

        void    Initialize                 ( void );
        void    Shutdown                   ( void );
        void    ResetFrame                 ( void );
        xbool   ResizeIfNeeded             ( u32 SourceWidth, u32 SourceHeight );
        const rtarget*
                BindForComposite          ( void ) const;
        void    FinalizeComposite          ( void );
        void    UpdateConstants            ( f32 Cutoff, f32 IntensityScale, f32 MotionBlend, f32 StepX, f32 StepY, f32 CompositeWeight = 1.0f );
        void    SetPendingResult           ( const rtarget* pResult );

        rtarget             Downsample[3];
        rtarget             Blur[2];
        rtarget             Composite;
        rtarget             Accum;
        rtarget             History;
        const rtarget*      ActiveResult;
        u32                 BufferWidth;
        u32                 BufferHeight;
        xbool               bResourcesValid;
        xbool               bPendingComposite;
        ID3D11PixelShader*  pDownsamplePS;
        ID3D11PixelShader*  pBlurHPS;
        ID3D11PixelShader*  pBlurVPS;
        ID3D11PixelShader*  pCombinePS;
        ID3D11PixelShader*  pCompositePS;
        ID3D11Buffer*       pConstantBuffer;
    };

    glow_resources          m_GlowResources;
    xbool                   m_bPostStageRegistered;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern post_mgr g_PostMgr;

//==============================================================================
//  END
//==============================================================================
#endif