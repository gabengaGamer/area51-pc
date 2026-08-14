//==============================================================================
//
//  GeomMgr_Rigid.cpp
//
//  Rigid material handling for the PC geom manager
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

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool GeomMgr::InitRigidShaders( void )
{
    x_DebugMsg( "GeomMgr: Initializing rigid shaders\n" );

    // Initialize member variables
    m_rigidVertexShader = shader();
    m_rigidPixelShader = shader();
    m_rigidScenePixelShader = shader();
    m_rigidInstanceDataBuffer = rbuffer();
    m_rigidInstanceIndexBuffer = rbuffer();
    m_rigidColorBuffer = rbuffer();
    m_rigidIndirectBuffer = rbuffer();
    m_rigidInstanceCapacity = 0;
    m_rigidInstanceIndexCapacity = 0;
    m_rigidColorCapacity = 0;
    m_rigidIndirectCapacity = 0;
    ResetRigidBatch();

    shader_LoadFromEcs( m_rigidVertexShader, "rigid_simple_vs.vs.ecs" );
    shader_LoadFromEcs( m_rigidPixelShader, "rigid_simple_ps.ps.ecs" );
    shader_LoadFromEcs( m_rigidScenePixelShader, "rigid_scene_ps.ps.ecs" );

    if ( !m_rigidVertexShader || !m_rigidPixelShader || !m_rigidScenePixelShader )
    {
        x_DebugMsg( "GeomMgr: Failed to initialize rigid shader resources\n" );
        KillRigidShaders();
        return FALSE;
    }

    x_DebugMsg( "GeomMgr: Rigid shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void GeomMgr::KillRigidShaders( void )
{
    ResetRigidBatch();

    rbuffer_Destroy( m_rigidColorBuffer );
    rbuffer_Destroy( m_rigidInstanceIndexBuffer );
    rbuffer_Destroy( m_rigidInstanceDataBuffer );
    rbuffer_Destroy( m_rigidIndirectBuffer );

    m_rigidInstanceCapacity = 0;
    m_rigidInstanceIndexCapacity = 0;
    m_rigidColorCapacity = 0;
    m_rigidIndirectCapacity = 0;

    DestroyRigidPipelines();
    shader_Destroy( m_rigidVertexShader );
    shader_Destroy( m_rigidPixelShader );
    shader_Destroy( m_rigidScenePixelShader );

    x_DebugMsg( "GeomMgr: Rigid shaders released\n" );
}

//==============================================================================

void GeomMgr::ResetRigidBatch( void )
{
    m_rigidBatchFirstInstance = m_lRigidFrameInstances.GetCount();
    m_hRigidBatchMesh.Handle = HNULL;
    m_rigidBatchFlags = 0;
    m_rigidBatchUOffset = 0;
    m_rigidBatchVOffset = 0;
    m_rigidBatchOverrideMat = FALSE;
    m_rigidBatchSortDepth = 0.0f;
}

//==============================================================================

xbool GeomMgr::HasRigidBatch( void ) const
{
    return ( static_cast<u32>( m_lRigidFrameInstances.GetCount() ) > m_rigidBatchFirstInstance );
}

//==============================================================================

xbool GeomMgr::CanAppendRigidBatch( RigidBatchDesc const& desc ) const
{
    if ( !HasRigidBatch() )
    {
        return TRUE;
    }

    return ( m_hRigidBatchMesh.Handle == desc.hMesh.Handle ) &&
           ( BuildBatchStateFlags( m_rigidBatchFlags ) == BuildBatchStateFlags( desc.RenderFlags ) ) &&
           ( m_rigidBatchUOffset == desc.UOffset ) && ( m_rigidBatchVOffset == desc.VOffset ) &&
           ( m_rigidBatchOverrideMat == desc.OverrideMat );
}

//==============================================================================

xbool GeomMgr::SetRigidMaterial( material const* pMaterial, u32 renderFlags, u8 uOffset, u8 vOffset, u8 overrideMat,
                                 xbool sceneOnly, geom_pass_desc const& pass )
{
    RenderStateSelection const state = ResolveRenderStates( pMaterial, renderFlags, overrideMat, sceneOnly );
    if ( !BindRigidPipeline( state ) )
    {
        x_DebugMsg( "GeomMgr: Failed to bind rigid render pipeline\n" );
        return FALSE;
    }

    if ( !UpdateRigidConstants( pMaterial, uOffset, vOffset, overrideMat, pass ) )
    {
        x_DebugMsg( "GeomMgr: Failed to update rigid constants\n" );
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

void GeomMgr::AddRigidBatchInstance( RigidBatchDesc const& desc )
{
    ASSERT( desc.pGeom );
    ASSERT( x_isvalid( desc.SortDepth ) );
    ASSERT( ( desc.iSubMesh >= 0 ) && ( desc.iSubMesh < desc.pGeom->m_nSubMeshes ) );

    if ( !HasRigidBatch() )
    {
        m_hRigidBatchMesh = desc.hMesh;
        m_rigidBatchFlags = desc.RenderFlags;
        m_rigidBatchUOffset = desc.UOffset;
        m_rigidBatchVOffset = desc.VOffset;
        m_rigidBatchOverrideMat = desc.OverrideMat;
        m_rigidBatchSortDepth = desc.SortDepth;
    }
    else
    {
        ASSERT( m_hRigidBatchMesh.Handle == desc.hMesh.Handle );
        ASSERT( BuildBatchStateFlags( m_rigidBatchFlags ) == BuildBatchStateFlags( desc.RenderFlags ) );
        ASSERT( m_rigidBatchUOffset == desc.UOffset );
        ASSERT( m_rigidBatchVOffset == desc.VOffset );
        ASSERT( m_rigidBatchOverrideMat == desc.OverrideMat );
    }

    geom::submesh const& subMesh = desc.pGeom->m_pSubMesh[desc.iSubMesh];
    ASSERT( subMesh.nSections == 1 );
    rigid_geom::section const& section = desc.pGeom->m_pSection[subMesh.iSection];

    RigidInstanceData& gpuInst = m_lRigidFrameInstances.Append();
    x_memset( &gpuInst, 0, sizeof( gpuInst ) );

    if ( desc.pL2W )
    {
        gpuInst.World = *desc.pL2W;
    }
    else
    {
        gpuInst.World.Identity();
    }

    gpuInst.ShaderFlags = BuildInstanceFlags( desc.RenderFlags );
    gpuInst.ColorOffset = 0xFFFFFFFFu;
    gpuInst.FadeAlpha = BuildInstanceFadeAlpha( desc.RenderFlags, desc.Alpha );
    m_lRigidFrameLighting.Append() = desc.pLighting;
    if ( desc.pLighting )
    {
        m_litInstanceCount++;
        m_instanceLightCount += MIN( desc.pLighting->LightCount, MAX_GEOM_LIGHTS );
    }

    if ( desc.pColorInfo )
    {
        gpuInst.ColorOffset = m_lRigidFrameColors.GetCount();

        for ( s32 i = 0; i < section.nVertices; i++ )
        {
            u32& color = m_lRigidFrameColors.Append();
            color = desc.pColorInfo[section.iColor + i];
        }
    }
}

//==============================================================================

void GeomMgr::FlushRigidBatch( material const* pMaterial, u8 materialOverride, geometry_render_pass pass, u32 sequence )
{
    if ( !HasRigidBatch() )
    {
        ResetRigidBatch();
        return;
    }

    GeomDrawPacket packet;
    packet.Kind = GEOM_PACKET_RIGID;
    packet.pMaterial = pMaterial;
    packet.hMesh = m_hRigidBatchMesh;
    packet.RenderFlags = m_rigidBatchFlags;
    packet.FirstInstance = m_rigidBatchFirstInstance;
    packet.InstanceCount = m_lRigidFrameInstances.GetCount() - m_rigidBatchFirstInstance;
    packet.UOffset = m_rigidBatchUOffset;
    packet.VOffset = m_rigidBatchVOffset;
    packet.MaterialOverride = materialOverride;
    packet.Pass = pass;
    packet.Sequence = sequence;
    packet.SortDepth = m_rigidBatchSortDepth;
    packet.Resources = CaptureResourceSnapshot();

    m_lDrawPackets.Append() = packet;
    ResetRigidBatch();
}

//==============================================================================

xbool GeomMgr::BindRigidFrameBuffers( void ) const
{
    shader_resource const* pInstanceResource = rbuffer_GetResource( m_rigidInstanceDataBuffer );
    shader_resource const* pColorResource = rbuffer_GetResource( m_rigidColorBuffer );
    shader_resource const* pLightingResource = rbuffer_GetResource( m_lightingDataBuffer );
    shader_resource const* pShadowMatrices = rbuffer_GetResource( m_shadowMatrixBuffer );
    shader_resource const* pShadowData = rbuffer_GetResource( m_shadowDataBuffer );
    if ( !pInstanceResource || !pColorResource || !pLightingResource || !pShadowMatrices || !pShadowData )
    {
        return FALSE;
    }

    shader_storage_buffer_binding const bindings[] = {
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, m_rigidShaderBindings.InstanceDataVertex,
                                       pInstanceResource ),
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, m_rigidShaderBindings.AuxiliaryDataVertex, pColorResource ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_rigidShaderBindings.InstanceDataPixel, pInstanceResource ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_rigidShaderBindings.LightingDataPixel, pLightingResource ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_rigidShaderBindings.ShadowMatricesPixel, pShadowMatrices ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_rigidShaderBindings.ShadowDataPixel, pShadowData ) };

    return shader_BindStorageBuffers( bindings, ARRAYSIZE( bindings ) );
}

//==============================================================================

xbool GeomMgr::BindRigidGeometryBuffers( VertexMgr::mesh_range const& range ) const
{
    return g_RigidVertMgr.BindPools( range ) && g_RigidVertMgr.BindVertexIndices( range, 2 ) &&
           rbuffer_BindVertex( m_rigidInstanceIndexBuffer, 1, 0 );
}

//==============================================================================

xbool GeomMgr::UpdateRigidConstants( material const* pMaterial, u8 uOffset, u8 vOffset, u8 overrideMat,
                                     geom_pass_desc const& pass )
{
    view const* pView = eng_GetView();
    if ( !pView )
    {
        x_DebugMsg( "GeomMgr: UpdateRigidConstants called without an active view\n" );
        return FALSE;
    }

    GeomFrameConstants const frameData =
        BuildFrameConstants( *pView, pMaterial, uOffset, vOffset, TRUE, overrideMat, pass );

    if ( !PushFrameConstants( GEOM_SHADER_RIGID, frameData ) )
    {
        return FALSE;
    }

    return TRUE;
}
