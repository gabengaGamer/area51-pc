//==============================================================================
//
//  GeomMgr_Dynamic.cpp
//
//  Dynamic G-buffer geometry for the PC geom manager.
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

#include "../../Texture.hpp"

//==============================================================================
//  TYPES
//==============================================================================

struct DynamicGeometryConstants
{
    matrix4 View;
    matrix4 Projection;
    vector4 CameraPosition;
    vector4 DepthParams;
    u32     LightingIndex;
    u32     ShaderFlags;
    u32     Padding[2];
};

//==============================================================================
//  HELPERS
//==============================================================================

namespace
{
xbool EnsureDynamicBuffer( rbuffer& Buffer, u32& Capacity, u32 RequiredCount, u32 Stride, u32 Usage,
                           char const* pDebugName )
{
    u32 const DesiredCapacity = MAX( RequiredCount, 1 );
    if( Buffer && ( Capacity >= DesiredCapacity ) )
    {
        return TRUE;
    }

    rbuffer_Destroy( Buffer );

    rbuffer_desc Desc;
    Desc.Size       = DesiredCapacity * Stride;
    Desc.Stride     = Stride;
    Desc.UsageFlags = Usage;
    Desc.pDebugName = pDebugName;
    if( !rbuffer_Create( Buffer, Desc ) )
    {
        Capacity = 0;
        return FALSE;
    }

    Capacity = DesiredCapacity;
    return TRUE;
}
} // namespace

//==============================================================================
//  LIFETIME
//==============================================================================

