//==============================================================================
//
//  GeomMgr.hpp
//
//  Geometry manager for the PC platform.
//
//==============================================================================

#ifndef GEOM_MANAGER_HPP
#define GEOM_MANAGER_HPP

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

#include "../../Material.hpp"
#include "../../Material_Prefs.hpp"
#include "../../ProjTextureMgr.hpp"
#include "../../Render.hpp"
#include "../../Texture.hpp"

#include "../ShadowMgr.hpp"

#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_shader.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"

#include "e_engine.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define MAX_SKIN_BONES     96
#define MAX_GEOM_LIGHTS    4

//==============================================================================
//  SHADER FLAGS
//==============================================================================

enum material_flags
{
    MATERIAL_FLAG_ALPHA_TEST             = (1u << 0),
    MATERIAL_FLAG_ADDITIVE               = (1u << 1),
    MATERIAL_FLAG_SUBTRACTIVE            = (1u << 2),
    MATERIAL_FLAG_VERTEX_COLOR           = (1u << 3),
    MATERIAL_FLAG_TWO_SIDED              = (1u << 4),
    MATERIAL_FLAG_ENVIRONMENT            = (1u << 5),
    MATERIAL_FLAG_DISTORTION             = (1u << 6),
    MATERIAL_FLAG_DISTORTION_PERPOLY_ENV = (1u << 7),
    MATERIAL_FLAG_DIFF_PERPIXEL_ILLUM    = (1u << 8),
    MATERIAL_FLAG_ALPHA_PERPIXEL_ILLUM   = (1u << 9),
    MATERIAL_FLAG_ALPHA_PERPOLY_ILLUM    = (1u << 10),
    MATERIAL_FLAG_DIFF_PERPIXEL_ENV      = (1u << 11),
    MATERIAL_FLAG_ALPHA_PERPOLY_ENV      = (1u << 12),
    MATERIAL_FLAG_DETAIL                 = (1u << 13),
    MATERIAL_FLAG_ENV_CUBEMAP            = (1u << 14),
    MATERIAL_FLAG_ENV_VIEWSPACE          = (1u << 15),
    MATERIAL_FLAG_ENV_WORLDSPACE         = (1u << 16),
    MATERIAL_FLAG_ALPHA_BLEND            = (1u << 17),
    MATERIAL_FLAG_ILLUM_USE_DIFFUSE      = (1u << 18),
};

//------------------------------------------------------------------------------

enum instance_flags
{
    INSTANCE_FLAG_CLIPPED                = (1u << 19),
    INSTANCE_FLAG_GLOWING                = (1u << 20),
    INSTANCE_FLAG_SHADOW_PASS            = (1u << 21),
    INSTANCE_FLAG_FILTERLIGHT            = (1u << 22),
    INSTANCE_FLAG_PROJ_LIGHT             = (1u << 23),
    INSTANCE_FLAG_FADING_ALPHA           = (1u << 24),
    INSTANCE_FLAG_DYNAMIC_LIGHT          = (1u << 25),
    INSTANCE_FLAG_DETAIL                 = (1u << 26),
    INSTANCE_FLAG_PROJ_SHADOW            = (1u << 27),
};

//------------------------------------------------------------------------------

//enum render_flags
//{
//    RENDER_FLAG_WIREFRAME            = (1u << 29),
//    RENDER_FLAG_WIREFRAME2           = (1u << 30),
//    RENDER_FLAG_PULSED               = (1u << 31),
//    RENDER_FLAG_SHADOW_PASS          = (1u << 32),
//    RENDER_FLAG_GLOWING              = (1u << 33),
//    RENDER_FLAG_FADING_ALPHA         = (1u << 34),
//    RENDER_FLAG_CLIPPED              = (1u << 35),
//    RENDER_FLAG_FORCE_LAST           = (1u << 36),
//    RENDER_FLAG_DISABLE_SPOTLIGHT    = (1u << 37),
//    RENDER_FLAG_DISABLE_FILTERLIGHT  = (1u << 38),
//    RENDER_FLAG_DISABLE_PROJ_SHADOWS = (1u << 39),
//    RENDER_FLAG_SIMPLE_LIGHTING      = (1u << 40),
//    RENDER_FLAG_PERPIXEL_POINTLIGHT  = (1u << 41),
//};

//==============================================================================
//  CONSTANT BUFFER STRUCTURES
//==============================================================================

struct d3d_lighting
{
    s32     LightCount;
    vector4 LightVec[MAX_GEOM_LIGHTS];
    vector4 LightCol[MAX_GEOM_LIGHTS];
    vector4 LightDir[MAX_GEOM_LIGHTS];
    vector4 LightCone[MAX_GEOM_LIGHTS];
    vector4 AmbCol;
};

//------------------------------------------------------------------------------

struct cb_geom_frame
{
    matrix4 View;                          // World to view matrix
    matrix4 Projection;                    // View to clip matrix

