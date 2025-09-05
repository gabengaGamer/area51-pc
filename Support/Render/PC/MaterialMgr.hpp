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

#include "../Texture.hpp"
#include "../ProjTextureMgr.hpp"
#include "../Material_Prefs.hpp"
#include "../Material.hpp"
#include "Entropy/D3DEngine/d3deng_shader.hpp"
#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define MAX_SKIN_BONES 96
#define MAX_SKIN_LIGHTS 4

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
    MATERIAL_FLAG_HAS_DETAIL        = (1<<11), 
    MATERIAL_FLAG_HAS_ENVIRONMENT   = (1<<12),
    MATERIAL_FLAG_PROJ_LIGHT        = (1<<13),
    MATERIAL_FLAG_PROJ_SHADOW       = (1<<14), 
};

//==============================================================================
//  CONSTANT BUFFER STRUCTURES
//==============================================================================

struct d3d_rigid_lighting
{
    vector3 Dir;
    vector4 DirCol;
    vector4 AmbCol;
};

struct rigid_vert_matrices
{
    matrix4 World;
    matrix4 View;
    matrix4 Projection;
    
    u32     MaterialFlags;
    f32     AlphaRef;
    vector3 CameraPosition;
    f32     Padding1;
    f32     Padding2;
};

struct cb_proj_textures
{
    matrix4 ProjLightMatrix[proj_texture_mgr::MAX_PROJ_LIGHTS];
    matrix4 ProjShadowMatrix[proj_texture_mgr::MAX_PROJ_SHADOWS];
    u32     ProjLightCount;
    u32     ProjShadowCount;
    f32     EdgeSize;
    f32     Padding[3];
};

struct d3d_skin_lighting
{
    s32     LightCount;
    vector3 Dir      [MAX_SKIN_LIGHTS];
    vector4 DirCol   [MAX_SKIN_LIGHTS];
    vector4 AmbCol;
};

struct cb_skin_vs_consts
{
    matrix4 W2C;                          // World to clip matrix
    f32     Zero;                         // 0.0f
    f32     One;                          // 1.0f
    f32     MinusOne;                     // -1.0f
    f32     Fog;                          // Fog factor
    vector4 LightDir[MAX_SKIN_LIGHTS];    // Directional light directions
    vector4 LightCol[MAX_SKIN_LIGHTS];    // Directional light colors
    vector4 LightAmbCol;                  // Ambient light color
    u32     LightCount;                   // Number of active lights
    f32     Padding[3];                   // Padding to 16-byte alignment
};

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
    void        SetRigidMaterial    ( const matrix4* pL2W, const bbox* pBBox, const d3d_rigid_lighting* pLighting, const material* pMaterial );

    // Skin material management  
    void        SetSkinMaterial     ( const matrix4* pL2W, const bbox* pBBox, const d3d_skin_lighting* pLighting );

    void        ResetProjTextures   ( void );

    // General material functions
    void        SetBitmap           ( const xbitmap* pBitmap, texture_slot slot );
    void        SetBlendMode        ( s32 BlendMode );
    void        SetDepthTestEnabled ( xbool ZTestEnabled );
    void        InvalidateCache     ( void );

    // Resource access for vertex managers
    ID3D11Buffer* GetSkinBoneBuffer ( void );

protected:

    // Shader initialization
    xbool       InitRigidShaders    ( void );
    xbool       InitSkinShaders     ( void );
    void        KillShaders         ( void );

    // Internal helpers
    xbool       UpdateRigidConstants( const matrix4* pL2W, const material* pMaterial );
    xbool       UpdateSkinConstants ( const d3d_skin_lighting* pLighting );
	xbool       UpdateProjTextures  ( const matrix4& L2W, const bbox& B, u32 Slot );

protected:

    // Initialization tracking
    xbool       m_bInitialized;

    // Rigid geometry resources
    ID3D11VertexShader*     m_pRigidVertexShader;
    ID3D11PixelShader*      m_pRigidPixelShader;
    ID3D11InputLayout*      m_pRigidInputLayout;
    ID3D11Buffer*           m_pRigidConstantBuffer;
	
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
    rigid_vert_matrices     m_CachedRigidMatrices;
    cb_skin_vs_consts       m_CachedSkinConsts;
	u32                     m_LastProjLightCount;
    u32                     m_LastProjShadowCount;
    xbool                   m_bRigidMatricesDirty;
    xbool                   m_bSkinConstsDirty;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern material_mgr g_MaterialMgr;

//==============================================================================
//  END
//==============================================================================
#endif