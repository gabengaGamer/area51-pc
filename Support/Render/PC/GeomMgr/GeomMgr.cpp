//==============================================================================
//
//  GeomMgr.cpp
//
//  Geom Manager for PC platform
//
//==============================================================================

//==============================================================================
//  BASE INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GeomMgr.hpp"

#include "../../LightMgr.hpp"
#include "../../ProjTextureMgr.hpp"
#include "../../RigidGeom.hpp"
#include "../../SkinGeom.hpp"
#include "../VertexMgr.hpp"
#include "../SoftVertexMgr.hpp"
#include "../GeomStorage.hpp"
#include "../PostMgr/PostMgr.hpp"

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

GeomMgr g_GeomMgr;

//==============================================================================

GeomMgr::GeomResourceSnapshot::GeomResourceSnapshot( void )
    : pDiffuse( NULL ), pDetail( NULL ), pEnvironment( NULL ), pEnvironmentCube( NULL ), bDistortionActive( FALSE )
{
    DistortionNormalRot.Zero();
}

//==============================================================================

GeomMgr::GeomDrawPacket::GeomDrawPacket( void )
    : Kind( GEOM_PACKET_RIGID ), pMaterial( NULL ), hMesh(), RenderFlags( 0 ), FirstInstance( 0 ), InstanceCount( 0 ),
      UOffset( 0 ), VOffset( 0 ), MaterialOverride( FALSE ), Pass( GEOMETRY_PASS_GBUFFER ), Sequence( 0 ),
      SortDepth( 0.0f ), FirstSkinDrawInstance( 0 ), FirstDynamicVertex( 0 ), FirstDynamicIndex( 0 ),
      DynamicIndexCount( 0 ), DynamicLightingIndex( 0 ), pDamageMask( NULL ), pDamageTexture( NULL ),
      pDamageUpload( NULL ),
      pDamageUploadPending( NULL ), DamageUploadX( 0 ), DamageUploadY( 0 ), DamageUploadWidth( 0 ),
      DamageUploadHeight( 0 ), Resources()
{
    hMesh.Handle = HNULL;
}

//==============================================================================

GeomMgr::ShaderBindingLayout::ShaderBindingLayout( void )
    : FrameConstantsVertex( 0xFFFFFFFFu ), FrameConstantsPixel( 0xFFFFFFFFu ), ProjTexturesPixel( 0xFFFFFFFFu ),
      ShadowMatricesPixel( 0xFFFFFFFFu ), ShadowDataPixel( 0xFFFFFFFFu ), DiffuseTexturePixel( 0xFFFFFFFFu ),
      DetailTexturePixel( 0xFFFFFFFFu ), EnvironmentTexturePixel( 0xFFFFFFFFu ),
      EnvironmentCubeTexturePixel( 0xFFFFFFFFu ), FaceShadowTexturePixel( 0xFFFFFFFFu ),
      FaceShadowDepthTexturePixel( 0xFFFFFFFFu ), DistortionTexturePixel( 0xFFFFFFFFu ),
      ProjectionAtlasTexturePixel( 0xFFFFFFFFu ),
      InstanceDataVertex( 0xFFFFFFFFu ), AuxiliaryDataVertex( 0xFFFFFFFFu ), InstanceDataPixel( 0xFFFFFFFFu ),
      LightingDataPixel( 0xFFFFFFFFu ), BoneRemapDataVertex( 0xFFFFFFFFu )
{
}

//==============================================================================

namespace
{
struct SkinRemapCacheEntry
{
    s32 MeshHandle;
    s32 SectionCount;
    u32 FirstRemap;
};

xbool CreateSolidTexture( vram_texture& texture, vram_texture_type type, u32 color, char const* pDebugName )
{
    vram_texture_desc desc;
    desc.Type = type;
    desc.Width = 1;
    desc.Height = 1;
    desc.LayerCount = ( type == VRAM_TEXTURE_TYPE_CUBE ) ? 6 : 1;
    desc.MipCount = 1;
    desc.Format = VRAM_TEXTURE_FORMAT_RGBA8;
    desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    desc.pDebugName = pDebugName;

    if ( !vram_CreateTexture( texture, desc ) )
    {
        return FALSE;
    }

    for ( u32 layer = 0; layer < desc.LayerCount; layer++ )
    {
        vram_texture_upload_desc upload;
        upload.Region.Layer = layer;
        upload.Region.Width = 1;
        upload.Region.Height = 1;
        upload.pData = &color;
        upload.Size = sizeof( color );
        upload.RowPitch = sizeof( color );
        upload.SlicePitch = sizeof( color );

        if ( !vram_UploadTexture( texture, upload ) )
        {
            vram_DestroyTexture( texture );
            return FALSE;
        }
    }

    return TRUE;
}

xbool EnsureBuffer( rbuffer& buffer, u32& capacity, u32 requiredCount, u32 elementStride, char const* pDebugName,
                    u32 usageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ )
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

xbool EnsureSequentialIndexBuffer( rbuffer& buffer, u32& capacity, u32 requiredCount, char const* pDebugName )
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

char const* GeomTextureSlotName( texture_slot slot )
{
    switch ( slot )
    {
        case TEXTURE_SLOT_DIFFUSE:
            {
                return "diffuse";
            }
        case TEXTURE_SLOT_DETAIL:
            {
                return "detail";
            }
        case TEXTURE_SLOT_ENVIRONMENT:
            {
                return "environment";
            }
        case TEXTURE_SLOT_ENVIRONMENT_CUBE:
            {
                return "environment cube";
            }
        default:
            {
                return "unknown";
            }
    }
}

char const* GeomTextureBindingName( texture_slot slot )
{
    switch ( slot )
    {
        case TEXTURE_SLOT_DIFFUSE:
            {
                return "txDiffuse";
            }
        case TEXTURE_SLOT_DETAIL:
            {
                return "txDetail";
            }
        case TEXTURE_SLOT_ENVIRONMENT:
            {
                return "txEnvironment";
            }
        case TEXTURE_SLOT_ENVIRONMENT_CUBE:
            {
                return "txEnvironmentCube";
            }
        default:
            {
                return NULL;
            }
    }
}

xbool ResolveBindingSlot( shader const& shader, shader_binding_kind kind, char const* pName, u32& slot )
{
    if ( shader_FindBindingSlot( shader, kind, pName, slot ) )
    {
        return TRUE;
    }

    x_DebugMsg( "GeomMgr: Shader binding '%s' was not found\n", pName );
    return FALSE;
}

xbool ResolveSharedPixelBindingSlot( shader const& pixelShader, shader const& scenePixelShader,
                                     shader_binding_kind kind, char const* pName, u32& slot )
{
    u32 sceneSlot = 0;
    if ( !ResolveBindingSlot( pixelShader, kind, pName, slot ) ||
         !ResolveBindingSlot( scenePixelShader, kind, pName, sceneSlot ) )
    {
        return FALSE;
    }

    if ( slot != sceneSlot )
    {
        x_DebugMsg( "GeomMgr: Shader binding '%s' uses different slots (%u and %u)\n", pName, slot, sceneSlot );
        return FALSE;
    }

    return TRUE;
}

} // namespace

//==============================================================================
//  INITIALIZATION / SHUTDOWN
//==============================================================================

void GeomMgr::Init( void )
{
    if ( m_isInitialized )
    {
        return;
    }

    x_DebugMsg( "GeomMgr: Initializing shaders\n" );

    // Initialize member variables
    m_activeShaderKind = GEOM_SHADER_NONE;
    m_activeSamplerPreset = RSTATE_SAMPLER_PRESET_ANISOTROPIC_WRAP;
    m_isDistortionStateActive = FALSE;
    m_pDistortionSceneResource = NULL;
    m_pDiffuseResource = NULL;
    m_pDetailResource = NULL;
    m_pEnvironmentResource = NULL;
    m_pEnvironmentCubeResource = NULL;
    m_pCachedFaceShadowResource = NULL;
    m_pCachedFaceShadowDepthResource = NULL;
    m_faceShadowSamplerPreset = RSTATE_SAMPLER_PRESET_POINT_CLAMP;
    m_shadowMatrixBuffer = rbuffer();
    m_shadowDataBuffer = rbuffer();
    m_areShadowMapConstantsValid = FALSE;
    m_distortionNormalRot.Zero();
    m_defaultDistortionMaterial = material();
    m_defaultDistortionMaterial.m_Type = Material_Distortion;
    m_defaultDistortionMaterial.m_detailScale = 1.0f;
    m_defaultDistortionMaterial.m_fixedAlpha = 0.0f;
    m_defaultDistortionMaterial.Finalize();
    m_lightingDataBuffer = rbuffer();
    m_lightingDataCapacity = 0;

    // Initialize shaders and resources
    if ( !InitSamplers() || !CreateSolidTexture( m_whiteTexture, VRAM_TEXTURE_TYPE_2D, 0xFFFFFFFF, "GeomWhite" ) ||
         !CreateSolidTexture( m_blackTexture, VRAM_TEXTURE_TYPE_2D, 0xFF000000, "GeomBlack" ) ||
         !CreateSolidTexture( m_blackCubeTexture, VRAM_TEXTURE_TYPE_CUBE, 0xFF000000, "GeomBlackCube" ) ||
         !InitRigidShaders() || !InitSkinShaders() || !InitDynamicGeometry() || !InitShaderBindings() || !InitProjTextures() ||
         !InitShadowMaps() || !PrewarmPipelines() )
    {
        m_isInitialized = TRUE;
        Kill();
        return;
    }

    m_isInitialized = TRUE;
    x_DebugMsg( "GeomMgr: Shaders initialized successfully\n" );
}

//==============================================================================

void GeomMgr::Kill( void )
{
    if ( !m_isInitialized )
    {
        return;
    }

    x_DebugMsg( "GeomMgr: Shutting down shaders\n" );

    ClearPackets();
    m_lDrawPackets.Clear();
    m_lRigidFrameInstances.Clear();
    m_lRigidFrameLighting.Clear();
    m_lRigidFrameColors.Clear();
    m_lRigidIndirectCommands.Clear();
    m_lRigidIndirectRuns.Clear();
    m_lSkinFrameInstances.Clear();
    m_lSkinFrameLighting.Clear();
    m_lSkinFrameBones.Clear();
    m_lSkinDrawInstances.Clear();
    m_lSkinBoneRemaps.Clear();
    m_lSkinIndirectCommands.Clear();
    m_lSkinIndirectRuns.Clear();
    m_lFrameLighting.Clear();
    m_lLightingReferences.Clear();
    m_lOrderedDraws.Clear();
    rbuffer_Destroy( m_lightingDataBuffer );
    m_lightingDataCapacity = 0;
    KillShaderBindings();
    KillDynamicGeometry();
    KillRigidShaders();
    KillSkinShaders();
    KillProjTextures();
    KillShadowMaps();
    vram_DestroyTexture( m_blackCubeTexture );
    vram_DestroyTexture( m_blackTexture );
    vram_DestroyTexture( m_whiteTexture );
    KillSamplers();
    ClearDistortionState();
    SetEnvironmentCubemap( NULL );
    InvalidateCache();

    m_isInitialized = FALSE;
    x_DebugMsg( "GeomMgr: Shaders shutdown complete\n" );
}

//==============================================================================

u32 GeomMgr::BuildBatchStateFlags( u32 renderFlags )
{
    u32 const batchStateMask =
        render::WIREFRAME | render::WIREFRAME2 | render::FADING_ALPHA | render::INSTFLAG_FADING_ALPHA;

    return renderFlags & batchStateMask;
}

//==============================================================================

u32 GeomMgr::BuildInstanceFlags( u32 renderFlags )
{
    u32 shaderFlags = 0;

    if ( renderFlags & render::GLOWING )
    {
        shaderFlags |= INSTANCE_FLAG_GLOWING;
    }

    if ( renderFlags & render::INSTFLAG_SPOTLIGHT )
    {
        shaderFlags |= INSTANCE_FLAG_PROJ_LIGHT;
    }

    if ( renderFlags & render::INSTFLAG_PROJ_SHADOW )
    {
        shaderFlags |= INSTANCE_FLAG_PROJ_SHADOW;
    }

    if ( renderFlags & render::INSTFLAG_FADING_ALPHA )
    {
        shaderFlags |= INSTANCE_FLAG_FADING_ALPHA;
    }

    if ( renderFlags & render::INSTFLAG_DYNAMICLIGHT )
    {
        shaderFlags |= INSTANCE_FLAG_DYNAMIC_LIGHT;
    }

    if ( renderFlags & render::INSTFLAG_FILTERLIGHT )
    {
        shaderFlags |= INSTANCE_FLAG_FILTERLIGHT;
    }

    return shaderFlags;
}