    u32     MaterialFlags;                 // Material and instance flags
    f32     AlphaRef;                      // Alpha test reference
    f32     NearZ;                         // View near plane
    f32     FarZ;                          // View far plane
    vector4 UVAnim;                        // xy = uv animation offsets, z = detail scale
    vector4 CameraPosition;                // xyz = camera position, w = 1
    vector4 EnvParams;                     // x = fixed alpha, y = cubemap intensity, z = fade alpha, w = z-prime override
    matrix4 DistortionNormalMatrix;        // World normal -> rotated view-space normal
    vector4 DistortionParams;              // x = pixel offset scale, zw = inverse scene size
};

//------------------------------------------------------------------------------

struct cb_geom_object
{
    matrix4 World;                         // Local to world matrix
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
    vector4 LightDir[MAX_GEOM_LIGHTS];
    vector4 LightCone[MAX_GEOM_LIGHTS];
    vector4 LightAmbCol;
    u32     LightCount;
    f32     Padding[3];
};

//------------------------------------------------------------------------------

struct cb_skin_bone
{
    matrix4 L2W;                           // Local to world matrix
};

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

inline f32 ComputeCubeMapIntensity( const material* pMaterial )
{
    const f32 kDefaultCubeMapIntensity = 0.35f;

    if( !pMaterial )
        return 1.0f;

    if( !( pMaterial->m_Flags & geom::material::FLAG_ENV_CUBE_MAP ) )
        return 1.0f;

    const s8 MaterialType = pMaterial->m_Type;
    if( ( MaterialType == Material_Alpha_PerPolyEnv ) ||
        ( MaterialType == Material_Distortion_PerPolyEnv ) )
    {
        return 1.0f;
    }

    f32 CubeIntensity = pMaterial->m_FixedAlpha;

    if( CubeIntensity <= 0.0f )
        CubeIntensity = kDefaultCubeMapIntensity;

    if( CubeIntensity < 0.0f )
        CubeIntensity = 0.0f;

    if( CubeIntensity > 1.0f )
        CubeIntensity = 1.0f;

    return CubeIntensity;
}

//==============================================================================
//  TEXTURE SLOTS
//==============================================================================

enum texture_slot
{
    TEXTURE_SLOT_DIFFUSE              = 0,
    TEXTURE_SLOT_DETAIL               = 1,
    TEXTURE_SLOT_ENVIRONMENT          = 2,
    TEXTURE_SLOT_ENVIRONMENT_CUBE     = TEXTURE_SLOT_ENVIRONMENT + 1,
    TEXTURE_SLOT_DISTORTION_SCENE     = 18,
};

//==============================================================================
//  GEOMETRY MANAGER CLASS
//==============================================================================

class geom_mgr
{
public:

    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------

    void        Init                        ( void );
    void        Kill                        ( void );

    //--------------------------------------------------------------------------
    // Material Binding
    //--------------------------------------------------------------------------

    void        SetRigidMaterial            ( const matrix4*      pL2W,
                                              const bbox*         pBBox,
                                              const d3d_lighting* pLighting,
                                              const material*     pMaterial,
                                              u32                 RenderFlags,
                                              u8                  UOffset,
                                              u8                  VOffset,
                                              u8                  Alpha = 255,
                                              u8                  OverrideMat = FALSE );
    void        SetSkinMaterial             ( const matrix4*      pL2W,
                                              const bbox*         pBBox,
                                              const d3d_lighting* pLighting,
                                              const material*     pMaterial,
                                              u32                 RenderFlags,
                                              u8                  UOffset,
                                              u8                  VOffset,
                                              u8                  Alpha = 255,
                                              u8                  OverrideMat = FALSE );

    //--------------------------------------------------------------------------
    // Projection And Shadow State
    //--------------------------------------------------------------------------

    void        ResetProjTextures           ( void );
    void        ResetShadowMaps             ( void );
    void        SetDistortionState          ( const radian3& NormalRot );
    void        ClearDistortionState        ( void );

    //--------------------------------------------------------------------------
    // Texture Binding And Cache Control
    //--------------------------------------------------------------------------

    void        SetBitmap                   ( const xbitmap* pBitmap,
                                              texture_slot   Slot );
    void        SetEnvironmentCubemap       ( const cubemap* pCubemap );
    void        InvalidateCache             ( void );

    //--------------------------------------------------------------------------
    // Resource Access
    //--------------------------------------------------------------------------

    ID3D11Buffer*
                GetSkinBoneBuffer           ( void );

protected:

    struct material_constants
    {
        u32 Flags;
        f32 AlphaRef;
    };

    //--------------------------------------------------------------------------
    // Render State Helpers
    //--------------------------------------------------------------------------

    void        ApplyRenderStates           ( const material* pMaterial,
                                              u32             RenderFlags,
                                              u8              OverrideMat );

    //--------------------------------------------------------------------------
    // Resource Initialization
    //--------------------------------------------------------------------------

