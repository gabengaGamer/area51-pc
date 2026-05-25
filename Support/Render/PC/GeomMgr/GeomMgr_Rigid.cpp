//==============================================================================
//
//  GeomMgr_Rigid.cpp
//
//  Rigid material handling for the PC geom manager
//
//==============================================================================

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

#include "GeomMgr.hpp"
#include "../VertexMgr.hpp"

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
xbool geom_EnsureStructuredBuffer( ID3D11Buffer*&             pBuffer,
                                   ID3D11ShaderResourceView*& pSRV,
                                   u32&                      Capacity,
                                   u32                       RequiredCount,
                                   u32                       ElementStride,
                                   const char*               pDebugName )
{
    if( !g_pd3dDevice )
        return FALSE;

    const u32 DesiredCapacity = MAX( RequiredCount, 1 );
    if( pBuffer && pSRV && (Capacity >= DesiredCapacity) )
        return TRUE;

    if( pSRV )
    {
        pSRV->Release();
        pSRV = NULL;
    }

    if( pBuffer )
    {
        pBuffer->Release();
        pBuffer = NULL;
    }

    D3D11_BUFFER_DESC BufferDesc;
    x_memset( &BufferDesc, 0, sizeof(BufferDesc) );
    BufferDesc.Usage               = D3D11_USAGE_DYNAMIC;
    BufferDesc.ByteWidth           = DesiredCapacity * ElementStride;
    BufferDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    BufferDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    BufferDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    BufferDesc.StructureByteStride = ElementStride;

    HRESULT hr = g_pd3dDevice->CreateBuffer( &BufferDesc, NULL, &pBuffer );
    if( FAILED(hr) )
    {
        x_DebugMsg( "GeomMgr: Failed to create %s buffer, HRESULT 0x%08X\n", pDebugName, hr );
        Capacity = 0;
        return FALSE;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    x_memset( &SRVDesc, 0, sizeof(SRVDesc) );
    SRVDesc.Format              = DXGI_FORMAT_UNKNOWN;
    SRVDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.FirstElement = 0;
    SRVDesc.Buffer.NumElements  = DesiredCapacity;

    hr = g_pd3dDevice->CreateShaderResourceView( pBuffer, &SRVDesc, &pSRV );
    if( FAILED(hr) )
    {
        x_DebugMsg( "GeomMgr: Failed to create %s SRV, HRESULT 0x%08X\n", pDebugName, hr );
        pBuffer->Release();
        pBuffer = NULL;
        Capacity = 0;
        return FALSE;
    }

    Capacity = DesiredCapacity;
    return TRUE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool geom_mgr::InitRigidShaders( void )
{
    x_DebugMsg( "GeomMgr: Initializing rigid shaders\n" );

    // Initialize member variables
    m_pRigidVertexShader    = NULL;
    m_pRigidPixelShader     = NULL;
    m_pRigidInputLayout     = NULL;
    m_pRigidFrameBuffer     = NULL;
    m_pRigidInstanceDataBuffer = NULL;
    m_pRigidInstanceDataSRV    = NULL;
    m_pRigidColorBuffer        = NULL;
    m_pRigidColorSRV           = NULL;
    m_RigidInstanceCapacity    = 0;
    m_RigidColorCapacity       = 0;
    ResetRigidBatch();
    m_bRigidFrameDirty      = TRUE;

    D3D11_INPUT_ELEMENT_DESC rigidLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    char shaderPath[256];
    x_sprintf( shaderPath, "a51_rigid_simple.hlsl" );

    m_pRigidVertexShader = shader_CompileVertexFromFileWithLayout( shaderPath,
                                                                  &m_pRigidInputLayout,
                                                                  rigidLayout,
                                                                  ARRAYSIZE(rigidLayout),
                                                                  "VSMain",
                                                                  "vs_5_0" );

    m_pRigidPixelShader   = shader_CompilePixelFromFile( shaderPath, "PSMain", "ps_5_0" );

    m_pRigidFrameBuffer   = shader_CreateConstantBuffer( sizeof(cb_geom_frame), CB_TYPE_DYNAMIC );

    x_DebugMsg( "GeomMgr: Rigid shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void geom_mgr::KillRigidShaders( void )
{
    ResetRigidInstanceData();
    ResetRigidBatch();

    if( m_pRigidColorSRV )
    {
        m_pRigidColorSRV->Release();
        m_pRigidColorSRV = NULL;
    }

    if( m_pRigidColorBuffer )
    {
        m_pRigidColorBuffer->Release();
        m_pRigidColorBuffer = NULL;
    }

    if( m_pRigidInstanceDataSRV )
    {
        m_pRigidInstanceDataSRV->Release();
        m_pRigidInstanceDataSRV = NULL;
    }

    if( m_pRigidInstanceDataBuffer )
    {
        m_pRigidInstanceDataBuffer->Release();
        m_pRigidInstanceDataBuffer = NULL;
    }

    m_RigidInstanceCapacity = 0;
    m_RigidColorCapacity    = 0;

    if( m_pRigidVertexShader )
    {
        m_pRigidVertexShader->Release();
        m_pRigidVertexShader = NULL;
    }

    if( m_pRigidPixelShader )
    {
        m_pRigidPixelShader->Release();
        m_pRigidPixelShader = NULL;
    }

    if( m_pRigidInputLayout )
    {
        m_pRigidInputLayout->Release();
        m_pRigidInputLayout = NULL;
    }

    if( m_pRigidFrameBuffer )
    {
        m_pRigidFrameBuffer->Release();
        m_pRigidFrameBuffer = NULL;
    }

    x_DebugMsg( "GeomMgr: Rigid shaders released\n" );
}

//==============================================================================

void geom_mgr::ResetRigidBatch( void )
{
    m_lRigidBatchInstances.Clear();
    m_lRigidBatchColors.Clear();
    m_hRigidBatchDList.Handle = HNULL;
    m_RigidBatchFlags = 0;
    m_RigidBatchUOffset = 0;
    m_RigidBatchVOffset = 0;
    m_RigidBatchOverrideMat = FALSE;
}

//==============================================================================

void geom_mgr::BeginRigidBatch( void )
{
    ResetRigidBatch();
}

//==============================================================================

xbool geom_mgr::HasRigidBatch( void ) const
{
    return ( m_lRigidBatchInstances.GetCount() > 0 );
}

//==============================================================================

xbool geom_mgr::CanAppendRigidBatch( const desc_rigid_batch& Desc ) const
{
    if( !HasRigidBatch() )
        return TRUE;

    return ( m_hRigidBatchDList.Handle == Desc.hDList.Handle ) &&
           ( BuildBatchStateFlags( m_RigidBatchFlags ) == BuildBatchStateFlags( Desc.RenderFlags ) ) &&
           ( m_RigidBatchUOffset == Desc.UOffset ) &&
           ( m_RigidBatchVOffset == Desc.VOffset ) &&
           ( m_RigidBatchOverrideMat == Desc.OverrideMat ) &&
           CanAppendLightCookies( &m_lRigidBatchInstances[0],
                                  m_lRigidBatchInstances.GetCount(),
                                  Desc.pLighting );
}

//==============================================================================

u32 geom_mgr::GetRigidBatchFlags( void ) const
{
    return m_RigidBatchFlags;
}

//==============================================================================

u8 geom_mgr::GetRigidBatchOverrideMat( void ) const
{
    return m_RigidBatchOverrideMat;
}

//==============================================================================

void geom_mgr::SetRigidMaterial( const material* pMaterial,
                                 u32             RenderFlags,
                                 u8              UOffset,
                                 u8              VOffset,
                                 u8              OverrideMat )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    shader_pass Pass;
    Pass.pInputLayout    = m_pRigidInputLayout;
    Pass.pVertexShader   = m_pRigidVertexShader;
    Pass.pPixelShader    = m_pRigidPixelShader;
    Pass.pGeometryShader = NULL;
    Pass.Topology        = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    shader_ApplyPass( Pass );

    if( !UpdateRigidConstants( pMaterial, UOffset, VOffset, OverrideMat ) )
    {
        x_DebugMsg( "GeomMgr: Failed to update rigid constants\n" );
        return;
    }
    
    ApplyRenderStates( pMaterial, RenderFlags, OverrideMat );

    if( OverrideMat )
    {
        ResetLightCookies();
        ResetProjTextures();
        ResetShadowMaps();
        return;
    }

    if( !pMaterial )
    {
        ResetLightCookies();
        ResetProjTextures();
        ResetShadowMaps();
        return;
    }

    if( g_ProjTextureMgr.CanReceiveProjTexture( *pMaterial ) )
    {
        UpdateProjTextures( 4 );
    }
    else
    {
        ResetProjTextures();
    }

    if( g_ShadowMapMgr.CanReceiveShadowMap( *pMaterial ) )
    {
        UpdateShadowMaps();
    }
    else
    {
        ResetShadowMaps();
    }
}

//==============================================================================

void geom_mgr::FillRigidInstanceLighting( cb_rigid_instance&  Instance,
                                          const cb_geom_lighting* pLighting )
{
    if( !pLighting )
        return;

    Instance.LightCount  = MIN( pLighting->LightCount, MAX_GEOM_LIGHTS );
    Instance.LightAmbCol = pLighting->AmbCol;

    for( s32 i = 0; i < (s32)Instance.LightCount; i++ )
    {
        Instance.LightVec [i] = pLighting->LightVec [i];
        Instance.LightCol [i] = pLighting->LightCol [i];
        Instance.LightDir [i] = pLighting->LightDir [i];
        Instance.LightCone[i] = pLighting->LightCone[i];
        Instance.LightCookieU[i] = pLighting->LightCookieU[i];
        Instance.LightCookieV[i] = pLighting->LightCookieV[i];
    }
}

//==============================================================================

void geom_mgr::AddRigidBatchInstance( const desc_rigid_batch& Desc )
{
    ASSERT( Desc.pGeom );
    ASSERT( ( Desc.iSubMesh >= 0 ) && ( Desc.iSubMesh < Desc.pGeom->m_nSubMeshes ) );

    if( m_lRigidBatchInstances.GetCount() == 0 )
    {
        m_hRigidBatchDList      = Desc.hDList;
        m_RigidBatchFlags       = Desc.RenderFlags;
        m_RigidBatchUOffset     = Desc.UOffset;
        m_RigidBatchVOffset     = Desc.VOffset;
        m_RigidBatchOverrideMat = Desc.OverrideMat;
    }
    else
    {
        ASSERT( m_hRigidBatchDList.Handle == Desc.hDList.Handle );
        ASSERT( BuildBatchStateFlags( m_RigidBatchFlags ) == BuildBatchStateFlags( Desc.RenderFlags ) );
        ASSERT( m_RigidBatchUOffset == Desc.UOffset );
        ASSERT( m_RigidBatchVOffset == Desc.VOffset );
        ASSERT( m_RigidBatchOverrideMat == Desc.OverrideMat );
    }

    const geom::submesh&        SubMesh = Desc.pGeom->m_pSubMesh[Desc.iSubMesh];
    const rigid_geom::dlist_pc& DList   = Desc.pGeom->m_System.pPC[SubMesh.iDList];

    cb_rigid_instance& GPUInst = m_lRigidBatchInstances.Append();
    x_memset( &GPUInst, 0, sizeof(GPUInst) );

    if( Desc.pL2W )
        GPUInst.World = *Desc.pL2W;
    else
        GPUInst.World.Identity();

    GPUInst.ShaderFlags = BuildInstanceFlags( Desc.RenderFlags );
    GPUInst.ColorOffset = 0xFFFFFFFFu;
    GPUInst.BaseVertex  = (u32)g_RigidVertMgr.GetDListVertexOffset( Desc.hDList );
    GPUInst.FadeAlpha   = BuildInstanceFadeAlpha( Desc.RenderFlags, Desc.Alpha );
    FillRigidInstanceLighting( GPUInst, Desc.pLighting );

    if( Desc.pColorInfo )
    {
        GPUInst.ColorOffset = m_lRigidBatchColors.GetCount();

        for( s32 i = 0; i < DList.nVerts; i++ )
        {
            u32& Color = m_lRigidBatchColors.Append();
            Color = Desc.pColorInfo[DList.iColor + i];
        }
    }
}

//==============================================================================

void geom_mgr::FlushRigidBatch( const material* pMaterial, u8 MaterialOverride )
{
    if( !HasRigidBatch() )
    {
        ResetRigidInstanceData();
        ResetRigidBatch();
        return;
    }

    SetRigidMaterial( pMaterial,
                      m_RigidBatchFlags,
                      m_RigidBatchUOffset,
                      m_RigidBatchVOffset,
                      MaterialOverride );
    if( !MaterialOverride && pMaterial )
    {
        BindLightCookies( &m_lRigidBatchInstances[0],
                          m_lRigidBatchInstances.GetCount() );
    }

    if( SetRigidInstanceData( &m_lRigidBatchInstances[0],
                              m_lRigidBatchInstances.GetCount(),
                              m_lRigidBatchColors.GetCount() ? &m_lRigidBatchColors[0] : NULL,
                              m_lRigidBatchColors.GetCount() ) )
    {
        g_RigidVertMgr.DrawDListInstanced( m_hRigidBatchDList, m_lRigidBatchInstances.GetCount() );
    }

    ResetRigidInstanceData();
    ResetLightCookies();
    ResetProjTextures();
    ResetShadowMaps();
    ResetRigidBatch();
}

//==============================================================================

xbool geom_mgr::SetRigidInstanceData( const cb_rigid_instance* pInstances,
                                      s32                      nInstances,
                                      const u32*               pColors,
                                      s32                      nColors )
{
    if( !g_pd3dContext || !g_pd3dDevice || !pInstances || (nInstances <= 0) )
        return FALSE;

    const u32 ColorCount = ( nColors > 0 ) ? (u32)nColors : 1;
    if( !geom_EnsureStructuredBuffer( m_pRigidInstanceDataBuffer,
                                      m_pRigidInstanceDataSRV,
                                      m_RigidInstanceCapacity,
                                      (u32)nInstances,
                                      sizeof(cb_rigid_instance),
                                      "rigid instance data" ) )
    {
        return FALSE;
    }

    if( !geom_EnsureStructuredBuffer( m_pRigidColorBuffer,
                                      m_pRigidColorSRV,
                                      m_RigidColorCapacity,
                                      ColorCount,
                                      sizeof(u32),
                                      "rigid color data" ) )
    {
        return FALSE;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped;
    HRESULT hr = g_pd3dContext->Map( m_pRigidInstanceDataBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped );
    if( FAILED(hr) )
    {
        x_DebugMsg( "GeomMgr: Failed to map rigid instance data buffer, HRESULT 0x%08X\n", hr );
        return FALSE;
    }
    x_memcpy( Mapped.pData, pInstances, sizeof(cb_rigid_instance) * nInstances );
    g_pd3dContext->Unmap( m_pRigidInstanceDataBuffer, 0 );

    hr = g_pd3dContext->Map( m_pRigidColorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped );
    if( FAILED(hr) )
    {
        x_DebugMsg( "GeomMgr: Failed to map rigid color data buffer, HRESULT 0x%08X\n", hr );
        return FALSE;
    }

    if( (nColors > 0) && pColors )
    {
        x_memcpy( Mapped.pData, pColors, sizeof(u32) * nColors );
    }
    else
    {
        *((u32*)Mapped.pData) = 0;
    }

    g_pd3dContext->Unmap( m_pRigidColorBuffer, 0 );

    ID3D11ShaderResourceView* pRigidVSSRV[2] =
    {
        m_pRigidInstanceDataSRV,
        m_pRigidColorSRV
    };

    g_pd3dContext->VSSetShaderResources( TEXTURE_SLOT_RIGID_INSTANCE_DATA, 2, pRigidVSSRV );
    g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_RIGID_INSTANCE_DATA, 1, &m_pRigidInstanceDataSRV );
    return TRUE;
}

//==============================================================================

void geom_mgr::ResetRigidInstanceData( void )
{
    if( !g_pd3dContext )
        return;

    ID3D11ShaderResourceView* pNullRigidVS[2] = { NULL, NULL };
    ID3D11ShaderResourceView* pNullRigidPS[1] = { NULL };
    g_pd3dContext->VSSetShaderResources( TEXTURE_SLOT_RIGID_INSTANCE_DATA, 2, pNullRigidVS );
    g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_RIGID_INSTANCE_DATA, 1, pNullRigidPS );
}

//==============================================================================

xbool geom_mgr::UpdateRigidConstants( const material* pMaterial,
                                      u8              UOffset,
                                      u8              VOffset,
                                      u8              OverrideMat )
{
    if( !m_pRigidFrameBuffer )
    {
        x_DebugMsg( "GeomMgr: UpdateRigidConstants missing frame buffer\n" );
        return FALSE;
    }

    if( !g_pd3dDevice || !g_pd3dContext )
    {
        x_DebugMsg( "GeomMgr: UpdateRigidConstants missing D3D state (device=%d context=%d)\n",
                    g_pd3dDevice  ? 1 : 0,
                    g_pd3dContext ? 1 : 0 );
        return FALSE;
    }

    const view* pView = eng_GetView();
    if( !pView )
    {
        x_DebugMsg( "GeomMgr: UpdateRigidConstants called without an active view\n" );
        return FALSE;
    }

    const cb_geom_frame frameData = BuildFrameConstants( *pView,
                                                         pMaterial,
                                                         UOffset,
                                                         VOffset,
                                                         TRUE,
                                                         OverrideMat );

    const xbool bFrameChanged = ( m_bRigidFrameDirty ||
                                  x_memcmp( &m_CachedRigidFrame,
                                            &frameData,
                                            sizeof(cb_geom_frame) ) != 0 );

    if( bFrameChanged )
    {
        if( !UploadConstantBuffer( m_pRigidFrameBuffer,
                                   &frameData,
                                   sizeof(cb_geom_frame),
                                   "rigid frame" ) )
            return FALSE;

        m_CachedRigidFrame  = frameData;
        m_bRigidFrameDirty  = FALSE;
    }

    g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pRigidFrameBuffer );
    g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pRigidFrameBuffer );

    return TRUE;
}
