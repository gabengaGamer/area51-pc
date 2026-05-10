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
#define MAX_POST_SCREEN_WARPS   8

//==============================================================================
//  POST-PROCESSING PARAMETER STRUCTURES
//==============================================================================

struct post_glow_params
{
    f32     MotionBlurIntensity;
    s32     Cutoff;

    post_glow_params() : MotionBlurIntensity(0.0f), Cutoff(0) {}
};

//------------------------------------------------------------------------------

struct post_motion_blur_params
{
    f32     Intensity;

    post_motion_blur_params() : Intensity(0.0f) {}
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

struct post_screen_warp_params
{
    s32     Count;
    vector3 WorldPos[MAX_POST_SCREEN_WARPS];
    f32     Radius[MAX_POST_SCREEN_WARPS];
    f32     Amount[MAX_POST_SCREEN_WARPS];

    post_screen_warp_params() : Count(0) {}
};

//------------------------------------------------------------------------------

struct post_fog_filter_params
{
    render::post_falloff_fn     Fn[5];
    s32                         PaletteIndex;

    post_fog_filter_params() : PaletteIndex(-1)
    {
        for( s32 i = 0; i < 5; i++ )
        {
            Fn[i] = (render::post_falloff_fn)0;
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

    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------

    void        Init                        ( void );
    void        Kill                        ( void );

    //--------------------------------------------------------------------------
    // Frame Pipeline
    //--------------------------------------------------------------------------

    void        BeginPostEffects            ( void );
    void        EndPostEffects              ( void );

    //--------------------------------------------------------------------------
    // Post-Effect Requests
    //--------------------------------------------------------------------------

    void        ApplySelfIllumGlows         ( f32 MotionBlurIntensity, s32 GlowCutoff );
    void        MotionBlur                  ( f32 Intensity );
    void        AddScreenWarp               ( const vector3& WorldPos, f32 Radius, f32 WarpAmount );
    void        MipFilter                   ( s32 nFilters, f32 Offset, render::post_falloff_fn Fn,
                                             xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex );
    void        MipFilter                   ( s32 nFilters, f32 Offset, render::post_falloff_fn Fn,
                                             const texture::handle& Texture, s32 PaletteIndex );
    void        NoiseFilter                 ( xcolor Color );
    void        ScreenFade                  ( xcolor Color );
    void        MultScreen                  ( xcolor MultColor, render::post_screen_blend FinalBlend );
    void        RadialBlur                  ( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale );

    //--------------------------------------------------------------------------
    // Fog Requests And Queries
    //--------------------------------------------------------------------------

    void        SetCustomFogPalette         ( const texture::handle& Texture, xbool ImmediateSwitch, s32 PaletteIndex );
    void        ZFogFilter                  ( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 );
    void        ZFogFilter                  ( render::post_falloff_fn Fn, s32 PaletteIndex );
    xcolor      GetFogValue                 ( const vector3& WorldPos, s32 PaletteIndex );

    //--------------------------------------------------------------------------
    // Engine Frame Hooks
    //--------------------------------------------------------------------------

    static void PostStage_BeginFrameThunk   ( void );
    static void PostStage_BeforePresentThunk( void );

protected:

    //--------------------------------------------------------------------------
    // Internal Effect Execution
    //--------------------------------------------------------------------------

    void        ExecuteMotionBlur           ( void );
    void        ExecuteSelfIllumGlow        ( void );
    void        ExecuteRadialBlur           ( void );
    void        ExecuteScreenWarps          ( void );
    void        ExecuteZFogFilter           ( void );
    void        ExecuteMipFilter            ( void );
    void        ExecuteNoiseFilter          ( void );
    void        ExecuteScreenFade           ( void );

    //--------------------------------------------------------------------------
    // Per-Frame Maintenance
    //--------------------------------------------------------------------------

    void        UpdateGlowStageBegin        ( void );
    void        CompositePendingGlow        ( void );
    void        UpdateFilterHistoryBeforePresent( void );

    //--------------------------------------------------------------------------
    // Internal Helpers
    //--------------------------------------------------------------------------

    void        BuildFogPalette             ( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 );
    void        BuildMipPalette             ( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex );
    void        CopyBackBuffer              ( void );
    void        BuildScreenMips             ( s32 nMips );
    void        PrepareFullscreenQuad       ( void ) const;
    void        RestoreDefaultState         ( void ) const;

protected:

    //--------------------------------------------------------------------------
    // Manager State
    //--------------------------------------------------------------------------

    xbool       m_bInitialized;
    xbool       m_bInPost;

    // Cached view state for the current post pass.
    s32         m_PostViewL, m_PostViewT, m_PostViewR, m_PostViewB;
    f32         m_PostNearZ, m_PostFarZ;

    // Pending effect requests gathered between BeginPostEffects/EndPostEffects.
    post_effect_flags           m_Flags;
    post_motion_blur_params     m_MotionBlur;
    post_glow_params            m_Glow;
    post_radial_blur_params     m_RadialBlur;
    post_screen_warp_params     m_ScreenWarp;
    post_fog_filter_params      m_FogFilter;
    post_mip_filter_params      m_MipFilter;

    // Optional custom ramp source requested by the current mip filter pass.
    const xbitmap*              m_pMipTexture;
    post_simple_params          m_Simple;

    //--------------------------------------------------------------------------
    // Fog Resources
    //--------------------------------------------------------------------------

    struct fog_resources
    {
        fog_resources();

        // Resource lifetime
        void    Initialize                  ( void );
        void    Shutdown                    ( void );

        // Shader state updates
        void    UpdateConstants             ( const vector4& FogColor, const vector4& FogCoeff, f32 FogStart, f32 NearZ, f32 FarZ, xbool bUsePolynomial );
        xbool   UpdatePaletteTexture        ( const u8* pPalette );

        // GPU resources
        ID3D11Texture2D*            pPaletteTexture;
        ID3D11ShaderResourceView*   pPaletteSRV;
        ID3D11PixelShader*          pCompositePS;
        ID3D11Buffer*               pConstantBuffer;
    };

    // Cached fog palettes and coefficients.
    xbool           m_bFogValid[5];
    u8              m_FogSourcePalette[5][64 * 4];
    u8              m_FogPalette[5][256 * 4];
    vector4         m_FogColor[5];
    vector4         m_FogConst[5];
    f32             m_FogStart[5];
    fog_resources   m_FogResources;

    //--------------------------------------------------------------------------
    // Glow Resources
    //--------------------------------------------------------------------------

    struct glow_resources
    {
        glow_resources();

        // Resource lifetime
        void    Initialize                 ( void );
        void    Shutdown                   ( void );

        // Per-frame state
        void    ResetFrame                 ( void );
        xbool   ResizeIfNeeded             ( u32 SourceWidth, u32 SourceHeight );
        const rtarget*
                BindForComposite          ( void ) const;
        void    FinalizeComposite          ( void );

        // Shader state updates
        void    UpdateConstants            ( f32 Cutoff, f32 IntensityScale, f32 MotionBlend, f32 StepX, f32 StepY, f32 CompositeWeight = 1.0f );
        void    SetPendingResult           ( const rtarget* pResult );

        // Render targets
        rtarget             Downsample[3];
        rtarget             Blur[2];
        rtarget             Composite;
        rtarget             Accum;
        rtarget             History;

        // Runtime state
        const rtarget*      ActiveResult;
        u32                 BufferWidth;
        u32                 BufferHeight;
        xbool               bResourcesValid;
        xbool               bPendingComposite;

        // GPU resources
        ID3D11PixelShader*  pDownsamplePS;
        ID3D11PixelShader*  pBlurHPS;
        ID3D11PixelShader*  pBlurVPS;
        ID3D11PixelShader*  pCombinePS;
        ID3D11PixelShader*  pCompositePS;
        ID3D11PixelShader*  pAccumulatePS;
        ID3D11Buffer*       pConstantBuffer;
    };

    glow_resources          m_GlowResources;

    //--------------------------------------------------------------------------
    // Filter Resources
    //--------------------------------------------------------------------------

    struct filter_resources
    {
        filter_resources();

        // Resource lifetime
        void    Initialize                  ( void );
        void    Shutdown                    ( void );

        // Target management
        xbool   EnsureCopyTargets           ( const rtarget* pSourceTarget );
        xbool   EnsureMipTargets            ( const rtarget* pSourceTarget );
        xbool   CaptureHistory              ( const rtarget* pSourceTarget );
        xbool   BeginPostChain              ( const rtarget* pSourceTarget );
        xbool   BindPostTarget              ( void );
        xbool   PrimePostTarget             ( void );
        xbool   ResolvePostChain            ( void );

        // Target access
        const rtarget*
                GetPostSource               ( void ) const;
        const rtarget*
                GetMipTarget                ( s32 Index ) const;
        void    SwapPostTargets             ( void );
        void    ResetPostChain              ( void );
        xbool   IsPostChainActive           ( void ) const;

        // Shader state updates
        xbool   UpdateMipRampTexture        ( const xbitmap* pBitmap );
        void    UpdateConstants             ( f32 MotionIntensity, f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale, f32 CenterU, f32 CenterV );
        void    UpdateScreenWarpConstants   ( const post_screen_warp_params& ScreenWarp );

        // Render targets
        rtarget                     History;
        rtarget                     Post[2];
        rtarget                     Mip[MAX_POST_MIPS];

        // Runtime state
        s32                         ActiveSourceIndex;
        s32                         ActiveTargetIndex;
        xbool                       bPostChainActive;

        // GPU resources
        ID3D11PixelShader*          pMotionBlurPS;
        ID3D11PixelShader*          pMipCompositePS;
        ID3D11PixelShader*          pRadialBlurPS;
        ID3D11PixelShader*          pScreenWarpPS;
        ID3D11PixelShader*          pNoisePS;
        ID3D11Buffer*               pConstantBuffer;
        ID3D11Texture2D*            pMipRampTexture;
        ID3D11ShaderResourceView*   pMipRampSRV;

        // Source target description
        u32                         CopyWidth;
        u32                         CopyHeight;
        rtarget_format              CopyFormat;
        u32                         MipSourceWidth;
        u32                         MipSourceHeight;
        rtarget_format              MipSourceFormat;
        xbool                       bHistoryValid;
    };

    filter_resources        m_FilterResources;

    //--------------------------------------------------------------------------
    // Engine Integration
    //--------------------------------------------------------------------------

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
