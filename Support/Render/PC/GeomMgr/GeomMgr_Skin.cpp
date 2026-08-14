//==============================================================================
//
//  GeomMgr_Skin.cpp
//
//  Skin material handling for the PC geom manager
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
#include "../SoftVertexMgr.hpp"

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool GeomMgr::InitSkinShaders( void )
{
    x_DebugMsg( "GeomMgr: Initializing skin shaders\n" );

    // Initialize member variables
    m_skinVertexShader = shader();
    m_skinPixelShader = shader();
    m_skinScenePixelShader = shader();
    m_skinInstanceDataBuffer = rbuffer();
    m_skinBoneDataBuffer = rbuffer();
    m_skinDrawInstanceBuffer = rbuffer();
    m_skinBoneRemapBuffer = rbuffer();
    m_skinIndirectBuffer = rbuffer();
    m_skinInstanceCapacity = 0;
    m_skinBoneCapacity = 0;
    m_skinDrawInstanceCapacity = 0;
    m_skinBoneRemapCapacity = 0;
    m_skinIndirectCapacity = 0;
    ResetSkinBatch();

    shader_LoadFromEcs( m_skinVertexShader, "skin_simple_vs.vs.ecs" );
    shader_LoadFromEcs( m_skinPixelShader, "skin_simple_ps.ps.ecs" );
    shader_LoadFromEcs( m_skinScenePixelShader, "skin_scene_ps.ps.ecs" );

    if ( !m_skinVertexShader || !m_skinPixelShader || !m_skinScenePixelShader )
    {
        x_DebugMsg( "GeomMgr: Failed to initialize skin shader resources\n" );
        KillSkinShaders();
        return FALSE;
    }

    x_DebugMsg( "GeomMgr: Skin shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void GeomMgr::KillSkinShaders( void )
{
    ResetSkinBatch();

    DestroySkinPipelines();
    shader_Destroy( m_skinVertexShader );
    shader_Destroy( m_skinPixelShader );
    shader_Destroy( m_skinScenePixelShader );

    rbuffer_Destroy( m_skinInstanceDataBuffer );
    rbuffer_Destroy( m_skinBoneDataBuffer );
    rbuffer_Destroy( m_skinDrawInstanceBuffer );
    rbuffer_Destroy( m_skinBoneRemapBuffer );
    rbuffer_Destroy( m_skinIndirectBuffer );
    m_skinInstanceCapacity = 0;
    m_skinBoneCapacity = 0;
    m_skinDrawInstanceCapacity = 0;
    m_skinBoneRemapCapacity = 0;
    m_skinIndirectCapacity = 0;

    x_DebugMsg( "GeomMgr: Skin shaders released\n" );
}

//==============================================================================

void GeomMgr::ResetSkinBatch( void )
{
    m_skinBatchFirstInstance = m_lSkinFrameInstances.GetCount();
    m_hSkinBatchMesh.Handle = HNULL;
    m_skinBatchFlags = 0;
    m_skinBatchUOffset = 0;
    m_skinBatchVOffset = 0;
    m_skinBatchOverrideMat = FALSE;
    m_skinBatchSortDepth = 0.0f;
}

//==============================================================================

xbool GeomMgr::HasSkinBatch( void ) const
{
    return ( static_cast<u32>( m_lSkinFrameInstances.GetCount() ) > m_skinBatchFirstInstance );
}

//==============================================================================

xbool GeomMgr::CanAppendSkinBatch( SkinBatchDesc const& desc ) const
{
    if ( !HasSkinBatch() )
    {
        return TRUE;
    }

    return ( m_hSkinBatchMesh.Handle == desc.hMesh.Handle ) &&
           ( BuildBatchStateFlags( m_skinBatchFlags ) == BuildBatchStateFlags( desc.RenderFlags ) ) &&
           ( m_skinBatchUOffset == desc.UOffset ) && ( m_skinBatchVOffset == desc.VOffset ) &&
           ( m_skinBatchOverrideMat == desc.OverrideMat );
}

//==============================================================================

xbool GeomMgr::SetSkinMaterial( material const* pMaterial, u32 renderFlags, u8 uOffset, u8 vOffset, u8 overrideMat,
                                xbool sceneOnly, geom_pass_desc const& pass )
{
    RenderStateSelection const state = ResolveRenderStates( pMaterial, renderFlags, overrideMat, sceneOnly );
    if ( !BindSkinPipeline( state ) )
    {
        x_DebugMsg( "GeomMgr: Failed to bind skin render pipeline\n" );
        return FALSE;
    }

    if ( !UpdateSkinConstants( pMaterial, uOffset, vOffset, overrideMat, pass ) )
    {
        x_DebugMsg( "GeomMgr: Failed to update skin constants\n" );
        return FALSE;
    }

    if ( overrideMat )
    {
        if ( !ResetProjTextures() )
        {
            return FALSE;
        }
        return TRUE;
    }

    if ( !pMaterial )
    {
        if ( !ResetProjTextures() )
        {
            return FALSE;
        }
        return TRUE;
    }

    if ( pMaterial->ReceivesProjection() )
    {
        if ( !UpdateProjTextures() )
        {
            return FALSE;
        }
    }
    else if ( !ResetProjTextures() )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void GeomMgr::AddSkinBatchInstance( SkinBatchDesc const& desc )
{
    ASSERT( desc.pGeom );
    ASSERT( x_isvalid( desc.SortDepth ) );
    ASSERT( ( desc.iSubMesh >= 0 ) && ( desc.iSubMesh < desc.pGeom->m_nSubMeshes ) );

    if ( !HasSkinBatch() )
    {
        m_hSkinBatchMesh = desc.hMesh;
        m_skinBatchFlags = desc.RenderFlags;
        m_skinBatchUOffset = desc.UOffset;
        m_skinBatchVOffset = desc.VOffset;
        m_skinBatchOverrideMat = desc.OverrideMat;
        m_skinBatchSortDepth = desc.SortDepth;
    }
    else
    {
        ASSERT( m_hSkinBatchMesh.Handle == desc.hMesh.Handle );
        ASSERT( BuildBatchStateFlags( m_skinBatchFlags ) == BuildBatchStateFlags( desc.RenderFlags ) );
        ASSERT( m_skinBatchUOffset == desc.UOffset );
        ASSERT( m_skinBatchVOffset == desc.VOffset );
        ASSERT( m_skinBatchOverrideMat == desc.OverrideMat );
    }

    SkinInstanceData& gpuInst = m_lSkinFrameInstances.Append();
    x_memset( &gpuInst, 0, sizeof( gpuInst ) );

    gpuInst.ShaderFlags = BuildInstanceFlags( desc.RenderFlags );
    gpuInst.BoneOffset = m_lSkinFrameBones.GetCount();
    gpuInst.FadeAlpha = BuildInstanceFadeAlpha( desc.RenderFlags, desc.Alpha );
    m_lSkinFrameLighting.Append() = desc.pLighting;
    if ( desc.pLighting )
    {
        m_litInstanceCount++;
        m_instanceLightCount += MIN( desc.pLighting->LightCount, MAX_GEOM_LIGHTS );
    }

    s32 const nBones = desc.pGeom->m_nBones;
    if ( nBones <= 0 )
    {
        return;
    }

    if ( desc.pBones )
    {
        for ( s32 i = 0; i < nBones; ++i )
        {
            m_lSkinFrameBones.Append() = desc.pBones[i];
        }
        return;
    }

    matrix4 identity;
    identity.Identity();

    for ( s32 i = 0; i < nBones; ++i )
    {
        m_lSkinFrameBones.Append() = identity;
    }
}

//==============================================================================

void GeomMgr::FlushSkinBatch( material const* pMaterial, u8 materialOverride, geometry_render_pass pass, u32 sequence )
{
    if ( !HasSkinBatch() )
    {
        ResetSkinBatch();
        return;
    }

    GeomDrawPacket packet;
    packet.Kind = GEOM_PACKET_SKIN;
    packet.pMaterial = pMaterial;
    packet.hMesh = m_hSkinBatchMesh;
    packet.RenderFlags = m_skinBatchFlags;
    packet.FirstInstance = m_skinBatchFirstInstance;
    packet.InstanceCount = m_lSkinFrameInstances.GetCount() - m_skinBatchFirstInstance;
    packet.UOffset = m_skinBatchUOffset;
    packet.VOffset = m_skinBatchVOffset;
    packet.MaterialOverride = materialOverride;
    packet.Pass = pass;
    packet.Sequence = sequence;
    packet.SortDepth = m_skinBatchSortDepth;
    packet.Resources = CaptureResourceSnapshot();

    m_lDrawPackets.Append() = packet;
    ResetSkinBatch();
}

//==============================================================================

xbool GeomMgr::UpdateSkinConstants( material const* pMaterial, u8 uOffset, u8 vOffset, u8 overrideMat,
                                    geom_pass_desc const& pass )
{
    view const* pView = eng_GetView();
    if ( !pView )
    {
        x_DebugMsg( "GeomMgr: UpdateSkinConstants called without an active view\n" );
        return FALSE;
    }

    GeomFrameConstants const frameData =
        BuildFrameConstants( *pView, pMaterial, uOffset, vOffset, FALSE, overrideMat, pass );

    if ( !PushFrameConstants( GEOM_SHADER_SKIN, frameData ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool GeomMgr::BindSkinFrameBuffers( void ) const
{
    shader_resource const* pInstanceResource = rbuffer_GetResource( m_skinInstanceDataBuffer );
    shader_resource const* pBoneResource = rbuffer_GetResource( m_skinBoneDataBuffer );
    shader_resource const* pBoneRemapResource = rbuffer_GetResource( m_skinBoneRemapBuffer );
    shader_resource const* pLightingResource = rbuffer_GetResource( m_lightingDataBuffer );
    shader_resource const* pShadowMatrices = rbuffer_GetResource( m_shadowMatrixBuffer );
    shader_resource const* pShadowData = rbuffer_GetResource( m_shadowDataBuffer );
    if ( !pInstanceResource || !pBoneResource || !pBoneRemapResource || !pLightingResource || !pShadowMatrices ||
         !pShadowData )
    {
        return FALSE;
    }

    shader_storage_buffer_binding const bindings[] = {
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, m_skinShaderBindings.InstanceDataVertex,
                                       pInstanceResource ),
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, m_skinShaderBindings.AuxiliaryDataVertex, pBoneResource ),
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, m_skinShaderBindings.BoneRemapDataVertex,
                                       pBoneRemapResource ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_skinShaderBindings.InstanceDataPixel, pInstanceResource ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_skinShaderBindings.LightingDataPixel, pLightingResource ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_skinShaderBindings.ShadowMatricesPixel, pShadowMatrices ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_skinShaderBindings.ShadowDataPixel, pShadowData ) };

    return shader_BindStorageBuffers( bindings, ARRAYSIZE( bindings ) );
}

//==============================================================================

xbool GeomMgr::BindSkinGeometryBuffers( VertexMgr::mesh_range const& range ) const
{
    return g_SkinVertMgr.BindPools( range ) && rbuffer_BindVertex( m_skinDrawInstanceBuffer, 1, 0 );
}
