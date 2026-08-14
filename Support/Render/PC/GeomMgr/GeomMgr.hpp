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
//  BASE INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "x_array.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../../Material.hpp"
#include "../../GeometryDraw.hpp"
#include "../../LightMgr.hpp"
#include "../../RenderPipelineCache.hpp"
#include "../../ProjTextureMgr.hpp"
#include "../../Render.hpp"
#include "../../SkinGeom.hpp"
#include "../../Texture.hpp"

#include "../ShadowMgr.hpp"
#include "../ProjectionAtlas.hpp"
#include "../VertexMgr.hpp"

#include "e_Engine.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define MAX_GEOM_LIGHTS 4

enum instance_flags
{
    INSTANCE_FLAG_CLIPPED = ( 1u << 19 ),
    INSTANCE_FLAG_GLOWING = ( 1u << 20 ),
    INSTANCE_FLAG_RECEIVE_LOCAL_SHADOW = ( 1u << 21 ),
    INSTANCE_FLAG_FILTERLIGHT = ( 1u << 22 ),
    INSTANCE_FLAG_PROJ_LIGHT = ( 1u << 23 ),
    INSTANCE_FLAG_FADING_ALPHA = ( 1u << 24 ),
    INSTANCE_FLAG_DYNAMIC_LIGHT = ( 1u << 25 ),
    INSTANCE_FLAG_DETAIL = ( 1u << 26 ),
    INSTANCE_FLAG_PROJ_SHADOW = ( 1u << 27 ),
};

//------------------------------------------------------------------------------

// enum render_flags
//{
//     RENDER_FLAG_WIREFRAME            = (1u << 29),
//     RENDER_FLAG_WIREFRAME2           = (1u << 30),
//     RENDER_FLAG_PULSED               = (1u << 31),
//     RENDER_FLAG_GLOWING              = (1u << 33),
//     RENDER_FLAG_FADING_ALPHA         = (1u << 34),
//     RENDER_FLAG_CLIPPED              = (1u << 35),
//     RENDER_FLAG_FORCE_LAST           = (1u << 36),
//     RENDER_FLAG_DISABLE_SPOTLIGHT    = (1u << 37),
//     RENDER_FLAG_DISABLE_FILTERLIGHT  = (1u << 38),
//     RENDER_FLAG_DISABLE_PROJ_SHADOWS = (1u << 39),
//     RENDER_FLAG_SIMPLE_LIGHTING      = (1u << 40),
//     RENDER_FLAG_PERPIXEL_POINTLIGHT  = (1u << 41),
// };

//==============================================================================
//  CONSTANT BUFFER STRUCTURES
//==============================================================================

struct cb_geom_lighting
{
    s32     LightCount;
    s32     DynamicLightIndex[MAX_GEOM_LIGHTS];
    vector4 LightVec[MAX_GEOM_LIGHTS];
    vector4 LightCol[MAX_GEOM_LIGHTS];
    vector4 LightDir[MAX_GEOM_LIGHTS];
    vector4 LightCone[MAX_GEOM_LIGHTS];
    vector4 LightCookieU[MAX_GEOM_LIGHTS];
    vector4 LightCookieV[MAX_GEOM_LIGHTS];
    vector4 AmbCol;
};

//------------------------------------------------------------------------------

struct GeomLightingConstants
{
    vector4 LightVec[MAX_GEOM_LIGHTS];
    vector4 LightCol[MAX_GEOM_LIGHTS];
    vector4 LightDir[MAX_GEOM_LIGHTS];
    vector4 LightCone[MAX_GEOM_LIGHTS];
    vector4 LightCookieU[MAX_GEOM_LIGHTS];
    vector4 LightCookieV[MAX_GEOM_LIGHTS];
    vector4 LightCookieAtlas[MAX_GEOM_LIGHTS];
    u32     LightCookieLayer[MAX_GEOM_LIGHTS];
    f32     LightCookieMaxMip[MAX_GEOM_LIGHTS];
    u32     LightShadowIndex[MAX_GEOM_LIGHTS];
    vector4 LightAmbCol;
    u32     LightCount;
    u32     Padding[3];
};

static_assert( sizeof( GeomLightingConstants ) == 528, "cb_geom_lighting layout must match HLSL" );