//==============================================================================

f32 GeomMgr::BuildInstanceFadeAlpha( u32 renderFlags, u8 alpha )
{
    if ( renderFlags & ( render::FADING_ALPHA | render::INSTFLAG_FADING_ALPHA ) )
    {
        return static_cast<f32>( alpha ) / 255.0f;
    }

    return 1.0f;
}

//==============================================================================

GeomFrameConstants GeomMgr::BuildFrameConstants( view const& view, material const* pMaterial, u8 uOffset, u8 vOffset,
                                                 xbool includeVertexColor, u8 overrideMat,
                                                 geom_pass_desc const& pass ) const
{
    ASSERT( ( pass.TargetWidth > 0 ) && ( pass.TargetHeight > 0 ) );
    xbool const bOverrideMaterial = ( overrideMat != FALSE );
    f32 const   kInvByte = 1.0f / 255.0f;
    f32 const   kDefaultDetailScale = 1.0f;
    f32 const   kDistortionPixelScale = 8.0f;

    GeomFrameConstants frameData;
    x_memset( &frameData, 0, sizeof( GeomFrameConstants ) );

    f32 nearZ = 0.0f;
    f32 farZ = 0.0f;
    view.GetZLimits( nearZ, farZ );

    matrix4 const viewMatrix( view.GetW2V() );

    frameData.View = viewMatrix;
    frameData.Projection = view.GetV2C();
    frameData.NearZ = nearZ;
    frameData.FarZ = farZ;

    vector3 const& camPos = view.GetPosition();
    frameData.CameraPosition.Set( camPos.GetX(), camPos.GetY(), camPos.GetZ(), 1.0f );

    f32 const detailScale = pMaterial ? pMaterial->GetDetailScale() : kDefaultDetailScale;

    frameData.UVAnim.Set( static_cast<f32>( uOffset ) * kInvByte, static_cast<f32>( vOffset ) * kInvByte, detailScale,
                          0.0f );

    u32 shaderFeatures = ( !bOverrideMaterial && pMaterial ) ? pMaterial->GetShaderFeatures() : 0;
    if ( shaderFeatures && includeVertexColor )
    {
        shaderFeatures |= MATERIAL_SHADER_VERTEX_COLOR;
    }
    if ( !bOverrideMaterial && pMaterial && pMaterial->ReceivesShadow() )
    {
        shaderFeatures |= INSTANCE_FLAG_RECEIVE_LOCAL_SHADOW;
    }

    frameData.MaterialFlags = shaderFeatures;
    frameData.AlphaRef = ( !bOverrideMaterial && pMaterial ) ? pMaterial->GetAlphaRef() : 0.0f;

    f32 const fixedAlpha = ( !bOverrideMaterial && pMaterial ) ? pMaterial->GetFixedAlpha() : 0.0f;
    f32 const cubeIntensity = bOverrideMaterial ? 0.0f : ( pMaterial ? pMaterial->GetCubeMapIntensity() : 1.0f );
    frameData.EnvParams.Set( fixedAlpha, cubeIntensity, 1.0f, bOverrideMaterial ? 1.0f : 0.0f );

    matrix4 distortionNormalMatrix( viewMatrix );
    if ( m_isDistortionStateActive )
    {
        matrix4 distortionRot( m_distortionNormalRot );
        distortionNormalMatrix = viewMatrix * distortionRot;
    }

    f32 const invSceneWidth = 1.0f / static_cast<f32>( pass.TargetWidth );
    f32 const invSceneHeight = 1.0f / static_cast<f32>( pass.TargetHeight );

    frameData.DistortionNormalMatrix = distortionNormalMatrix;
    frameData.DistortionParams.Set( kDistortionPixelScale, 0.0f, invSceneWidth, invSceneHeight );
    g_PostMgr.GetGeometryFogConstants( frameData.FogColor, frameData.FogCoeff, frameData.FogParams );
    return frameData;
}

//==============================================================================

