//==============================================================================
//
//  ShadowMgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShadowMgr.hpp"

#include "GeomStorage.hpp"
#include "GeomMgr/GeomMgr.hpp"
#include "../GeometryDraw.hpp"
#include "../RigidGeom.hpp"
#include "../SkinGeom.hpp"
#include "SoftVertexMgr.hpp"
#include "VertexMgr.hpp"

#include <stdint.h>

//==============================================================================
//  FILE-LOCAL HELPERS
//==============================================================================

namespace
{
enum
{
    SHADOW_CASTER_NONE = -1,
    SHADOW_CASTER_RIGID = 0,
    SHADOW_CASTER_SKIN = 1,
    SHADOW_CASTER_DYNAMIC = 8,
};

enum class ShadowPipeline : u64
{
    RigidOpaqueSingleSided = 0,
    SkinOpaqueSingleSided,
    RigidAlphaSingleSided,
    SkinAlphaSingleSided,
    RigidOpaqueTwoSided,
    SkinOpaqueTwoSided,
    RigidAlphaTwoSided,
    SkinAlphaTwoSided,
    DynamicAlphaTwoSided,
};

enum class ShadowEVSMPipeline : u64
{
    Convert = 0,
    BlurHorizontal,
    BlurVertical,
};

static f32 const kShadowDepthBias = 2.0f;
static f32 const kShadowSlopeScaledDepthBias = 0.75f;
static f32 const kShadowDepthBiasClamp = 0.0f;
static f32 const kShadowEVSMPositiveExponent = 40.0f;
static f32 const kShadowEVSMNegativeExponent = 5.0f;
static f32 const kShadowEVSMLightBleedReduction = 0.1f;
static s32 const kMaxShadowAtlasDimension = 16384;

struct PreparedShadowDraw
{
    geometry_draw_type Type;
    xhandle            hMesh;
    matrix4 const*     pBones;
    s32                BoneCount;
    texture const*     pAlphaTexture;
    s32                SourceIndex;
    u32                RigidInstanceIndex;
    u32                SkinPaletteBase;
    s32                SkinSectionCount;
    u8                 UOffset;
    u8                 VOffset;
    xbool              bPunchThru;
    xbool              bTwoSided;

    PreparedShadowDraw( void )
        : Type( GEOMETRY_DRAW_RIGID ), hMesh(), pBones( NULL ), BoneCount( 0 ), pAlphaTexture( NULL ), SourceIndex( -1 ),
          RigidInstanceIndex( 0 ), SkinPaletteBase( 0 ), SkinSectionCount( 0 ), UOffset( 0 ), VOffset( 0 ),
          bPunchThru( FALSE ), bTwoSided( FALSE )
    {
        hMesh.Handle = HNULL;
    }
};

struct SkinPaletteRef
{
    matrix4 const* pBones;
    s32            BoneCount;
    s32            MeshHandle;
    u32            DrawIndex;
};

struct ShadowDrawPacket
{
    geometry_draw_type Type;
    xhandle            hMesh;
    texture const*     pAlphaTexture;
    s32                SourceIndex;
    u32                FirstDraw;
    u32                FirstInstance;
    u32                InstanceCount;
    u32                FirstSkinDrawInstance;
    s32                SkinSectionCount;
    u8                 UOffset;
    u8                 VOffset;
    xbool              bPunchThru;
    xbool              bTwoSided;

    ShadowDrawPacket( void )
        : Type( GEOMETRY_DRAW_RIGID ), hMesh(), pAlphaTexture( NULL ), SourceIndex( -1 ), FirstDraw( 0 ),
          FirstInstance( 0 ), InstanceCount( 0 ), FirstSkinDrawInstance( 0 ), SkinSectionCount( 0 ), UOffset( 0 ),
          VOffset( 0 ), bPunchThru( FALSE ), bTwoSided( FALSE )
    {
        hMesh.Handle = HNULL;
    }
};

struct ShadowIndirectItem
{
    u32                   PacketIndex;
    s32                   SourceIndex;
    u32                   FirstInstance;
    xbool                 bTwoSided;
    VertexMgr::mesh_range Range;
};

struct ShadowIndirectRun
{
    u32 FirstItem;
    u32 FirstCommand;
    u32 CommandCount;
    s32 SourceIndex;
    s32 VertexPool;
    s32 IndexPool;
    xbool bTwoSided;

    ShadowIndirectRun( void )
        : FirstItem( 0 ), FirstCommand( 0 ), CommandCount( 0 ), SourceIndex( -1 ), VertexPool( -1 ), IndexPool( -1 ),
          bTwoSided( FALSE )
    {
    }
};

struct PreparedDynamicShadowDraw
{
    shader_resource const* pDiffuse;
    shader_resource const* pDamageMask;
    u32                    FirstVertex;
    u32                    FirstIndex;
    u32                    VertexCount;
    u32                    IndexCount;
    u64                    ShadowSourceMask;

    PreparedDynamicShadowDraw( void )
        : pDiffuse( NULL ), pDamageMask( NULL ), FirstVertex( 0 ), FirstIndex( 0 ), VertexCount( 0 ), IndexCount( 0 ),
          ShadowSourceMask( 0 )
    {
    }
};

static s32 GetShadowCasterVariant( s32 casterShader, xbool bAlphaTest, xbool bTwoSided )
{
    ASSERT( ( casterShader == SHADOW_CASTER_RIGID ) || ( casterShader == SHADOW_CASTER_SKIN ) );
    return casterShader + ( bAlphaTest ? 2 : 0 ) + ( bTwoSided ? 4 : 0 );
}

static void ConfigureShadowRasterizer( rstate_raster_desc& rasterizer, rstate_raster_preset preset )
{
    rasterizer = rstate_GetRasterDesc( preset );
    rasterizer.DepthBias = kShadowDepthBias;
    rasterizer.DepthBiasClamp = kShadowDepthBiasClamp;
    rasterizer.SlopeScaledDepthBias = kShadowSlopeScaledDepthBias;
    rasterizer.bDepthBiasEnable = TRUE;
}

static s32 CompareShadowIndirectItems( void const* pA, void const* pB )
{
    ShadowIndirectItem const& a = *static_cast<ShadowIndirectItem const*>( pA );
    ShadowIndirectItem const& b = *static_cast<ShadowIndirectItem const*>( pB );
    if ( a.SourceIndex < b.SourceIndex )
    {
        return -1;
    }
    if ( a.SourceIndex > b.SourceIndex )
    {
        return 1;
    }
    if ( a.bTwoSided != b.bTwoSided )
    {
        return a.bTwoSided ? 1 : -1;
    }
    if ( a.Range.VertexPool < b.Range.VertexPool )
    {
        return -1;
    }
    if ( a.Range.VertexPool > b.Range.VertexPool )
    {
        return 1;
    }
    if ( a.Range.IndexPool < b.Range.IndexPool )
    {
        return -1;
    }
    if ( a.Range.IndexPool > b.Range.IndexPool )
    {
        return 1;
    }
    if ( a.PacketIndex < b.PacketIndex )
    {
        return -1;
    }
    if ( a.PacketIndex > b.PacketIndex )
    {
        return 1;
    }
    return 0;
}

static s32 CompareSkinPaletteRefs( void const* pA, void const* pB )
{
    SkinPaletteRef const& a = *static_cast<SkinPaletteRef const*>( pA );
    SkinPaletteRef const& b = *static_cast<SkinPaletteRef const*>( pB );

    if ( a.MeshHandle < b.MeshHandle )
    {
        return -1;
    }
    if ( a.MeshHandle > b.MeshHandle )
    {
        return 1;
    }

    uaddr const bonesA = reinterpret_cast<uaddr>( a.pBones );
    uaddr const bonesB = reinterpret_cast<uaddr>( b.pBones );
    if ( bonesA < bonesB )
    {
        return -1;
    }
    if ( bonesA > bonesB )
    {
        return 1;
    }
    if ( a.BoneCount < b.BoneCount )
    {
        return -1;
    }
    if ( a.BoneCount > b.BoneCount )
    {
        return 1;
    }
    return 0;
}

static xbool EnsureCasterBuffer( rbuffer& buffer, u32& capacity, u32 requiredCount, u32 elementStride,
                                 char const* pDebugName, u32 usageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ )
{
    u32 const desiredCapacity = MAX( requiredCount, 1 );
    if ( buffer && ( capacity >= desiredCapacity ) )
    {
        return TRUE;
    }

    rbuffer_Destroy( buffer );

    rbuffer_desc desc;
    desc.Size = desiredCapacity * elementStride;
    desc.Stride = elementStride;
    desc.UsageFlags = usageFlags;
    desc.pDebugName = pDebugName;

    if ( !rbuffer_Create( buffer, desc ) )
    {
        capacity = 0;
        return FALSE;
    }

    capacity = desiredCapacity;
    return TRUE;
}

static xbool EnsureInstanceIndexBuffer( rbuffer& buffer, u32& capacity, u32 requiredCount, char const* pDebugName )
{
    u32 const desiredCapacity = MAX( requiredCount, 1 );
    if ( buffer && ( capacity >= desiredCapacity ) )
    {
        return TRUE;
    }

    xarray<u32> indices;
    indices.SetCount( desiredCapacity );
    for ( u32 i = 0; i < desiredCapacity; ++i )
    {
        indices[i] = i;
    }

    rbuffer_Destroy( buffer );
    rbuffer_desc desc;
    desc.Size = desiredCapacity * sizeof( u32 );
    desc.Stride = sizeof( u32 );
    desc.UsageFlags = RBUFFER_USAGE_VERTEX;
    desc.pDebugName = pDebugName;
    if ( !rbuffer_Create( buffer, desc, indices.GetPtr() ) )
    {
        capacity = 0;
        return FALSE;
    }

    capacity = desiredCapacity;
    return TRUE;
}

static xbool IsShadowDepthTargetValid( rtarget const& target, s32 size )
{
    return rtarget_HasDepthStencil( target ) && rtarget_HasShaderResource( target ) &&
           ( target.Desc.Width == static_cast<u32>( size ) ) && ( target.Desc.Height == static_cast<u32>( size ) ) &&
           ( target.Desc.Format == RTARGET_FORMAT_DEPTH32F );
}

static xbool IsShadowMomentTargetValid( rtarget const& target, s32 size )
{
    return rtarget_HasRenderTarget( target ) && rtarget_HasShaderResource( target ) &&
           ( target.Desc.Width == static_cast<u32>( size ) ) && ( target.Desc.Height == static_cast<u32>( size ) ) &&
           ( target.Desc.Format == RTARGET_FORMAT_RGBA32F );
}

static xbool CreateShadowTarget( rtarget& target, s32 size, rtarget_format format, xbool bBindAsTexture,
                                 char const* pDebugName )
{
    rtarget_EndPass();
    rtarget_Destroy( target );

    rtarget_desc desc;
    desc.Width = static_cast<u32>( size );
    desc.Height = static_cast<u32>( size );
    desc.Format = format;
    desc.SampleCount = 1;
    desc.SampleQuality = 0;
    desc.bBindAsTexture = bBindAsTexture;
    desc.pDebugName = pDebugName;

    return rtarget_Create( target, desc );
}

static void ReleaseShadowTarget( rtarget& target )
{
    rtarget_EndPass();
    rtarget_Destroy( target );
    target = rtarget();
}

static texture const* GetShadowAlphaTexture( material const* pMaterial, xbool& bPunchThru )
{
    bPunchThru = FALSE;
    if ( !pMaterial )
    {
        return NULL;
    }

    bPunchThru = pMaterial->IsPunchThrough();
    if ( !pMaterial->UsesShadowAlpha() )
    {
        return NULL;
    }

    texture const* pDiffuse = pMaterial->m_diffuseMap.GetPointer();
    if ( !pDiffuse || !pDiffuse->GetShaderResource() )
    {
        return NULL;
    }

    return pDiffuse;
}

static rtarget_depth_attachment_desc ShadowDepthAttachment( rtarget const* pTarget, rtarget_load_op loadOp )
{
    rtarget_depth_attachment_desc depth;
    depth.pTarget = pTarget;
    depth.DepthLoadOp = loadOp;
    depth.DepthStoreOp = RTARGET_STORE_STORE;
    depth.StencilLoadOp = RTARGET_LOAD_DONT_CARE;
    depth.StencilStoreOp = RTARGET_STORE_DONT_CARE;
    depth.ClearDepth = 1.0f;
    depth.ClearStencil = 0;
    return depth;
}

static rtarget_color_attachment_desc ShadowColorAttachment( rtarget const* pTarget, rtarget_load_op loadOp )
{
    rtarget_color_attachment_desc color;
    color.pTarget = pTarget;
    color.LoadOp = loadOp;
    color.StoreOp = RTARGET_STORE_STORE;
    color.ClearColor[0] = 0.0f;
    color.ClearColor[1] = 0.0f;
    color.ClearColor[2] = 0.0f;
    color.ClearColor[3] = 0.0f;
    return color;
}

} // namespace

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