xbool GeomMgr::InitDynamicGeometry( void )
{
    m_dynamicVertexShader = shader();
    m_dynamicPixelShader = shader();
    m_dynamicShaderBindings = ShaderBindingLayout();
    m_dynamicVertexBuffer = rbuffer();
    m_dynamicIndexBuffer = rbuffer();
    m_dynamicVertexCapacity = 0;
    m_dynamicIndexCapacity = 0;
    m_dynamicConstantsVertexSlot = 0xFFFFFFFFu;
    m_dynamicConstantsPixelSlot = 0xFFFFFFFFu;
    m_dynamicDiffuseSlot = 0xFFFFFFFFu;
    m_dynamicDamageSlot = 0xFFFFFFFFu;
    m_lDynamicFrameVertices.SetCapacity( 65536 );
    m_lDynamicFrameIndices.SetCapacity( 65536 );
    m_lDynamicFrameLighting.SetCapacity( 32768 );

    shader_LoadFromEcs( m_dynamicVertexShader, "dynamic_geometry_vs.vs.ecs" );
    shader_LoadFromEcs( m_dynamicPixelShader, "dynamic_geometry_ps.ps.ecs" );
    if( !m_dynamicVertexShader || !m_dynamicPixelShader )
    {
        x_DebugMsg( "GeomMgr: Failed to initialize dynamic geometry shaders\n" );
        KillDynamicGeometry();
        return FALSE;
    }

    if( !shader_FindUniformSlot( m_dynamicVertexShader, "cbDynamicGeometry", m_dynamicConstantsVertexSlot ) ||
        !shader_FindUniformSlot( m_dynamicPixelShader, "cbDynamicGeometry", m_dynamicConstantsPixelSlot ) ||
        !shader_FindUniformSlot( m_dynamicPixelShader, "cbProjTextures", m_dynamicShaderBindings.ProjTexturesPixel ) ||
        !shader_FindSampledTextureSlot( m_dynamicPixelShader, "txDiffuse", m_dynamicDiffuseSlot ) ||
        !shader_FindSampledTextureSlot( m_dynamicPixelShader, "txDamage", m_dynamicDamageSlot ) ||
        !shader_FindSampledTextureSlot( m_dynamicPixelShader, "txFaceShadowAtlas",
                                        m_dynamicShaderBindings.FaceShadowTexturePixel ) ||
        !shader_FindSampledTextureSlot( m_dynamicPixelShader, "txFaceShadowDepthAtlas",
                                        m_dynamicShaderBindings.FaceShadowDepthTexturePixel ) ||
        !shader_FindSampledTextureSlot( m_dynamicPixelShader, "txProjectionAtlas",
                                        m_dynamicShaderBindings.ProjectionAtlasTexturePixel ) ||
        !shader_FindStorageBufferSlot( m_dynamicPixelShader, "GeomLighting",
                                       m_dynamicShaderBindings.LightingDataPixel ) ||
        !shader_FindStorageBufferSlot( m_dynamicPixelShader, "FaceShadowMatrices",
                                       m_dynamicShaderBindings.ShadowMatricesPixel ) ||
        !shader_FindStorageBufferSlot( m_dynamicPixelShader, "ShadowMapData",
                                       m_dynamicShaderBindings.ShadowDataPixel ) )
    {
        x_DebugMsg( "GeomMgr: Failed to resolve dynamic geometry shader bindings\n" );
        KillDynamicGeometry();
        return FALSE;
    }

    if( !GetDynamicPipeline( 0 ) || !GetDynamicPipeline( render::WIREFRAME ) )
    {
        KillDynamicGeometry();
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void GeomMgr::KillDynamicGeometry( void )
{
    m_lDynamicFrameVertices.Clear();
    m_lDynamicFrameIndices.Clear();
    m_lDynamicFrameLighting.Clear();
    rbuffer_Destroy( m_dynamicIndexBuffer );
    rbuffer_Destroy( m_dynamicVertexBuffer );
    m_dynamicVertexCapacity = 0;
    m_dynamicIndexCapacity = 0;
    m_dynamicPipelines.Reset();
    shader_Destroy( m_dynamicPixelShader );
    shader_Destroy( m_dynamicVertexShader );
}

//==============================================================================
//  PACKETS
//==============================================================================

xbool GeomMgr::BuildDynamicPackets( xarray<dynamic_geometry_draw> const& Draws )
{
    for( s32 i = 0; i < Draws.GetCount(); ++i )
    {
        dynamic_geometry_draw const& Draw = Draws[i];
        shader_resource const* pDiffuse = Draw.pDiffuseTexture ? Draw.pDiffuseTexture->GetShaderResource() : NULL;
        if( !Draw.pVertices || !Draw.pIndices || !pDiffuse || !Draw.pDamageMask || !Draw.pDamageTexture ||
            !Draw.pDamageUploadPending ||
            ( Draw.VertexCount <= 0 ) || ( Draw.VertexCount > 65535 ) ||
            ( Draw.IndexCount <= 0 ) || ( ( Draw.IndexCount % 3 ) != 0 ) )
        {
            x_DebugMsg( "GeomMgr: Invalid dynamic geometry draw %d\n", i );
            return FALSE;
        }

        for( s32 Index = 0; Index < Draw.IndexCount; ++Index )
        {
            if( Draw.pIndices[Index] >= Draw.VertexCount )
            {
                x_DebugMsg( "GeomMgr: Dynamic geometry draw %d index %d exceeds %d vertices\n",
                            i, Index, Draw.VertexCount );
                return FALSE;
            }
        }

        GeomDrawPacket Packet;
        Packet.Kind               = GEOM_PACKET_DYNAMIC;
        Packet.Pass               = GEOMETRY_PASS_GBUFFER;
        Packet.RenderFlags        = Draw.Flags;
        Packet.Sequence           = Draw.Sequence;
        Packet.FirstDynamicVertex = m_lDynamicFrameVertices.GetCount();
        Packet.FirstDynamicIndex  = m_lDynamicFrameIndices.GetCount();
        Packet.DynamicIndexCount  = Draw.IndexCount;
        Packet.DynamicLightingIndex = 0;
        Packet.Resources.pDiffuse = pDiffuse;
        Packet.pDamageMask        = Draw.pDamageMask;
        Packet.pDamageTexture     = Draw.pDamageTexture;
        Packet.pDamageUpload      = Draw.pDamageUpload;
        Packet.pDamageUploadPending = Draw.pDamageUploadPending;
        Packet.DamageUploadX      = Draw.DamageUploadX;
        Packet.DamageUploadY      = Draw.DamageUploadY;
        Packet.DamageUploadWidth  = Draw.DamageUploadWidth;
        Packet.DamageUploadHeight = Draw.DamageUploadHeight;

        s32 const FirstVertex = m_lDynamicFrameVertices.GetCount();
        m_lDynamicFrameVertices.SetCount( FirstVertex + Draw.VertexCount );
        x_memcpy( &m_lDynamicFrameVertices[FirstVertex], Draw.pVertices,
                  Draw.VertexCount * sizeof( dynamic_geometry_vertex ) );

        s32 const FirstIndex = m_lDynamicFrameIndices.GetCount();
        m_lDynamicFrameIndices.SetCount( FirstIndex + Draw.IndexCount );
        x_memcpy( &m_lDynamicFrameIndices[FirstIndex], Draw.pIndices, Draw.IndexCount * sizeof( u16 ) );

        m_lDrawPackets.Append() = Packet;
        m_lDynamicFrameLighting.Append() = (cb_geom_lighting const*)Draw.pLighting;
    }

    return TRUE;
}

//==============================================================================

xbool GeomMgr::UploadDynamicGeometry( void )
{
    if( m_lDynamicFrameVertices.GetCount() == 0 )
    {
        return m_lDynamicFrameIndices.GetCount() == 0;
    }

    u32 const VertexCount = m_lDynamicFrameVertices.GetCount();
    u32 const IndexCount = m_lDynamicFrameIndices.GetCount();
    u32 const VertexBytes = VertexCount * sizeof( dynamic_geometry_vertex );
    u32 const IndexBytes = IndexCount * sizeof( u16 );

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        GeomDrawPacket const& Packet = m_lDrawPackets[i];
        if( ( Packet.Kind != GEOM_PACKET_DYNAMIC ) || !Packet.pDamageUploadPending ||
            !*Packet.pDamageUploadPending )
        {
            continue;
        }

        if( !Packet.pDamageTexture || !Packet.pDamageUpload || ( Packet.DamageUploadWidth <= 0 ) ||
            ( Packet.DamageUploadHeight <= 0 ) )
        {
            return FALSE;
        }

        vram_texture_upload_desc Upload;
        Upload.Region.X      = Packet.DamageUploadX;
        Upload.Region.Y      = Packet.DamageUploadY;
        Upload.Region.Width  = Packet.DamageUploadWidth;
        Upload.Region.Height = Packet.DamageUploadHeight;
        Upload.pData         = Packet.pDamageUpload;
        Upload.Size          = Packet.DamageUploadWidth * Packet.DamageUploadHeight;
        Upload.RowPitch      = Packet.DamageUploadWidth;
        Upload.SlicePitch    = Upload.Size;
        if( !vram_UploadTexture( *Packet.pDamageTexture, Upload ) )
        {
            return FALSE;
        }

        *Packet.pDamageUploadPending = FALSE;

        static xprofile_counter DamageUploadBytes =
            x_GetProfiler().RegisterCounter( "ClothDamageUploadBytes", "RenderCounter" );
        DamageUploadBytes.Add( Upload.Size );
    }

    if( !EnsureDynamicBuffer( m_dynamicVertexBuffer, m_dynamicVertexCapacity, VertexCount,
                              sizeof( dynamic_geometry_vertex ), RBUFFER_USAGE_VERTEX, "GeomDynamicVertices" ) ||
        !EnsureDynamicBuffer( m_dynamicIndexBuffer, m_dynamicIndexCapacity, IndexCount,
                              sizeof( u16 ), RBUFFER_USAGE_INDEX, "GeomDynamicIndices" ) ||
        !rbuffer_Upload( m_dynamicVertexBuffer, m_lDynamicFrameVertices.GetPtr(), VertexBytes, 0, TRUE ) ||
        !rbuffer_Upload( m_dynamicIndexBuffer, m_lDynamicFrameIndices.GetPtr(), IndexBytes, 0, TRUE ) )
    {
        return FALSE;
    }

    static xprofile_counter DynamicUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomDynamicUploadBytes", "RenderCounter" );
    DynamicUploadBytes.Add( VertexBytes + IndexBytes );
    return TRUE;
}

//==============================================================================
//  PIPELINE
//==============================================================================

render_pipeline* GeomMgr::GetDynamicPipeline( u32 RenderFlags )
{
    xbool const Wireframe = ( RenderFlags & ( render::WIREFRAME | render::WIREFRAME2 ) ) != 0;

    static shader_vertex_buffer_desc VertexBuffer;
    VertexBuffer.Slot   = 0;
    VertexBuffer.Stride = sizeof( dynamic_geometry_vertex );

    static shader_vertex_element Layout[] = {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( dynamic_geometry_vertex, Position ) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( dynamic_geometry_vertex, Normal ) ),
        shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, offsetof( dynamic_geometry_vertex, UV ) ),
        shader_vertex_element( 3, 0, SHADER_VERTEX_FORMAT_UBYTE4N_BGRA, offsetof( dynamic_geometry_vertex, Color ) ) };

    render_pipeline_desc Desc;
    Desc.Shader.pVertexShader = &m_dynamicVertexShader;
    Desc.Shader.pPixelShader = &m_dynamicPixelShader;
    Desc.Shader.pVertexBuffers = &VertexBuffer;
    Desc.Shader.VertexBufferCount = 1;
    Desc.Shader.pInputElements = Layout;
    Desc.Shader.InputElementCount = ARRAYSIZE( Layout );
    Desc.Shader.Topology = SHADER_TOPOLOGY_TRIANGLE_LIST;
    Desc.Depth = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_NORMAL );
    Desc.Raster = rstate_GetRasterDesc( Wireframe ? RSTATE_RASTER_PRESET_WIRE_NO_CULL
                                                  : RSTATE_RASTER_PRESET_SOLID_NO_CULL );
    Desc.ColorCount = 3;
    Desc.ColorTargets[0].Format = RTARGET_FORMAT_RGBA8;
    Desc.ColorTargets[1].Format = RTARGET_FORMAT_RGBA16F;
    Desc.ColorTargets[2].Format = RTARGET_FORMAT_RGBA16F;
    Desc.ColorTargets[0].Blend = rstate_GetBlendDesc( RSTATE_BLEND_PRESET_NONE );
    Desc.ColorTargets[1].Blend = rstate_GetBlendDesc( RSTATE_BLEND_PRESET_NONE );
    Desc.ColorTargets[2].Blend = rstate_GetBlendDesc( RSTATE_BLEND_PRESET_NONE );
    Desc.DepthFormat = RTARGET_FORMAT_DEPTH24_STENCIL8;
    Desc.pDebugName = Wireframe ? "GeomDynamicWireGBuffer" : "GeomDynamicGBuffer";

    return m_dynamicPipelines.GetOrCreate( Wireframe ? 1 : 0, Desc );
}