xbool GeomMgr::InitSamplers( void )
{
    for ( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        if ( !rstate_CreateSampler( m_samplers[i], static_cast<rstate_sampler_preset>( i ), "GeomSampler" ) )
        {
            KillSamplers();
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

void GeomMgr::KillSamplers( void )
{
    for ( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        rstate_DestroySampler( m_samplers[i] );
    }
}

//==============================================================================

GeomMgr::ShaderBindingLayout const* GeomMgr::GetShaderBindings( geom_shader_kind kind ) const
{
    switch ( kind )
    {
        case GEOM_SHADER_RIGID:
            {
                return &m_rigidShaderBindings;
            }
        case GEOM_SHADER_SKIN:
            {
                return &m_skinShaderBindings;
            }
        case GEOM_SHADER_DYNAMIC:
            {
                return &m_dynamicShaderBindings;
            }
        default:
            {
                return NULL;
            }
    }
}

//==============================================================================

xbool GeomMgr::ResolveShaderBindings( ShaderBindingLayout& bindings, shader const& vertexShader,
                                      shader const& pixelShader, shader const& scenePixelShader,
                                      char const* pInstanceName, char const* pAuxiliaryName,
                                      char const* pBoneRemapName )
{
    bindings = ShaderBindingLayout();

    if ( !ResolveBindingSlot( vertexShader, SHADER_BINDING_UNIFORM_BUFFER, "cbFrameConstants",
                              bindings.FrameConstantsVertex ) ||
         !ResolveBindingSlot( vertexShader, SHADER_BINDING_STORAGE_BUFFER, pInstanceName,
                              bindings.InstanceDataVertex ) ||
         !ResolveBindingSlot( vertexShader, SHADER_BINDING_STORAGE_BUFFER, pAuxiliaryName,
                              bindings.AuxiliaryDataVertex ) )
    {
        return FALSE;
    }

    if ( pBoneRemapName && !ResolveBindingSlot( vertexShader, SHADER_BINDING_STORAGE_BUFFER, pBoneRemapName,
                                                bindings.BoneRemapDataVertex ) )
    {
        return FALSE;
    }

    if ( !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_UNIFORM_BUFFER,
                                         "cbFrameConstants", bindings.FrameConstantsPixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_UNIFORM_BUFFER, "cbProjTextures",
                                         bindings.ProjTexturesPixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_STORAGE_BUFFER,
                                         "FaceShadowMatrices", bindings.ShadowMatricesPixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_STORAGE_BUFFER, "ShadowMapData",
                                         bindings.ShadowDataPixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_SAMPLED_TEXTURE, "txDiffuse",
                                         bindings.DiffuseTexturePixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_SAMPLED_TEXTURE, "txDetail",
                                         bindings.DetailTexturePixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_SAMPLED_TEXTURE, "txEnvironment",
                                         bindings.EnvironmentTexturePixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_SAMPLED_TEXTURE,
                                         "txEnvironmentCube", bindings.EnvironmentCubeTexturePixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_SAMPLED_TEXTURE,
                                         "txFaceShadowAtlas", bindings.FaceShadowTexturePixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_SAMPLED_TEXTURE,
                                         "txFaceShadowDepthAtlas", bindings.FaceShadowDepthTexturePixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_SAMPLED_TEXTURE,
                                         "txDistortionScene", bindings.DistortionTexturePixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_SAMPLED_TEXTURE,
                                         "txProjectionAtlas", bindings.ProjectionAtlasTexturePixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_STORAGE_BUFFER, pInstanceName,
                                         bindings.InstanceDataPixel ) ||
         !ResolveSharedPixelBindingSlot( pixelShader, scenePixelShader, SHADER_BINDING_STORAGE_BUFFER, "GeomLighting",
                                         bindings.LightingDataPixel ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool GeomMgr::InitShaderBindings( void )
{
    if ( !ResolveShaderBindings( m_rigidShaderBindings, m_rigidVertexShader, m_rigidPixelShader,
                                 m_rigidScenePixelShader, "RigidInstances", "RigidVertexColors", NULL ) ||
         !ResolveShaderBindings( m_skinShaderBindings, m_skinVertexShader, m_skinPixelShader, m_skinScenePixelShader,
                                 "SkinInstances", "SkinBoneMatrices", "SkinBoneRemaps" ) )
    {
        KillShaderBindings();
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void GeomMgr::KillShaderBindings( void )
{
    m_rigidShaderBindings = ShaderBindingLayout();
    m_skinShaderBindings = ShaderBindingLayout();
}

//==============================================================================

rstate_sampler const* GeomMgr::GetSampler( rstate_sampler_preset preset ) const
{
    if ( ( preset < 0 ) || ( preset >= RSTATE_SAMPLER_PRESET_COUNT ) )
    {
        preset = RSTATE_SAMPLER_PRESET_ANISOTROPIC_WRAP;
    }

    return m_samplers[preset] ? &m_samplers[preset] : NULL;
}

//==============================================================================

xbool GeomMgr::PushFrameConstants( geom_shader_kind kind, GeomFrameConstants const& frame ) const
{
    ShaderBindingLayout const* pBindings = GetShaderBindings( kind );
    if ( !pBindings )
    {
        return FALSE;
    }

    if ( !shader_PushUniformData( SHADER_STAGE_VERTEX, pBindings->FrameConstantsVertex, &frame, sizeof( frame ) ) )
    {
        return FALSE;
    }

    return shader_PushUniformData( SHADER_STAGE_PIXEL, pBindings->FrameConstantsPixel, &frame, sizeof( frame ) );
}

//==============================================================================

xbool GeomMgr::BindPixelTexture( u32 slot, shader_resource const* pResource, rstate_sampler const* pSampler ) const
{
    if ( !pResource || !*pResource || !pSampler || !*pSampler )
    {
        return FALSE;
    }

    return shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, slot, pResource, pSampler ) );
}

//==============================================================================

GeomMgr::GeomResourceSnapshot GeomMgr::CaptureResourceSnapshot( void ) const
{
    GeomResourceSnapshot resources;
    resources.pDiffuse = m_pDiffuseResource;
    resources.pDetail = m_pDetailResource;
    resources.pEnvironment = m_pEnvironmentResource;
    resources.pEnvironmentCube = m_pEnvironmentCubeResource;
    resources.bDistortionActive = m_isDistortionStateActive;
    resources.DistortionNormalRot = m_distortionNormalRot;
    return resources;
}

//==============================================================================

xbool GeomMgr::BindPacketResources( GeomDrawPacket const& packet )
{
    ShaderBindingLayout const* pBindings = GetShaderBindings( m_activeShaderKind );
    rstate_sampler const*      pMaterialSampler = GetSampler( m_activeSamplerPreset );
    shader_resource const*     pWhiteResource = vram_GetShaderResource( m_whiteTexture );
    shader_resource const*     pBlackResource = vram_GetShaderResource( m_blackTexture );
    shader_resource const*     pBlackCubeResource = vram_GetShaderResource( m_blackCubeTexture );
    shader_resource const*     pFaceShadowResource =
        m_pCachedFaceShadowResource ? m_pCachedFaceShadowResource : pWhiteResource;
    shader_resource const* pFaceShadowDepthResource =
        m_pCachedFaceShadowDepthResource ? m_pCachedFaceShadowDepthResource : pWhiteResource;
    shader_resource const* pProjectionAtlasResource = g_ProjectionAtlas.GetShaderResource();
    rstate_sampler const*  pShadowSampler = GetSampler( m_faceShadowSamplerPreset );
    rstate_sampler const*  pShadowDepthSampler = GetSampler( RSTATE_SAMPLER_PRESET_POINT_CLAMP );
    rstate_sampler const*  pProjectionSampler = GetSampler( RSTATE_SAMPLER_PRESET_LINEAR_CLAMP );

    if ( !pBindings || !pMaterialSampler || !pShadowSampler || !pShadowDepthSampler || !pProjectionSampler ||
         !pProjectionAtlasResource || !pWhiteResource || !pBlackResource || !pBlackCubeResource )
    {
        return FALSE;
    }

    if ( !BindPixelTexture( pBindings->DiffuseTexturePixel,
                            packet.Resources.pDiffuse ? packet.Resources.pDiffuse : pWhiteResource, pMaterialSampler ) )
    {
        return FALSE;
    }

    if ( !BindPixelTexture( pBindings->DetailTexturePixel,
                            packet.Resources.pDetail ? packet.Resources.pDetail : pWhiteResource, pMaterialSampler ) )
    {
        return FALSE;
    }

    if ( !BindPixelTexture( pBindings->EnvironmentTexturePixel,
                            packet.Resources.pEnvironment ? packet.Resources.pEnvironment : pBlackResource,
                            pMaterialSampler ) )
    {
        return FALSE;
    }

    if ( !BindPixelTexture( pBindings->EnvironmentCubeTexturePixel,
                            packet.Resources.pEnvironmentCube ? packet.Resources.pEnvironmentCube : pBlackCubeResource,
                            pMaterialSampler ) )
    {
        return FALSE;
    }

    if ( !BindPixelTexture( pBindings->FaceShadowTexturePixel, pFaceShadowResource, pShadowSampler ) )
    {
        return FALSE;
    }

    if ( !BindPixelTexture( pBindings->FaceShadowDepthTexturePixel, pFaceShadowDepthResource, pShadowDepthSampler ) )
    {
        return FALSE;
    }

    if ( !BindPixelTexture( pBindings->DistortionTexturePixel,
                            m_pDistortionSceneResource ? m_pDistortionSceneResource : pBlackResource,
                            pMaterialSampler ) )
    {
        return FALSE;
    }

    if ( !BindPixelTexture( pBindings->ProjectionAtlasTexturePixel, pProjectionAtlasResource, pProjectionSampler ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void GeomMgr::BeginPacketCollection( void )
{
    ClearPackets();
    m_gBufferGpuDrawCount = 0;
    m_gBufferInstanceCount = 0;
    m_gBufferSkinSectionDrawCount = 0;
    m_gBufferSubmittedIndexCount = 0;
    InvalidateCache();
}

//==============================================================================

s32 GeomMgr::CompareGBufferDraws( void const* pA, void const* pB )
{
    geometry_draw_item const& a = **static_cast<geometry_draw_item const* const*>( pA );
    geometry_draw_item const& b = **static_cast<geometry_draw_item const* const*>( pB );

    if ( a.MaterialOrder < b.MaterialOrder )
    {
        return -1;
    }
    if ( a.MaterialOrder > b.MaterialOrder )
    {
        return 1;
    }
    if ( a.Type < b.Type )
    {
        return -1;
    }
    if ( a.Type > b.Type )
    {
        return 1;
    }

    u32 const stateA = BuildBatchStateFlags( a.Flags );
    u32 const stateB = BuildBatchStateFlags( b.Flags );
    if ( stateA < stateB )
    {
        return -1;
    }
    if ( stateA > stateB )
    {
        return 1;
    }
    if ( a.UOffset < b.UOffset )
    {
        return -1;
    }
    if ( a.UOffset > b.UOffset )
    {
        return 1;
    }
    if ( a.VOffset < b.VOffset )
    {
        return -1;
    }
    if ( a.VOffset > b.VOffset )
    {
        return 1;
    }
    if ( a.MaterialOverride < b.MaterialOverride )
    {
        return -1;
    }
    if ( a.MaterialOverride > b.MaterialOverride )
    {
        return 1;
    }
    if ( a.hRenderGeom.Handle < b.hRenderGeom.Handle )
    {
        return -1;
    }
    if ( a.hRenderGeom.Handle > b.hRenderGeom.Handle )
    {
        return 1;
    }
    if ( a.iSurface < b.iSurface )
    {
        return -1;
    }
    if ( a.iSurface > b.iSurface )
    {
        return 1;
    }
    if ( a.Sequence < b.Sequence )
    {
        return -1;
    }
    if ( a.Sequence > b.Sequence )
    {
        return 1;
    }
    return 0;
}

//==============================================================================

void GeomMgr::ConfigureDrawResources( material const* pMaterial, geometry_render_pass pass,
                                      radian3 const& distortionNormalRot, cubemap const* pCubeMap )
{
    SetBitmap( NULL, TEXTURE_SLOT_DIFFUSE );
    SetBitmap( NULL, TEXTURE_SLOT_DETAIL );
    SetBitmap( NULL, TEXTURE_SLOT_ENVIRONMENT );
    SetEnvironmentCubemap( NULL );
    ClearDistortionState();

    if ( pass == GEOMETRY_PASS_ZPRIME )
    {
        return;
    }

    if ( pass == GEOMETRY_PASS_DISTORTION )
    {
        if ( pMaterial && pMaterial->IsDistortionPerPolyEnv() )
        {
            if ( pMaterial->UsesCubeMap() )
            {
                SetEnvironmentCubemap( pCubeMap );
            }
            else
            {
                SetBitmap( pMaterial->m_environmentMap.GetPointer(), TEXTURE_SLOT_ENVIRONMENT );
            }
        }

        SetDistortionState( distortionNormalRot );
        return;
    }

    if ( !pMaterial )
    {
        return;
    }

    SetBitmap( pMaterial->m_diffuseMap.GetPointer(), TEXTURE_SLOT_DIFFUSE );
    SetBitmap( pMaterial->m_detailMap.GetPointer(), TEXTURE_SLOT_DETAIL );

    if ( pMaterial->UsesCubeMap() )
    {
        SetEnvironmentCubemap( pCubeMap );
    }
    else
    {
        SetBitmap( pMaterial->m_environmentMap.GetPointer(), TEXTURE_SLOT_ENVIRONMENT );
    }
}

//==============================================================================

xbool GeomMgr::BuildPackets( xarray<geometry_draw_item> const& draws,
                             xarray<dynamic_geometry_draw> const& dynamicDraws,
                             cubemap const* pCubeMap )
{
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/ResetPackets" );
        BeginPacketCollection();
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/CollectAndSort" );
        for ( s32 i = 0; i < draws.GetCount(); ++i )
        {
            if ( draws[i].Pass == GEOMETRY_PASS_GBUFFER )
            {
                m_lOrderedDraws.Append() = &draws[i];
            }
        }

        s32 const gBufferDrawCount = m_lOrderedDraws.GetCount();
        if ( gBufferDrawCount > 1 )
        {
            x_qsort( m_lOrderedDraws.GetPtr(), gBufferDrawCount, sizeof( geometry_draw_item const* ),
                     CompareGBufferDraws );
        }

        for ( s32 i = 0; i < draws.GetCount(); ++i )
        {
            if ( draws[i].Pass != GEOMETRY_PASS_GBUFFER )
            {
                m_lOrderedDraws.Append() = &draws[i];
            }
        }
    }

    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/AssemblePackets" );

    xbool                batchActive = FALSE;
    geometry_draw_type   batchType = GEOMETRY_DRAW_RIGID;
    geometry_render_pass batchPass = GEOMETRY_PASS_GBUFFER;
    material const*      pBatchMaterial = NULL;
    u8                   batchOverride = FALSE;
    u32                  batchSequence = 0;

    for ( s32 i = 0; i < m_lOrderedDraws.GetCount(); ++i )
    {
        geometry_draw_item const& item = *m_lOrderedDraws[i];
        material const*           pMaterial = item.pMaterial;
        if ( ( item.Pass == GEOMETRY_PASS_DISTORTION ) && !pMaterial )
        {
            pMaterial = &m_defaultDistortionMaterial;
        }

        u8 const    materialOverride = item.MaterialOverride;
        xbool const depthSorted = ( item.Pass == GEOMETRY_PASS_TRANSPARENT ) || ( item.Pass == GEOMETRY_PASS_FADING ) ||
                                  ( item.Pass == GEOMETRY_PASS_DISTORTION );

        xbool canAppend = batchActive && !depthSorted && ( batchType == item.Type ) && ( batchPass == item.Pass ) &&
                          ( pBatchMaterial == pMaterial ) && ( batchOverride == materialOverride );

        RigidBatchDesc rigidBatch;
        SkinBatchDesc  skinBatch;
        if ( item.Type == GEOMETRY_DRAW_RIGID )
        {
            rigidBatch.pGeom = item.Data.Rigid.pGeom;
            rigidBatch.pL2W = item.Data.Rigid.pL2W;
            rigidBatch.pLighting = static_cast<cb_geom_lighting const*>( item.pLighting );
            rigidBatch.pColorInfo = item.Data.Rigid.pColorInfo;
            rigidBatch.hMesh = g_GeomStorage.GetRigidSurface( item.hRenderGeom, item.iSurface );
            rigidBatch.iSubMesh = item.iSurface;
            rigidBatch.RenderFlags = item.Flags;
            rigidBatch.UOffset = item.UOffset;
            rigidBatch.VOffset = item.VOffset;
            rigidBatch.Alpha = item.Alpha;
            rigidBatch.OverrideMat = materialOverride;
            rigidBatch.SortDepth = item.SortDepth;
            canAppend = canAppend && CanAppendRigidBatch( rigidBatch );
        }
        else
        {
            skinBatch.pGeom = item.Data.Skin.pGeom;
            skinBatch.pBones = item.Data.Skin.pBones;
            skinBatch.pLighting = static_cast<cb_geom_lighting const*>( item.pLighting );
            skinBatch.hMesh = g_GeomStorage.GetSkinSurface( item.hRenderGeom, item.iSurface );
            skinBatch.iSubMesh = item.iSurface;
            skinBatch.RenderFlags = item.Flags;
            skinBatch.UOffset = item.UOffset;
            skinBatch.VOffset = item.VOffset;
            skinBatch.Alpha = item.Alpha;
            skinBatch.OverrideMat = materialOverride;
            skinBatch.SortDepth = item.SortDepth;
            canAppend = canAppend && CanAppendSkinBatch( skinBatch );
        }

        if ( batchActive && !canAppend )
        {
            if ( batchType == GEOMETRY_DRAW_RIGID )
            {
                FlushRigidBatch( pBatchMaterial, batchOverride, batchPass, batchSequence );
            }
            else
            {
                FlushSkinBatch( pBatchMaterial, batchOverride, batchPass, batchSequence );
            }
            batchActive = FALSE;
        }

        if ( !batchActive )
        {
            ConfigureDrawResources( pMaterial, item.Pass, item.DistortionNormalRot, pCubeMap );
            batchType = item.Type;
            batchPass = item.Pass;
            pBatchMaterial = pMaterial;
            batchOverride = materialOverride;
            batchSequence = item.Sequence;
            batchActive = TRUE;
        }

        if ( item.Type == GEOMETRY_DRAW_RIGID )
        {
            AddRigidBatchInstance( rigidBatch );
        }
        else
        {
            AddSkinBatchInstance( skinBatch );
        }

        if ( depthSorted )
        {
            if ( batchType == GEOMETRY_DRAW_RIGID )
            {
                FlushRigidBatch( pBatchMaterial, batchOverride, batchPass, batchSequence );
            }
            else
            {
                FlushSkinBatch( pBatchMaterial, batchOverride, batchPass, batchSequence );
            }
            batchActive = FALSE;
        }
    }

    if ( batchActive )
    {
        if ( batchType == GEOMETRY_DRAW_RIGID )
        {
            FlushRigidBatch( pBatchMaterial, batchOverride, batchPass, batchSequence );
        }
        else
        {
            FlushSkinBatch( pBatchMaterial, batchOverride, batchPass, batchSequence );
        }
    }

    ClearDistortionState();
    if ( !BuildRigidIndirectRuns() )
    {
        return FALSE;
    }

    if( !BuildSkinDrawData() )
    {
        return FALSE;
    }

    return BuildDynamicPackets( dynamicDraws );
}

//==============================================================================

xbool GeomMgr::CanAppendRigidIndirectRun( RigidIndirectRun const& run, u32 packetIndex,
                                          GeomDrawPacket const& packet ) const
{
    if ( ( run.CommandCount == 0 ) || ( ( run.LastPacket + 1 ) != packetIndex ) ||
         ( run.VertexPool != packet.Range.VertexPool ) || ( run.IndexPool != packet.Range.IndexPool ) )
    {
        return FALSE;
    }

    GeomDrawPacket const& first = m_lDrawPackets[run.FirstPacket];
    return ( first.Kind == GEOM_PACKET_RIGID ) && ( packet.Kind == GEOM_PACKET_RIGID ) &&
           ( first.Pass == GEOMETRY_PASS_GBUFFER ) && ( packet.Pass == GEOMETRY_PASS_GBUFFER ) &&
           ( first.pMaterial == packet.pMaterial ) &&
           ( BuildBatchStateFlags( first.RenderFlags ) == BuildBatchStateFlags( packet.RenderFlags ) ) &&
           ( first.UOffset == packet.UOffset ) && ( first.VOffset == packet.VOffset ) &&
           ( first.MaterialOverride == packet.MaterialOverride ) &&
           ( first.Resources.pDiffuse == packet.Resources.pDiffuse ) &&
           ( first.Resources.pDetail == packet.Resources.pDetail ) &&
           ( first.Resources.pEnvironment == packet.Resources.pEnvironment ) &&
           ( first.Resources.pEnvironmentCube == packet.Resources.pEnvironmentCube ) &&
           ( first.Resources.bDistortionActive == packet.Resources.bDistortionActive );
}

//==============================================================================

xbool GeomMgr::BuildRigidIndirectRuns( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BuildIndirectRuns" );

    m_lRigidIndirectCommands.SetCount( 0 );
    m_lRigidIndirectRuns.SetCount( 0 );
    m_lDynamicFrameVertices.SetCount( 0 );
    m_lDynamicFrameIndices.SetCount( 0 );

    for ( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        GeomDrawPacket& packet = m_lDrawPackets[i];
        if ( packet.Kind != GEOM_PACKET_RIGID )
        {
            continue;
        }

        if ( !g_RigidVertMgr.GetMeshDrawRange( packet.hMesh, packet.Range ) )
        {
            return FALSE;
        }

        if ( ( packet.FirstInstance + packet.InstanceCount ) > static_cast<u32>( m_lRigidFrameInstances.GetCount() ) )
        {
            return FALSE;
        }

        if ( packet.Pass != GEOMETRY_PASS_GBUFFER )
        {
            continue;
        }

        s32 runIndex = -1;
        if ( m_lRigidIndirectRuns.GetCount() > 0 )
        {
            RigidIndirectRun& last = m_lRigidIndirectRuns[m_lRigidIndirectRuns.GetCount() - 1];
            if ( CanAppendRigidIndirectRun( last, i, packet ) )
            {
                runIndex = m_lRigidIndirectRuns.GetCount() - 1;
            }
        }

        if ( runIndex < 0 )
        {
            RigidIndirectRun& run = m_lRigidIndirectRuns.Append();
            run.FirstPacket = i;
            run.LastPacket = i;
            run.FirstCommand = m_lRigidIndirectCommands.GetCount();
            run.CommandCount = 0;
            run.VertexPool = packet.Range.VertexPool;
            run.IndexPool = packet.Range.IndexPool;
            runIndex = m_lRigidIndirectRuns.GetCount() - 1;
        }

        RigidIndirectRun&               run = m_lRigidIndirectRuns[runIndex];
        rdraw_indexed_indirect_command& command = m_lRigidIndirectCommands.Append();
        command.IndexCount = packet.Range.IndexCount;
        command.InstanceCount = packet.InstanceCount;
        command.FirstIndex = packet.Range.FirstIndex;
        command.BaseVertex = packet.Range.BaseVertex;
        command.FirstInstance = packet.FirstInstance;
        run.LastPacket = i;
        run.CommandCount++;
    }

    return TRUE;
}

//==============================================================================

xbool GeomMgr::CanAppendSkinIndirectRun( SkinIndirectRun const& run, GeomDrawPacket const& packet ) const
{
    if ( ( run.CommandCount == 0 ) || ( run.VertexPool != packet.Range.VertexPool ) ||
         ( run.IndexPool != packet.Range.IndexPool ) )
    {
        return FALSE;
    }

    GeomDrawPacket const& first = m_lDrawPackets[run.FirstPacket];
    return ( first.Kind == GEOM_PACKET_SKIN ) && ( packet.Kind == GEOM_PACKET_SKIN ) &&
           ( first.Pass == GEOMETRY_PASS_GBUFFER ) && ( packet.Pass == GEOMETRY_PASS_GBUFFER ) &&
           ( first.pMaterial == packet.pMaterial ) &&
           ( BuildBatchStateFlags( first.RenderFlags ) == BuildBatchStateFlags( packet.RenderFlags ) ) &&
           ( first.UOffset == packet.UOffset ) && ( first.VOffset == packet.VOffset ) &&
           ( first.MaterialOverride == packet.MaterialOverride ) &&
           ( first.Resources.pDiffuse == packet.Resources.pDiffuse ) &&
           ( first.Resources.pDetail == packet.Resources.pDetail ) &&
           ( first.Resources.pEnvironment == packet.Resources.pEnvironment ) &&
           ( first.Resources.pEnvironmentCube == packet.Resources.pEnvironmentCube ) &&
           ( first.Resources.bDistortionActive == packet.Resources.bDistortionActive );
}

//==============================================================================

xbool GeomMgr::BuildSkinDrawData( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BuildSkinDrawData" );

    m_lSkinDrawInstances.SetCount( 0 );
    m_lSkinBoneRemaps.SetCount( 0 );
    m_lSkinIndirectCommands.SetCount( 0 );
    m_lSkinIndirectRuns.SetCount( 0 );
    xarray<SkinRemapCacheEntry> remapCache;

    for ( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        GeomDrawPacket& packet = m_lDrawPackets[i];
        if ( packet.Kind != GEOM_PACKET_SKIN )
        {
            continue;
        }

        SoftVertexMgr::MeshView view;
        if ( !g_SkinVertMgr.GetMeshView( packet.hMesh, view ) || ( view.SectionCount <= 0 ) || !view.pSections )
        {
            x_DebugMsg( "GeomMgr: failed to resolve skin mesh for packet %d\n", i );
            return FALSE;
        }

        packet.Range = view.Geometry;
        if ( ( packet.FirstInstance + packet.InstanceCount ) > static_cast<u32>( m_lSkinFrameInstances.GetCount() ) )
        {
            x_DebugMsg( "GeomMgr: skin packet %d has invalid instances %u+%u/%d\n", i, packet.FirstInstance,
                        packet.InstanceCount, m_lSkinFrameInstances.GetCount() );
            return FALSE;
        }

        packet.FirstSkinDrawInstance = m_lSkinDrawInstances.GetCount();

        s32 remapCacheIndex = -1;
        for ( s32 r = 0; r < remapCache.GetCount(); ++r )
        {
            if ( remapCache[r].MeshHandle == packet.hMesh.Handle )
            {
                remapCacheIndex = r;
                break;
            }
        }

        xbool const bNewRemap = ( remapCacheIndex < 0 );
        if ( bNewRemap )
        {
            SkinRemapCacheEntry& entry = remapCache.Append();
            entry.MeshHandle = packet.hMesh.Handle;
            entry.SectionCount = view.SectionCount;
            entry.FirstRemap = m_lSkinBoneRemaps.GetCount();
            remapCacheIndex = remapCache.GetCount() - 1;
        }
        else if ( remapCache[remapCacheIndex].SectionCount != view.SectionCount )
        {
            x_DebugMsg( "GeomMgr: skin mesh %d changed section count within a frame\n", packet.hMesh.Handle );
            return FALSE;
        }

        for ( s32 s = 0; s < view.SectionCount; ++s )
        {
            SoftVertexMgr::SoftSection const& section = view.pSections[s];
            if ( ( section.FirstIndex < 0 ) || ( section.IndexCount <= 0 ) ||
                 ( ( section.FirstIndex + section.IndexCount ) > view.Geometry.IndexCount ) )
            {
                x_DebugMsg( "GeomMgr: skin packet %d has invalid section %d (%d+%d/%d)\n", i, s, section.FirstIndex,
                            section.IndexCount, view.Geometry.IndexCount );
                return FALSE;
            }

            u32 const boneRemapOffset = remapCache[remapCacheIndex].FirstRemap + s * SoftVertexMgr::MAX_BONE_PALETTE;
            if ( bNewRemap )
            {
                for ( s32 b = 0; b < SoftVertexMgr::MAX_BONE_PALETTE; ++b )
                {
                    u16 const boneIndex = section.BoneRemap[b];
                    m_lSkinBoneRemaps.Append() =
                        ( boneIndex == SoftVertexMgr::INVALID_BONE_REMAP ) ? 0u : static_cast<u32>( boneIndex );
                }
            }

            u32 const firstDrawInstance = m_lSkinDrawInstances.GetCount();
            for ( u32 j = 0; j < packet.InstanceCount; ++j )
            {
                SkinDrawInstance& drawInstance = m_lSkinDrawInstances.Append();
                drawInstance.InstanceIndex = packet.FirstInstance + j;
                drawInstance.BoneRemapOffset = boneRemapOffset;
            }

            if ( packet.Pass != GEOMETRY_PASS_GBUFFER )
            {
                continue;
            }

            s32 runIndex = -1;
            if ( m_lSkinIndirectRuns.GetCount() > 0 )
            {
                SkinIndirectRun& last = m_lSkinIndirectRuns[m_lSkinIndirectRuns.GetCount() - 1];
                if ( CanAppendSkinIndirectRun( last, packet ) )
                {
                    runIndex = m_lSkinIndirectRuns.GetCount() - 1;
                }
            }

            if ( runIndex < 0 )
            {
                SkinIndirectRun& run = m_lSkinIndirectRuns.Append();
                run.FirstPacket = i;
                run.FirstCommand = m_lSkinIndirectCommands.GetCount();
                run.CommandCount = 0;
                run.VertexPool = packet.Range.VertexPool;
                run.IndexPool = packet.Range.IndexPool;
                runIndex = m_lSkinIndirectRuns.GetCount() - 1;
            }

            SkinIndirectRun&                run = m_lSkinIndirectRuns[runIndex];
            rdraw_indexed_indirect_command& command = m_lSkinIndirectCommands.Append();
            command.IndexCount = section.IndexCount;
            command.InstanceCount = packet.InstanceCount;
            command.FirstIndex = view.Geometry.FirstIndex + section.FirstIndex;
            command.BaseVertex = view.Geometry.BaseVertex;
            command.FirstInstance = firstDrawInstance;
            run.CommandCount++;
        }
    }

    return TRUE;
}

//==============================================================================

xbool GeomMgr::RequestLightCookies( xarray<cb_geom_lighting const*> const& lighting ) const
{
    for ( s32 i = 0; i < lighting.GetCount(); ++i )
    {
        cb_geom_lighting const* pLighting = lighting[i];
        if ( !pLighting )
        {
            continue;
        }

        s32 const lightCount = MIN( pLighting->LightCount, MAX_GEOM_LIGHTS );
        for ( s32 j = 0; j < lightCount; ++j )
        {
            s32 const cookieIndex = static_cast<s32>( pLighting->LightCookieU[j].GetW() ) - 1;
            if ( cookieIndex < 0 )
            {
                continue;
            }

            if ( cookieIndex >= g_LightMgr.GetLightCookieCount() )
            {
                return FALSE;
            }

            texture::handle const& texture = g_LightMgr.GetLightCookieHandle( cookieIndex );
            if ( !texture.GetPointer() ||
                 !g_ProjectionAtlas.Request( texture, ProjectionAtlasEncoding::LuminanceAlpha ) )
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

//==============================================================================

xbool GeomMgr::PrepareProjectionAtlas( void )
{
    s32 const nProjLights = g_ProjTextureMgr.GetProjLightCount();
    for ( s32 i = 0; i < nProjLights; ++i )
    {
        texture::handle const& texture = g_ProjTextureMgr.GetProjLightTexture( i );
        if ( !texture.GetPointer() || !g_ProjectionAtlas.Request( texture, ProjectionAtlasEncoding::BlueMask ) )
        {
            return FALSE;
        }
    }

    s32 const nProjShadows = g_ProjTextureMgr.GetProjShadowCount();
    for ( s32 i = 0; i < nProjShadows; ++i )
    {
        texture::handle const& texture = g_ProjTextureMgr.GetProjShadowTexture( i );
        if ( !texture.GetPointer() || !g_ProjectionAtlas.Request( texture, ProjectionAtlasEncoding::BlueMask ) )
        {
            return FALSE;
        }
    }

    if ( !RequestLightCookies( m_lRigidFrameLighting ) || !RequestLightCookies( m_lSkinFrameLighting ) ||
         !RequestLightCookies( m_lDynamicFrameLighting ) )
    {
        return FALSE;
    }

    return g_ProjectionAtlas.Prepare();
}

//==============================================================================

void GeomMgr::ClearPackets( void )
{
    m_lOrderedDraws.SetCount( 0 );
    m_lDrawPackets.SetCount( 0 );
    m_lRigidFrameInstances.SetCount( 0 );
    m_lRigidFrameLighting.SetCount( 0 );
    m_lRigidFrameColors.SetCount( 0 );
    m_lSkinFrameInstances.SetCount( 0 );
    m_lSkinFrameLighting.SetCount( 0 );
    m_lDynamicFrameLighting.SetCount( 0 );
    m_lSkinFrameBones.SetCount( 0 );
    m_lSkinDrawInstances.SetCount( 0 );
    m_lSkinBoneRemaps.SetCount( 0 );
    m_lSkinIndirectCommands.SetCount( 0 );
    m_lSkinIndirectRuns.SetCount( 0 );
    m_lFrameLighting.SetCount( 0 );
    m_lLightingReferences.SetCount( 0 );
    m_lRigidIndirectCommands.SetCount( 0 );
    m_lRigidIndirectRuns.SetCount( 0 );
    m_litInstanceCount = 0;
    m_instanceLightCount = 0;
    ResetRigidBatch();
    ResetSkinBatch();
}

//==============================================================================

s32 GeomMgr::GetPacketCount( void ) const
{
    return m_lDrawPackets.GetCount();
}

//==============================================================================

geometry_render_pass GeomMgr::GetPacketPass( s32 packetIndex ) const
{
    ASSERT( ( packetIndex >= 0 ) && ( packetIndex < m_lDrawPackets.GetCount() ) );
    return m_lDrawPackets[packetIndex].Pass;
}

//==============================================================================

u32 GeomMgr::GetPacketSequence( s32 packetIndex ) const
{
    ASSERT( ( packetIndex >= 0 ) && ( packetIndex < m_lDrawPackets.GetCount() ) );
    return m_lDrawPackets[packetIndex].Sequence;
}

//==============================================================================

f32 GeomMgr::GetPacketSortDepth( s32 packetIndex ) const
{
    ASSERT( ( packetIndex >= 0 ) && ( packetIndex < m_lDrawPackets.GetCount() ) );
    return m_lDrawPackets[packetIndex].SortDepth;
}

//==============================================================================

s32 GeomMgr::GetRigidInstanceCount( void ) const
{
    return m_lRigidFrameInstances.GetCount();
}

//==============================================================================

s32 GeomMgr::GetSkinInstanceCount( void ) const
{
    return m_lSkinFrameInstances.GetCount();
}

//==============================================================================

s32 GeomMgr::GetLitInstanceCount( void ) const
{
    return m_litInstanceCount;
}

//==============================================================================

s32 GeomMgr::GetInstanceLightCount( void ) const
{
    return m_instanceLightCount;
}

//==============================================================================

s32 GeomMgr::GetLightingRecordCount( void ) const
{
    return MAX( m_lFrameLighting.GetCount() - 1, 0 );
}

//==============================================================================

u32 GeomMgr::GetGBufferGpuDrawCount( void ) const
{
    return m_gBufferGpuDrawCount;
}

//==============================================================================

u32 GeomMgr::GetGBufferInstanceCount( void ) const
{
    return m_gBufferInstanceCount;
}

//==============================================================================

u32 GeomMgr::GetGBufferSkinSectionDrawCount( void ) const
{
    return m_gBufferSkinSectionDrawCount;
}

//==============================================================================

u64 GeomMgr::GetGBufferSubmittedIndexCount( void ) const
{
    return m_gBufferSubmittedIndexCount;
}

//==============================================================================

u32 GeomMgr::GetGBufferRigidIndirectRunCount( void ) const
{
    return m_lRigidIndirectRuns.GetCount();
}

//==============================================================================

u32 GeomMgr::GetGBufferRigidIndirectCommandCount( void ) const
{
    return m_lRigidIndirectCommands.GetCount();
}

//==============================================================================

u32 GeomMgr::GetGBufferSkinIndirectRunCount( void ) const
{
    return m_lSkinIndirectRuns.GetCount();
}

//==============================================================================

u32 GeomMgr::GetGBufferSkinIndirectCommandCount( void ) const
{
    return m_lSkinIndirectCommands.GetCount();
}

//==============================================================================

s32 GeomMgr::CompareLightingReferences( void const* pA, void const* pB )
{
    LightingReference const& a = *static_cast<LightingReference const*>( pA );
    LightingReference const& b = *static_cast<LightingReference const*>( pB );
    uaddr const              addressA = reinterpret_cast<uaddr>( a.pLighting );
    uaddr const              addressB = reinterpret_cast<uaddr>( b.pLighting );
    return ( addressA < addressB ) ? -1 : ( ( addressA > addressB ) ? 1 : 0 );
}

//==============================================================================

xbool GeomMgr::CopyLightingData( GeomLightingConstants& destination, cb_geom_lighting const& source ) const
{
    x_memset( &destination, 0, sizeof( destination ) );
    for ( s32 i = 0; i < MAX_GEOM_LIGHTS; ++i )
    {
        destination.LightShadowIndex[i] = 0xFFFFFFFFu;
    }
    x_memcpy( destination.LightVec, source.LightVec, sizeof( destination.LightVec ) );
    x_memcpy( destination.LightCol, source.LightCol, sizeof( destination.LightCol ) );
    x_memcpy( destination.LightDir, source.LightDir, sizeof( destination.LightDir ) );
    x_memcpy( destination.LightCone, source.LightCone, sizeof( destination.LightCone ) );
    x_memcpy( destination.LightCookieU, source.LightCookieU, sizeof( destination.LightCookieU ) );
    x_memcpy( destination.LightCookieV, source.LightCookieV, sizeof( destination.LightCookieV ) );

    s32 const lightCount = MIN( source.LightCount, MAX_GEOM_LIGHTS );
    for ( s32 i = 0; i < lightCount; ++i )
    {
        s32 const dynamicLightIndex = source.DynamicLightIndex[i];
        if ( ( dynamicLightIndex >= 0 ) && ( dynamicLightIndex < light_mgr::MAX_DYNAMIC_LIGHTS ) )
        {
            destination.LightShadowIndex[i] = m_dynamicLightShadowIndex[dynamicLightIndex];
        }

        s32 const cookieIndex = static_cast<s32>( source.LightCookieU[i].GetW() ) - 1;
        destination.LightCookieU[i].GetW() = 0.0f;
        destination.LightCookieV[i].GetW() = 0.0f;

        if ( cookieIndex < 0 )
        {
            continue;
        }

        if ( cookieIndex >= g_LightMgr.GetLightCookieCount() )
        {
            return FALSE;
        }

        ProjectionAtlasRegion region;
        if ( !g_ProjectionAtlas.GetRegion( g_LightMgr.GetLightCookieHandle( cookieIndex ),
                                           ProjectionAtlasEncoding::LuminanceAlpha, region ) )
        {
            return FALSE;
        }

        destination.LightCookieAtlas[i] = region.m_uvScaleBias;
        destination.LightCookieLayer[i] = region.m_layer + 1;
        destination.LightCookieMaxMip[i] = region.m_maxMip;
    }

    destination.LightAmbCol = source.AmbCol;
    destination.LightCount = lightCount;
    return TRUE;
}

//==============================================================================

xbool GeomMgr::BuildLightingData( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BuildLighting" );

    if ( m_lRigidFrameLighting.GetCount() != m_lRigidFrameInstances.GetCount() ||
         m_lSkinFrameLighting.GetCount() != m_lSkinFrameInstances.GetCount() )
    {
        return FALSE;
    }

    m_lFrameLighting.SetCount( 0 );
    m_lLightingReferences.SetCount( 0 );

    GeomLightingConstants& emptyLighting = m_lFrameLighting.Append();
    x_memset( &emptyLighting, 0, sizeof( emptyLighting ) );

    for ( s32 i = 0; i < m_lRigidFrameInstances.GetCount(); ++i )
    {
        cb_geom_lighting const* pLighting = m_lRigidFrameLighting[i];
        if ( !pLighting )
        {
            m_lRigidFrameInstances[i].LightingIndex = 0;
            continue;
        }

        LightingReference& reference = m_lLightingReferences.Append();
        reference.pLighting = pLighting;
        reference.Kind = GEOM_PACKET_RIGID;
        reference.InstanceIndex = i;
    }

    for ( s32 i = 0; i < m_lSkinFrameInstances.GetCount(); ++i )
    {
        cb_geom_lighting const* pLighting = m_lSkinFrameLighting[i];
        if ( !pLighting )
        {
            m_lSkinFrameInstances[i].LightingIndex = 0;
            continue;
        }

        LightingReference& reference = m_lLightingReferences.Append();
        reference.pLighting = pLighting;
        reference.Kind = GEOM_PACKET_SKIN;
        reference.InstanceIndex = i;
    }

    s32 DynamicIndex = 0;
    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        if( m_lDrawPackets[i].Kind != GEOM_PACKET_DYNAMIC )
            continue;

        if( DynamicIndex >= m_lDynamicFrameLighting.GetCount() )
            return FALSE;

        cb_geom_lighting const* pLighting = m_lDynamicFrameLighting[DynamicIndex++];
        if( !pLighting )
            continue;

        LightingReference& reference = m_lLightingReferences.Append();
        reference.pLighting = pLighting;
        reference.Kind = GEOM_PACKET_DYNAMIC;
        reference.InstanceIndex = i;
    }

    if( DynamicIndex != m_lDynamicFrameLighting.GetCount() )
        return FALSE;

    if ( m_lLightingReferences.GetCount() > 1 )
    {
        x_qsort( m_lLightingReferences.GetPtr(), m_lLightingReferences.GetCount(), sizeof( LightingReference ),
                 CompareLightingReferences );
    }

    cb_geom_lighting const* pPrevious = NULL;
    u32                     lightingIndex = 0;
    for ( s32 i = 0; i < m_lLightingReferences.GetCount(); ++i )
    {
        LightingReference const& reference = m_lLightingReferences[i];
        if ( reference.pLighting != pPrevious )
        {
            GeomLightingConstants& lighting = m_lFrameLighting.Append();
            if ( !CopyLightingData( lighting, *reference.pLighting ) )
            {
                return FALSE;
            }
            lightingIndex = m_lFrameLighting.GetCount() - 1;
            pPrevious = reference.pLighting;
        }

        if ( reference.Kind == GEOM_PACKET_RIGID )
        {
            m_lRigidFrameInstances[reference.InstanceIndex].LightingIndex = lightingIndex;
        }
        else if( reference.Kind == GEOM_PACKET_SKIN )
        {
            m_lSkinFrameInstances[reference.InstanceIndex].LightingIndex = lightingIndex;
        }
        else
        {
            m_lDrawPackets[reference.InstanceIndex].DynamicLightingIndex = lightingIndex;
        }
    }

    return TRUE;
}

//==============================================================================

xbool GeomMgr::UploadPackets( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadPackets" );

    static xprofile_counter geomUploadBytes = x_GetProfiler().RegisterCounter( "GeomUploadBytes", "RenderCounter" );
    static xprofile_counter geomLightingUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomLightingUploadBytes", "RenderCounter" );
    static xprofile_counter geomRigidUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomRigidUploadBytes", "RenderCounter" );
    static xprofile_counter geomSkinUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomSkinUploadBytes", "RenderCounter" );
    static xprofile_counter geomShadowUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomShadowUploadBytes", "RenderCounter" );
    static xprofile_counter geomBufferReallocs =
        x_GetProfiler().RegisterCounter( "GeomBufferReallocs", "RenderCounter" );

    if ( m_lDrawPackets.GetCount() == 0 )
    {
        return TRUE;
    }

    if ( !m_areShadowMapConstantsValid )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadShadowMaps" );
        if ( !PrepareSharedShadowData() )
        {
            return FALSE;
        }

        u32 const shadowBytes = sizeof( m_faceShadowMatrices ) + sizeof( m_shadowMapData );
        geomShadowUploadBytes.Add( shadowBytes );
        geomUploadBytes.Add( shadowBytes );
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BuildLightingData" );
        if ( !BuildLightingData() )
        {
            return FALSE;
        }
    }

    u32 const lightingCount = m_lFrameLighting.GetCount();
    u32 const lightingBytes = sizeof( GeomLightingConstants ) * lightingCount;
    if ( !m_lightingDataBuffer || ( m_lightingDataCapacity < lightingCount ) )
    {
        geomBufferReallocs.Add();
    }
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadLighting" );
        if ( !EnsureBuffer( m_lightingDataBuffer, m_lightingDataCapacity, lightingCount,
                            sizeof( GeomLightingConstants ), "GeomLighting" ) ||
             !rbuffer_Upload( m_lightingDataBuffer, &m_lFrameLighting[0], lightingBytes, 0, TRUE ) )
        {
            return FALSE;
        }
    }
    geomLightingUploadBytes.Add( lightingBytes );
    geomUploadBytes.Add( lightingBytes );

    if ( m_lRigidFrameInstances.GetCount() > 0 )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadRigid" );

        u32 const instanceCount = m_lRigidFrameInstances.GetCount();
        u32 const colorCount = MAX( m_lRigidFrameColors.GetCount(), 1 );
        u32 const instanceBytes = sizeof( RigidInstanceData ) * instanceCount;
        u32 const colorBytes = sizeof( u32 ) * colorCount;
        if ( !m_rigidInstanceDataBuffer || ( m_rigidInstanceCapacity < instanceCount ) )
        {
            geomBufferReallocs.Add();
        }
        if ( !m_rigidInstanceIndexBuffer || ( m_rigidInstanceIndexCapacity < instanceCount ) )
        {
            geomBufferReallocs.Add();
        }
        if ( !m_rigidColorBuffer || ( m_rigidColorCapacity < colorCount ) )
        {
            geomBufferReallocs.Add();
        }
        if ( !EnsureBuffer( m_rigidInstanceDataBuffer, m_rigidInstanceCapacity, instanceCount,
                            sizeof( RigidInstanceData ), "GeomRigidInstances" ) ||
             !EnsureSequentialIndexBuffer( m_rigidInstanceIndexBuffer, m_rigidInstanceIndexCapacity, instanceCount,
                                           "GeomRigidInstanceIndices" ) ||
             !EnsureBuffer( m_rigidColorBuffer, m_rigidColorCapacity, colorCount, sizeof( u32 ), "GeomRigidColors" ) )
        {
            return FALSE;
        }

        if ( !rbuffer_Upload( m_rigidInstanceDataBuffer, &m_lRigidFrameInstances[0], instanceBytes, 0, TRUE ) )
        {
            return FALSE;
        }

        u32 const   emptyColor = 0;
        void const* pColorData =
            m_lRigidFrameColors.GetCount() ? (void const*)&m_lRigidFrameColors[0] : (void const*)&emptyColor;
        if ( !rbuffer_Upload( m_rigidColorBuffer, pColorData, colorBytes, 0, TRUE ) )
        {
            return FALSE;
        }

        geomRigidUploadBytes.Add( instanceBytes + colorBytes );
        geomUploadBytes.Add( instanceBytes + colorBytes );
    }

    if ( m_lRigidIndirectCommands.GetCount() > 0 )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadIndirect" );

        u32 const commandCount = m_lRigidIndirectCommands.GetCount();
        u32 const commandBytes = commandCount * sizeof( rdraw_indexed_indirect_command );
        if ( !m_rigidIndirectBuffer || ( m_rigidIndirectCapacity < commandCount ) )
        {
            geomBufferReallocs.Add();
        }
        if ( !EnsureBuffer( m_rigidIndirectBuffer, m_rigidIndirectCapacity, commandCount,
                            sizeof( rdraw_indexed_indirect_command ), "GeomRigidIndirect", RBUFFER_USAGE_INDIRECT ) ||
             !rbuffer_Upload( m_rigidIndirectBuffer, &m_lRigidIndirectCommands[0], commandBytes, 0, TRUE ) )
        {
            return FALSE;
        }

        geomRigidUploadBytes.Add( commandBytes );
        geomUploadBytes.Add( commandBytes );
    }

    if ( m_lSkinFrameInstances.GetCount() > 0 )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadSkin" );

        u32 const instanceCount = m_lSkinFrameInstances.GetCount();
        u32 const boneCount = MAX( m_lSkinFrameBones.GetCount(), 1 );
        u32 const drawInstanceCount = m_lSkinDrawInstances.GetCount();
        u32 const boneRemapCount = m_lSkinBoneRemaps.GetCount();
        u32 const instanceBytes = sizeof( SkinInstanceData ) * instanceCount;
        u32 const boneBytes = sizeof( matrix4 ) * boneCount;
        u32 const drawInstanceBytes = sizeof( SkinDrawInstance ) * drawInstanceCount;
        u32 const boneRemapBytes = sizeof( u32 ) * boneRemapCount;
        if ( ( drawInstanceCount == 0 ) || ( boneRemapCount == 0 ) )
        {
            x_DebugMsg( "GeomMgr: skin packets have no draw-instance data\n" );
            return FALSE;
        }
        if ( !m_skinInstanceDataBuffer || ( m_skinInstanceCapacity < instanceCount ) )
        {
            geomBufferReallocs.Add();
        }
        if ( !m_skinBoneDataBuffer || ( m_skinBoneCapacity < boneCount ) )
        {
            geomBufferReallocs.Add();
        }
        if ( !m_skinDrawInstanceBuffer || ( m_skinDrawInstanceCapacity < drawInstanceCount ) )
        {
            geomBufferReallocs.Add();
        }
        if ( !m_skinBoneRemapBuffer || ( m_skinBoneRemapCapacity < boneRemapCount ) )
        {
            geomBufferReallocs.Add();
        }
        if ( !EnsureBuffer( m_skinInstanceDataBuffer, m_skinInstanceCapacity, instanceCount, sizeof( SkinInstanceData ),
                            "GeomSkinInstances" ) ||
             !EnsureBuffer( m_skinBoneDataBuffer, m_skinBoneCapacity, boneCount, sizeof( matrix4 ), "GeomSkinBones" ) ||
             !EnsureBuffer( m_skinDrawInstanceBuffer, m_skinDrawInstanceCapacity, drawInstanceCount,
                            sizeof( SkinDrawInstance ), "GeomSkinDrawInstances", RBUFFER_USAGE_VERTEX ) ||
             !EnsureBuffer( m_skinBoneRemapBuffer, m_skinBoneRemapCapacity, boneRemapCount, sizeof( u32 ),
                            "GeomSkinBoneRemaps" ) )
        {
            return FALSE;
        }

        if ( !rbuffer_Upload( m_skinInstanceDataBuffer, &m_lSkinFrameInstances[0], instanceBytes, 0, TRUE ) )
        {
            return FALSE;
        }

        matrix4 identity;
        identity.Identity();
        void const* pBoneData =
            m_lSkinFrameBones.GetCount() ? (void const*)&m_lSkinFrameBones[0] : (void const*)&identity;
        if ( !rbuffer_Upload( m_skinBoneDataBuffer, pBoneData, boneBytes, 0, TRUE ) )
        {
            return FALSE;
        }

        if ( !rbuffer_Upload( m_skinDrawInstanceBuffer, &m_lSkinDrawInstances[0], drawInstanceBytes, 0, TRUE ) ||
             !rbuffer_Upload( m_skinBoneRemapBuffer, &m_lSkinBoneRemaps[0], boneRemapBytes, 0, TRUE ) )
        {
            return FALSE;
        }

        u32 const skinBytes = instanceBytes + boneBytes + drawInstanceBytes + boneRemapBytes;
        geomSkinUploadBytes.Add( skinBytes );
        geomUploadBytes.Add( skinBytes );
    }

    if ( m_lSkinIndirectCommands.GetCount() > 0 )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadSkinIndirect" );

        u32 const commandCount = m_lSkinIndirectCommands.GetCount();
        u32 const commandBytes = commandCount * sizeof( rdraw_indexed_indirect_command );
        if ( !m_skinIndirectBuffer || ( m_skinIndirectCapacity < commandCount ) )
        {
            geomBufferReallocs.Add();
        }
        if ( !EnsureBuffer( m_skinIndirectBuffer, m_skinIndirectCapacity, commandCount,
                            sizeof( rdraw_indexed_indirect_command ), "GeomSkinIndirect", RBUFFER_USAGE_INDIRECT ) ||
             !rbuffer_Upload( m_skinIndirectBuffer, &m_lSkinIndirectCommands[0], commandBytes, 0, TRUE ) )
        {
            return FALSE;
        }

        geomSkinUploadBytes.Add( commandBytes );
        geomUploadBytes.Add( commandBytes );
    }

    if( !UploadDynamicGeometry() )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool GeomMgr::ExecuteRigidIndirectRun( RigidIndirectRun const& run, geom_pass_desc const& pass )
{
    if ( ( run.CommandCount == 0 ) || ( run.FirstPacket > run.LastPacket ) ||
         ( run.FirstPacket >= static_cast<u32>( m_lDrawPackets.GetCount() ) ) ||
         ( ( run.FirstCommand + run.CommandCount ) > static_cast<u32>( m_lRigidIndirectCommands.GetCount() ) ) )
    {
        x_DebugMsg( "GeomMgr: invalid indirect run packets %u-%u commands %u+%u\n", run.FirstPacket, run.LastPacket,
                    run.FirstCommand, run.CommandCount );
        return FALSE;
    }

    GeomDrawPacket const& packet = m_lDrawPackets[run.FirstPacket];
    m_pDiffuseResource = packet.Resources.pDiffuse;
    m_pDetailResource = packet.Resources.pDetail;
    m_pEnvironmentResource = packet.Resources.pEnvironment;
    m_pEnvironmentCubeResource = packet.Resources.pEnvironmentCube;
    m_isDistortionStateActive = packet.Resources.bDistortionActive;
    m_distortionNormalRot = packet.Resources.DistortionNormalRot;

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindIndirectRun" );
        if ( !SetRigidMaterial( packet.pMaterial, packet.RenderFlags, packet.UOffset, packet.VOffset,
                                packet.MaterialOverride, FALSE, pass ) )
        {
            x_DebugMsg( "GeomMgr: indirect run %u failed to bind material\n", run.FirstCommand );
            return FALSE;
        }
        if ( !BindPacketResources( packet ) || !BindRigidFrameBuffers() )
        {
            x_DebugMsg( "GeomMgr: indirect run %u failed to bind shader resources\n", run.FirstCommand );
            return FALSE;
        }
        if ( !BindRigidGeometryBuffers( packet.Range ) )
        {
            x_DebugMsg( "GeomMgr: indirect run %u failed to bind pool %d/%d\n", run.FirstCommand, run.VertexPool,
                        run.IndexPool );
            return FALSE;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/DrawIndirectRun" );
        if ( !rdraw_DrawIndexedIndirect( m_rigidIndirectBuffer,
                                         run.FirstCommand * sizeof( rdraw_indexed_indirect_command ),
                                         run.CommandCount ) )
        {
            x_DebugMsg( "GeomMgr: indirect run %u rejected (%u commands)\n", run.FirstCommand, run.CommandCount );
            return FALSE;
        }
    }

    m_gBufferGpuDrawCount++;
    for ( u32 i = 0; i < run.CommandCount; ++i )
    {
        rdraw_indexed_indirect_command const& command = m_lRigidIndirectCommands[run.FirstCommand + i];
        m_gBufferInstanceCount += command.InstanceCount;
        m_gBufferSubmittedIndexCount += static_cast<u64>( command.IndexCount ) * command.InstanceCount;
    }
    return TRUE;
}

//==============================================================================

xbool GeomMgr::ExecuteSkinIndirectRun( SkinIndirectRun const& run, geom_pass_desc const& pass )
{
    if ( ( run.CommandCount == 0 ) || ( run.FirstPacket >= static_cast<u32>( m_lDrawPackets.GetCount() ) ) ||
         ( ( run.FirstCommand + run.CommandCount ) > static_cast<u32>( m_lSkinIndirectCommands.GetCount() ) ) )
    {
        x_DebugMsg( "GeomMgr: invalid skin indirect run packet %u commands %u+%u\n", run.FirstPacket, run.FirstCommand,
                    run.CommandCount );
        return FALSE;
    }

    GeomDrawPacket const& packet = m_lDrawPackets[run.FirstPacket];
    m_pDiffuseResource = packet.Resources.pDiffuse;
    m_pDetailResource = packet.Resources.pDetail;
    m_pEnvironmentResource = packet.Resources.pEnvironment;
    m_pEnvironmentCubeResource = packet.Resources.pEnvironmentCube;
    m_isDistortionStateActive = packet.Resources.bDistortionActive;
    m_distortionNormalRot = packet.Resources.DistortionNormalRot;

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindSkinIndirectRun" );
        if ( !SetSkinMaterial( packet.pMaterial, packet.RenderFlags, packet.UOffset, packet.VOffset,
                               packet.MaterialOverride, FALSE, pass ) )
        {
            x_DebugMsg( "GeomMgr: skin indirect run %u failed to bind material\n", run.FirstCommand );
            return FALSE;
        }
        if ( !BindPacketResources( packet ) || !BindSkinFrameBuffers() )
        {
            x_DebugMsg( "GeomMgr: skin indirect run %u failed to bind resources\n", run.FirstCommand );
            return FALSE;
        }
        if ( !BindSkinGeometryBuffers( packet.Range ) )
        {
            x_DebugMsg( "GeomMgr: skin indirect run %u failed to bind pool %d/%d\n", run.FirstCommand, run.VertexPool,
                        run.IndexPool );
            return FALSE;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/DrawSkinIndirectRun" );
        if ( !rdraw_DrawIndexedIndirect(
                 m_skinIndirectBuffer, run.FirstCommand * sizeof( rdraw_indexed_indirect_command ), run.CommandCount ) )
        {
            x_DebugMsg( "GeomMgr: skin indirect run %u rejected (%u commands)\n", run.FirstCommand, run.CommandCount );
            return FALSE;
        }
    }

    m_gBufferGpuDrawCount++;
    m_gBufferSkinSectionDrawCount += run.CommandCount;
    for ( u32 i = 0; i < run.CommandCount; ++i )
    {
        rdraw_indexed_indirect_command const& command = m_lSkinIndirectCommands[run.FirstCommand + i];
        m_gBufferSubmittedIndexCount += static_cast<u64>( command.IndexCount ) * command.InstanceCount;
    }
    return TRUE;
}

//==============================================================================

xbool GeomMgr::ExecuteGBuffer( geom_pass_desc const& pass )
{
    if ( ( pass.TargetWidth == 0 ) || ( pass.TargetHeight == 0 ) )
    {
        x_DebugMsg( "GeomMgr: invalid G-buffer extent %ux%u\n", pass.TargetWidth, pass.TargetHeight );
        return FALSE;
    }

    // Opaque rigid and skin geometry has no cross-kind ordering dependency.
    for ( s32 runIndex = 0; runIndex < m_lRigidIndirectRuns.GetCount(); ++runIndex )
    {
        if ( !ExecuteRigidIndirectRun( m_lRigidIndirectRuns[runIndex], pass ) )
        {
            x_DebugMsg( "GeomMgr: G-buffer rigid run %d/%d failed\n", runIndex, m_lRigidIndirectRuns.GetCount() );
            return FALSE;
        }
    }

    for ( s32 runIndex = 0; runIndex < m_lSkinIndirectRuns.GetCount(); ++runIndex )
    {
        if ( !ExecuteSkinIndirectRun( m_lSkinIndirectRuns[runIndex], pass ) )
        {
            x_DebugMsg( "GeomMgr: G-buffer skin run %d/%d failed\n", runIndex, m_lSkinIndirectRuns.GetCount() );
            return FALSE;
        }
    }

    for( s32 packetIndex = 0; packetIndex < m_lDrawPackets.GetCount(); ++packetIndex )
    {
        GeomDrawPacket const& packet = m_lDrawPackets[packetIndex];
        if( ( packet.Pass == GEOMETRY_PASS_GBUFFER ) && ( packet.Kind == GEOM_PACKET_DYNAMIC ) )
        {
            if( !ExecuteDynamicPacket( packet, pass ) )
            {
                x_DebugMsg( "GeomMgr: G-buffer dynamic packet %d failed\n", packetIndex );
                return FALSE;
            }
        }
    }

    for ( s32 packetIndex = 0; packetIndex < m_lDrawPackets.GetCount(); ++packetIndex )
    {
        GeomDrawPacket const& packet = m_lDrawPackets[packetIndex];
        if ( ( packet.Pass == GEOMETRY_PASS_GBUFFER ) && ( packet.Kind == GEOM_PACKET_SKIN ) )
        {
            m_gBufferInstanceCount += packet.InstanceCount;
        }
    }
    return TRUE;
}

//==============================================================================

xbool GeomMgr::ExecutePacket( s32 packetIndex, geom_pass_desc const& pass )
{
    if ( ( packetIndex < 0 ) || ( packetIndex >= m_lDrawPackets.GetCount() ) || ( pass.TargetWidth == 0 ) ||
         ( pass.TargetHeight == 0 ) )
    {
        x_DebugMsg( "GeomMgr: invalid direct packet %d/%d extent %ux%u\n", packetIndex, m_lDrawPackets.GetCount(),
                    pass.TargetWidth, pass.TargetHeight );
        return FALSE;
    }

    GeomDrawPacket const& packet = m_lDrawPackets[packetIndex];

    if( packet.Kind == GEOM_PACKET_DYNAMIC )
    {
        return ExecuteDynamicPacket( packet, pass );
    }

    m_pDiffuseResource = packet.Resources.pDiffuse;
    m_pDetailResource = packet.Resources.pDetail;
    m_pEnvironmentResource = packet.Resources.pEnvironment;
    m_pEnvironmentCubeResource = packet.Resources.pEnvironmentCube;
    m_isDistortionStateActive = packet.Resources.bDistortionActive;
    m_distortionNormalRot = packet.Resources.DistortionNormalRot;

    if ( packet.Kind == GEOM_PACKET_RIGID )
    {
        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindMaterial" );
            if ( !SetRigidMaterial( packet.pMaterial, packet.RenderFlags, packet.UOffset, packet.VOffset,
                                    packet.MaterialOverride, packet.Pass != GEOMETRY_PASS_GBUFFER, pass ) )
            {
                x_DebugMsg( "GeomMgr: rigid packet %d failed to bind material\n", packetIndex );
                return FALSE;
            }
        }

        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindResources" );
            if ( !BindPacketResources( packet ) || !BindRigidFrameBuffers() )
            {
                x_DebugMsg( "GeomMgr: rigid packet %d failed to bind resources\n", packetIndex );
                return FALSE;
            }
        }

        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindMesh" );
            if ( ( packet.Range.IndexCount <= 0 ) || !BindRigidGeometryBuffers( packet.Range ) )
            {
                x_DebugMsg( "GeomMgr: rigid packet %d failed to bind mesh (indices %d)\n", packetIndex,
                            packet.Range.IndexCount );
                return FALSE;
            }
        }

        xbool bDrawn;
        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/Draw" );
            bDrawn = rdraw_DrawIndexedInstanced( packet.Range.IndexCount, packet.InstanceCount, packet.Range.FirstIndex,
                                                 packet.Range.BaseVertex, packet.FirstInstance );
        }
        if ( bDrawn && ( packet.Pass == GEOMETRY_PASS_GBUFFER ) )
        {
            m_gBufferGpuDrawCount++;
            m_gBufferInstanceCount += packet.InstanceCount;
            m_gBufferSubmittedIndexCount += static_cast<u64>( packet.Range.IndexCount ) * packet.InstanceCount;
        }
        if ( !bDrawn )
        {
            x_DebugMsg( "GeomMgr: rigid packet %d draw rejected\n", packetIndex );
        }
        return bDrawn;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindMaterial" );
        if ( !SetSkinMaterial( packet.pMaterial, packet.RenderFlags, packet.UOffset, packet.VOffset,
                               packet.MaterialOverride, packet.Pass != GEOMETRY_PASS_GBUFFER, pass ) )
        {
            x_DebugMsg( "GeomMgr: skin packet %d failed to bind material\n", packetIndex );
            return FALSE;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindResources" );
        if ( !BindPacketResources( packet ) || !BindSkinFrameBuffers() )
        {
            x_DebugMsg( "GeomMgr: skin packet %d failed to bind resources\n", packetIndex );
            return FALSE;
        }
    }

    SoftVertexMgr::MeshView view;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindMesh" );
        if ( !g_SkinVertMgr.GetMeshView( packet.hMesh, view ) || !BindSkinGeometryBuffers( packet.Range ) )
        {
            x_DebugMsg( "GeomMgr: skin packet %d failed to bind mesh\n", packetIndex );
            return FALSE;
        }
    }

    xbool bAnyDraw = FALSE;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/SkinSections" );
        for ( s32 s = 0; s < view.SectionCount; ++s )
        {
            SoftVertexMgr::SoftSection const& section = view.pSections[s];
            xbool                             bDrawn;
            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/Draw" );
                bDrawn = rdraw_DrawIndexedInstanced(
                    section.IndexCount, packet.InstanceCount, view.Geometry.FirstIndex + section.FirstIndex,
                    view.Geometry.BaseVertex, packet.FirstSkinDrawInstance + ( s * packet.InstanceCount ) );
            }
            if ( !bDrawn )
            {
                x_DebugMsg( "GeomMgr: skin packet %d section %d draw rejected\n", packetIndex, s );
                return FALSE;
            }

            bAnyDraw = TRUE;
            if ( packet.Pass == GEOMETRY_PASS_GBUFFER )
            {
                m_gBufferGpuDrawCount++;
                m_gBufferSkinSectionDrawCount++;
                m_gBufferSubmittedIndexCount += static_cast<u64>( section.IndexCount ) * packet.InstanceCount;
            }
        }
    }

    if ( bAnyDraw && ( packet.Pass == GEOMETRY_PASS_GBUFFER ) )
    {
        m_gBufferInstanceCount += packet.InstanceCount;
    }

    return TRUE;
}