shadow_mgr g_ShadowMgr;

//==============================================================================
//  MANAGER LIFETIME
//==============================================================================

shadow_mgr::shadow_mgr( void )
    : m_isInitialized( FALSE ), m_isCastPassActive( FALSE ), m_currentSource( -1 ),
      m_currentCasterVariant( SHADOW_CASTER_NONE ), m_shadowAtlasSize( 0 ), m_shadowMomentAtlasSize( 0 ),
      m_isShadowSampleAtlasReady( FALSE ), m_lastCasterStats(), m_shadowAtlas(), m_shadowMomentAtlas(),
      m_shadowMomentTempAtlas(), m_rigidVertexShader(), m_skinVertexShader(), m_dynamicVertexShader(),
      m_dynamicPixelShader(), m_opaqueDepthPixelShader(),
      m_alphaDepthPixelShader(), m_evsmVertexShader(), m_evsmConvertPixelShader(),
      m_evsmBlurHorizontalPixelShader(), m_evsmBlurVerticalPixelShader(), m_casterPipelines(), m_evsmPipelines(),
      m_diffuseSampler(), m_evsmPointSampler(), m_evsmLinearSampler(), m_shadowSingleSidedRasterizerDesc(),
      m_shadowTwoSidedRasterizerDesc(),
      m_rigidInstanceBuffer(), m_rigidInstanceIndexBuffer(), m_rigidIndirectBuffer(), m_skinBoneBuffer(),
      m_skinBoneBaseBuffer(), m_skinIndirectBuffer(), m_dynamicVertexBuffer(), m_dynamicIndexBuffer(),
      m_rigidInstanceCapacity( 0 ), m_rigidInstanceIndexCapacity( 0 ),
      m_rigidIndirectCapacity( 0 ), m_skinBoneCapacity( 0 ), m_skinBoneBaseCapacity( 0 ),
      m_skinIndirectCapacity( 0 ), m_dynamicVertexCapacity( 0 ), m_dynamicIndexCapacity( 0 ),
      m_rigidCastConstantsSlot( 0 ), m_rigidInstanceSlot( 0 ), m_skinCastConstantsSlot( 0 ), m_skinBoneSlot( 0 ),
      m_dynamicCastConstantsSlot( 0 ), m_dynamicDiffuseTextureSlot( 0 ), m_dynamicDamageTextureSlot( 0 ),
      m_alphaConstantsSlot( 0 ), m_alphaTextureSlot( 0 ),
      m_evsmConvertConstantsSlot( 0 ), m_evsmConvertTextureSlot( 0 ), m_evsmBlurHorizontalConstantsSlot( 0 ),
      m_evsmBlurHorizontalTextureSlot( 0 ), m_evsmBlurVerticalConstantsSlot( 0 ),
      m_evsmBlurVerticalTextureSlot( 0 ), m_shadowNormalBiasTexels( 0.75f ), m_shadowSeamBlendTexels( 4.0f )
{
}

//==============================================================================

shadow_mgr::~shadow_mgr( void )
{
    Kill();
}

//==============================================================================

void shadow_mgr::Init( void )
{
    if ( m_isInitialized )
    {
        return;
    }

    static shader_vertex_element rigidLayout[] = { shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, 0 ),
                                                   shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT3, 12 ),
                                                   shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, 24 ),
                                                   shader_vertex_element( 3, 1, SHADER_VERTEX_FORMAT_UINT1, 0 ) };

    static shader_vertex_element skinLayout[] = { shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT4, 0 ),
                                                  shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT4, 16 ),
                                                  shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT4, 32 ),
                                                  shader_vertex_element( 3, 1, SHADER_VERTEX_FORMAT_UINT1, 0 ) };

    static shader_vertex_buffer_desc rigidVertexBuffers[2];
    rigidVertexBuffers[0].Slot = 0;
    rigidVertexBuffers[0].Stride = sizeof( rigid_geom::vertex );
    rigidVertexBuffers[1].Slot = 1;
    rigidVertexBuffers[1].Stride = sizeof( u32 );
    rigidVertexBuffers[1].bPerInstance = TRUE;

    static shader_vertex_buffer_desc skinVertexBuffers[2];
    skinVertexBuffers[0].Slot = 0;
    skinVertexBuffers[0].Stride = sizeof( SkinGpuVertex );
    skinVertexBuffers[1].Slot = 1;
    skinVertexBuffers[1].Stride = sizeof( u32 );
    skinVertexBuffers[1].bPerInstance = TRUE;

    static shader_vertex_element dynamicLayout[] = {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( dynamic_geometry_vertex, Position ) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT2, offsetof( dynamic_geometry_vertex, UV ) ) };

    static shader_vertex_buffer_desc dynamicVertexBuffer;
    dynamicVertexBuffer.Slot = 0;
    dynamicVertexBuffer.Stride = sizeof( dynamic_geometry_vertex );

    shader_LoadFromEcs( m_rigidVertexShader, "shadow_cast_rigid_vs.vs.ecs" );
    shader_LoadFromEcs( m_skinVertexShader, "shadow_cast_skin_vs.vs.ecs" );
    shader_LoadFromEcs( m_dynamicVertexShader, "shadow_cast_dynamic_vs.vs.ecs" );
    shader_LoadFromEcs( m_dynamicPixelShader, "shadow_cast_dynamic_ps.ps.ecs" );
    shader_LoadFromEcs( m_opaqueDepthPixelShader, "shadow_depth_cast_opaque_ps.ps.ecs" );
    shader_LoadFromEcs( m_alphaDepthPixelShader, "shadow_depth_cast_alpha_ps.ps.ecs" );
    shader_LoadFromEcs( m_evsmVertexShader, "shadow_evsm_vs.vs.ecs" );
    shader_LoadFromEcs( m_evsmConvertPixelShader, "shadow_evsm_convert_ps.ps.ecs" );
    shader_LoadFromEcs( m_evsmBlurHorizontalPixelShader, "shadow_evsm_blur_horizontal_ps.ps.ecs" );
    shader_LoadFromEcs( m_evsmBlurVerticalPixelShader, "shadow_evsm_blur_vertical_ps.ps.ecs" );
    rstate_CreateSampler( m_diffuseSampler, RSTATE_SAMPLER_PRESET_ANISOTROPIC_WRAP, "ShadowDiffuse" );
    rstate_CreateSampler( m_evsmPointSampler, RSTATE_SAMPLER_PRESET_POINT_CLAMP, "ShadowEVSMDepth" );
    rstate_CreateSampler( m_evsmLinearSampler, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "ShadowEVSMMoments" );

    ConfigureShadowRasterizer( m_shadowSingleSidedRasterizerDesc, RSTATE_RASTER_PRESET_SOLID );
    ConfigureShadowRasterizer( m_shadowTwoSidedRasterizerDesc, RSTATE_RASTER_PRESET_SOLID_NO_CULL );

    xbool const bHasRigidCaster = m_rigidVertexShader;
    xbool const bHasSkinCaster = m_skinVertexShader;
    xbool const bHasDynamicCaster = m_dynamicVertexShader && m_dynamicPixelShader;

    if ( !bHasRigidCaster )
    {
        shader_Destroy( m_rigidVertexShader );
    }

    if ( !bHasSkinCaster )
    {
        shader_Destroy( m_skinVertexShader );
    }

    if ( !bHasDynamicCaster )
    {
        shader_Destroy( m_dynamicVertexShader );
        shader_Destroy( m_dynamicPixelShader );
    }

    if ( !m_opaqueDepthPixelShader || !m_alphaDepthPixelShader || !m_evsmVertexShader ||
         !m_evsmConvertPixelShader || !m_evsmBlurHorizontalPixelShader || !m_evsmBlurVerticalPixelShader ||
         !m_diffuseSampler || !m_evsmPointSampler || !m_evsmLinearSampler ||
         !( bHasRigidCaster || bHasSkinCaster || bHasDynamicCaster ) )
    {
        Kill();
        return;
    }

    if ( ( bHasRigidCaster &&
           ( !shader_FindUniformSlot( m_rigidVertexShader, "cbShadowCast", m_rigidCastConstantsSlot ) ||
             !shader_FindStorageBufferSlot( m_rigidVertexShader, "ShadowRigidWorld", m_rigidInstanceSlot ) ) ) ||
         ( bHasSkinCaster &&
           ( !shader_FindUniformSlot( m_skinVertexShader, "cbShadowCast", m_skinCastConstantsSlot ) ||
             !shader_FindStorageBufferSlot( m_skinVertexShader, "ShadowSkinBones", m_skinBoneSlot ) ) ) ||
         ( bHasDynamicCaster &&
           ( !shader_FindUniformSlot( m_dynamicVertexShader, "cbShadowCast", m_dynamicCastConstantsSlot ) ||
             !shader_FindSampledTextureSlot( m_dynamicPixelShader, "txDiffuse", m_dynamicDiffuseTextureSlot ) ||
             !shader_FindSampledTextureSlot( m_dynamicPixelShader, "txDamage", m_dynamicDamageTextureSlot ) ) ) ||
         !shader_FindUniformSlot( m_alphaDepthPixelShader, "cbShadowAlpha", m_alphaConstantsSlot ) ||
         !shader_FindSampledTextureSlot( m_alphaDepthPixelShader, "txDiffuse", m_alphaTextureSlot ) ||
         !shader_FindUniformSlot( m_evsmConvertPixelShader, "cbShadowFilter", m_evsmConvertConstantsSlot ) ||
         !shader_FindSampledTextureSlot( m_evsmConvertPixelShader, "txShadowDepth", m_evsmConvertTextureSlot ) ||
         !shader_FindUniformSlot( m_evsmBlurHorizontalPixelShader, "cbShadowFilter",
                                  m_evsmBlurHorizontalConstantsSlot ) ||
         !shader_FindSampledTextureSlot( m_evsmBlurHorizontalPixelShader, "txShadowMoments",
                                         m_evsmBlurHorizontalTextureSlot ) ||
         !shader_FindUniformSlot( m_evsmBlurVerticalPixelShader, "cbShadowFilter",
                                  m_evsmBlurVerticalConstantsSlot ) ||
         !shader_FindSampledTextureSlot( m_evsmBlurVerticalPixelShader, "txShadowMoments",
                                         m_evsmBlurVerticalTextureSlot ) )
    {
        x_DebugMsg( "ShadowMgr: failed to resolve shadow shader bindings\n" );
        Kill();
        return;
    }

    if ( bHasRigidCaster )
    {
        render_pipeline_desc desc;
        desc.Shader.pVertexShader     = &m_rigidVertexShader;
        desc.Shader.pPixelShader      = &m_opaqueDepthPixelShader;
        desc.Shader.pVertexBuffers    = rigidVertexBuffers;
        desc.Shader.VertexBufferCount = ARRAYSIZE( rigidVertexBuffers );
        desc.Shader.pInputElements    = rigidLayout;
        desc.Shader.InputElementCount = ARRAYSIZE( rigidLayout );
        desc.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
        desc.Depth                    = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_NORMAL );
        desc.ColorCount               = 0;
        desc.DepthFormat              = RTARGET_FORMAT_DEPTH32F;

        for ( s32 sidedness = 0; sidedness < 2; ++sidedness )
        {
            xbool const bTwoSided = sidedness != 0;
            desc.Raster =
                bTwoSided ? m_shadowTwoSidedRasterizerDesc : m_shadowSingleSidedRasterizerDesc;
            desc.Shader.pPixelShader = &m_opaqueDepthPixelShader;
            desc.pDebugName =
                bTwoSided ? "ShadowRigidOpaqueTwoSidedCaster" : "ShadowRigidOpaqueSingleSidedCaster";

            ShadowPipeline const opaquePipeline = bTwoSided ? ShadowPipeline::RigidOpaqueTwoSided
                                                            : ShadowPipeline::RigidOpaqueSingleSided;
            if ( !m_casterPipelines.Prewarm( static_cast<u64>( opaquePipeline ), desc ) )
            {
                x_DebugMsg( "ShadowMgr: failed to prewarm rigid opaque %s-sided caster pipeline\n",
                            bTwoSided ? "two" : "single" );
                Kill();
                return;
            }

            desc.Shader.pPixelShader = &m_alphaDepthPixelShader;
            desc.pDebugName =
                bTwoSided ? "ShadowRigidAlphaTwoSidedCaster" : "ShadowRigidAlphaSingleSidedCaster";
            ShadowPipeline const alphaPipeline =
                bTwoSided ? ShadowPipeline::RigidAlphaTwoSided : ShadowPipeline::RigidAlphaSingleSided;
            if ( !m_casterPipelines.Prewarm( static_cast<u64>( alphaPipeline ), desc ) )
            {
                x_DebugMsg( "ShadowMgr: failed to prewarm rigid alpha %s-sided caster pipeline\n",
                            bTwoSided ? "two" : "single" );
                Kill();
                return;
            }
        }
    }

    if ( bHasSkinCaster )
    {
        render_pipeline_desc desc;
        desc.Shader.pVertexShader     = &m_skinVertexShader;
        desc.Shader.pPixelShader      = &m_opaqueDepthPixelShader;
        desc.Shader.pVertexBuffers    = skinVertexBuffers;
        desc.Shader.VertexBufferCount = ARRAYSIZE( skinVertexBuffers );
        desc.Shader.pInputElements    = skinLayout;
        desc.Shader.InputElementCount = ARRAYSIZE( skinLayout );
        desc.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
        desc.Depth                    = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_NORMAL );
        desc.ColorCount               = 0;
        desc.DepthFormat              = RTARGET_FORMAT_DEPTH32F;

        for ( s32 sidedness = 0; sidedness < 2; ++sidedness )
        {
            xbool const bTwoSided = sidedness != 0;
            desc.Raster =
                bTwoSided ? m_shadowTwoSidedRasterizerDesc : m_shadowSingleSidedRasterizerDesc;
            desc.Shader.pPixelShader = &m_opaqueDepthPixelShader;
            desc.pDebugName =
                bTwoSided ? "ShadowSkinOpaqueTwoSidedCaster" : "ShadowSkinOpaqueSingleSidedCaster";

            ShadowPipeline const opaquePipeline =
                bTwoSided ? ShadowPipeline::SkinOpaqueTwoSided : ShadowPipeline::SkinOpaqueSingleSided;
            if ( !m_casterPipelines.Prewarm( static_cast<u64>( opaquePipeline ), desc ) )
            {
                x_DebugMsg( "ShadowMgr: failed to prewarm skin opaque %s-sided caster pipeline\n",
                            bTwoSided ? "two" : "single" );
                Kill();
                return;
            }

            desc.Shader.pPixelShader = &m_alphaDepthPixelShader;
            desc.pDebugName =
                bTwoSided ? "ShadowSkinAlphaTwoSidedCaster" : "ShadowSkinAlphaSingleSidedCaster";
            ShadowPipeline const alphaPipeline =
                bTwoSided ? ShadowPipeline::SkinAlphaTwoSided : ShadowPipeline::SkinAlphaSingleSided;
            if ( !m_casterPipelines.Prewarm( static_cast<u64>( alphaPipeline ), desc ) )
            {
                x_DebugMsg( "ShadowMgr: failed to prewarm skin alpha %s-sided caster pipeline\n",
                            bTwoSided ? "two" : "single" );
                Kill();
                return;
            }
        }
    }

    if ( bHasDynamicCaster )
    {
        render_pipeline_desc desc;
        desc.Shader.pVertexShader = &m_dynamicVertexShader;
        desc.Shader.pPixelShader = &m_dynamicPixelShader;
        desc.Shader.pVertexBuffers = &dynamicVertexBuffer;
        desc.Shader.VertexBufferCount = 1;
        desc.Shader.pInputElements = dynamicLayout;
        desc.Shader.InputElementCount = ARRAYSIZE( dynamicLayout );
        desc.Shader.Topology = SHADER_TOPOLOGY_TRIANGLE_LIST;
        desc.Depth = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_NORMAL );
        desc.Raster = m_shadowTwoSidedRasterizerDesc;
        desc.ColorCount = 0;
        desc.DepthFormat = RTARGET_FORMAT_DEPTH32F;
        desc.pDebugName = "ShadowDynamicAlphaTwoSidedCaster";
        if ( !m_casterPipelines.Prewarm( static_cast<u64>( ShadowPipeline::DynamicAlphaTwoSided ), desc ) )
        {
            x_DebugMsg( "ShadowMgr: failed to prewarm dynamic alpha two-sided caster pipeline\n" );
            Kill();
            return;
        }
    }

    {
        shader const* pixelShaders[] = {
            &m_evsmConvertPixelShader,
            &m_evsmBlurHorizontalPixelShader,
            &m_evsmBlurVerticalPixelShader,
        };
        char const* debugNames[] = {
            "ShadowEVSMConvert",
            "ShadowEVSMBlurHorizontal",
            "ShadowEVSMBlurVertical",
        };

        for ( u32 i = 0; i < ARRAYSIZE( pixelShaders ); ++i )
        {
            render_pipeline_desc desc;
            desc.Shader.pVertexShader = &m_evsmVertexShader;
            desc.Shader.pPixelShader = pixelShaders[i];
            desc.Shader.Topology = SHADER_TOPOLOGY_TRIANGLE_LIST;
            desc.Raster = rstate_GetRasterDesc( RSTATE_RASTER_PRESET_SOLID_NO_CULL );
            desc.Depth = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE );
            desc.ColorTargets[0].Format = RTARGET_FORMAT_RGBA32F;
            desc.ColorTargets[0].Blend = rstate_GetBlendDesc( RSTATE_BLEND_PRESET_NONE );
            desc.ColorCount = 1;
            desc.DepthFormat = RTARGET_FORMAT_COUNT;
            desc.pDebugName = debugNames[i];

            ShadowEVSMPipeline const pipeline = static_cast<ShadowEVSMPipeline>( i );
            if ( !m_evsmPipelines.Prewarm( static_cast<u64>( pipeline ), desc ) )
            {
                x_DebugMsg( "ShadowMgr: failed to prewarm %s pipeline\n", debugNames[i] );
                Kill();
                return;
            }
        }
    }

    m_isInitialized = TRUE;
}