//------------------------------------------------------------------------------

struct GeomFrameConstants
{
    matrix4 View;       // World to view matrix
    matrix4 Projection; // View to clip matrix

    u32     MaterialFlags;          // Material and instance flags
    f32     AlphaRef;               // Alpha test reference
    f32     NearZ;                  // View near plane
    f32     FarZ;                   // View far plane
    vector4 UVAnim;                 // xy = uv animation offsets, z = detail scale
    vector4 CameraPosition;         // xyz = camera position, w = 1
    vector4 EnvParams;              // x = fixed alpha, y = cubemap intensity, z = fade alpha, w = z-prime override
    matrix4 DistortionNormalMatrix; // World normal -> rotated view-space normal
    vector4 DistortionParams;       // x = pixel offset scale, zw = inverse scene size
    vector4 FogColor;                // rgb = fog color
    vector4 FogCoeff;                // polynomial fog coefficients
    vector4 FogParams;               // x = near, y = far, z = fog start, w = enabled
};

static_assert( sizeof( GeomFrameConstants ) == 320, "cbFrameConstants layout must match HLSL" );

//------------------------------------------------------------------------------

struct ProjectionTextureConstants
{
    matrix4 ProjLightMatrix[ProjTextureMgr::MaxLightProjectionCount];
    matrix4 ProjShadowMatrix[ProjTextureMgr::MaxShadowProjectionCount];
    vector4 ProjLightAtlas[ProjTextureMgr::MaxLightProjectionCount];
    vector4 ProjShadowAtlas[ProjTextureMgr::MaxShadowProjectionCount];
    vector4 ProjLightInfo[ProjTextureMgr::MaxLightProjectionCount];
    vector4 ProjShadowInfo[ProjTextureMgr::MaxShadowProjectionCount];
    u32     ProjLightCount;
    u32     ProjShadowCount;
    u32     Padding[2];
};

static_assert( sizeof( ProjectionTextureConstants ) == 1168, "cbProjTextures layout must match HLSL" );

//------------------------------------------------------------------------------

struct SkinBoneConstants
{
    matrix4 L2W; // Local to world matrix
};

//------------------------------------------------------------------------------

struct RigidInstanceData
{
    matrix4 World;
    u32     ShaderFlags;
    u32     ColorOffset;
    u32     LightingIndex;
    f32     FadeAlpha;
};

static_assert( sizeof( RigidInstanceData ) == 80, "RigidInstanceData layout must match HLSL" );

//------------------------------------------------------------------------------

struct SkinInstanceData
{
    u32 ShaderFlags;
    u32 BoneOffset;
    u32 LightingIndex;
    u32 Padding0;
    f32 FadeAlpha;
    f32 Padding[3];
};

static_assert( sizeof( SkinInstanceData ) == 32, "SkinInstanceData layout must match HLSL" );

//------------------------------------------------------------------------------

struct SkinDrawInstance
{
    u32 InstanceIndex;
    u32 BoneRemapOffset;
};

static_assert( sizeof( SkinDrawInstance ) == 8, "Skin draw-instance vertex layout must match HLSL" );

//==============================================================================
//  BATCH DESCRIPTOR STRUCTURES
//==============================================================================

struct geom_pass_desc
{
    // Dimensions of the color attachment selected by the frame scheduler.
    // GeomMgr uses them for screen-space shader constants only.
    u32 TargetWidth;
    u32 TargetHeight;