//==============================================================================
//  GENERAL STATE HELPERS
//==============================================================================

static u32 GeomPipelineVariantIndex( rstate_blend_preset blend, rstate_depth_preset depth, rstate_raster_preset raster,
                                     xbool sceneOnly )
{
    u32 const stateIndex = ( static_cast<u32>( blend ) * RSTATE_DEPTH_PRESET_COUNT * RSTATE_RASTER_PRESET_COUNT ) +
                           ( static_cast<u32>( depth ) * RSTATE_RASTER_PRESET_COUNT ) + ( static_cast<u32>( raster ) );
    return ( stateIndex * 2 ) + ( sceneOnly ? 1 : 0 );
}

//==============================================================================

xbool GeomMgr::PrewarmPipelines( void )
{
    static rstate_blend_preset const  blendPresets[] = { RSTATE_BLEND_PRESET_NONE, RSTATE_BLEND_PRESET_ALPHA,
                                                         RSTATE_BLEND_PRESET_ADD, RSTATE_BLEND_PRESET_SUB };
    static rstate_depth_preset const  depthPresets[] = { RSTATE_DEPTH_PRESET_NORMAL, RSTATE_DEPTH_PRESET_NO_WRITE };
    static rstate_raster_preset const rasterPresets[] = { RSTATE_RASTER_PRESET_SOLID,
                                                          RSTATE_RASTER_PRESET_SOLID_NO_CULL };

    u32 pipelineCount = 0;
    for ( s32 sceneOnly = 0; sceneOnly < 2; ++sceneOnly )
    {
        RenderStateSelection state;
        state.SceneOnly = sceneOnly ? TRUE : FALSE;

        for ( u32 blendIndex = 0; blendIndex < ARRAYSIZE( blendPresets ); ++blendIndex )
        {
            state.Blend = blendPresets[blendIndex];
            for ( u32 depthIndex = 0; depthIndex < ARRAYSIZE( depthPresets ); ++depthIndex )
            {
                state.Depth = depthPresets[depthIndex];
                for ( u32 rasterIndex = 0; rasterIndex < ARRAYSIZE( rasterPresets ); ++rasterIndex )
                {
                    state.Raster = rasterPresets[rasterIndex];
                    if ( !GetRigidPipeline( state, TRUE ) || !GetSkinPipeline( state, TRUE ) )
                    {
                        x_DebugMsg( "GeomMgr: failed to prewarm graphics pipelines "
                                    "(blend=%d, depth=%d, raster=%d, scene=%d)\n",
                                    state.Blend, state.Depth, state.Raster, state.SceneOnly );
                        return FALSE;
                    }
                    pipelineCount += 2;
                }
            }
        }

        // Material override is a dedicated depth-only combination and cannot
        // be produced by the normal material state path above.
        state.Blend = RSTATE_BLEND_PRESET_COLOR_WRITE_DISABLE;
        state.Depth = RSTATE_DEPTH_PRESET_NORMAL;
        state.Raster = RSTATE_RASTER_PRESET_SOLID_NO_CULL;
        if ( !GetRigidPipeline( state, TRUE ) || !GetSkinPipeline( state, TRUE ) )
        {
            x_DebugMsg( "GeomMgr: failed to prewarm material-override graphics pipelines\n" );
            return FALSE;
        }
        pipelineCount += 2;
    }

    x_DebugMsg( "GeomMgr: prewarmed %u graphics pipeline variants\n", pipelineCount );
    return TRUE;
}