//==============================================================================

void shadow_mgr::Kill( void )
{
    if ( m_isCastPassActive )
    {
        rtarget_EndPass();
        m_isCastPassActive = FALSE;
    }

    m_casterPipelines.Reset();
    m_evsmPipelines.Reset();

    rbuffer_Destroy( m_rigidInstanceBuffer );
    rbuffer_Destroy( m_rigidInstanceIndexBuffer );
    rbuffer_Destroy( m_rigidIndirectBuffer );
    rbuffer_Destroy( m_skinBoneBuffer );
    rbuffer_Destroy( m_skinBoneBaseBuffer );
    rbuffer_Destroy( m_skinIndirectBuffer );
    rbuffer_Destroy( m_dynamicVertexBuffer );
    rbuffer_Destroy( m_dynamicIndexBuffer );

    rstate_DestroySampler( m_diffuseSampler );
    rstate_DestroySampler( m_evsmPointSampler );
    rstate_DestroySampler( m_evsmLinearSampler );
    shader_Destroy( m_evsmBlurVerticalPixelShader );
    shader_Destroy( m_evsmBlurHorizontalPixelShader );
    shader_Destroy( m_evsmConvertPixelShader );
    shader_Destroy( m_evsmVertexShader );
    shader_Destroy( m_alphaDepthPixelShader );
    shader_Destroy( m_opaqueDepthPixelShader );
    shader_Destroy( m_rigidVertexShader );
    shader_Destroy( m_skinVertexShader );
    shader_Destroy( m_dynamicVertexShader );
    shader_Destroy( m_dynamicPixelShader );

    ReleaseShadowTarget( m_shadowAtlas );
    ReleaseShadowTarget( m_shadowMomentAtlas );
    ReleaseShadowTarget( m_shadowMomentTempAtlas );

    m_isCastPassActive = FALSE;
    m_currentSource = -1;
    m_currentCasterVariant = SHADOW_CASTER_NONE;
    m_shadowAtlasSize = 0;
    m_shadowMomentAtlasSize = 0;
    m_isShadowSampleAtlasReady = FALSE;
    m_rigidInstanceCapacity = 0;
    m_rigidInstanceIndexCapacity = 0;
    m_rigidIndirectCapacity = 0;
    m_skinBoneCapacity = 0;
    m_skinBoneBaseCapacity = 0;
    m_skinIndirectCapacity = 0;
    m_dynamicVertexCapacity = 0;
    m_dynamicIndexCapacity = 0;
    m_isInitialized = FALSE;
}

//==============================================================================
//  SOURCE MANAGEMENT
//==============================================================================

