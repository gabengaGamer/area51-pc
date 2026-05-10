//==============================================================================
//
//  ShadowMgr.hpp
//
//  Shadow-map manager for the PC platform.
//
//==============================================================================

#ifndef PC_SHADOW_MGR_HPP
#define PC_SHADOW_MGR_HPP

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

#include "x_math.hpp"

#include "../ProjTextureMgr.hpp"
#include "../ShadowMapMgr.hpp"

#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_shader.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    PC_PROJ_LIGHT_TEX_SLOT     = 4,
    PC_PROJ_SHADOW_TEX_SLOT    = PC_PROJ_LIGHT_TEX_SLOT + proj_texture_mgr::MAX_PROJ_LIGHTS,
    PC_POINT_SHADOW_TEX_SLOT   = PC_PROJ_SHADOW_TEX_SLOT + proj_texture_mgr::MAX_PROJ_SHADOWS,
    PC_SPOT_SHADOW_TEX_SLOT    = PC_POINT_SHADOW_TEX_SLOT + 1,
    PC_POINT_SHADOW_SAMP_SLOT  = 7,
    PC_SPOT_SHADOW_SAMP_SLOT   = 8,
    PC_SHADOW_BUFFER_SLOT      = 5,
};

//==============================================================================
//  CONSTANT BUFFER LAYOUTS
//==============================================================================

struct cb_shadow_cast
{
    matrix4 ShadowViewProjection;
};

//------------------------------------------------------------------------------

struct cb_shadow_maps
{
    matrix4 FaceShadowMatrix[MAX_SHADOW_SOURCES];
    vector4 FaceShadowLightPosRadius[MAX_SHADOW_SOURCES];
    vector4 FaceShadowLightDirFalloff[MAX_SHADOW_SOURCES];
    vector4 FaceShadowLightData[MAX_SHADOW_SOURCES];
    vector4 PointShadowLightPosRadius[MAX_SHADOW_LIGHTS];
    vector4 PointShadowLightData[MAX_SHADOW_LIGHTS];
    u32     FaceShadowCount;
    u32     PointShadowLightCount;
    f32     Padding[2];
    vector4 ShadowParams;
};

//==============================================================================
//  SHADOW MANAGER
//==============================================================================

class shadow_mgr
{
public:

    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------

                shadow_mgr               ( void );
               ~shadow_mgr               ( void );

    void        Init                     ( void );
    void        Kill                     ( void );

    //--------------------------------------------------------------------------
    // Shadow Caster Pipeline
    //--------------------------------------------------------------------------

    void        BeginShadowShaders       ( void );
    void        EndShadowShaders         ( void );
    void        BeginCastPass            ( void );
    void        EndCastPass              ( void );
    void        RenderSkinCaster         ( xhandle         hDList,
                                           const matrix4*  pBones,
                                           s32             SourceIndex );

    //--------------------------------------------------------------------------
    // Runtime Queries
    //--------------------------------------------------------------------------

    ID3D11ShaderResourceView* GetPointShadowSRV ( void ) const;
    ID3D11ShaderResourceView* GetSpotShadowSRV  ( void ) const;
    f32         GetShadowBias            ( void ) const;
    f32         GetShadowStrength        ( void ) const;
    f32         GetShadowFilterRadius    ( void ) const;
    f32         GetShadowMinVariance     ( void ) const;
    f32         GetShadowLightBleedReduction( void ) const;
    f32         GetAtlasTexelSize        ( void ) const;

private:

    //--------------------------------------------------------------------------
    // Internal Helpers
    //--------------------------------------------------------------------------

    void        EnsureAtlas              ( void );
    void        EnsurePointShadows       ( void );
    void        ApplySource              ( s32 SourceIndex );
    void        BlurAtlas                ( void );
    void        UnbindShadowSRVs         ( void );

private:

    //--------------------------------------------------------------------------
    // Runtime State
    //--------------------------------------------------------------------------

    xbool                   m_bInitialized;
    xbool                   m_bTargetsPushed;
    xbool                   m_bViewportSaved;
    u32                     m_SavedViewportCount;
    D3D11_VIEWPORT          m_SavedViewport;
    s32                     m_CurrentSource;
    xbool                   m_SourceCleared[MAX_SHADOW_SOURCES];

    //--------------------------------------------------------------------------
    // GPU Resources
    //--------------------------------------------------------------------------

    rtarget                 m_ShadowAtlas;
    rtarget                 m_ShadowBlurAtlas;
    rtarget                 m_ShadowDepthAtlas;
    ID3D11Texture2D*        m_pPointShadowTexture;
    ID3D11ShaderResourceView* m_pPointShadowSRV;
    ID3D11DepthStencilView* m_pPointShadowDSV[MAX_SHADOW_LIGHTS * POINT_SHADOW_FACE_COUNT];
    ID3D11VertexShader*     m_pSkinVertexShader;
    ID3D11PixelShader*      m_pMomentPixelShader;
    ID3D11PixelShader*      m_pBlurHPixelShader;
    ID3D11PixelShader*      m_pBlurVPixelShader;
    ID3D11InputLayout*      m_pSkinInputLayout;
    ID3D11Buffer*           m_pShadowCastBuffer;
    ID3D11Buffer*           m_pShadowBlurBuffer;

    //--------------------------------------------------------------------------
    // Shadow Tuning
    //--------------------------------------------------------------------------

    f32                     m_ShadowBias;
    f32                     m_ShadowStrength;
    f32                     m_ShadowFilterRadius;
    f32                     m_ShadowMinVariance;
    f32                     m_ShadowLightBleedReduction;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern shadow_mgr g_ShadowMgr;

//==============================================================================
//  END
//==============================================================================

#endif