//==============================================================================

GeomMgr::RenderStateSelection GeomMgr::ResolveRenderStates( material const* pMaterial, u32 renderFlags, u8 overrideMat,
                                                            xbool sceneOnly ) const
{
    RenderStateSelection state;
    state.SceneOnly = sceneOnly;

    if ( overrideMat )
    {
        state.Blend = RSTATE_BLEND_PRESET_COLOR_WRITE_DISABLE;
        state.Depth = RSTATE_DEPTH_PRESET_NORMAL;
        state.Raster = RSTATE_RASTER_PRESET_SOLID_NO_CULL;
        return state;
    }

    u32 const   fadeMask = ( render::FADING_ALPHA | render::INSTFLAG_FADING_ALPHA );
    xbool const bFadingAlpha = !!( renderFlags & fadeMask );

    if ( bFadingAlpha )
    {
        state.Blend = RSTATE_BLEND_PRESET_ALPHA;
    }
    else
    {
        material_blend_mode const blendMode = pMaterial ? pMaterial->GetBlendMode() : MATERIAL_BLEND_OPAQUE;
        switch ( blendMode )
        {
            case MATERIAL_BLEND_ALPHA:
                {
                    state.Blend = RSTATE_BLEND_PRESET_ALPHA;
                }
                break;
            case MATERIAL_BLEND_ADDITIVE:
                {
                    state.Blend = RSTATE_BLEND_PRESET_ADD;
                }
                break;
            case MATERIAL_BLEND_SUBTRACTIVE:
                {
                    state.Blend = RSTATE_BLEND_PRESET_SUB;
                }
                break;
            case MATERIAL_BLEND_DISTORTION:
            case MATERIAL_BLEND_OPAQUE:
                {
                }
                break;

            default:
                {
                    ASSERT( FALSE );
                }
                break;
        }
    }

    if ( pMaterial && pMaterial->ClampsSampler() )
    {
        state.Sampler = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
    }

    xbool const bForceZFill = pMaterial && pMaterial->ForcesZFill();
    xbool const bDistortion = pMaterial && pMaterial->IsDistortion();
    xbool const bDisableDepthWrite =
        bFadingAlpha ? ( !bForceZFill && !bDistortion ) : ( pMaterial && !pMaterial->WritesDepth() );

    state.Depth = bDisableDepthWrite ? RSTATE_DEPTH_PRESET_NO_WRITE : RSTATE_DEPTH_PRESET_NORMAL;

    // const xbool bWireframe       = !!(RenderFlags & render::WIREFRAME);
    // const xbool bWireframeNoCull = !!(RenderFlags & render::WIREFRAME2);
    xbool const bDisableCull = pMaterial && !pMaterial->CullsBackFaces();

    // if( bWireframeNoCull )
    //{
    //     rasterMode = RSTATE_RASTER_PRESET_WIRE_NO_CULL;
    // }
    // else if( bWireframe )
    //{
    //     rasterMode = bDisableCull ? RSTATE_RASTER_PRESET_WIRE_NO_CULL : RSTATE_RASTER_PRESET_WIRE;
    // }
    // else
    {
        state.Raster = bDisableCull ? RSTATE_RASTER_PRESET_SOLID_NO_CULL : RSTATE_RASTER_PRESET_SOLID;
    }

    // if( RenderFlags & (render::CLIPPED | render::INSTFLAG_CLIPPED) )
    //{
    //     samplerMode = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
    // }

    return state;
}