void shadow_mgr::EnsureAtlas( void )
{
    s32 shadowAtlasSize = g_ShadowMapMgr.GetAtlasSize();
    if ( shadowAtlasSize <= 0 )
    {
        shadowAtlasSize = MAX_SHADOW_ATLAS_SIZE;
    }

    ASSERT( shadowAtlasSize > 0 );
    ASSERT( ( shadowAtlasSize & ( shadowAtlasSize - 1 ) ) == 0 );
    ASSERT( shadowAtlasSize <= kMaxShadowAtlasDimension );
    if ( shadowAtlasSize <= 0 )
    {
        x_DebugMsg( "ShadowMgr: invalid shadow atlas size %d\n", shadowAtlasSize );
        return;
    }
    if ( ( shadowAtlasSize & ( shadowAtlasSize - 1 ) ) != 0 )
    {
        x_DebugMsg( "ShadowMgr: shadow atlas size %d is not a power of two\n", shadowAtlasSize );
        return;
    }
    if ( shadowAtlasSize > kMaxShadowAtlasDimension )
    {
        x_DebugMsg( "ShadowMgr: shadow atlas size %d exceeds max %d\n", shadowAtlasSize, kMaxShadowAtlasDimension );
        return;
    }

    xbool const useEVSM = g_ShadowMapMgr.GetShadowFilterType() == ShadowFilterType::Evsm;
    xbool const hasValidDepthAtlas =
        IsShadowDepthTargetValid( m_shadowAtlas, shadowAtlasSize ) && ( m_shadowAtlasSize == shadowAtlasSize );

    if( !useEVSM )
    {
        if( !hasValidDepthAtlas )
        {
            ReleaseShadowTarget( m_shadowAtlas );
            m_shadowAtlasSize = 0;
            if( !CreateShadowTarget( m_shadowAtlas, shadowAtlasSize, RTARGET_FORMAT_DEPTH32F, TRUE,
                                     "ShadowDepthAtlas" ) )
            {
                x_DebugMsg( "ShadowMgr: failed to create shadow depth atlas\n" );
                return;
            }
            m_shadowAtlasSize = shadowAtlasSize;
        }

        if( m_shadowMomentAtlasSize > 0 )
        {
            ReleaseShadowTarget( m_shadowMomentAtlas );
            ReleaseShadowTarget( m_shadowMomentTempAtlas );
            m_shadowMomentAtlasSize = 0;
        }
        m_isShadowSampleAtlasReady = FALSE;
        return;
    }

    s32 const shadowMomentAtlasSize =
        MIN( MAX( shadowAtlasSize / 2, 1 ), static_cast<s32>( MAX_SHADOW_MOMENT_ATLAS_SIZE ) );
    if( hasValidDepthAtlas &&
        IsShadowMomentTargetValid( m_shadowMomentAtlas, shadowMomentAtlasSize ) &&
        IsShadowMomentTargetValid( m_shadowMomentTempAtlas, shadowMomentAtlasSize ) &&
        ( m_shadowMomentAtlasSize == shadowMomentAtlasSize ) )
    {
        return;
    }

    ReleaseShadowTarget( m_shadowAtlas );
    ReleaseShadowTarget( m_shadowMomentAtlas );
    ReleaseShadowTarget( m_shadowMomentTempAtlas );
    m_shadowAtlasSize = 0;
    m_shadowMomentAtlasSize = 0;
    m_isShadowSampleAtlasReady = FALSE;

    if ( !CreateShadowTarget( m_shadowAtlas, shadowAtlasSize, RTARGET_FORMAT_DEPTH32F, TRUE, "ShadowDepthAtlas" ) )
    {
        x_DebugMsg( "ShadowMgr: failed to create shadow depth atlas\n" );
        return;
    }

    if ( !CreateShadowTarget( m_shadowMomentAtlas, shadowMomentAtlasSize, RTARGET_FORMAT_RGBA32F, TRUE,
                              "ShadowEVSMMomentAtlas" ) ||
         !CreateShadowTarget( m_shadowMomentTempAtlas, shadowMomentAtlasSize, RTARGET_FORMAT_RGBA32F, TRUE,
                              "ShadowEVSMMomentTempAtlas" ) )
    {
        x_DebugMsg( "ShadowMgr: failed to create EVSM moment atlases\n" );
        ReleaseShadowTarget( m_shadowAtlas );
        ReleaseShadowTarget( m_shadowMomentAtlas );
        ReleaseShadowTarget( m_shadowMomentTempAtlas );
        return;
    }

    m_shadowAtlasSize = shadowAtlasSize;
    m_shadowMomentAtlasSize = shadowMomentAtlasSize;
}

//==============================================================================

//==============================================================================

void shadow_mgr::BeginShadowShaders( void )
{
    m_isShadowSampleAtlasReady = FALSE;

    if ( !m_isInitialized || !g_ShadowMapMgr.HasActiveSources() )
    {
        return;
    }

    if ( g_ShadowMapMgr.GetAtlasSourceCount() > 0 )
    {
        EnsureAtlas();
        xbool const useEVSM = g_ShadowMapMgr.GetShadowFilterType() == ShadowFilterType::Evsm;
        if( !rtarget_HasDepthStencil( m_shadowAtlas ) || !rtarget_HasShaderResource( m_shadowAtlas ) ||
            ( useEVSM &&
              ( !IsShadowMomentTargetValid( m_shadowMomentAtlas, m_shadowMomentAtlasSize ) ||
                !IsShadowMomentTargetValid( m_shadowMomentTempAtlas, m_shadowMomentAtlasSize ) ) ) )
        {
            return;
        }
    }
}

//==============================================================================

void shadow_mgr::BeginCastPass( void )
{
    if ( m_isCastPassActive )
    {
        rtarget_EndPass();
    }

    m_isCastPassActive = FALSE;
    m_isShadowSampleAtlasReady = FALSE;
    m_currentSource = -1;
    m_currentCasterVariant = SHADOW_CASTER_NONE;

    if ( !m_isInitialized || !g_ShadowMapMgr.HasActiveSources() || !m_opaqueDepthPixelShader ||
         !m_alphaDepthPixelShader || !m_diffuseSampler )
    {
        return;
    }

    if ( g_ShadowMapMgr.GetAtlasSourceCount() > 0 )
    {
        EnsureAtlas();
        xbool const useEVSM = g_ShadowMapMgr.GetShadowFilterType() == ShadowFilterType::Evsm;
        if( !rtarget_HasDepthStencil( m_shadowAtlas ) || !rtarget_HasShaderResource( m_shadowAtlas ) ||
            ( useEVSM &&
              ( !IsShadowMomentTargetValid( m_shadowMomentAtlas, m_shadowMomentAtlasSize ) ||
                !IsShadowMomentTargetValid( m_shadowMomentTempAtlas, m_shadowMomentAtlasSize ) ) ) )
        {
            return;
        }

        rtarget_depth_attachment_desc depth = ShadowDepthAttachment( &m_shadowAtlas, RTARGET_LOAD_CLEAR );

        rtarget_EndPass();
        if ( !rtarget_BeginPass( NULL, 0, &depth ) )
        {
            return;
        }

        m_isCastPassActive = TRUE;
    }
}

//==============================================================================

void shadow_mgr::EndCastPass( void )
{
    m_currentSource = -1;
    m_currentCasterVariant = SHADOW_CASTER_NONE;

    if ( m_isCastPassActive )
    {
        rtarget_EndPass();
        m_isCastPassActive = FALSE;
        if( g_ShadowMapMgr.GetShadowFilterType() == ShadowFilterType::Evsm )
        {
            m_isShadowSampleAtlasReady = FilterShadowAtlas();
        }
        else
        {
            m_isShadowSampleAtlasReady =
                IsShadowDepthTargetValid( m_shadowAtlas, m_shadowAtlasSize );
        }
    }
}

//==============================================================================

xbool shadow_mgr::FilterShadowAtlas( void )
{
    if ( ( m_shadowAtlasSize <= 0 ) || ( m_shadowMomentAtlasSize <= 0 ) ||
         !IsShadowDepthTargetValid( m_shadowAtlas, m_shadowAtlasSize ) ||
         !IsShadowMomentTargetValid( m_shadowMomentAtlas, m_shadowMomentAtlasSize ) ||
         !IsShadowMomentTargetValid( m_shadowMomentTempAtlas, m_shadowMomentAtlasSize ) )
    {
        return FALSE;
    }

    render_pipeline* pConvertPipeline =
        m_evsmPipelines.Find( static_cast<u64>( ShadowEVSMPipeline::Convert ) );
    shader_resource const* pDepthResource = rtarget_GetShaderResource( m_shadowAtlas );
    if ( !pConvertPipeline || !pDepthResource )
    {
        return FALSE;
    }

    rtarget_color_attachment_desc color = ShadowColorAttachment( &m_shadowMomentAtlas, RTARGET_LOAD_CLEAR );
    rtarget_EndPass();
    if ( !rtarget_BeginPass( &color, 1, NULL ) || !render_BindPipeline( *pConvertPipeline ) ||
         !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_evsmConvertTextureSlot, pDepthResource,
                                                       &m_evsmPointSampler ) ) )
    {
        rtarget_EndPass();
        return FALSE;
    }

    f32 const depthTexelSize = 1.0f / static_cast<f32>( m_shadowAtlasSize );
    f32 const momentTexelSize = 1.0f / static_cast<f32>( m_shadowMomentAtlasSize );
    s32 const nSources = g_ShadowMapMgr.GetSourceCount();

    for ( s32 i = 0; i < nSources; ++i )
    {
        ShadowMapMgr::ShadowSource const& source = g_ShadowMapMgr.GetSource( i );
        s32 const targetX = source.AtlasTileX * m_shadowMomentAtlasSize / m_shadowAtlasSize;
        s32 const targetY = source.AtlasTileY * m_shadowMomentAtlasSize / m_shadowAtlasSize;
        s32 const targetWidth = source.AtlasTileWidth * m_shadowMomentAtlasSize / m_shadowAtlasSize;
        s32 const targetHeight = source.AtlasTileHeight * m_shadowMomentAtlasSize / m_shadowAtlasSize;
        if ( ( targetWidth <= 0 ) || ( targetHeight <= 0 ) )
        {
            continue;
        }

        cb_shadow_filter constants;
        constants.TextureParams.Set( depthTexelSize, depthTexelSize, momentTexelSize, momentTexelSize );
        constants.SourceClampRect.Set(
            ( static_cast<f32>( source.AtlasX ) + 0.5f ) * depthTexelSize,
            ( static_cast<f32>( source.AtlasY ) + 0.5f ) * depthTexelSize,
            ( static_cast<f32>( source.AtlasX + source.AtlasWidth ) - 0.5f ) * depthTexelSize,
            ( static_cast<f32>( source.AtlasY + source.AtlasHeight ) - 0.5f ) * depthTexelSize );
        constants.FilterParams.Set( kShadowEVSMPositiveExponent, kShadowEVSMNegativeExponent,
                                    SHADOW_EVSM_BLUR_SCALE, 0.0f );
        xbool const isPointFace = source.Type == ShadowMapMgr::SHADOW_SOURCE_POINT_FACE;
        constants.DepthParams.Set( source.NearZ, source.FarZ, source.LightPosRadius.GetW(),
                                   isPointFace ? 1.0f : 0.0f );
        constants.SourceProjectionParams.Set(
            ( static_cast<f32>( source.AtlasX ) + static_cast<f32>( source.AtlasWidth ) * 0.5f ) *
                depthTexelSize,
            ( static_cast<f32>( source.AtlasY ) + static_cast<f32>( source.AtlasHeight ) * 0.5f ) *
                depthTexelSize,
            2.0f / MAX( static_cast<f32>( source.AtlasWidth ) * depthTexelSize, 1.0e-8f ),
            source.FaceLightData.GetX() );

        rdraw_viewport viewport;
        viewport.TopLeftX = static_cast<f32>( targetX );
        viewport.TopLeftY = static_cast<f32>( targetY );
        viewport.Width = static_cast<f32>( targetWidth );
        viewport.Height = static_cast<f32>( targetHeight );

        rdraw_scissor scissor;
        scissor.X = targetX;
        scissor.Y = targetY;
        scissor.Width = targetWidth;
        scissor.Height = targetHeight;

        if ( !shader_PushUniformData( SHADER_STAGE_PIXEL, m_evsmConvertConstantsSlot, &constants,
                                      sizeof( constants ) ) ||
             !rdraw_SetViewport( viewport ) || !rdraw_SetScissor( scissor ) || !rdraw_Draw( 3 ) )
        {
            rtarget_EndPass();
            return FALSE;
        }
    }

    rtarget_EndPass();
    return RunEVSMBlurPass( m_shadowMomentAtlas, m_shadowMomentTempAtlas, TRUE ) &&
           RunEVSMBlurPass( m_shadowMomentTempAtlas, m_shadowMomentAtlas, FALSE );
}

//==============================================================================