//==============================================================================

xbool GeomMgr::ExecuteDynamicPacket( GeomDrawPacket const& Packet, geom_pass_desc const& Pass )
{
    if( ( Packet.Kind != GEOM_PACKET_DYNAMIC ) || ( Packet.DynamicIndexCount == 0 ) ||
        ( Pass.TargetWidth == 0 ) || ( Pass.TargetHeight == 0 ) )
    {
        return FALSE;
    }

    view const* pView = eng_GetView();
    render_pipeline* pPipeline = GetDynamicPipeline( Packet.RenderFlags );
    rstate_sampler const* pSampler = GetSampler( RSTATE_SAMPLER_PRESET_LINEAR_CLAMP );
    if( !pView || !pPipeline || !pSampler || !Packet.Resources.pDiffuse || !Packet.pDamageMask ||
        !render_BindPipeline( *pPipeline ) )
    {
        return FALSE;
    }
    m_activeShaderKind = GEOM_SHADER_DYNAMIC;

    DynamicGeometryConstants Constants;
    Constants.View = pView->GetW2V();
    Constants.Projection = pView->GetV2C();
    vector3 const& CameraPosition = pView->GetPosition();
    Constants.CameraPosition.Set( CameraPosition.GetX(), CameraPosition.GetY(), CameraPosition.GetZ(), 1.0f );
    f32 NearZ = 0.0f;
    f32 FarZ = 0.0f;
    pView->GetZLimits( NearZ, FarZ );
    Constants.DepthParams.Set( NearZ, FarZ, 0.0f, 0.0f );
    Constants.LightingIndex = Packet.DynamicLightingIndex;
    Constants.ShaderFlags = BuildInstanceFlags( Packet.RenderFlags ) | INSTANCE_FLAG_RECEIVE_LOCAL_SHADOW;
    Constants.Padding[0] = 0;
    Constants.Padding[1] = 0;

    shader_resource const* pLighting = rbuffer_GetResource( m_lightingDataBuffer );
    shader_resource const* pShadowMatrices = GetShadowMatricesResource();
    shader_resource const* pShadowData = GetShadowDataResource();
    shader_resource const* pFaceShadow = GetFaceShadowResource();
    shader_resource const* pFaceShadowDepth = GetFaceShadowDepthResource();
    shader_resource const* pProjectionAtlas = g_ProjectionAtlas.GetShaderResource();
    rstate_sampler const* pPointSampler = GetSampler( RSTATE_SAMPLER_PRESET_POINT_CLAMP );
    rstate_sampler const* pFaceShadowSampler = GetSampler( m_faceShadowSamplerPreset );
    if( !pLighting || !pShadowMatrices || !pShadowData || !pFaceShadow || !pFaceShadowDepth ||
        !pProjectionAtlas || !pPointSampler || !pFaceShadowSampler )
        return FALSE;

    if( !shader_PushUniformData( SHADER_STAGE_VERTEX, m_dynamicConstantsVertexSlot, &Constants, sizeof( Constants ) ) ||
        !shader_PushUniformData( SHADER_STAGE_PIXEL, m_dynamicConstantsPixelSlot, &Constants, sizeof( Constants ) ) ||
        !UpdateProjTextures() ||
        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_dynamicDiffuseSlot,
                                                     Packet.Resources.pDiffuse, pSampler ) ) ||
        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_dynamicDamageSlot,
                                                     Packet.pDamageMask, pSampler ) ) ||
        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL,
                                                     m_dynamicShaderBindings.FaceShadowTexturePixel,
                                                     pFaceShadow, pFaceShadowSampler ) ) ||
        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL,
                                                     m_dynamicShaderBindings.FaceShadowDepthTexturePixel,
                                                     pFaceShadowDepth, pPointSampler ) ) ||
        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL,
                                                     m_dynamicShaderBindings.ProjectionAtlasTexturePixel,
                                                     pProjectionAtlas, pSampler ) ) ||
        !shader_BindStorageBuffer( shader_storage_buffer_binding( SHADER_STAGE_PIXEL,
                                                                  m_dynamicShaderBindings.LightingDataPixel,
                                                                  pLighting ) ) ||
        !shader_BindStorageBuffer( shader_storage_buffer_binding( SHADER_STAGE_PIXEL,
                                                                  m_dynamicShaderBindings.ShadowMatricesPixel,
                                                                  pShadowMatrices ) ) ||
        !shader_BindStorageBuffer( shader_storage_buffer_binding( SHADER_STAGE_PIXEL,
                                                                  m_dynamicShaderBindings.ShadowDataPixel,
                                                                  pShadowData ) ) ||
        !rbuffer_BindVertex( m_dynamicVertexBuffer, 0 ) ||
        !rbuffer_BindIndex( m_dynamicIndexBuffer, RBUFFER_INDEX_FORMAT_U16 ) ||
        !rdraw_DrawIndexedInstanced( Packet.DynamicIndexCount, 1, Packet.FirstDynamicIndex,
                                     Packet.FirstDynamicVertex, 0 ) )
    {
        return FALSE;
    }

    m_gBufferGpuDrawCount++;
    m_gBufferInstanceCount++;
    m_gBufferSubmittedIndexCount += Packet.DynamicIndexCount;
    return TRUE;
}