//==============================================================================

render_pipeline* GeomMgr::GetRigidPipeline( RenderStateSelection const& state, xbool isPrewarm )
{
    static shader_vertex_element rigidLayout[] = { shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, 0 ),
                                                   shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT3, 12 ),
                                                   shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, 24 ),
                                                   shader_vertex_element( 3, 1, SHADER_VERTEX_FORMAT_UINT1, 0 ),
                                                   shader_vertex_element( 4, 2, SHADER_VERTEX_FORMAT_UINT1, 0 ) };

    static shader_vertex_buffer_desc rigidVertexBuffers[3];
    rigidVertexBuffers[0].Slot = 0;
    rigidVertexBuffers[0].Stride = sizeof( rigid_geom::vertex );
    rigidVertexBuffers[1].Slot = 1;
    rigidVertexBuffers[1].Stride = sizeof( u32 );
    rigidVertexBuffers[1].bPerInstance = TRUE;
    rigidVertexBuffers[2].Slot = 2;
    rigidVertexBuffers[2].Stride = sizeof( u32 );

    u32 const index = GeomPipelineVariantIndex( state.Blend, state.Depth, state.Raster, state.SceneOnly );
    if ( index >= GEOM_PIPELINE_VARIANT_COUNT )
    {
        return NULL;
    }

    render_pipeline_desc desc;
    desc.Shader.pVertexShader = &m_rigidVertexShader;
    desc.Shader.pPixelShader = state.SceneOnly ? &m_rigidScenePixelShader : &m_rigidPixelShader;
    desc.Shader.pVertexBuffers = rigidVertexBuffers;
    desc.Shader.VertexBufferCount = ARRAYSIZE( rigidVertexBuffers );
    desc.Shader.pInputElements = rigidLayout;
    desc.Shader.InputElementCount = ARRAYSIZE( rigidLayout );
    desc.Shader.Topology = SHADER_TOPOLOGY_TRIANGLE_LIST;
    desc.Depth = rstate_GetDepthDesc( state.Depth );
    desc.Raster = rstate_GetRasterDesc( state.Raster );
    desc.ColorCount = state.SceneOnly ? 1 : 3;
    desc.ColorTargets[0].Format = RTARGET_FORMAT_RGBA8;
    desc.ColorTargets[1].Format = RTARGET_FORMAT_RGBA16F;
    desc.ColorTargets[2].Format = RTARGET_FORMAT_RGBA16F;
    desc.ColorTargets[0].Blend = rstate_GetBlendDesc( state.Blend );
    desc.ColorTargets[1].Blend = rstate_GetBlendDesc( state.Blend );
    desc.ColorTargets[2].Blend = rstate_GetBlendDesc( state.Blend );
    desc.ColorTargets[3].Blend = rstate_GetBlendDesc( state.Blend );
    desc.DepthFormat = RTARGET_FORMAT_DEPTH_STENCIL;
    desc.pDebugName = state.SceneOnly ? "GeomRigidScene" : "GeomRigidGBuffer";

    return isPrewarm ? m_rigidPipelines.Prewarm( index, desc ) : m_rigidPipelines.GetOrCreate( index, desc );
}