xbool shadow_mgr::RunEVSMBlurPass( rtarget const& source, rtarget const& destination, xbool horizontal )
{
    ShadowEVSMPipeline const pipeline =
        horizontal ? ShadowEVSMPipeline::BlurHorizontal : ShadowEVSMPipeline::BlurVertical;
    render_pipeline* pPipeline = m_evsmPipelines.Find( static_cast<u64>( pipeline ) );
    shader_resource const* pSourceResource = rtarget_GetShaderResource( source );
    u32 const constantsSlot =
        horizontal ? m_evsmBlurHorizontalConstantsSlot : m_evsmBlurVerticalConstantsSlot;
    u32 const textureSlot = horizontal ? m_evsmBlurHorizontalTextureSlot : m_evsmBlurVerticalTextureSlot;
    if ( !pPipeline || !pSourceResource || !IsShadowMomentTargetValid( destination, m_shadowMomentAtlasSize ) )
    {
        return FALSE;
    }

    rtarget_color_attachment_desc color = ShadowColorAttachment( &destination, RTARGET_LOAD_DONT_CARE );
    rtarget_EndPass();
    if ( !rtarget_BeginPass( &color, 1, NULL ) || !render_BindPipeline( *pPipeline ) ||
         !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, textureSlot, pSourceResource,
                                                       &m_evsmLinearSampler ) ) )
    {
        rtarget_EndPass();
        return FALSE;
    }

    f32 const momentTexelSize = 1.0f / static_cast<f32>( m_shadowMomentAtlasSize );
    cb_shadow_filter constants;
    constants.TextureParams.Set( momentTexelSize, momentTexelSize, momentTexelSize, momentTexelSize );
    constants.SourceClampRect.Set( 0.0f, 0.0f, 1.0f, 1.0f );
    constants.FilterParams.Set( kShadowEVSMPositiveExponent, kShadowEVSMNegativeExponent,
                                SHADOW_EVSM_BLUR_SCALE, 0.0f );
    constants.DepthParams.Zero();
    constants.SourceProjectionParams.Zero();

    rdraw_viewport viewport;
    viewport.Width = static_cast<f32>( m_shadowMomentAtlasSize );
    viewport.Height = static_cast<f32>( m_shadowMomentAtlasSize );

    rdraw_scissor scissor;
    scissor.Width = m_shadowMomentAtlasSize;
    scissor.Height = m_shadowMomentAtlasSize;

    xbool const bSuccess =
        shader_PushUniformData( SHADER_STAGE_PIXEL, constantsSlot, &constants, sizeof( constants ) ) &&
        rdraw_SetViewport( viewport ) && rdraw_SetScissor( scissor ) && rdraw_Draw( 3 );
    rtarget_EndPass();
    return bSuccess;
}

//==============================================================================

xbool shadow_mgr::SetShadowCastConstants( u32 uniformSlot, matrix4 const& shadowViewProjection )
{
    cb_shadow_cast cbData;
    x_memset( &cbData, 0, sizeof( cbData ) );
    cbData.ShadowViewProjection = shadowViewProjection;
    return shader_PushUniformData( SHADER_STAGE_VERTEX, uniformSlot, &cbData, sizeof( cbData ) );
}

//==============================================================================

