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
    PC_PROJ_LIGHT_TEX_SLOT   = 4,
    PC_PROJ_SHADOW_TEX_SLOT  = PC_PROJ_LIGHT_TEX_SLOT + proj_texture_mgr::MAX_PROJ_LIGHTS,
    PC_SHADOW_ATLAS_TEX_SLOT = 20,
    PC_SHADOW_ATLAS_SAMP_SLOT= 8,
    PC_SHADOW_BUFFER_SLOT    = 5,
};

//==============================================================================
//  CONSTANT BUFFER LAYOUTS
//==============================================================================

struct cb_shadow_cast
{
    matrix4 ShadowViewProjection;
    matrix4 World;
};

//------------------------------------------------------------------------------

struct cb_shadow_maps
{
    matrix4 FaceShadowMatrix[MAX_SHADOW_SOURCES];
    vector4 FaceShadowLightPosRadius[MAX_SHADOW_SOURCES];
    vector4 FaceShadowLightDirFalloff[MAX_SHADOW_SOURCES];
    vector4 FaceShadowLightData[MAX_SHADOW_SOURCES];   // x = cone/coverage cosine, y = receive near z, z = far z, w = 0 spot / 1 point face
    vector4 PointShadowLightPosRadius[MAX_SHADOW_LIGHTS];
    vector4 PointShadowLightData[MAX_SHADOW_LIGHTS];   // x = falloff, y = receive near z, z = far z
    vector4 PointShadowLightParams[MAX_SHADOW_LIGHTS]; // x = first face source, y = face count
    u32     FaceShadowCount;
    u32     PointShadowLightCount;
    f32     Padding[2];
    vector4 ShadowParams;                          // z = min variance, w = light bleed reduction
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
    void        RenderRigidCaster        ( xhandle         hDList,
                                           const matrix4*  pL2W,
                                           s32             SourceIndex );
    void        RenderSkinCaster         ( xhandle         hDList,
                                           const matrix4*  pBones,
                                           s32             SourceIndex );

    //--------------------------------------------------------------------------
    // Runtime Queries
    //--------------------------------------------------------------------------

    ID3D11ShaderResourceView* GetShadowAtlasSRV ( void ) const;
    f32         GetShadowFilterRadius    ( void ) const;
    f32         GetShadowMinVariance     ( void ) const;
    f32         GetShadowLightBleedReduction( void ) const;
    f32         GetAtlasTexelSize        ( void ) const;

private:

    //--------------------------------------------------------------------------
    // Internal Helpers
    //--------------------------------------------------------------------------

    void        EnsureAtlas              ( void );
    xbool       SetShadowCastConstants   ( const matrix4&  ShadowViewProjection,
                                           const matrix4*  pWorld = NULL );
    void        ApplySource              ( s32 SourceIndex,
                                           s32             CasterShader );
    void        BlurAtlas                ( void );
    void        UnbindShadowSRVs         ( void );

private:

    //--------------------------------------------------------------------------
    // Runtime State
    //--------------------------------------------------------------------------

    xbool                   m_bInitialized;
    xbool                   m_bTargetsPushed;
    xbool                   m_bViewportSaved;
    xbool                   m_bRasterizerSaved;
    u32                     m_SavedViewportCount;
    D3D11_VIEWPORT          m_SavedViewport;
    ID3D11RasterizerState*  m_pSavedRasterizerState;
    s32                     m_CurrentSource;
    s32                     m_CurrentCasterShader;
    s32                     m_ShadowAtlasSize;

    //--------------------------------------------------------------------------
    // GPU Resources
    //--------------------------------------------------------------------------

    rtarget                 m_ShadowAtlas;
    rtarget                 m_ShadowBlurAtlas;
    rtarget                 m_ShadowDepthAtlas;
    ID3D11VertexShader*     m_pRigidVertexShader;
    ID3D11VertexShader*     m_pSkinVertexShader;
    ID3D11PixelShader*      m_pMomentPixelShader;
    ID3D11PixelShader*      m_pBlurHPixelShader;
    ID3D11PixelShader*      m_pBlurVPixelShader;
    ID3D11InputLayout*      m_pRigidInputLayout;
    ID3D11InputLayout*      m_pSkinInputLayout;
    ID3D11Buffer*           m_pShadowCastBuffer;
    ID3D11Buffer*           m_pShadowBlurBuffer;
    ID3D11RasterizerState*  m_pShadowCasterRasterizer;

    //--------------------------------------------------------------------------
    // Shadow Tuning
    //--------------------------------------------------------------------------

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