//==============================================================================

xbool GeomMgr::BindRigidPipeline( RenderStateSelection const& state )
{
    u32 const index = GeomPipelineVariantIndex( state.Blend, state.Depth, state.Raster, state.SceneOnly );
    if ( index >= GEOM_PIPELINE_VARIANT_COUNT )
    {
        return FALSE;
    }

    render_pipeline* pPipeline = GetRigidPipeline( state );
    if ( !pPipeline || !render_BindPipeline( *pPipeline ) )
    {
        return FALSE;
    }

    m_activeShaderKind = GEOM_SHADER_RIGID;
    m_activeSamplerPreset = state.Sampler;
    return TRUE;
}

//==============================================================================

render_pipeline* GeomMgr::GetSkinPipeline( RenderStateSelection const& state, xbool isPrewarm )
{
    static shader_vertex_element skinLayout[] = { shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT4, 0 ),
                                                  shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT4, 16 ),
                                                  shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT4, 32 ),
                                                  shader_vertex_element( 3, 1, SHADER_VERTEX_FORMAT_UINT1, 0 ),
                                                  shader_vertex_element( 4, 1, SHADER_VERTEX_FORMAT_UINT1, 4 ) };

    static shader_vertex_buffer_desc skinVertexBuffers[2];
    skinVertexBuffers[0].Slot = 0;
    skinVertexBuffers[0].Stride = sizeof( SkinGpuVertex );
    skinVertexBuffers[1].Slot = 1;
    skinVertexBuffers[1].Stride = sizeof( SkinDrawInstance );
    skinVertexBuffers[1].bPerInstance = TRUE;

    u32 const index = GeomPipelineVariantIndex( state.Blend, state.Depth, state.Raster, state.SceneOnly );
    if ( index >= GEOM_PIPELINE_VARIANT_COUNT )
    {
        return NULL;
    }

    render_pipeline_desc desc;
    desc.Shader.pVertexShader = &m_skinVertexShader;
    desc.Shader.pPixelShader = state.SceneOnly ? &m_skinScenePixelShader : &m_skinPixelShader;
    desc.Shader.pVertexBuffers = skinVertexBuffers;
    desc.Shader.VertexBufferCount = ARRAYSIZE( skinVertexBuffers );
    desc.Shader.pInputElements = skinLayout;
    desc.Shader.InputElementCount = ARRAYSIZE( skinLayout );
    desc.Shader.Topology = SHADER_TOPOLOGY_TRIANGLE_LIST;
    desc.Depth = rstate_GetDepthDesc( state.Depth );
    desc.Raster = rstate_GetRasterDesc( state.Raster );
    desc.ColorCount = state.SceneOnly ? 1 : 3;
    desc.ColorTargets[0].Format = RTARGET_FORMAT_RGBA8;
    desc.ColorTargets[1].Format = RTARGET_FORMAT_RGBA16F;
    desc.ColorTargets[2].Format = RTARGET_FORMAT_RGBA16F;
    desc.ColorTargets[0].Blend = rstate_GetBlendDesc( state.Blend );
    desc.ColorTargets[1].Blend = rstate_GetBlendDesc( state.Blend );
    desc.ColorTargets[2].Blend = rstate_GetBlendDesc( state.Blend );
    desc.ColorTargets[3].Blend = rstate_GetBlendDesc( state.Blend );
    desc.DepthFormat = RTARGET_FORMAT_DEPTH_STENCIL;
    desc.pDebugName = state.SceneOnly ? "GeomSkinScene" : "GeomSkinGBuffer";

    return isPrewarm ? m_skinPipelines.Prewarm( index, desc ) : m_skinPipelines.GetOrCreate( index, desc );
}