xbool shadow_mgr::SetShadowAlphaConstants( texture const& diffuseTexture, xbool bPunchThru, u8 uOffset, u8 vOffset )
{
    if ( !m_alphaDepthPixelShader || !diffuseTexture.GetShaderResource() )
    {
        return FALSE;
    }

    cb_shadow_alpha cbData;
    x_memset( &cbData, 0, sizeof( cbData ) );
    cbData.AlphaRef = bPunchThru ? 0.5f : ( 4.0f / 255.0f );

    f32 const kInvByte = 1.0f / 255.0f;
    cbData.UVOffset[0] = static_cast<f32>( uOffset ) * kInvByte;
    cbData.UVOffset[1] = static_cast<f32>( vOffset ) * kInvByte;

    if ( !shader_PushUniformData( SHADER_STAGE_PIXEL, m_alphaConstantsSlot, &cbData, sizeof( cbData ) ) )
    {
        return FALSE;
    }

    if ( !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_alphaTextureSlot,
                                                      diffuseTexture.GetShaderResource(), &m_diffuseSampler ) ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void shadow_mgr::ApplySource( s32 sourceIndex, s32 casterShader, xbool bAlphaTest, xbool bTwoSided )
{
    ASSERT( sourceIndex >= 0 );
    ASSERT( sourceIndex < g_ShadowMapMgr.GetSourceCount() );

    if ( !m_isCastPassActive )
    {
        return;
    }

    s32 const casterVariant = GetShadowCasterVariant( casterShader, bAlphaTest, bTwoSided );
    if ( ( sourceIndex == m_currentSource ) && ( casterVariant == m_currentCasterVariant ) )
    {
        return;
    }

    ShadowMapMgr::ShadowSource const& source = g_ShadowMapMgr.GetSource( sourceIndex );

    render_pipeline* pPipeline = NULL;
    shader const*    pVertexShader = NULL;

    if ( casterShader == SHADOW_CASTER_RIGID )
    {
        ShadowPipeline const pipeline = static_cast<ShadowPipeline>( casterVariant );
        pPipeline = m_casterPipelines.Find( static_cast<u64>( pipeline ) );
        pVertexShader = &m_rigidVertexShader;
    }
    else if ( casterShader == SHADOW_CASTER_SKIN )
    {
        ShadowPipeline const pipeline = static_cast<ShadowPipeline>( casterVariant );
        pPipeline = m_casterPipelines.Find( static_cast<u64>( pipeline ) );
        pVertexShader = &m_skinVertexShader;
    }

    if ( !pPipeline || !( *pPipeline ) || !pVertexShader || !( *pVertexShader ) )
    {
        return;
    }

    rdraw_viewport viewport;
    x_memset( &viewport, 0, sizeof( viewport ) );
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    viewport.TopLeftX = static_cast<f32>( source.AtlasX );
    viewport.TopLeftY = static_cast<f32>( source.AtlasY );
    viewport.Width = static_cast<f32>( source.AtlasWidth );
    viewport.Height = static_cast<f32>( source.AtlasHeight );
    rdraw_SetViewport( viewport );

    if ( !render_BindPipeline( *pPipeline ) )
    {
        return;
    }

    if ( casterShader == SHADOW_CASTER_RIGID )
    {
        shader_resource const* pInstances = rbuffer_GetResource( m_rigidInstanceBuffer );
        if ( !pInstances || !shader_BindStorageBuffer( shader_storage_buffer_binding(
                                SHADER_STAGE_VERTEX, m_rigidInstanceSlot, pInstances ) ) )
        {
            return;
        }
    }
    else
    {
        shader_resource const* pBones = rbuffer_GetResource( m_skinBoneBuffer );
        if ( !pBones )
        {
            return;
        }

        if ( !shader_BindStorageBuffer( shader_storage_buffer_binding( SHADER_STAGE_VERTEX, m_skinBoneSlot, pBones ) ) )
        {
            return;
        }
    }

    m_currentSource = sourceIndex;
    m_currentCasterVariant = casterVariant;
}

//==============================================================================

void shadow_mgr::ApplyDynamicSource( s32 sourceIndex )
{
    ASSERT( sourceIndex >= 0 );
    ASSERT( sourceIndex < g_ShadowMapMgr.GetSourceCount() );

    s32 const casterVariant = SHADOW_CASTER_DYNAMIC;
    if( !m_isCastPassActive ||
        ( ( sourceIndex == m_currentSource ) && ( casterVariant == m_currentCasterVariant ) ) )
    {
        return;
    }

    render_pipeline* pPipeline = m_casterPipelines.Find(
        static_cast<u64>( ShadowPipeline::DynamicAlphaTwoSided ) );
    if( !pPipeline || !( *pPipeline ) || !m_dynamicVertexShader || !m_dynamicPixelShader )
        return;

    ShadowMapMgr::ShadowSource const& source = g_ShadowMapMgr.GetSource( sourceIndex );
    rdraw_viewport viewport;
    x_memset( &viewport, 0, sizeof( viewport ) );
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = static_cast<f32>( source.AtlasX );
    viewport.TopLeftY = static_cast<f32>( source.AtlasY );
    viewport.Width = static_cast<f32>( source.AtlasWidth );
    viewport.Height = static_cast<f32>( source.AtlasHeight );
    rdraw_SetViewport( viewport );

    if( !render_BindPipeline( *pPipeline ) )
        return;

    m_currentSource = sourceIndex;
    m_currentCasterVariant = casterVariant;
}

//==============================================================================

void shadow_mgr::RenderCasters( xarray<geometry_draw_item const*> const& draws,
                                xarray<dynamic_geometry_shadow_draw> const& dynamicDraws )
{
    m_lastCasterStats = caster_stats();
    m_lastCasterStats.InputDrawCount = draws.GetCount() + dynamicDraws.GetCount();

    if ( !m_isInitialized || !g_ShadowMapMgr.HasActiveSources() )
    {
        return;
    }

    if ( ( draws.GetCount() == 0 ) && ( dynamicDraws.GetCount() == 0 ) )
    {
        BeginCastPass();
        EndCastPass();
        return;
    }

    xarray<PreparedShadowDraw>             preparedDraws;
    xarray<SkinPaletteRef>                 skinPaletteRefs;
    xarray<matrix4>                        rigidInstances;
    xarray<matrix4>                        skinBoneMatrices;
    xarray<u32>                            skinBoneBases;
    xarray<ShadowDrawPacket>               packets;
    xarray<ShadowIndirectItem>             rigidIndirectItems;
    xarray<ShadowIndirectRun>              rigidIndirectRuns;
    xarray<rdraw_indexed_indirect_command> rigidIndirectCommands;
    xarray<ShadowIndirectItem>             skinIndirectItems;
    xarray<ShadowIndirectRun>              skinIndirectRuns;
    xarray<rdraw_indexed_indirect_command> skinIndirectCommands;
    xarray<PreparedDynamicShadowDraw>       preparedDynamicDraws;
    xarray<dynamic_geometry_vertex>        dynamicVertices;
    xarray<u16>                            dynamicIndices;

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/ResolveCasters" );

        // Resolve render surfaces and collect all per-instance data before the
        // render pass. SDL GPU uploads cannot be recorded inside a render pass.
        for ( s32 i = 0; i < draws.GetCount(); ++i )
        {
            geometry_draw_item const* pItem = draws[i];
            if ( !pItem || ( pItem->ShadowSourceIndex < 0 ) ||
                 ( pItem->ShadowSourceIndex >= g_ShadowMapMgr.GetSourceCount() ) )
            {
                continue;
            }

            PreparedShadowDraw draw;
            draw.Type = pItem->Type;
            draw.SourceIndex = pItem->ShadowSourceIndex;

            if ( draw.Type == GEOMETRY_DRAW_RIGID )
            {
                if ( !pItem->Data.Rigid.pL2W )
                {
                    continue;
                }

                draw.hMesh = g_GeomStorage.GetRigidSurface( pItem->hRenderGeom, pItem->iSurface );
                if ( draw.hMesh.IsNull() )
                {
                    continue;
                }

                draw.RigidInstanceIndex = rigidInstances.GetCount();
                rigidInstances.Append() = *pItem->Data.Rigid.pL2W;
            }
            else
            {
                draw.hMesh = g_GeomStorage.GetSkinSurface( pItem->hRenderGeom, pItem->iSurface );
                draw.pBones = pItem->Data.Skin.pBones;
                draw.BoneCount = pItem->Data.Skin.BoneCount;
                if ( draw.hMesh.IsNull() || !draw.pBones || ( draw.BoneCount <= 0 ) )
                {
                    continue;
                }

                SoftVertexMgr::MeshView view;
                if ( !g_SkinVertMgr.GetMeshView( draw.hMesh, view ) || ( view.SectionCount <= 0 ) )
                {
                    continue;
                }

                xbool validBoneRemaps = TRUE;
                for ( s32 s = 0; validBoneRemaps && ( s < view.SectionCount ); ++s )
                {
                    SoftVertexMgr::SoftSection const& section = view.pSections[s];
                    for ( s32 b = 0; b < SoftVertexMgr::MAX_BONE_PALETTE; ++b )
                    {
                        u16 const boneId = section.BoneRemap[b];
                        if ( ( boneId != SoftVertexMgr::INVALID_BONE_REMAP ) &&
                             ( static_cast<s32>( boneId ) >= draw.BoneCount ) )
                        {
                            validBoneRemaps = FALSE;
                            break;
                        }
                    }
                }
                ASSERTS( validBoneRemaps, "Skin shadow section references a bone outside the submitted pose" );
                if ( !validBoneRemaps )
                {
                    continue;
                }

                draw.SkinSectionCount = view.SectionCount;
            }

            draw.pAlphaTexture = GetShadowAlphaTexture( pItem->pMaterial, draw.bPunchThru );
            draw.bTwoSided = pItem->pMaterial && !pItem->pMaterial->CullsBackFaces();
            if ( draw.pAlphaTexture )
            {
                draw.UOffset = pItem->UOffset;
                draw.VOffset = pItem->VOffset;
            }

            u32 const drawIndex = preparedDraws.GetCount();
            preparedDraws.Append() = draw;

            if ( draw.Type == GEOMETRY_DRAW_RIGID )
            {
                m_lastCasterStats.RigidInstanceCount++;
            }
            else
            {
                m_lastCasterStats.SkinInstanceCount++;
            }

            if ( draw.Type == GEOMETRY_DRAW_SKIN )
            {
                SkinPaletteRef& ref = skinPaletteRefs.Append();
                ref.pBones = draw.pBones;
                ref.BoneCount = draw.BoneCount;
                ref.MeshHandle = draw.hMesh.Handle;
                ref.DrawIndex = drawIndex;
            }
        }

        for( s32 i = 0; i < dynamicDraws.GetCount(); ++i )
        {
            dynamic_geometry_shadow_draw const& sourceDraw = dynamicDraws[i];
            if( !sourceDraw.pVertices || !sourceDraw.pIndices || !sourceDraw.pDiffuse ||
                !sourceDraw.pDamageMask || !sourceDraw.pDamageTexture || !sourceDraw.pDamageUploadPending ||
                ( sourceDraw.VertexCount <= 0 ) || ( sourceDraw.VertexCount > 65535 ) ||
                ( sourceDraw.IndexCount <= 0 ) || ( ( sourceDraw.IndexCount % 3 ) != 0 ) ||
                ( sourceDraw.ShadowSourceMask == 0 ) )
            {
                continue;
            }

            xbool validIndices = TRUE;
            for( s32 j = 0; j < sourceDraw.IndexCount; ++j )
            {
                if( sourceDraw.pIndices[j] >= sourceDraw.VertexCount )
                {
                    validIndices = FALSE;
                    break;
                }
            }
            if( !validIndices )
                continue;

            if( *sourceDraw.pDamageUploadPending )
            {
                if( !sourceDraw.pDamageUpload || ( sourceDraw.DamageUploadWidth <= 0 ) ||
                    ( sourceDraw.DamageUploadHeight <= 0 ) )
                {
                    continue;
                }

                vram_texture_upload_desc upload;
                upload.Region.X = sourceDraw.DamageUploadX;
                upload.Region.Y = sourceDraw.DamageUploadY;
                upload.Region.Width = sourceDraw.DamageUploadWidth;
                upload.Region.Height = sourceDraw.DamageUploadHeight;
                upload.pData = sourceDraw.pDamageUpload;
                upload.Size = sourceDraw.DamageUploadWidth * sourceDraw.DamageUploadHeight;
                upload.RowPitch = sourceDraw.DamageUploadWidth;
                upload.SlicePitch = upload.Size;
                if( !vram_UploadTexture( *sourceDraw.pDamageTexture, upload ) )
                    continue;

                *sourceDraw.pDamageUploadPending = FALSE;
                m_lastCasterStats.UploadBytes += upload.Size;
            }

            PreparedDynamicShadowDraw& draw = preparedDynamicDraws.Append();
            draw.pDiffuse = sourceDraw.pDiffuse;
            draw.pDamageMask = sourceDraw.pDamageMask;
            draw.FirstVertex = dynamicVertices.GetCount();
            draw.FirstIndex = dynamicIndices.GetCount();
            draw.VertexCount = sourceDraw.VertexCount;
            draw.IndexCount = sourceDraw.IndexCount;
            draw.ShadowSourceMask = sourceDraw.ShadowSourceMask;

            s32 const oldVertexCount = dynamicVertices.GetCount();
            dynamicVertices.SetCount( oldVertexCount + sourceDraw.VertexCount );
            x_memcpy( &dynamicVertices[oldVertexCount], sourceDraw.pVertices,
                      sourceDraw.VertexCount * sizeof( dynamic_geometry_vertex ) );

            s32 const oldIndexCount = dynamicIndices.GetCount();
            dynamicIndices.SetCount( oldIndexCount + sourceDraw.IndexCount );
            x_memcpy( &dynamicIndices[oldIndexCount], sourceDraw.pIndices,
                      sourceDraw.IndexCount * sizeof( u16 ) );
        }
    }

    m_lastCasterStats.PreparedDrawCount = preparedDraws.GetCount() + preparedDynamicDraws.GetCount();

    if ( ( preparedDraws.GetCount() == 0 ) && ( preparedDynamicDraws.GetCount() == 0 ) )
    {
        BeginCastPass();
        EndCastPass();
        return;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BuildSkinPalettes" );

        // The same animated object is submitted to every point-light face. Build
        // its remapped section palettes once and reuse them across all sources.
        if ( skinPaletteRefs.GetCount() > 1 )
        {
            x_qsort( skinPaletteRefs.GetPtr(), skinPaletteRefs.GetCount(), sizeof( SkinPaletteRef ),
                     CompareSkinPaletteRefs );
        }

        for ( s32 begin = 0; begin < skinPaletteRefs.GetCount(); )
        {
            s32 end = begin + 1;
            while ( ( end < skinPaletteRefs.GetCount() ) &&
                    ( skinPaletteRefs[end].MeshHandle == skinPaletteRefs[begin].MeshHandle ) &&
                    ( skinPaletteRefs[end].pBones == skinPaletteRefs[begin].pBones ) &&
                    ( skinPaletteRefs[end].BoneCount == skinPaletteRefs[begin].BoneCount ) )
            {
                ++end;
            }

            PreparedShadowDraw&     firstDraw = preparedDraws[skinPaletteRefs[begin].DrawIndex];
            SoftVertexMgr::MeshView view;
            if ( g_SkinVertMgr.GetMeshView( firstDraw.hMesh, view ) )
            {
                u32 const paletteBase = skinBoneMatrices.GetCount();
                for ( s32 s = 0; s < view.SectionCount; ++s )
                {
                    SoftVertexMgr::SoftSection const& section = view.pSections[s];
                    for ( s32 b = 0; b < SoftVertexMgr::MAX_BONE_PALETTE; ++b )
                    {
                        matrix4& bone = skinBoneMatrices.Append();
                        bone.Identity();

                        u16 const boneId = section.BoneRemap[b];
                        if ( firstDraw.pBones && ( boneId != SoftVertexMgr::INVALID_BONE_REMAP ) &&
                             ( static_cast<s32>( boneId ) < firstDraw.BoneCount ) )
                        {
                            bone = firstDraw.pBones[boneId];
                        }
                    }
                }

                for ( s32 i = begin; i < end; ++i )
                {
                    preparedDraws[skinPaletteRefs[i].DrawIndex].SkinPaletteBase = paletteBase;
                }

                m_lastCasterStats.SkinPaletteCount++;
            }

            begin = end;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BuildPackets" );

        // Consecutive compatible casters become one packet. Source stays in the
        // key, so all objects still cast into every selected shadow source.
        for ( s32 i = 0; i < preparedDraws.GetCount(); ++i )
        {
            PreparedShadowDraw const& draw = preparedDraws[i];
            xbool                     bAppend = FALSE;
            if ( packets.GetCount() > 0 )
            {
                ShadowDrawPacket const& last = packets[packets.GetCount() - 1];
                bAppend = ( last.Type == draw.Type ) && ( last.hMesh.Handle == draw.hMesh.Handle ) &&
                          ( last.SourceIndex == draw.SourceIndex ) && ( last.pAlphaTexture == draw.pAlphaTexture ) &&
                          ( last.bPunchThru == draw.bPunchThru ) && ( last.UOffset == draw.UOffset ) &&
                          ( last.VOffset == draw.VOffset ) && ( last.bTwoSided == draw.bTwoSided );
            }

            if ( !bAppend )
            {
                ShadowDrawPacket& packet = packets.Append();
                packet.Type = draw.Type;
                packet.hMesh = draw.hMesh;
                packet.pAlphaTexture = draw.pAlphaTexture;
                packet.SourceIndex = draw.SourceIndex;
                packet.FirstDraw = i;
                packet.FirstInstance = draw.RigidInstanceIndex;
                packet.SkinSectionCount = draw.SkinSectionCount;
                packet.UOffset = draw.UOffset;
                packet.VOffset = draw.VOffset;
                packet.bPunchThru = draw.bPunchThru;
                packet.bTwoSided = draw.bTwoSided;

                if ( draw.Type == GEOMETRY_DRAW_RIGID )
                {
                    m_lastCasterStats.RigidPacketCount++;
                }
                else
                {
                    m_lastCasterStats.SkinPacketCount++;
                }
                if ( draw.pAlphaTexture )
                {
                    m_lastCasterStats.AlphaPacketCount++;
                }
            }

            ShadowDrawPacket& packet = packets[packets.GetCount() - 1];
            packet.InstanceCount++;
            m_lastCasterStats.MaxPacketInstanceCount =
                MAX( m_lastCasterStats.MaxPacketInstanceCount, packet.InstanceCount );
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BuildSkinOffsets" );

        // Bone palette bases are a regular per-instance vertex stream. Each
        // section selects its own contiguous range through first_instance.
        for ( s32 i = 0; i < packets.GetCount(); ++i )
        {
            ShadowDrawPacket& packet = packets[i];
            if ( packet.Type != GEOMETRY_DRAW_SKIN )
            {
                continue;
            }

            packet.FirstSkinDrawInstance = skinBoneBases.GetCount();
            for ( s32 s = 0; s < packet.SkinSectionCount; ++s )
            {
                for ( u32 d = 0; d < packet.InstanceCount; ++d )
                {
                    PreparedShadowDraw const& draw = preparedDraws[packet.FirstDraw + d];
                    skinBoneBases.Append() = draw.SkinPaletteBase + s * SoftVertexMgr::MAX_BONE_PALETTE;
                }
            }
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BuildIndirectRuns" );

        for ( s32 i = 0; i < packets.GetCount(); ++i )
        {
            ShadowDrawPacket const& packet = packets[i];
            if ( ( packet.Type != GEOMETRY_DRAW_RIGID ) || packet.pAlphaTexture )
            {
                continue;
            }

            ShadowIndirectItem item;
            item.PacketIndex = i;
            item.SourceIndex = packet.SourceIndex;
            item.FirstInstance = packet.FirstInstance;
            item.bTwoSided = packet.bTwoSided;
            if ( !g_RigidVertMgr.GetMeshDrawRange( packet.hMesh, item.Range ) )
            {
                return;
            }
            rigidIndirectItems.Append() = item;
        }

        if ( rigidIndirectItems.GetCount() > 1 )
        {
            x_qsort( rigidIndirectItems.GetPtr(), rigidIndirectItems.GetCount(), sizeof( ShadowIndirectItem ),
                     CompareShadowIndirectItems );
        }

        for ( s32 i = 0; i < rigidIndirectItems.GetCount(); ++i )
        {
            ShadowIndirectItem const& item = rigidIndirectItems[i];
            ShadowDrawPacket const&   packet = packets[item.PacketIndex];

            ShadowIndirectRun* pRun = NULL;
            if ( rigidIndirectRuns.GetCount() > 0 )
            {
                ShadowIndirectRun& last = rigidIndirectRuns[rigidIndirectRuns.GetCount() - 1];
                if ( ( last.SourceIndex == item.SourceIndex ) && ( last.VertexPool == item.Range.VertexPool ) &&
                     ( last.IndexPool == item.Range.IndexPool ) && ( last.bTwoSided == item.bTwoSided ) )
                {
                    pRun = &last;
                }
            }

            if ( !pRun )
            {
                ShadowIndirectRun& run = rigidIndirectRuns.Append();
                run.FirstItem = i;
                run.FirstCommand = rigidIndirectCommands.GetCount();
                run.CommandCount = 0;
                run.SourceIndex = item.SourceIndex;
                run.VertexPool = item.Range.VertexPool;
                run.IndexPool = item.Range.IndexPool;
                run.bTwoSided = item.bTwoSided;
                pRun = &run;
            }

            rdraw_indexed_indirect_command& command = rigidIndirectCommands.Append();
            command.IndexCount = item.Range.IndexCount;
            command.InstanceCount = packet.InstanceCount;
            command.FirstIndex = item.Range.FirstIndex;
            command.BaseVertex = item.Range.BaseVertex;
            command.FirstInstance = item.FirstInstance;
            pRun->CommandCount++;
        }

        for ( s32 i = 0; i < packets.GetCount(); ++i )
        {
            ShadowDrawPacket const& packet = packets[i];
            if ( ( packet.Type != GEOMETRY_DRAW_SKIN ) || packet.pAlphaTexture )
            {
                continue;
            }

            SoftVertexMgr::MeshView view;
            if ( !g_SkinVertMgr.GetMeshView( packet.hMesh, view ) )
            {
                return;
            }

            for ( s32 s = 0; s < view.SectionCount; ++s )
            {
                SoftVertexMgr::SoftSection const& section = view.pSections[s];
                ShadowIndirectItem                item;
                item.PacketIndex = i;
                item.SourceIndex = packet.SourceIndex;
                item.FirstInstance = packet.FirstSkinDrawInstance + s * packet.InstanceCount;
                item.bTwoSided = packet.bTwoSided;
                item.Range = view.Geometry;
                item.Range.FirstIndex += section.FirstIndex;
                item.Range.IndexCount = section.IndexCount;
                skinIndirectItems.Append() = item;
            }
        }

        if ( skinIndirectItems.GetCount() > 1 )
        {
            x_qsort( skinIndirectItems.GetPtr(), skinIndirectItems.GetCount(), sizeof( ShadowIndirectItem ),
                     CompareShadowIndirectItems );
        }

        for ( s32 i = 0; i < skinIndirectItems.GetCount(); ++i )
        {
            ShadowIndirectItem const& item = skinIndirectItems[i];
            ShadowDrawPacket const&   packet = packets[item.PacketIndex];

            ShadowIndirectRun* pRun = NULL;
            if ( skinIndirectRuns.GetCount() > 0 )
            {
                ShadowIndirectRun& last = skinIndirectRuns[skinIndirectRuns.GetCount() - 1];
                if ( ( last.SourceIndex == item.SourceIndex ) && ( last.VertexPool == item.Range.VertexPool ) &&
                     ( last.IndexPool == item.Range.IndexPool ) && ( last.bTwoSided == item.bTwoSided ) )
                {
                    pRun = &last;
                }
            }

            if ( !pRun )
            {
                ShadowIndirectRun& run = skinIndirectRuns.Append();
                run.FirstItem = i;
                run.FirstCommand = skinIndirectCommands.GetCount();
                run.CommandCount = 0;
                run.SourceIndex = item.SourceIndex;
                run.VertexPool = item.Range.VertexPool;
                run.IndexPool = item.Range.IndexPool;
                run.bTwoSided = item.bTwoSided;
                pRun = &run;
            }

            rdraw_indexed_indirect_command& command = skinIndirectCommands.Append();
            command.IndexCount = item.Range.IndexCount;
            command.InstanceCount = packet.InstanceCount;
            command.FirstIndex = item.Range.FirstIndex;
            command.BaseVertex = item.Range.BaseVertex;
            command.FirstInstance = item.FirstInstance;
            pRun->CommandCount++;
        }
    }

    m_lastCasterStats.PacketCount = packets.GetCount() + preparedDynamicDraws.GetCount();
    m_lastCasterStats.AlphaPacketCount += preparedDynamicDraws.GetCount();
    m_lastCasterStats.SkinPaletteMatrixCount = skinBoneMatrices.GetCount();
    m_lastCasterStats.RigidIndirectRunCount = rigidIndirectRuns.GetCount();
    m_lastCasterStats.RigidIndirectCommandCount = rigidIndirectCommands.GetCount();
    m_lastCasterStats.SkinIndirectRunCount = skinIndirectRuns.GetCount();
    m_lastCasterStats.SkinIndirectCommandCount = skinIndirectCommands.GetCount();
    m_lastCasterStats.UploadBytes +=
        static_cast<u64>( rigidInstances.GetCount() ) * sizeof( matrix4 ) +
        static_cast<u64>( skinBoneMatrices.GetCount() ) * sizeof( matrix4 ) +
        static_cast<u64>( skinBoneBases.GetCount() ) * sizeof( u32 ) +
        static_cast<u64>( rigidIndirectCommands.GetCount() + skinIndirectCommands.GetCount() ) *
            sizeof( rdraw_indexed_indirect_command ) +
        static_cast<u64>( dynamicVertices.GetCount() ) * sizeof( dynamic_geometry_vertex ) +
        static_cast<u64>( dynamicIndices.GetCount() ) * sizeof( u16 );
    if ( ( rigidInstances.GetCount() > 0 ) &&
         ( !m_rigidInstanceBuffer || ( m_rigidInstanceCapacity < static_cast<u32>( rigidInstances.GetCount() ) ) ) )
    {
        m_lastCasterStats.BufferReallocationCount++;
    }
    if ( ( rigidInstances.GetCount() > 0 ) &&
         ( !m_rigidInstanceIndexBuffer ||
           ( m_rigidInstanceIndexCapacity < static_cast<u32>( rigidInstances.GetCount() ) ) ) )
    {
        m_lastCasterStats.BufferReallocationCount++;
    }
    if ( ( skinBoneMatrices.GetCount() > 0 ) &&
         ( !m_skinBoneBuffer || ( m_skinBoneCapacity < static_cast<u32>( skinBoneMatrices.GetCount() ) ) ) )
    {
        m_lastCasterStats.BufferReallocationCount++;
    }
    if ( ( skinBoneBases.GetCount() > 0 ) &&
         ( !m_skinBoneBaseBuffer || ( m_skinBoneBaseCapacity < static_cast<u32>( skinBoneBases.GetCount() ) ) ) )
    {
        m_lastCasterStats.BufferReallocationCount++;
    }
    if ( ( rigidIndirectCommands.GetCount() > 0 ) &&
         ( !m_rigidIndirectBuffer ||
           ( m_rigidIndirectCapacity < static_cast<u32>( rigidIndirectCommands.GetCount() ) ) ) )
    {
        m_lastCasterStats.BufferReallocationCount++;
    }
    if ( ( skinIndirectCommands.GetCount() > 0 ) &&
         ( !m_skinIndirectBuffer || ( m_skinIndirectCapacity < static_cast<u32>( skinIndirectCommands.GetCount() ) ) ) )
    {
        m_lastCasterStats.BufferReallocationCount++;
    }
    if( ( dynamicVertices.GetCount() > 0 ) &&
        ( !m_dynamicVertexBuffer ||
          ( m_dynamicVertexCapacity < static_cast<u32>( dynamicVertices.GetCount() ) ) ) )
    {
        m_lastCasterStats.BufferReallocationCount++;
    }
    if( ( dynamicIndices.GetCount() > 0 ) &&
        ( !m_dynamicIndexBuffer ||
          ( m_dynamicIndexCapacity < static_cast<u32>( dynamicIndices.GetCount() ) ) ) )
    {
        m_lastCasterStats.BufferReallocationCount++;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/UploadBuffers" );

        if ( ( rigidInstances.GetCount() > 0 &&
               ( !EnsureCasterBuffer( m_rigidInstanceBuffer, m_rigidInstanceCapacity, rigidInstances.GetCount(),
                                      sizeof( matrix4 ), "ShadowRigidWorld" ) ||
                 !EnsureInstanceIndexBuffer( m_rigidInstanceIndexBuffer, m_rigidInstanceIndexCapacity,
                                             rigidInstances.GetCount(), "ShadowRigidInstanceIndices" ) ||
                 !rbuffer_Upload( m_rigidInstanceBuffer, &rigidInstances[0],
                                  sizeof( matrix4 ) * rigidInstances.GetCount(), 0, TRUE ) ) ) ||
             ( skinBoneMatrices.GetCount() > 0 &&
               ( !EnsureCasterBuffer( m_skinBoneBuffer, m_skinBoneCapacity, skinBoneMatrices.GetCount(),
                                      sizeof( matrix4 ), "ShadowSkinBones" ) ||
                 !rbuffer_Upload( m_skinBoneBuffer, &skinBoneMatrices[0],
                                  sizeof( matrix4 ) * skinBoneMatrices.GetCount(), 0, TRUE ) ) ) ||
             ( skinBoneBases.GetCount() > 0 &&
               ( !EnsureCasterBuffer( m_skinBoneBaseBuffer, m_skinBoneBaseCapacity, skinBoneBases.GetCount(),
                                      sizeof( u32 ), "ShadowSkinBoneBases", RBUFFER_USAGE_VERTEX ) ||
                 !rbuffer_Upload( m_skinBoneBaseBuffer, &skinBoneBases[0], sizeof( u32 ) * skinBoneBases.GetCount(), 0,
                                  TRUE ) ) ) ||
             ( rigidIndirectCommands.GetCount() > 0 &&
               ( !EnsureCasterBuffer( m_rigidIndirectBuffer, m_rigidIndirectCapacity, rigidIndirectCommands.GetCount(),
                                      sizeof( rdraw_indexed_indirect_command ), "ShadowRigidIndirect",
                                      RBUFFER_USAGE_INDIRECT ) ||
                 !rbuffer_Upload( m_rigidIndirectBuffer, &rigidIndirectCommands[0],
                                  sizeof( rdraw_indexed_indirect_command ) * rigidIndirectCommands.GetCount(), 0,
                                  TRUE ) ) ) ||
             ( skinIndirectCommands.GetCount() > 0 &&
               ( !EnsureCasterBuffer( m_skinIndirectBuffer, m_skinIndirectCapacity, skinIndirectCommands.GetCount(),
                                      sizeof( rdraw_indexed_indirect_command ), "ShadowSkinIndirect",
                                      RBUFFER_USAGE_INDIRECT ) ||
                 !rbuffer_Upload( m_skinIndirectBuffer, &skinIndirectCommands[0],
                                  sizeof( rdraw_indexed_indirect_command ) * skinIndirectCommands.GetCount(), 0,
                                  TRUE ) ) ) ||
             ( dynamicVertices.GetCount() > 0 &&
               ( !EnsureCasterBuffer( m_dynamicVertexBuffer, m_dynamicVertexCapacity, dynamicVertices.GetCount(),
                                      sizeof( dynamic_geometry_vertex ), "ShadowDynamicVertices",
                                      RBUFFER_USAGE_VERTEX ) ||
                 !EnsureCasterBuffer( m_dynamicIndexBuffer, m_dynamicIndexCapacity, dynamicIndices.GetCount(),
                                      sizeof( u16 ), "ShadowDynamicIndices", RBUFFER_USAGE_INDEX ) ||
                 !rbuffer_Upload( m_dynamicVertexBuffer, &dynamicVertices[0],
                                  sizeof( dynamic_geometry_vertex ) * dynamicVertices.GetCount(), 0, TRUE ) ||
                 !rbuffer_Upload( m_dynamicIndexBuffer, &dynamicIndices[0],
                                  sizeof( u16 ) * dynamicIndices.GetCount(), 0, TRUE ) ) ) )
        {
            return;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/EncodePackets" );

        BeginCastPass();
        if ( !m_isCastPassActive )
        {
            return;
        }

        for ( s32 i = 0; i < rigidIndirectRuns.GetCount(); ++i )
        {
            ShadowIndirectRun const&          run = rigidIndirectRuns[i];
            ShadowIndirectItem const&         firstItem = rigidIndirectItems[run.FirstItem];
            ShadowMapMgr::ShadowSource const& source = g_ShadowMapMgr.GetSource( run.SourceIndex );
            s32 const casterVariant =
                GetShadowCasterVariant( SHADOW_CASTER_RIGID, FALSE, run.bTwoSided );

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/ApplyIndirectState" );
                if ( ( run.SourceIndex != m_currentSource ) || ( casterVariant != m_currentCasterVariant ) )
                {
                    m_lastCasterStats.SourceStateChangeCount++;
                }
                ApplySource( run.SourceIndex, SHADOW_CASTER_RIGID, FALSE, run.bTwoSided );
            }
            if ( ( run.SourceIndex != m_currentSource ) || ( casterVariant != m_currentCasterVariant ) )
            {
                continue;
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindIndirectRun" );
                if ( !SetShadowCastConstants( m_rigidCastConstantsSlot, source.WorldToClip ) ||
                     !g_RigidVertMgr.BindPools( firstItem.Range ) ||
                     !rbuffer_BindVertex( m_rigidInstanceIndexBuffer, 1, 0 ) )
                {
                    continue;
                }
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/DrawIndirectRun" );
                if ( !rdraw_DrawIndexedIndirect( m_rigidIndirectBuffer,
                                                 run.FirstCommand * sizeof( rdraw_indexed_indirect_command ),
                                                 run.CommandCount ) )
                {
                    continue;
                }
            }

            m_lastCasterStats.GpuDrawCount++;
            for ( u32 commandIndex = 0; commandIndex < run.CommandCount; ++commandIndex )
            {
                rdraw_indexed_indirect_command const& command = rigidIndirectCommands[run.FirstCommand + commandIndex];
                m_lastCasterStats.SubmittedIndexCount += static_cast<u64>( command.IndexCount ) * command.InstanceCount;
            }
        }

        for ( s32 i = 0; i < skinIndirectRuns.GetCount(); ++i )
        {
            ShadowIndirectRun const&          run = skinIndirectRuns[i];
            ShadowIndirectItem const&         firstItem = skinIndirectItems[run.FirstItem];
            ShadowMapMgr::ShadowSource const& source = g_ShadowMapMgr.GetSource( run.SourceIndex );
            s32 const casterVariant =
                GetShadowCasterVariant( SHADOW_CASTER_SKIN, FALSE, run.bTwoSided );

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/ApplySkinIndirectState" );
                if ( ( run.SourceIndex != m_currentSource ) || ( casterVariant != m_currentCasterVariant ) )
                {
                    m_lastCasterStats.SourceStateChangeCount++;
                }
                ApplySource( run.SourceIndex, SHADOW_CASTER_SKIN, FALSE, run.bTwoSided );
            }
            if ( ( run.SourceIndex != m_currentSource ) || ( casterVariant != m_currentCasterVariant ) )
            {
                continue;
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindSkinIndirectRun" );
                if ( !SetShadowCastConstants( m_skinCastConstantsSlot, source.WorldToClip ) ||
                     !g_SkinVertMgr.BindPools( firstItem.Range ) || !rbuffer_BindVertex( m_skinBoneBaseBuffer, 1, 0 ) )
                {
                    continue;
                }
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/DrawSkinIndirectRun" );
                if ( !rdraw_DrawIndexedIndirect( m_skinIndirectBuffer,
                                                 run.FirstCommand * sizeof( rdraw_indexed_indirect_command ),
                                                 run.CommandCount ) )
                {
                    continue;
                }
            }

            m_lastCasterStats.GpuDrawCount++;
            for ( u32 commandIndex = 0; commandIndex < run.CommandCount; ++commandIndex )
            {
                rdraw_indexed_indirect_command const& command = skinIndirectCommands[run.FirstCommand + commandIndex];
                m_lastCasterStats.SubmittedIndexCount += static_cast<u64>( command.IndexCount ) * command.InstanceCount;
            }
        }

        for ( s32 i = 0; i < packets.GetCount(); ++i )
        {
            ShadowDrawPacket const& packet = packets[i];
            xbool const             bAlphaTest = ( packet.pAlphaTexture != NULL );
            if ( !bAlphaTest )
            {
                continue;
            }
            s32 const casterShader = ( packet.Type == GEOMETRY_DRAW_RIGID ) ? SHADOW_CASTER_RIGID : SHADOW_CASTER_SKIN;
            s32 const casterVariant = GetShadowCasterVariant( casterShader, bAlphaTest, packet.bTwoSided );
            ShadowMapMgr::ShadowSource const& source = g_ShadowMapMgr.GetSource( packet.SourceIndex );

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/ApplyState" );
                if ( ( packet.SourceIndex != m_currentSource ) || ( casterVariant != m_currentCasterVariant ) )
                {
                    m_lastCasterStats.SourceStateChangeCount++;
                }
                ApplySource( packet.SourceIndex, casterShader, bAlphaTest, packet.bTwoSided );
            }
            if ( ( packet.SourceIndex != m_currentSource ) || ( casterVariant != m_currentCasterVariant ) )
            {
                continue;
            }

            if ( bAlphaTest )
            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindAlpha" );
                if ( !SetShadowAlphaConstants( *packet.pAlphaTexture, packet.bPunchThru, packet.UOffset,
                                               packet.VOffset ) )
                {
                    continue;
                }
            }

            if ( packet.Type == GEOMETRY_DRAW_RIGID )
            {
                VertexMgr::mesh_range range;
                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindMesh" );
                    if ( !g_RigidVertMgr.GetMeshDrawRange( packet.hMesh, range ) ||
                         !g_RigidVertMgr.BindPools( range ) || !rbuffer_BindVertex( m_rigidInstanceIndexBuffer, 1, 0 ) )
                    {
                        continue;
                    }
                }
                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/PushDrawConstants" );
                    if ( !SetShadowCastConstants( m_rigidCastConstantsSlot, source.WorldToClip ) )
                    {
                        continue;
                    }
                }
                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/Draw" );
                    if ( rdraw_DrawIndexedInstanced( range.IndexCount, packet.InstanceCount, range.FirstIndex,
                                                     range.BaseVertex, packet.FirstInstance ) )
                    {
                        m_lastCasterStats.GpuDrawCount++;
                        m_lastCasterStats.SubmittedIndexCount +=
                            static_cast<u64>( range.IndexCount ) * packet.InstanceCount;
                    }
                }
            }
            else
            {
                SoftVertexMgr::MeshView view;
                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindMesh" );
                    if ( !g_SkinVertMgr.GetMeshView( packet.hMesh, view ) ||
                         !g_SkinVertMgr.BindPools( view.Geometry ) ||
                         !rbuffer_BindVertex( m_skinBoneBaseBuffer, 1, 0 ) )
                    {
                        continue;
                    }
                }

                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/PushDrawConstants" );
                    if ( !SetShadowCastConstants( m_skinCastConstantsSlot, source.WorldToClip ) )
                    {
                        continue;
                    }
                }

                for ( s32 s = 0; s < view.SectionCount; ++s )
                {
                    SoftVertexMgr::SoftSection const& section = view.pSections[s];
                    {
                        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/Draw" );
                        if ( rdraw_DrawIndexedInstanced( section.IndexCount, packet.InstanceCount,
                                                         view.Geometry.FirstIndex + section.FirstIndex,
                                                         view.Geometry.BaseVertex,
                                                         packet.FirstSkinDrawInstance + s * packet.InstanceCount ) )
                        {
                            m_lastCasterStats.GpuDrawCount++;
                            m_lastCasterStats.SubmittedIndexCount +=
                                static_cast<u64>( section.IndexCount ) * packet.InstanceCount;
                        }
                    }
                }
            }
        }

        if( preparedDynamicDraws.GetCount() > 0 )
        {
            s32 const sourceCount = g_ShadowMapMgr.GetSourceCount();
            for( s32 i = 0; i < preparedDynamicDraws.GetCount(); ++i )
            {
                PreparedDynamicShadowDraw const& draw = preparedDynamicDraws[i];
                for( s32 sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex )
                {
                    if( ( draw.ShadowSourceMask & ( u64{ 1 } << sourceIndex ) ) == 0 )
                        continue;

                    s32 const casterVariant = SHADOW_CASTER_DYNAMIC;
                    if( ( sourceIndex != m_currentSource ) ||
                        ( casterVariant != m_currentCasterVariant ) )
                    {
                        m_lastCasterStats.SourceStateChangeCount++;
                    }
                    ApplyDynamicSource( sourceIndex );
                    if( ( sourceIndex != m_currentSource ) ||
                        ( casterVariant != m_currentCasterVariant ) )
                    {
                        continue;
                    }

                    ShadowMapMgr::ShadowSource const& source = g_ShadowMapMgr.GetSource( sourceIndex );
                    if( !SetShadowCastConstants( m_dynamicCastConstantsSlot, source.WorldToClip ) ||
                        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL,
                                                                     m_dynamicDiffuseTextureSlot,
                                                                     draw.pDiffuse, &m_diffuseSampler ) ) ||
                        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL,
                                                                     m_dynamicDamageTextureSlot,
                                                                     draw.pDamageMask, &m_evsmLinearSampler ) ) ||
                        !rbuffer_BindVertex( m_dynamicVertexBuffer, 0, 0 ) ||
                        !rbuffer_BindIndex( m_dynamicIndexBuffer, RBUFFER_INDEX_FORMAT_U16 ) )
                    {
                        continue;
                    }

                    if( rdraw_DrawIndexed( draw.IndexCount, draw.FirstIndex,
                                          static_cast<s32>( draw.FirstVertex ) ) )
                    {
                        m_lastCasterStats.GpuDrawCount++;
                        m_lastCasterStats.SubmittedIndexCount += draw.IndexCount;
                    }
                }
            }
        }

        EndCastPass();
    }
}