    geom_pass_desc( void ) : TargetWidth( 0 ), TargetHeight( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct RigidBatchDesc
{
    rigid_geom const*       pGeom;
    matrix4 const*          pL2W;
    cb_geom_lighting const* pLighting;
    u32 const*              pColorInfo;
    xhandle                 hMesh;
    s32                     iSubMesh;
    u32                     RenderFlags;
    u8                      UOffset;
    u8                      VOffset;
    u8                      Alpha;
    u8                      OverrideMat;
    f32                     SortDepth;
};

//------------------------------------------------------------------------------

struct SkinBatchDesc
{
    skin_geom const*        pGeom;
    matrix4 const*          pBones;
    cb_geom_lighting const* pLighting;
    xhandle                 hMesh;
    s32                     iSubMesh;
    u32                     RenderFlags;
    u8                      UOffset;
    u8                      VOffset;
    u8                      Alpha;
    u8                      OverrideMat;
    f32                     SortDepth;
};

//==============================================================================
//  TEXTURE SLOTS
//==============================================================================

enum texture_slot
{
    TEXTURE_SLOT_DIFFUSE = 0,
    TEXTURE_SLOT_DETAIL = 1,
    TEXTURE_SLOT_ENVIRONMENT = 2,
    TEXTURE_SLOT_ENVIRONMENT_CUBE = 3,
};

//==============================================================================
//  GEOMETRY MANAGER CLASS
//==============================================================================

class GeomMgr
{
  public:
    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------

    void  Init( void );
    void  Kill( void );
    xbool BuildPackets( xarray<geometry_draw_item> const& draws,
                        xarray<dynamic_geometry_draw> const& dynamicDraws,
                        cubemap const* pCubeMap );

  protected:
    //--------------------------------------------------------------------------
    // Material Binding
    //--------------------------------------------------------------------------

    xbool SetRigidMaterial( material const* pMaterial, u32 renderFlags, u8 uOffset, u8 vOffset, u8 overrideMat,
                            xbool sceneOnly, geom_pass_desc const& pass );
    xbool SetSkinMaterial( material const* pMaterial, u32 renderFlags, u8 uOffset, u8 vOffset, u8 overrideMat,
                           xbool sceneOnly, geom_pass_desc const& pass );

    //--------------------------------------------------------------------------
    // Projection And Shadow State
    //--------------------------------------------------------------------------

    xbool ResetProjTextures( void );
    void  SetDistortionState( radian3 const& normalRot );

  public:
    void SetDistortionScene( shader_resource const* pResource );
    void ClearDistortionState( void );

  protected:
    //--------------------------------------------------------------------------
    // Texture Binding And Instance Data
    //--------------------------------------------------------------------------

    void SetBitmap( texture const* pTexture, texture_slot slot );
    void SetEnvironmentCubemap( cubemap const* pCubemap );
    void BeginPacketCollection( void );

  public:
    void                 InvalidateCache( void );
    xbool                PrepareProjectionAtlas( void );
    xbool                PrepareSharedShadowData( void );
    xbool                UploadPackets( void );
    u32                  GetDynamicLightShadowIndex ( s32 dynamicLightIndex ) const;
    shader_resource const* GetFaceShadowResource      ( void ) const;
    shader_resource const* GetFaceShadowDepthResource ( void ) const;
    shader_resource const* GetShadowMatricesResource  ( void ) const;
    shader_resource const* GetShadowDataResource      ( void ) const;
    rstate_sampler_preset  GetFaceShadowSampler       ( void ) const;
    s32                  GetPacketCount( void ) const;
    s32                  GetRigidInstanceCount( void ) const;
    s32                  GetSkinInstanceCount( void ) const;
    s32                  GetLitInstanceCount( void ) const;
    s32                  GetInstanceLightCount( void ) const;
    s32                  GetLightingRecordCount( void ) const;
    u32                  GetGBufferGpuDrawCount( void ) const;
    u32                  GetGBufferInstanceCount( void ) const;
    u32                  GetGBufferSkinSectionDrawCount( void ) const;
    u64                  GetGBufferSubmittedIndexCount( void ) const;
    u32                  GetGBufferRigidIndirectRunCount( void ) const;
    u32                  GetGBufferRigidIndirectCommandCount( void ) const;
    u32                  GetGBufferSkinIndirectRunCount( void ) const;
    u32                  GetGBufferSkinIndirectCommandCount( void ) const;
    geometry_render_pass GetPacketPass( s32 packetIndex ) const;
    u32                  GetPacketSequence( s32 packetIndex ) const;
    f32                  GetPacketSortDepth( s32 packetIndex ) const;
    xbool                ExecutePacket( s32 packetIndex, geom_pass_desc const& pass );
    xbool                ExecuteGBuffer( geom_pass_desc const& pass );
    void                 ClearPackets( void );

  protected:
    xbool HasRigidBatch( void ) const;
    xbool CanAppendRigidBatch( RigidBatchDesc const& desc ) const;
    void  AddRigidBatchInstance( RigidBatchDesc const& desc );
    void  FlushRigidBatch( material const* pMaterial, u8 materialOverride, geometry_render_pass pass, u32 sequence );
    xbool HasSkinBatch( void ) const;
    xbool CanAppendSkinBatch( SkinBatchDesc const& desc ) const;
    void  AddSkinBatchInstance( SkinBatchDesc const& desc );
    void  FlushSkinBatch( material const* pMaterial, u8 materialOverride, geometry_render_pass pass, u32 sequence );

    struct RenderStateSelection
    {
        rstate_blend_preset   Blend;
        rstate_depth_preset   Depth;
        rstate_raster_preset  Raster;
        rstate_sampler_preset Sampler;
        xbool                 SceneOnly;

        RenderStateSelection( void )
            : Blend( RSTATE_BLEND_PRESET_NONE ), Depth( RSTATE_DEPTH_PRESET_NORMAL ),
              Raster( RSTATE_RASTER_PRESET_SOLID ), Sampler( RSTATE_SAMPLER_PRESET_ANISOTROPIC_WRAP ),
              SceneOnly( FALSE )
        {
        }
    };

    enum
    {
        GEOM_PIPELINE_VARIANT_COUNT =
            RSTATE_BLEND_PRESET_COUNT * RSTATE_DEPTH_PRESET_COUNT * RSTATE_RASTER_PRESET_COUNT * 2
    };

    enum geom_shader_kind
    {
        GEOM_SHADER_NONE = -1,
        GEOM_SHADER_RIGID,
        GEOM_SHADER_SKIN,
        GEOM_SHADER_DYNAMIC,
        GEOM_SHADER_COUNT
    };

    enum geom_packet_kind
    {
        GEOM_PACKET_RIGID = 0,
        GEOM_PACKET_SKIN,
        GEOM_PACKET_DYNAMIC
    };

    struct GeomResourceSnapshot
    {
        shader_resource const* pDiffuse;
        shader_resource const* pDetail;
        shader_resource const* pEnvironment;
        shader_resource const* pEnvironmentCube;
        xbool                  bDistortionActive;
        radian3                DistortionNormalRot;

        GeomResourceSnapshot( void );
    };

    struct GeomDrawPacket
    {
        geom_packet_kind      Kind;
        material const*       pMaterial;
        xhandle               hMesh;
        u32                   RenderFlags;
        u32                   FirstInstance;
        u32                   InstanceCount;
        u8                    UOffset;
        u8                    VOffset;
        u8                    MaterialOverride;
        geometry_render_pass  Pass;
        u32                   Sequence;
        f32                   SortDepth;
        u32                   FirstSkinDrawInstance;
        u32                   FirstDynamicVertex;
        u32                   FirstDynamicIndex;
        u32                   DynamicIndexCount;
        u32                   DynamicLightingIndex;
        shader_resource const* pDamageMask;
        vram_texture*          pDamageTexture;
        u8 const*              pDamageUpload;
        xbool*                 pDamageUploadPending;
        s32                    DamageUploadX;
        s32                    DamageUploadY;
        s32                    DamageUploadWidth;
        s32                    DamageUploadHeight;
        GeomResourceSnapshot  Resources;
        VertexMgr::mesh_range Range;

        GeomDrawPacket( void );
    };

    struct SkinIndirectRun
    {
        u32 FirstPacket;
        u32 FirstCommand;
        u32 CommandCount;
        s32 VertexPool;
        s32 IndexPool;

        SkinIndirectRun( void )
            : FirstPacket( 0 ), FirstCommand( 0 ), CommandCount( 0 ), VertexPool( -1 ), IndexPool( -1 )
        {
        }
    };

    struct RigidIndirectRun
    {
        u32 FirstPacket;
        u32 LastPacket;
        u32 FirstCommand;
        u32 CommandCount;
        s32 VertexPool;
        s32 IndexPool;

        RigidIndirectRun( void )
            : FirstPacket( 0 ), LastPacket( 0 ), FirstCommand( 0 ), CommandCount( 0 ), VertexPool( -1 ), IndexPool( -1 )
        {
        }
    };

    struct ShaderBindingLayout
    {
        u32 FrameConstantsVertex;
        u32 FrameConstantsPixel;
        u32 ProjTexturesPixel;
        u32 ShadowMatricesPixel;
        u32 ShadowDataPixel;
        u32 DiffuseTexturePixel;
        u32 DetailTexturePixel;
        u32 EnvironmentTexturePixel;
        u32 EnvironmentCubeTexturePixel;
        u32 FaceShadowTexturePixel;
        u32 FaceShadowDepthTexturePixel;
        u32 DistortionTexturePixel;
        u32 ProjectionAtlasTexturePixel;
        u32 InstanceDataVertex;
        u32 AuxiliaryDataVertex;
        u32 InstanceDataPixel;
        u32 LightingDataPixel;
        u32 BoneRemapDataVertex;

        ShaderBindingLayout( void );
    };

    //--------------------------------------------------------------------------
    // Render State Helpers
    //--------------------------------------------------------------------------

    RenderStateSelection ResolveRenderStates( material const* pMaterial, u32 renderFlags, u8 overrideMat,
                                              xbool sceneOnly ) const;
    xbool                PrewarmPipelines( void );
    render_pipeline*     GetRigidPipeline( RenderStateSelection const& state, xbool isPrewarm = FALSE );
    render_pipeline*     GetSkinPipeline( RenderStateSelection const& state, xbool isPrewarm = FALSE );
    xbool                BindRigidPipeline( RenderStateSelection const& state );
    xbool                BindSkinPipeline( RenderStateSelection const& state );
    void                 DestroyRigidPipelines( void );
    void                 DestroySkinPipelines( void );

    //--------------------------------------------------------------------------
    // Resource Initialization
    //--------------------------------------------------------------------------

    xbool InitRigidShaders( void );
    void  KillRigidShaders( void );
    xbool InitSkinShaders( void );
    void  KillSkinShaders( void );
    xbool InitDynamicGeometry( void );
    void  KillDynamicGeometry( void );
    xbool InitShaderBindings( void );
    void  KillShaderBindings( void );
    xbool ResolveShaderBindings( ShaderBindingLayout& bindings, shader const& vertexShader, shader const& pixelShader,
                                 shader const& scenePixelShader, char const* pInstanceName, char const* pAuxiliaryName,
                                 char const* pBoneRemapName );
    xbool InitProjTextures( void );
    void  KillProjTextures( void );
    xbool InitShadowMaps( void );
    void  KillShadowMaps( void );
    xbool InitSamplers( void );
    void  KillSamplers( void );

    //--------------------------------------------------------------------------
    // Internal Helpers
    //--------------------------------------------------------------------------

    xbool      UpdateRigidConstants( material const* pMaterial, u8 uOffset, u8 vOffset, u8 overrideMat,
                                     geom_pass_desc const& pass );
    xbool      UpdateSkinConstants( material const* pMaterial, u8 uOffset, u8 vOffset, u8 overrideMat,
                                    geom_pass_desc const& pass );
    xbool      UpdateProjTextures( void );
    xbool      BuildShadowMapConstants( void );
    static u32 BuildBatchStateFlags( u32 renderFlags );
    static u32 BuildInstanceFlags( u32 renderFlags );
    static f32 BuildInstanceFadeAlpha( u32 renderFlags, u8 alpha );
    xbool      BuildLightingData( void );
    xbool      RequestLightCookies( xarray<cb_geom_lighting const*> const& lighting ) const;
    static s32 CompareLightingReferences( void const* pA, void const* pB );
    xbool      CopyLightingData( GeomLightingConstants& destination, cb_geom_lighting const& source ) const;
    ShaderBindingLayout const* GetShaderBindings( geom_shader_kind kind ) const;
    rstate_sampler const*      GetSampler( rstate_sampler_preset preset ) const;
    xbool                      PushFrameConstants( geom_shader_kind kind, GeomFrameConstants const& frame ) const;
    xbool BindPixelTexture( u32 slot, shader_resource const* pResource, rstate_sampler const* pSampler ) const;
    xbool BindPacketResources( GeomDrawPacket const& packet );
    xbool BindRigidFrameBuffers( void ) const;
    xbool BindRigidGeometryBuffers( VertexMgr::mesh_range const& range ) const;
    xbool BindSkinFrameBuffers( void ) const;
    xbool BindSkinGeometryBuffers( VertexMgr::mesh_range const& range ) const;
    xbool BuildDynamicPackets( xarray<dynamic_geometry_draw> const& draws );
    xbool UploadDynamicGeometry( void );
    xbool ExecuteDynamicPacket( GeomDrawPacket const& packet, geom_pass_desc const& pass );
    render_pipeline* GetDynamicPipeline( u32 renderFlags );
    GeomResourceSnapshot CaptureResourceSnapshot( void ) const;
    void                 ConfigureDrawResources( material const* pMaterial, geometry_render_pass pass,
                                                 radian3 const& distortionNormalRot, cubemap const* pCubeMap );
    static s32           CompareGBufferDraws( void const* pA, void const* pB );
    xbool                BuildRigidIndirectRuns( void );
    xbool CanAppendRigidIndirectRun( RigidIndirectRun const& run, u32 packetIndex, GeomDrawPacket const& packet ) const;
    xbool ExecuteRigidIndirectRun( RigidIndirectRun const& run, geom_pass_desc const& pass );
    xbool BuildSkinDrawData( void );
    xbool CanAppendSkinIndirectRun( SkinIndirectRun const& run, GeomDrawPacket const& packet ) const;
    xbool ExecuteSkinIndirectRun( SkinIndirectRun const& run, geom_pass_desc const& pass );
    void  ResetRigidBatch( void );
    void  ResetSkinBatch( void );
    GeomFrameConstants BuildFrameConstants( view const& view, material const* pMaterial, u8 uOffset, u8 vOffset,
                                            xbool includeVertexColor, u8 overrideMat,
                                            geom_pass_desc const& pass ) const;

  protected:
    //--------------------------------------------------------------------------
    // Manager State
    //--------------------------------------------------------------------------

    xbool               m_isInitialized;
    RenderPipelineCache m_rigidPipelines;
    RenderPipelineCache m_skinPipelines;

    //--------------------------------------------------------------------------
    // Rigid Geometry Resources
    //--------------------------------------------------------------------------

    shader                          m_rigidVertexShader;
    shader                          m_rigidPixelShader;
    shader                          m_rigidScenePixelShader;
    ShaderBindingLayout             m_rigidShaderBindings;
    rbuffer                         m_rigidInstanceDataBuffer;
    rbuffer                         m_rigidInstanceIndexBuffer;
    rbuffer                         m_rigidColorBuffer;
    rbuffer                         m_rigidIndirectBuffer;
    u32                             m_rigidInstanceCapacity;
    u32                             m_rigidInstanceIndexCapacity;
    u32                             m_rigidColorCapacity;
    u32                             m_rigidIndirectCapacity;
    xarray<RigidInstanceData>       m_lRigidFrameInstances;
    xarray<cb_geom_lighting const*> m_lRigidFrameLighting;
    xarray<u32>                     m_lRigidFrameColors;
    u32                             m_rigidBatchFirstInstance;
    xhandle                         m_hRigidBatchMesh;
    u32                             m_rigidBatchFlags;
    u8                              m_rigidBatchUOffset;
    u8                              m_rigidBatchVOffset;
    u8                              m_rigidBatchOverrideMat;
    f32                             m_rigidBatchSortDepth;

    //--------------------------------------------------------------------------
    // Skin Geometry Resources
    //--------------------------------------------------------------------------

    shader                                 m_skinVertexShader;
    shader                                 m_skinPixelShader;
    shader                                 m_skinScenePixelShader;
    ShaderBindingLayout                    m_skinShaderBindings;
    rbuffer                                m_skinInstanceDataBuffer;
    rbuffer                                m_skinBoneDataBuffer;
    rbuffer                                m_skinDrawInstanceBuffer;
    rbuffer                                m_skinBoneRemapBuffer;
    rbuffer                                m_skinIndirectBuffer;
    u32                                    m_skinInstanceCapacity;
    u32                                    m_skinBoneCapacity;
    u32                                    m_skinDrawInstanceCapacity;
    u32                                    m_skinBoneRemapCapacity;
    u32                                    m_skinIndirectCapacity;
    xarray<SkinInstanceData>               m_lSkinFrameInstances;
    xarray<cb_geom_lighting const*>        m_lSkinFrameLighting;
    xarray<matrix4>                        m_lSkinFrameBones;
    xarray<SkinDrawInstance>               m_lSkinDrawInstances;
    xarray<u32>                            m_lSkinBoneRemaps;
    xarray<rdraw_indexed_indirect_command> m_lSkinIndirectCommands;
    xarray<SkinIndirectRun>                m_lSkinIndirectRuns;
    u32                                    m_skinBatchFirstInstance;
    xhandle                                m_hSkinBatchMesh;
    u32                                    m_skinBatchFlags;
    u8                                     m_skinBatchUOffset;
    u8                                     m_skinBatchVOffset;
    u8                                     m_skinBatchOverrideMat;
    f32                                    m_skinBatchSortDepth;
    u32                                    m_litInstanceCount;
    u32                                    m_instanceLightCount;
    u32                                    m_gBufferGpuDrawCount;
    u32                                    m_gBufferInstanceCount;
    u32                                    m_gBufferSkinSectionDrawCount;
    u64                                    m_gBufferSubmittedIndexCount;
    xarray<rdraw_indexed_indirect_command> m_lRigidIndirectCommands;
    xarray<RigidIndirectRun>               m_lRigidIndirectRuns;

    //--------------------------------------------------------------------------
    // Dynamic Geometry Resources
    //--------------------------------------------------------------------------

    shader                          m_dynamicVertexShader;
    shader                          m_dynamicPixelShader;
    ShaderBindingLayout             m_dynamicShaderBindings;
    RenderPipelineCache             m_dynamicPipelines;
    rbuffer                         m_dynamicVertexBuffer;
    rbuffer                         m_dynamicIndexBuffer;
    u32                             m_dynamicVertexCapacity;
    u32                             m_dynamicIndexCapacity;
    u32                             m_dynamicConstantsVertexSlot;
    u32                             m_dynamicConstantsPixelSlot;
    u32                             m_dynamicDiffuseSlot;
    u32                             m_dynamicDamageSlot;
    xarray<dynamic_geometry_vertex> m_lDynamicFrameVertices;
    xarray<u16>                     m_lDynamicFrameIndices;
    xarray<cb_geom_lighting const*> m_lDynamicFrameLighting;

    struct LightingReference
    {
        cb_geom_lighting const* pLighting;
        geom_packet_kind        Kind;
        u32                     InstanceIndex;
    };

    rbuffer                       m_lightingDataBuffer;
    u32                           m_lightingDataCapacity;
    xarray<GeomLightingConstants> m_lFrameLighting;
    xarray<LightingReference>     m_lLightingReferences;

    //--------------------------------------------------------------------------
    // Shader Bind State
    //--------------------------------------------------------------------------

    geom_shader_kind                  m_activeShaderKind;
    rstate_sampler_preset             m_activeSamplerPreset;
    rstate_sampler                    m_samplers[RSTATE_SAMPLER_PRESET_COUNT];
    vram_texture                      m_whiteTexture;
    vram_texture                      m_blackTexture;
    vram_texture                      m_blackCubeTexture;
    xbool                             m_isDistortionStateActive;
    radian3                           m_distortionNormalRot;
    shader_resource const*            m_pDistortionSceneResource;
    shader_resource const*            m_pDiffuseResource;
    shader_resource const*            m_pDetailResource;
    shader_resource const*            m_pEnvironmentResource;
    shader_resource const*            m_pEnvironmentCubeResource;
    shader_resource const*            m_pCachedFaceShadowResource;
    shader_resource const*            m_pCachedFaceShadowDepthResource;
    rstate_sampler_preset             m_faceShadowSamplerPreset;
    rbuffer                           m_shadowMatrixBuffer;
    rbuffer                           m_shadowDataBuffer;
    matrix4                           m_faceShadowMatrices[MAX_SHADOW_SOURCES];
    cb_shadow_map_data                m_shadowMapData;
    u32                               m_dynamicLightShadowIndex[light_mgr::MAX_DYNAMIC_LIGHTS];
    xbool                             m_areShadowMapConstantsValid;
    xarray<geometry_draw_item const*> m_lOrderedDraws;
    xarray<GeomDrawPacket>            m_lDrawPackets;
    material                          m_defaultDistortionMaterial;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern GeomMgr g_GeomMgr;

//==============================================================================
#endif // GEOM_MANAGER_HPP
//==============================================================================