//==============================================================================

xbool GeomMgr::BindSkinPipeline( RenderStateSelection const& state )
{
    u32 const index = GeomPipelineVariantIndex( state.Blend, state.Depth, state.Raster, state.SceneOnly );
    if ( index >= GEOM_PIPELINE_VARIANT_COUNT )
    {
        return FALSE;
    }

    render_pipeline* pPipeline = GetSkinPipeline( state );
    if ( !pPipeline || !render_BindPipeline( *pPipeline ) )
    {
        return FALSE;
    }

    m_activeShaderKind = GEOM_SHADER_SKIN;
    m_activeSamplerPreset = state.Sampler;
    return TRUE;
}

//==============================================================================

void GeomMgr::DestroyRigidPipelines( void )
{
    m_rigidPipelines.Reset();
}

//==============================================================================

void GeomMgr::DestroySkinPipelines( void )
{
    m_skinPipelines.Reset();
}

//==============================================================================

void GeomMgr::SetBitmap( texture const* pTexture, texture_slot slot )
{
    char const* pSlotName = GeomTextureSlotName( slot );
    char const* pBindingName = GeomTextureBindingName( slot );
    if ( !pBindingName )
    {
        x_DebugMsg( "GeomMgr: WARNING - SetBitmap does not manage slot %d (%s)\n", slot, pSlotName );
        return;
    }

    shader_resource const* pResource = NULL;

    if ( pTexture )
    {
        pResource = pTexture->GetShaderResource();
        if ( !pResource )
        {
            x_DebugMsg( "GeomMgr: WARNING - %s texture has no GPU resource\n", pSlotName );
        }
    }

    switch ( slot )
    {
        case TEXTURE_SLOT_DIFFUSE:
            {
                m_pDiffuseResource = pResource;
            }
            break;
        case TEXTURE_SLOT_DETAIL:
            {
                m_pDetailResource = pResource;
            }
            break;
        case TEXTURE_SLOT_ENVIRONMENT:
            {
                m_pEnvironmentResource = pResource;
            }
            break;
        case TEXTURE_SLOT_ENVIRONMENT_CUBE:
            {
                m_pEnvironmentCubeResource = pResource;
            }
            break;
        default:
            {
            }
            break;
    }
}

//==============================================================================

void GeomMgr::SetEnvironmentCubemap( cubemap const* pCubemap )
{
    shader_resource const* pResource = NULL;

    if ( pCubemap )
    {
        pResource = pCubemap->GetShaderResource();
        if ( !pResource )
        {
            x_DebugMsg( "GeomMgr: WARNING - Cubemap has no GPU resource\n" );
        }
    }

    m_pEnvironmentCubeResource = pResource;
}

//==============================================================================

void GeomMgr::InvalidateCache( void )
{
    m_pCachedFaceShadowResource = NULL;
    m_areShadowMapConstantsValid = FALSE;
}

//==============================================================================

void GeomMgr::SetDistortionState( radian3 const& normalRot )
{
    m_isDistortionStateActive = TRUE;
    m_distortionNormalRot = normalRot;
}

//==============================================================================

void GeomMgr::SetDistortionScene( shader_resource const* pResource )
{
    m_pDistortionSceneResource = pResource;
}

//==============================================================================

void GeomMgr::ClearDistortionState( void )
{
    m_isDistortionStateActive = FALSE;
    m_pDistortionSceneResource = NULL;
    m_distortionNormalRot.Zero();
}