//==============================================================================

void shadow_mgr::EndShadowShaders( void )
{
    EndCastPass();

    g_GeomMgr.InvalidateCache();

    m_currentSource = -1;
    m_currentCasterVariant = SHADOW_CASTER_NONE;
}

//==============================================================================
//  RUNTIME QUERIES
//==============================================================================

rtarget const* shadow_mgr::GetShadowSampleAtlasTarget( void ) const
{
    if( !m_isShadowSampleAtlasReady )
    {
        return NULL;
    }

    rtarget const& sampleAtlas = ( g_ShadowMapMgr.GetShadowFilterType() == ShadowFilterType::Evsm )
                                     ? m_shadowMomentAtlas
                                     : m_shadowAtlas;
    return rtarget_HasShaderResource( sampleAtlas ) ? &sampleAtlas : NULL;
}

//==============================================================================

rtarget const* shadow_mgr::GetShadowDepthAtlasTarget( void ) const
{
    if( !m_isShadowSampleAtlasReady )
    {
        return NULL;
    }

    return rtarget_HasShaderResource( m_shadowAtlas ) ? &m_shadowAtlas : NULL;
}

//==============================================================================

f32 shadow_mgr::GetShadowNormalBiasTexels( void ) const
{
    return m_shadowNormalBiasTexels;
}