    xbool       InitRigidShaders            ( void );
    void        KillRigidShaders            ( void );
    xbool       InitSkinShaders             ( void );
    void        KillSkinShaders             ( void );
    xbool       InitProjTextures            ( void );
    void        KillProjTextures            ( void );
    xbool       InitShadowMaps              ( void );
    void        KillShadowMaps              ( void );

    //--------------------------------------------------------------------------
    // Internal Helpers
    //--------------------------------------------------------------------------

    xbool       UpdateRigidConstants        ( const matrix4*      pL2W,
                                              const material*     pMaterial,
                                              u32                 RenderFlags,
                                              const d3d_lighting* pLighting,
                                              u8                  UOffset,
                                              u8                  VOffset,
                                              u8                  Alpha = 255,
                                              u8                  OverrideMat = FALSE );
    xbool       UpdateSkinConstants         ( const d3d_lighting* pLighting,
                                              const material*     pMaterial,
                                              u32                 RenderFlags,
                                              u8                  UOffset,
                                              u8                  VOffset,
                                              u8                  Alpha = 255,
                                              u8                  OverrideMat = FALSE );
    xbool       UpdateProjTextures          ( const matrix4& L2W,
                                              const bbox&    B,
                                              u32            Slot,
                                              u32            RenderFlags );
    xbool       UpdateShadowMaps            ( const matrix4& L2W,
                                              const bbox&    B );
    material_constants
                BuildMaterialFlags          ( const material* pMaterial,
                                              xbool           IncludeVertexColor ) const;
    xbool       UploadConstantBuffer        ( ID3D11Buffer*   pBuffer,
                                              const void*     pData,
                                              u32             Size,
                                              const char*     pBufferName ) const;
    cb_geom_frame
                BuildFrameConstants         ( const view&     View,
                                              const material* pMaterial,
                                              u32             RenderFlags,
                                              u8              UOffset,
                                              u8              VOffset,
                                              u8              Alpha,
                                              xbool           IncludeVertexColor,
                                              u8              OverrideMat ) const;
    u32         BuildInstanceFlags          ( u32 RenderFlags ) const;
    cb_lighting BuildLightingConstants      ( const d3d_lighting* pLighting ) const;

protected:

    //--------------------------------------------------------------------------
    // Manager State
    //--------------------------------------------------------------------------

    xbool                   m_bInitialized;

    //--------------------------------------------------------------------------
    // Rigid Geometry Resources
    //--------------------------------------------------------------------------

    ID3D11VertexShader*     m_pRigidVertexShader;
    ID3D11PixelShader*      m_pRigidPixelShader;
    ID3D11InputLayout*      m_pRigidInputLayout;
    ID3D11Buffer*           m_pRigidFrameBuffer;
    ID3D11Buffer*           m_pRigidObjectBuffer;
    ID3D11Buffer*           m_pRigidLightBuffer;

    //--------------------------------------------------------------------------
    // Projection And Shadow Resources
    //--------------------------------------------------------------------------

    ID3D11Buffer*           m_pProjTextureBuffer;
    ID3D11SamplerState*     m_pProjSampler;
    ID3D11Buffer*           m_pShadowBuffer;
    ID3D11SamplerState*     m_pPointShadowSampler;
    ID3D11SamplerState*     m_pSpotShadowSampler;

    //--------------------------------------------------------------------------
    // Skin Geometry Resources
    //--------------------------------------------------------------------------

    ID3D11VertexShader*     m_pSkinVertexShader;
    ID3D11PixelShader*      m_pSkinPixelShader;
    ID3D11InputLayout*      m_pSkinInputLayout;
    ID3D11Buffer*           m_pSkinFrameBuffer;
    ID3D11Buffer*           m_pSkinBoneBuffer;
    ID3D11Buffer*           m_pSkinLightBuffer;

    //--------------------------------------------------------------------------
    // Cached Bind State
    //--------------------------------------------------------------------------

    const xbitmap*          m_pCurrentTexture;
    const xbitmap*          m_pCurrentDetailTexture;
    const xbitmap*          m_pCurrentEnvironmentTexture;
    const cubemap*          m_pCurrentEnvCubemap;
    xbool                   m_bDistortionStateActive;
    radian3                 m_DistortionNormalRot;

    //--------------------------------------------------------------------------
    // Cached Constant Data
    //--------------------------------------------------------------------------

    cb_geom_frame           m_CachedRigidFrame;
    cb_geom_object          m_CachedRigidObject;
    cb_lighting             m_CachedRigidLighting;
    cb_geom_frame           m_CachedSkinFrame;
    cb_lighting             m_CachedSkinLighting;
    u32                     m_LastProjLightCount;
    u32                     m_LastProjShadowCount;
    xbool                   m_bRigidFrameDirty;
    xbool                   m_bRigidObjectDirty;
    xbool                   m_bRigidLightingDirty;
    xbool                   m_bSkinFrameDirty;
    xbool                   m_bSkinLightingDirty;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern geom_mgr g_GeomMgr;

//==============================================================================
#endif // GEOM_MANAGER_HPP
//==============================================================================
