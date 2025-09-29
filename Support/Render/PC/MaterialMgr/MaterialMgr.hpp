//==============================================================================
//
//  MaterialMgr.hpp
//
//  Material Manager for PC platform
//
//==============================================================================

#ifndef MATERIAL_MANAGER_HPP
#define MATERIAL_MANAGER_HPP

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
#include "../../ProjTextureMgr.hpp"
#include "../../Material_Prefs.hpp"
#include "../../Material.hpp"
#include "../../Render.hpp"

#include "e_engine.hpp"

#include "Entropy/D3DEngine/d3deng_shader.hpp"
#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define MAX_SKIN_BONES 96
#define MAX_GEOM_LIGHTS 4

//==============================================================================
//  MATERIAL FLAGS
//==============================================================================

enum material_flags
{
    MATERIAL_FLAG_ALPHA_TEST        = (1<<0),
    MATERIAL_FLAG_ADDITIVE          = (1<<1),
    MATERIAL_FLAG_SUBTRACTIVE       = (1<<2),
    MATERIAL_FLAG_VERTEX_COLOR      = (1<<3),
    MATERIAL_FLAG_TWO_SIDED         = (1<<4),
    MATERIAL_FLAG_ENVIRONMENT       = (1<<5),
    MATERIAL_FLAG_DISTORTION        = (1<<6),
    MATERIAL_FLAG_PERPIXEL_ILLUM    = (1<<7),
    MATERIAL_FLAG_PERPOLY_ILLUM     = (1<<8),
    MATERIAL_FLAG_PERPIXEL_ENV      = (1<<9),
    MATERIAL_FLAG_PERPOLY_ENV       = (1<<10),
    MATERIAL_FLAG_DETAIL            = (1<<11),
    MATERIAL_FLAG_PROJ_LIGHT        = (1<<12),
    MATERIAL_FLAG_PROJ_SHADOW       = (1<<13),
};

//==============================================================================
//  CONSTANT BUFFER STRUCTURES
//==============================================================================

struct d3d_lighting
{
    s32     LightCount;
    vector4 LightVec[MAX_GEOM_LIGHTS];
    vector4 LightCol[MAX_GEOM_LIGHTS];
    vector4 AmbCol;
};

//------------------------------------------------------------------------------

struct cb_rigid_matrices
{
    matrix4 World;
    matrix4 View;
    matrix4 Projection;

    u32     MaterialFlags;
    f32     AlphaRef;
    vector3 CameraPosition;
    f32     DepthParams[2];
};

//------------------------------------------------------------------------------

struct cb_skin_matrices
{
    matrix4 View;                         // World to view matrix
    matrix4 Projection;                   // View to clip matrix

    u32     MaterialFlags;                // Material feature flags
    f32     AlphaRef;                     // Alpha test reference
    f32     DepthParams[2];               // Near/Far depth range
};

//------------------------------------------------------------------------------

struct cb_proj_textures
{
    matrix4 ProjLightMatrix[proj_texture_mgr::MAX_PROJ_LIGHTS];
    matrix4 ProjShadowMatrix[proj_texture_mgr::MAX_PROJ_SHADOWS];
    u32     ProjLightCount;
    u32     ProjShadowCount;
    f32     EdgeSize;
    f32     Padding[3];
};

//------------------------------------------------------------------------------

struct cb_lighting
{
    vector4 LightVec[MAX_GEOM_LIGHTS];
    vector4 LightCol[MAX_GEOM_LIGHTS];
    vector4 LightAmbCol;
    u32     LightCount;
    f32     Padding[3];
};

//------------------------------------------------------------------------------

struct cb_skin_bone
{
    matrix4 L2W;                               // Local to world matrix
};

//==============================================================================
//  MATERIAL TYPES
//==============================================================================

enum texture_slot
{
    TEXTURE_SLOT_DIFFUSE     = 0,
    TEXTURE_SLOT_DETAIL      = 1,
    TEXTURE_SLOT_ENVIRONMENT = 2,
};

//==============================================================================
//  MATERIAL MANAGER CLASS
//==============================================================================

class material_mgr
{
public:

    void        Init                ( void );
    void        Kill                ( void );

