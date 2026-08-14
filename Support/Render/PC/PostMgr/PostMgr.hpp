//==============================================================================
//
//  PostMgr.hpp
//
//==============================================================================

#ifndef POST_MANAGER_HPP
#define POST_MANAGER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../../Texture.hpp"
#include "../../Render.hpp"

#include "../GBufferMgr.hpp"

#include "e_Engine.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define ZBUFFER_BITS 19
#define MAX_POST_MIPS 3
#define MAX_POST_SCREEN_WARPS 8

//==============================================================================
//  POST-PROCESSING PARAMETER STRUCTURES
//==============================================================================

struct PostGlowParams
{
    f32 MotionBlurIntensity;
    s32 Cutoff;

    PostGlowParams() : MotionBlurIntensity( 0.0f ), Cutoff( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct PostMotionBlurParams
{
    f32 Intensity;

    PostMotionBlurParams() : Intensity( 0.0f )
    {
    }
};

//------------------------------------------------------------------------------

struct PostRadialBlurParams
{
    f32    Zoom;
    radian Angle;
    f32    AlphaSub;
    f32    AlphaScale;

    PostRadialBlurParams() : Zoom( 1.0f ), Angle( 0.0f ), AlphaSub( 0.0f ), AlphaScale( 1.0f )
    {
    }
};

//------------------------------------------------------------------------------

struct PostScreenWarpParams
{
    s32     Count;
    vector3 WorldPos[MAX_POST_SCREEN_WARPS];
    f32     Radius[MAX_POST_SCREEN_WARPS];
    f32     Amount[MAX_POST_SCREEN_WARPS];

    PostScreenWarpParams() : Count( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct PostFogFilterParams
{
    render::post_falloff_fn Fn[5];
    s32                     PaletteIndex;

    PostFogFilterParams() : PaletteIndex( -1 )
    {
        for ( s32 i = 0; i < 5; i++ )
        {
            Fn[i] = render::FALLOFF_CONSTANT;
        }
    }
};

//------------------------------------------------------------------------------

struct PostMipFilterParams
{
    render::post_falloff_fn Fn[4];
    xcolor                  Color[4];
    f32                     Param1[4];
    f32                     Param2[4];
    s32                     Count[4];
    f32                     Offset[4];
    s32                     PaletteIndex;

    PostMipFilterParams() : PaletteIndex( -1 )
    {
        for ( s32 i = 0; i < 4; i++ )
        {
            Fn[i]     = render::FALLOFF_CONSTANT;
            Color[i]  = xcolor( 255, 255, 255, 255 );
            Param1[i] = 0.0f;
            Param2[i] = 0.0f;
            Count[i]  = 0;
            Offset[i] = 0.0f;
        }
    }
};

//------------------------------------------------------------------------------

struct PostSimpleParams
{
    xcolor NoiseColor;
    xcolor FadeColor;

    PostSimpleParams() : NoiseColor( 255, 255, 255, 255 ), FadeColor( 0, 0, 0, 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct PostEffectFlags
{
    // Debug override flag
    xbool Override : 1; // Set this to play around with values in the debugger

    // Effect enable flags
    xbool DoMotionBlur : 1;
    xbool DoSelfIllumGlow : 1;
    xbool DoRadialBlur : 1;
    xbool DoZFogFn : 1;
    xbool DoZFogCustom : 1;
    xbool DoMipFn : 1;
    xbool DoMipCustom : 1;
    xbool DoNoise : 1;
    xbool DoScreenFade : 1;

    PostEffectFlags()
    {
        x_memset( this, 0, sizeof( PostEffectFlags ) );
    }
};

//==============================================================================
//  POST-PROCESSING MANAGER CLASS
//==============================================================================

class PostMgr
{
public:
    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------
    
    void Init ( void );
    void Kill ( void );
    void Update ( f32 deltaTime );
    
    //--------------------------------------------------------------------------
    // Frame Pipeline
    //--------------------------------------------------------------------------
    
    void BeginPostEffects ( void );
    void EndPostEffects   ( void );
    
    //--------------------------------------------------------------------------
    // Post-Effect Requests
    //--------------------------------------------------------------------------
    
    void ApplySelfIllumGlows ( f32 motionBlurIntensity, s32 glowCutoff );
    void MotionBlur          ( f32 intensity );
    void AddScreenWarp       ( vector3 const& worldPos, f32 radius, f32 warpAmount );
    void MipFilter           ( s32 nFilters, f32 offset, render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2,
                               s32 paletteIndex );
    void MipFilter           ( s32 nFilters, f32 offset, render::post_falloff_fn fn, texture::handle const& texture,
                               s32 paletteIndex );
    void NoiseFilter         ( xcolor color );
    void ScreenFade          ( xcolor color );
    void MultScreen          ( xcolor multColor, render::post_screen_blend finalBlend );
    void RadialBlur          ( f32 zoom, radian angle, f32 alphaSub, f32 alphaScale );
    
    //--------------------------------------------------------------------------
    // Fog Requests And Queries
    //--------------------------------------------------------------------------
    
    void   SetCustomFogPalette ( texture::handle const& texture, xbool immediateSwitch, s32 paletteIndex );
    void   ZFogFilter          ( render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2 );
    void   ZFogFilter          ( render::post_falloff_fn fn, s32 paletteIndex );
    xcolor GetFogValue         ( vector3 const& worldPos, s32 paletteIndex );
    void   GetGeometryFogConstants( vector4& color, vector4& coeff, vector4& params ) const;
    
    //--------------------------------------------------------------------------
    // Engine Frame Hooks
    //--------------------------------------------------------------------------
    
    static void PostStage_BeginFrameThunk    ( void );
    static void PostStage_BeforePresentThunk ( void );
    static void PostStage_AfterUIThunk       ( void );
    
protected:
    //--------------------------------------------------------------------------
    // Internal Effect Execution
    //--------------------------------------------------------------------------
    
    void ExecuteMotionBlur    ( void );
    void ExecuteSelfIllumGlow ( void );
    void ExecuteRadialBlur    ( void );
    void ExecuteScreenWarps   ( void );
    void ExecuteZFogFilter    ( void );
    void ExecuteMipFilter     ( void );
    void ExecuteNoiseFilter   ( void );
    void ExecuteScreenFadeLate( void );
    
    //--------------------------------------------------------------------------
    // Per-Frame Maintenance
    //--------------------------------------------------------------------------
    
    void UpdateGlowStageBegin             ( void );
    void CompositePendingGlow             ( void );
    void UpdateFilterHistoryBeforePresent ( void );
    void InvalidateTemporalHistory        ( void );
    
    //--------------------------------------------------------------------------
    // Internal Helpers
    //--------------------------------------------------------------------------
    
    void  BuildFogPalette  ( render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2 );
    void  BuildMipPalette  ( render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2, s32 paletteIndex );
    xbool PrewarmPipelines ( void );
    void  CopyBackBuffer   ( void );
    void  BuildScreenMips  ( s32 nMips );
    
protected:
    //--------------------------------------------------------------------------
    // Manager State
    //--------------------------------------------------------------------------
    
    xbool m_isInitialized;
    xbool m_isInPost;
    f32   m_frameDeltaTime;
    
    // Cached view state for the current post pass.
    s32 m_postViewL, m_postViewT, m_postViewR, m_postViewB;
    f32 m_postNearZ, m_postFarZ;
    
    // Pending effect requests gathered between BeginPostEffects/EndPostEffects.
    PostEffectFlags      m_Flags;
    PostMotionBlurParams m_motionBlur;
    PostGlowParams       m_glow;
    PostRadialBlurParams m_radialBlur;
    PostScreenWarpParams m_ScreenWarp;
    PostFogFilterParams  m_fogFilter;
    PostMipFilterParams  m_mipFilter;
    
    // Optional custom ramp source requested by the current mip filter pass.
    xbitmap const*   m_pMipTexture;
    PostSimpleParams m_simple;
    
    //--------------------------------------------------------------------------
    // Fog Resources
    //--------------------------------------------------------------------------
    
    struct FogResources
    {
        FogResources();
    
        // Resource lifetime
        void Initialize ( void );
        void Shutdown   ( void );
    
        // Shader state updates
        void  UpdateConstants      ( vector4 const& fogColor, vector4 const& fogCoeff, f32 fogStart, f32 nearZ, f32 farZ,
                                     xbool bUsePolynomial );
        xbool UpdatePaletteTexture ( u8 const* pPalette );
        xbool BindForComposite     ( shader const& pixelShader, xbool bBindPalette ) const;
    
        // GPU resources
        vram_texture   PaletteTexture;
        shader         CompositePS;
        shader         PolynomialPS;
        rstate_sampler PaletteSampler;
        vector4        ConstantFogColor;
        vector4        ConstantFogCoeff;
        vector4        ConstantFogParams;
    };
    
    // Cached fog palettes and coefficients.
    xbool        m_isFogValid[5];
    u8           m_fogSourcePalette[5][64 * 4];
    u8           m_fogPalette[5][256 * 4];
    vector4      m_fogColor[5];
    vector4      m_fogConst[5];
    f32          m_fogStart[5];
    FogResources m_fogResources;
    
    //--------------------------------------------------------------------------
    // Glow Resources
    //--------------------------------------------------------------------------
    
    struct GlowResources
    {
        GlowResources();
    
        // Resource lifetime
        void Initialize ( void );
        void Shutdown   ( void );
    
        // Per-frame state
        void           ResetFrame        ( void );
        void           InvalidateHistory ( void );
        xbool          ResizeIfNeeded    ( u32 sourceWidth, u32 sourceHeight );
        rtarget const* BindForComposite  ( void ) const;
        void           FinalizeComposite ( void );
    
        // Shader state updates
        void SetPendingResult ( rtarget const* pResult, xbool storeHistory );
    
        // Render targets
        rtarget Downsample[3];
        rtarget Blur[2];
        rtarget Composite;
        rtarget History;
    
        // Runtime state
        rtarget const* ActiveResult;
        u32            BufferWidth;
        u32            BufferHeight;
        xbool          bResourcesValid;
        xbool          bPendingComposite;
        xbool          bHistoryValid;
        xbool          bStoreHistory;
        f32            PulseScale;
        f32            PulseDirection;
    
        // GPU resources
        shader         DownsamplePS;
        shader         BlurHPS;
        shader         BlurVPS;
        shader         CombinePS;
        shader         CompositePS;
        rstate_sampler AuxSampler;
    };
    
    GlowResources m_glowResources;
    
    //--------------------------------------------------------------------------
    // Filter Resources
    //--------------------------------------------------------------------------
    
    struct FilterResources
    {
        FilterResources();
    
        // Resource lifetime
        void Initialize ( void );
        void Shutdown   ( void );
    
        // Target management
        xbool EnsureCopyTargets ( rtarget const* pSourceTarget );
        xbool EnsureMipTargets  ( rtarget const* pSourceTarget );
        xbool CaptureHistory    ( rtarget const* pSourceTarget );
        void  InvalidateHistory ( void );
        xbool BeginPostChain    ( rtarget const* pSourceTarget );
        xbool BindPostTarget    ( void );
        xbool PrimePostTarget   ( void );
        xbool ResolvePostChain  ( void );
    
        // Target access
        rtarget const* GetPostSource     ( void ) const;
        rtarget const* GetMipTarget      ( s32 index ) const;
        void           SwapPostTargets   ( void );
        void           ResetPostChain    ( void );
        xbool          IsPostChainActive ( void ) const;
    
        // Shader state updates
        xbool UpdateMipRampTexture( xbitmap const* pBitmap );
    
        // Render targets
        rtarget History;
        rtarget Post[2];
        rtarget Mip[MAX_POST_MIPS];
        rtarget Pain;
        rtarget PainScratch;
    
        // Runtime state
        s32   ActiveSourceIndex;
        s32   ActiveTargetIndex;
        xbool bPostChainActive;
    
        // GPU resources
        shader         MotionBlurPS;
        shader         MipDownsamplePS;
        shader         PainBlurPS;
        shader         MipCompositePS;
        shader         MipCompositeCustomPS;
        shader         RadialBlurPS;
        shader         ScreenWarpPS;
        shader         NoisePS;
        shader         ScreenFadePS;
        vram_texture   MipRampTexture;
        rstate_sampler LinearSampler;
        rstate_sampler PointSampler;
    
        // Source target description
        u32            CopyWidth;
        u32            CopyHeight;
        rtarget_format CopyFormat;
        u32            MipSourceWidth;
        u32            MipSourceHeight;
        rtarget_format MipSourceFormat;
        xbool          bHistoryValid;
    };
    
    FilterResources m_FilterResources;
    
    //--------------------------------------------------------------------------
    // Engine Integration
    //--------------------------------------------------------------------------
    
    xbool m_isPostStageRegistered;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern PostMgr g_PostMgr;

//==============================================================================
//  END
//==============================================================================
#endif