//==============================================================================

f32 shadow_mgr::GetShadowSeamBlendTexels( void ) const
{
    return m_shadowSeamBlendTexels;
}

//==============================================================================

f32 shadow_mgr::GetAtlasTexelSize( void ) const
{
    s32 const shadowAtlasSize = ( m_shadowAtlasSize > 0 ) ? m_shadowAtlasSize : MAX_SHADOW_ATLAS_SIZE;
    return 1.0f / static_cast<f32>( shadowAtlasSize );
}

//==============================================================================

vector4 shadow_mgr::GetShadowFilterParams( void ) const
{
    s32 const sampleAtlasSize = ( g_ShadowMapMgr.GetShadowFilterType() == ShadowFilterType::Evsm )
                                    ? m_shadowMomentAtlasSize
                                    : m_shadowAtlasSize;
    f32 const sampleAtlasTexelSize =
        ( sampleAtlasSize > 0 ) ? ( 1.0f / static_cast<f32>( sampleAtlasSize ) ) : 1.0f;
    return vector4( sampleAtlasTexelSize, kShadowEVSMPositiveExponent, kShadowEVSMNegativeExponent,
                    kShadowEVSMLightBleedReduction );
}

//==============================================================================

shadow_mgr::caster_stats const& shadow_mgr::GetLastCasterStats( void ) const
{
    return m_lastCasterStats;
}