    // Rigid material management
    void        SetRigidMaterial    ( const matrix4*      pL2W,
                                      const bbox*         pBBox,
                                      const d3d_lighting* pLighting,
                                      const material*     pMaterial,
                                      u32                 RenderFlags );

    // Skin material management
    void        SetSkinMaterial     ( const matrix4*      pL2W,
                                      const bbox*         pBBox,
                                      const d3d_lighting* pLighting,
                                      const material*     pMaterial,
                                      u32                 RenderFlags );

    void        ResetProjTextures   ( void );

    // General material functions
    void        SetBitmap           ( const xbitmap* pBitmap,
                                      texture_slot   slot );
    void        SetBlendMode        ( s32 BlendMode );
    void        SetDepthTestEnabled ( xbool ZTestEnabled );
    void        InvalidateCache     ( void );

    // Resource access for vertex managers
    ID3D11Buffer* GetSkinBoneBuffer ( void );

protected:

    struct material_constants
    {
        u32 Flags;
        f32 AlphaRef;
    };

    // Shader initialization
    xbool       InitRigidShaders    ( void );
    void        KillRigidShaders    ( void );
    xbool       InitSkinShaders     ( void );
    void        KillSkinShaders     ( void );
    xbool       InitProjTextures    ( void );
    void        KillProjTextures    ( void );

    // Internal helpers
    xbool               UpdateRigidConstants   ( const matrix4*      pL2W,
                                                 const material*     pMaterial,
                                                 u32                 RenderFlags,
                                                 const d3d_lighting* pLighting );
    xbool               UpdateSkinConstants    ( const d3d_lighting* pLighting,
                                                 const material*     pMaterial,
                                                 u32                 RenderFlags );
    xbool               UpdateProjTextures     ( const matrix4& L2W,
                                                 const bbox&    B,
                                                 u32            Slot,
                                                 u32            RenderFlags );
    material_constants  BuildMaterialFlags     ( const material* pMaterial,
                                                 u32             RenderFlags,
                                                 xbool           SupportsDetailMap,
                                                 xbool           IncludeVertexColor ) const;
    cb_lighting         BuildLightingConstants ( const d3d_lighting* pLighting,
                                                 const vector4&      AmbientBias ) const;

protected:

    // Initialization tracking
    xbool       m_bInitialized;

    // Rigid geometry resources
    ID3D11VertexShader*     m_pRigidVertexShader;
    ID3D11PixelShader*      m_pRigidPixelShader;
    ID3D11InputLayout*      m_pRigidInputLayout;
    ID3D11Buffer*           m_pRigidConstantBuffer;
    ID3D11Buffer*           m_pSkinLightBuffer;
    ID3D11Buffer*           m_pRigidLightBuffer;

    // Proj textures resources
    ID3D11Buffer*           m_pProjTextureBuffer;
    ID3D11SamplerState*     m_pProjSampler;

    // Skin geometry resources
    ID3D11VertexShader*     m_pSkinVertexShader;
    ID3D11PixelShader*      m_pSkinPixelShader;
    ID3D11InputLayout*      m_pSkinInputLayout;
    ID3D11Buffer*           m_pSkinVSConstBuffer;
    ID3D11Buffer*           m_pSkinBoneBuffer;

    // Current state tracking for caching
    const xbitmap*          m_pCurrentTexture;
    const xbitmap*          m_pCurrentDetailTexture;
    xbool                   m_bZTestEnabled;

    // Cached constant buffer data to avoid redundant updates
    cb_rigid_matrices       m_CachedRigidMatrices;
    cb_lighting             m_CachedRigidLighting;
    cb_skin_matrices        m_CachedSkinMatrices;
    cb_lighting             m_CachedSkinLighting;
    u32                     m_LastProjLightCount;
    u32                     m_LastProjShadowCount;
    xbool                   m_bRigidMatricesDirty;
    xbool                   m_bRigidLightingDirty;
    xbool                   m_bSkinMatricesDirty;
    xbool                   m_bSkinLightingDirty;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern material_mgr g_MaterialMgr;

//==============================================================================
//  END
//==============================================================================
#endif