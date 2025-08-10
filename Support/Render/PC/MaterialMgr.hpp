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
#include "../Material_Prefs.hpp"
#include "../Material.hpp"
#include "Entropy/D3DEngine/d3deng_shader.hpp"
#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define MAX_SKIN_BONES 96

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
};

enum blend_mode
{
    BLEND_MODE_NORMAL       = 0,
    BLEND_MODE_ADDITIVE     = 1,
    BLEND_MODE_SUBTRACTIVE  = 2,
    BLEND_MODE_ALPHA_BLEND  = 3,
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

struct d3d_skin_lighting
{
    vector3 Dir;
    vector4 DirCol;
    vector4 AmbCol;
};

struct cb_skin_vs_consts
{
    matrix4 W2C;               // World to clip matrix
    f32     Zero;              // 0.0f
    f32     One;               // 1.0f
    f32     MinusOne;          // -1.0f
    f32     Fog;               // Fog factor
    vector4 LightDirCol;       // Directional light color
    vector4 LightAmbCol;       // Ambient light color
    f32     Padding[2];        // Padding to 16-byte alignment
};

struct cb_skin_bone
{
    matrix4 L2W;               // Local to world matrix
    vector3 LightDir;          // Light direction in bone space
    f32     Padding;           // Padding to 16-byte alignment
};

//==============================================================================
//  MATERIAL TYPES
//==============================================================================

enum material_type_new //"new" is temp.
{
    MATERIAL_TYPE_RIGID = 0,
    MATERIAL_TYPE_SKIN,
    MATERIAL_TYPE_COUNT
};

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
    void        SetRigidMaterial    ( const matrix4* pL2W, const d3d_rigid_lighting* pLighting, const material* pMaterial );
    void        ActivateRigidShader ( void );

    // Skin material management  
    void        SetSkinMaterial     ( const d3d_skin_lighting* pLighting );
    void        ActivateSkinShader  ( void );

    // General material functions
    void        SetBitmap           ( const xbitmap* pBitmap, texture_slot slot );
    void        SetBlendMode        ( blend_mode BlendMode );
    void        InvalidateCache     ( void );

    // Resource access for vertex managers
    ID3D11Buffer* GetSkinBoneBuffer ( void );

protected:

    // Shader initialization
    xbool       InitRigidShaders    ( void );
    xbool       InitSkinShaders     ( void );
    void        KillShaders         ( void );

    // State management
    void        SetRenderStates     ( void );
    void        ClearRenderStates   ( void );

    // Internal helpers
    xbool       UpdateRigidConstants( const matrix4* pL2W, const material* pMaterial );
    xbool       UpdateSkinConstants ( const d3d_skin_lighting* pLighting );

protected:

    // Initialization tracking
    xbool       m_bInitialized;

    // Rigid geometry resources
    ID3D11VertexShader*     m_pRigidVertexShader;
    ID3D11PixelShader*      m_pRigidPixelShader;
    ID3D11InputLayout*      m_pRigidInputLayout;
    ID3D11Buffer*           m_pRigidConstantBuffer;

    // Skin geometry resources
    ID3D11VertexShader*     m_pSkinVertexShader;
    ID3D11PixelShader*      m_pSkinPixelShader;
    ID3D11InputLayout*      m_pSkinInputLayout;
    ID3D11Buffer*           m_pSkinVSConstBuffer;
    ID3D11Buffer*           m_pSkinBoneBuffer;

    // Note: Sampler states handled by d3deng_state system

    // Current state tracking for caching
    material_type_new       m_CurrentMaterialType;
    const xbitmap*          m_pCurrentTexture;
    const xbitmap*          m_pCurrentDetailTexture;
    blend_mode              m_CurrentBlendMode;
    
    // Cached constant buffer data to avoid redundant updates
    rigid_vert_matrices     m_CachedRigidMatrices;
    cb_skin_vs_consts       m_CachedSkinConsts;
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