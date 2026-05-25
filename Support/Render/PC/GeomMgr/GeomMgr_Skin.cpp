//==============================================================================
//
//  GeomMgr_Skin.cpp
//
//  Skin material handling for the PC geom manager
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
#include "../SoftVertexMgr.hpp"

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
xbool geom_EnsureSkinStructuredBuffer( ID3D11Buffer*&             pBuffer,
                                       ID3D11ShaderResourceView*& pSRV,
                                       u32&                      Capacity,
                                       u32                       RequiredCount,
                                       u32                       ElementStride,
                                       const char*               pDebugName )
{
    if( !g_pd3dDevice )
        return FALSE;

    const u32 DesiredCapacity = MAX( RequiredCount, 1 );
    if( pBuffer && pSRV && ( Capacity >= DesiredCapacity ) )
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

xbool geom_mgr::InitSkinShaders( void )
{
    x_DebugMsg( "GeomMgr: Initializing skin shaders\n" );

    // Initialize member variables
    m_pSkinVertexShader   = NULL;
    m_pSkinPixelShader    = NULL;
    m_pSkinInputLayout    = NULL;
    m_pSkinFrameBuffer    = NULL;
    m_pSkinBoneBuffer     = NULL;
    m_pSkinSectionBuffer  = NULL;
    m_pSkinInstanceDataBuffer = NULL;
    m_pSkinInstanceDataSRV    = NULL;
    m_pSkinBoneDataBuffer     = NULL;
    m_pSkinBoneDataSRV        = NULL;
    m_SkinInstanceCapacity    = 0;
    m_SkinBoneCapacity        = 0;
    ResetSkinBatch();
    m_bSkinFrameDirty     = TRUE;

    D3D11_INPUT_ELEMENT_DESC skinLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    char shaderPath[256];
    x_sprintf( shaderPath, "a51_skin_simple.hlsl" );

    m_pSkinVertexShader = shader_CompileVertexFromFileWithLayout( shaderPath,
                                                                 &m_pSkinInputLayout,
                                                                 skinLayout,
                                                                 ARRAYSIZE(skinLayout),
                                                                 "VSMain",
                                                                 "vs_5_0" );

    m_pSkinPixelShader  = shader_CompilePixelFromFile( shaderPath, "PSMain", "ps_5_0" );

    m_pSkinFrameBuffer  = shader_CreateConstantBuffer( sizeof(cb_geom_frame), CB_TYPE_DYNAMIC );
    m_pSkinBoneBuffer   = shader_CreateConstantBuffer( sizeof(cb_skin_bone) * MAX_SKIN_BONES, CB_TYPE_DYNAMIC );
    m_pSkinSectionBuffer= shader_CreateConstantBuffer( sizeof(u32) * MAX_SKIN_BONES, CB_TYPE_DYNAMIC );

    x_DebugMsg( "GeomMgr: Skin shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void geom_mgr::KillSkinShaders( void )
{
    ResetSkinInstanceData();
    ResetSkinBatch();

    if( m_pSkinVertexShader )
    {
        m_pSkinVertexShader->Release();
        m_pSkinVertexShader = NULL;
    }

    if( m_pSkinPixelShader )
    {
        m_pSkinPixelShader->Release();
        m_pSkinPixelShader = NULL;
    }

    if( m_pSkinInputLayout )
    {
        m_pSkinInputLayout->Release();
        m_pSkinInputLayout = NULL;
    }

    if( m_pSkinFrameBuffer )
    {
        m_pSkinFrameBuffer->Release();
        m_pSkinFrameBuffer = NULL;
    }

    if( m_pSkinBoneBuffer )
    {
        m_pSkinBoneBuffer->Release();
        m_pSkinBoneBuffer = NULL;
    }

    if( m_pSkinSectionBuffer )
    {
        m_pSkinSectionBuffer->Release();
        m_pSkinSectionBuffer = NULL;
    }

    if( m_pSkinInstanceDataSRV )
    {
        m_pSkinInstanceDataSRV->Release();
        m_pSkinInstanceDataSRV = NULL;
    }

    if( m_pSkinInstanceDataBuffer )
    {
        m_pSkinInstanceDataBuffer->Release();
        m_pSkinInstanceDataBuffer = NULL;
    }

    if( m_pSkinBoneDataSRV )
    {
        m_pSkinBoneDataSRV->Release();
        m_pSkinBoneDataSRV = NULL;
    }

    if( m_pSkinBoneDataBuffer )
    {
        m_pSkinBoneDataBuffer->Release();
        m_pSkinBoneDataBuffer = NULL;
    }

    x_DebugMsg( "GeomMgr: Skin shaders released\n" );
}

//==============================================================================

void geom_mgr::ResetSkinBatch( void )
{
    m_lSkinBatchInstances.Clear();
    m_lSkinBatchBones.Clear();
    m_hSkinBatchDList.Handle = HNULL;
    m_SkinBatchFlags = 0;
    m_SkinBatchUOffset = 0;
    m_SkinBatchVOffset = 0;
    m_SkinBatchOverrideMat = FALSE;
}

//==============================================================================

void geom_mgr::BeginSkinBatch( void )
{
    ResetSkinBatch();
}

//==============================================================================

xbool geom_mgr::HasSkinBatch( void ) const
{
    return ( m_lSkinBatchInstances.GetCount() > 0 );
}

//==============================================================================

xbool geom_mgr::CanAppendSkinBatch( const desc_skin_batch& Desc ) const
{
    if( !HasSkinBatch() )
        return TRUE;

    return ( m_hSkinBatchDList.Handle == Desc.hDList.Handle ) &&
           ( BuildBatchStateFlags( m_SkinBatchFlags ) == BuildBatchStateFlags( Desc.RenderFlags ) ) &&
           ( m_SkinBatchUOffset == Desc.UOffset ) &&
           ( m_SkinBatchVOffset == Desc.VOffset ) &&
           ( m_SkinBatchOverrideMat == Desc.OverrideMat ) &&
           CanAppendLightCookies( &m_lSkinBatchInstances[0],
                                  m_lSkinBatchInstances.GetCount(),
                                  Desc.pLighting );
}

//==============================================================================

u32 geom_mgr::GetSkinBatchFlags( void ) const
{
    return m_SkinBatchFlags;
}

//==============================================================================

u8 geom_mgr::GetSkinBatchOverrideMat( void ) const
{
    return m_SkinBatchOverrideMat;
}

//==============================================================================

void geom_mgr::SetSkinMaterial( const material* pMaterial,
                                u32             RenderFlags,
                                u8              UOffset,
                                u8              VOffset,
                                u8              OverrideMat )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    shader_pass Pass;
    Pass.pInputLayout    = m_pSkinInputLayout;
    Pass.pVertexShader   = m_pSkinVertexShader;
    Pass.pPixelShader    = m_pSkinPixelShader;
    Pass.pGeometryShader = NULL;
    Pass.Topology        = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    shader_ApplyPass( Pass );

    if( !UpdateSkinConstants( pMaterial, UOffset, VOffset, OverrideMat ) )
    {
        x_DebugMsg( "GeomMgr: Failed to update skin constants\n" );
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

void geom_mgr::FillSkinInstanceLighting( cb_skin_instance&   Instance,
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

void geom_mgr::AddSkinBatchInstance( const desc_skin_batch& Desc )
{
    ASSERT( Desc.pGeom );
    ASSERT( ( Desc.iSubMesh >= 0 ) && ( Desc.iSubMesh < Desc.pGeom->m_nSubMeshes ) );

    if( m_lSkinBatchInstances.GetCount() == 0 )
    {
        m_hSkinBatchDList      = Desc.hDList;
        m_SkinBatchFlags       = Desc.RenderFlags;
        m_SkinBatchUOffset     = Desc.UOffset;
        m_SkinBatchVOffset     = Desc.VOffset;
        m_SkinBatchOverrideMat = Desc.OverrideMat;
    }
    else
    {
        ASSERT( m_hSkinBatchDList.Handle == Desc.hDList.Handle );
        ASSERT( BuildBatchStateFlags( m_SkinBatchFlags ) == BuildBatchStateFlags( Desc.RenderFlags ) );
        ASSERT( m_SkinBatchUOffset == Desc.UOffset );
        ASSERT( m_SkinBatchVOffset == Desc.VOffset );
        ASSERT( m_SkinBatchOverrideMat == Desc.OverrideMat );
    }

    cb_skin_instance& GPUInst = m_lSkinBatchInstances.Append();
    x_memset( &GPUInst, 0, sizeof(GPUInst) );

    GPUInst.ShaderFlags = BuildInstanceFlags( Desc.RenderFlags );
    GPUInst.BoneOffset  = m_lSkinBatchBones.GetCount();
    GPUInst.FadeAlpha   = BuildInstanceFadeAlpha( Desc.RenderFlags, Desc.Alpha );
    FillSkinInstanceLighting( GPUInst, Desc.pLighting );

    const s32 nBones = Desc.pGeom->m_nBones;
    if( nBones <= 0 )
        return;

    if( Desc.pBones )
    {
        for( s32 i = 0; i < nBones; ++i )
            m_lSkinBatchBones.Append() = Desc.pBones[i];
        return;
    }

    matrix4 Identity;
    Identity.Identity();

    for( s32 i = 0; i < nBones; ++i )
        m_lSkinBatchBones.Append() = Identity;
}

//==============================================================================

void geom_mgr::FlushSkinBatch( const material* pMaterial, u8 MaterialOverride )
{
    if( !HasSkinBatch() )
    {
        ResetSkinInstanceData();
        ResetSkinBatch();
        return;
    }

    SetSkinMaterial( pMaterial,
                     m_SkinBatchFlags,
                     m_SkinBatchUOffset,
                     m_SkinBatchVOffset,
                     MaterialOverride );
    if( !MaterialOverride && pMaterial )
    {
        BindLightCookies( &m_lSkinBatchInstances[0],
                          m_lSkinBatchInstances.GetCount() );
    }

    if( SetSkinInstanceData( &m_lSkinBatchInstances[0],
                             m_lSkinBatchInstances.GetCount(),
                             m_lSkinBatchBones.GetCount() ? &m_lSkinBatchBones[0] : NULL,
                             m_lSkinBatchBones.GetCount() ) )
    {
        g_SkinVertMgr.DrawDListInstanced( m_hSkinBatchDList, m_lSkinBatchInstances.GetCount() );
    }

    ResetSkinInstanceData();
    ResetLightCookies();
    ResetProjTextures();
    ResetShadowMaps();
    ResetSkinBatch();
}

//==============================================================================

xbool geom_mgr::UpdateSkinConstants( const material* pMaterial,
                                     u8              UOffset,
                                     u8              VOffset,
                                     u8              OverrideMat )
{
    if( !m_pSkinFrameBuffer || !m_pSkinSectionBuffer )
    {
        x_DebugMsg( "GeomMgr: UpdateSkinConstants missing buffers (frame=%d section=%d)\n",
                    m_pSkinFrameBuffer ? 1 : 0,
                    m_pSkinSectionBuffer ? 1 : 0 );
        return FALSE;
    }

    if( !g_pd3dDevice || !g_pd3dContext )
    {
        x_DebugMsg( "GeomMgr: UpdateSkinConstants missing D3D state (device=%d context=%d)\n",
                    g_pd3dDevice  ? 1 : 0,
                    g_pd3dContext ? 1 : 0 );
        return FALSE;
    }

    const view* pView = eng_GetView();
    if( !pView )
    {
        x_DebugMsg( "GeomMgr: UpdateSkinConstants called without an active view\n" );
        return FALSE;
    }

    const cb_geom_frame frameData = BuildFrameConstants( *pView,
                                                         pMaterial,
                                                         UOffset,
                                                         VOffset,
                                                         FALSE,
                                                         OverrideMat );

    const xbool bFrameChanged = ( m_bSkinFrameDirty ||
                                  x_memcmp( &m_CachedSkinFrame,
                                            &frameData,
                                            sizeof(cb_geom_frame) ) != 0 );

    if( bFrameChanged )
    {
        if( !UploadConstantBuffer( m_pSkinFrameBuffer,
                                   &frameData,
                                   sizeof(cb_geom_frame),
                                   "skin frame" ) )
            return FALSE;

        m_CachedSkinFrame = frameData;
        m_bSkinFrameDirty = FALSE;
    }

    g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pSkinFrameBuffer );
    g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pSkinFrameBuffer );
    g_pd3dContext->VSSetConstantBuffers( 2, 1, &m_pSkinSectionBuffer );

    return TRUE;
}

//==============================================================================

xbool geom_mgr::SetSkinInstanceData( const cb_skin_instance* pInstances,
                                     s32                     nInstances,
                                     const matrix4*          pBones,
                                     s32                     nBones )
{
    if( !g_pd3dContext || !g_pd3dDevice || !pInstances || ( nInstances <= 0 ) )
        return FALSE;

    const u32 BoneCount = ( nBones > 0 ) ? (u32)nBones : 1;
    if( !geom_EnsureSkinStructuredBuffer( m_pSkinInstanceDataBuffer,
                                          m_pSkinInstanceDataSRV,
                                          m_SkinInstanceCapacity,
                                          (u32)nInstances,
                                          sizeof(cb_skin_instance),
                                          "skin instance data" ) )
    {
        return FALSE;
    }

    if( !geom_EnsureSkinStructuredBuffer( m_pSkinBoneDataBuffer,
                                          m_pSkinBoneDataSRV,
                                          m_SkinBoneCapacity,
                                          BoneCount,
                                          sizeof(matrix4),
                                          "skin bone data" ) )
    {
        return FALSE;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped;
    HRESULT hr = g_pd3dContext->Map( m_pSkinInstanceDataBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped );
    if( FAILED(hr) )
    {
        x_DebugMsg( "GeomMgr: Failed to map skin instance buffer, HRESULT 0x%08X\n", hr );
        return FALSE;
    }

    x_memcpy( Mapped.pData, pInstances, sizeof(cb_skin_instance) * nInstances );
    g_pd3dContext->Unmap( m_pSkinInstanceDataBuffer, 0 );

    hr = g_pd3dContext->Map( m_pSkinBoneDataBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped );
    if( FAILED(hr) )
    {
        x_DebugMsg( "GeomMgr: Failed to map skin bone data buffer, HRESULT 0x%08X\n", hr );
        return FALSE;
    }

    if( pBones && ( nBones > 0 ) )
        x_memcpy( Mapped.pData, pBones, sizeof(matrix4) * nBones );
    else
        x_memset( Mapped.pData, 0, sizeof(matrix4) * BoneCount );

    g_pd3dContext->Unmap( m_pSkinBoneDataBuffer, 0 );

    ID3D11ShaderResourceView* pVSSRV[2] =
    {
        m_pSkinInstanceDataSRV,
        m_pSkinBoneDataSRV
    };
    g_pd3dContext->VSSetShaderResources( TEXTURE_SLOT_SKIN_INSTANCE_DATA, 2, pVSSRV );
    g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_SKIN_INSTANCE_DATA, 1, &m_pSkinInstanceDataSRV );
    return TRUE;
}

//==============================================================================

void geom_mgr::ResetSkinInstanceData( void )
{
    if( !g_pd3dContext )
        return;

    ID3D11ShaderResourceView* pNullVSSRV[2] = { NULL, NULL };
    ID3D11ShaderResourceView* pNullPSSRV[1] = { NULL };
    g_pd3dContext->VSSetShaderResources( TEXTURE_SLOT_SKIN_INSTANCE_DATA, 2, pNullVSSRV );
    g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_SKIN_INSTANCE_DATA, 1, pNullPSSRV );
}

//==============================================================================

xbool geom_mgr::SetSkinSectionRemap( const u16* pBoneRemap )
{
    if( !m_pSkinSectionBuffer || !g_pd3dContext )
        return FALSE;

    u32 BoneRemap[MAX_SKIN_BONES];
    for( s32 i = 0; i < MAX_SKIN_BONES; ++i )
    {
        BoneRemap[i] = pBoneRemap ? (u32)pBoneRemap[i] : 0u;
        if( BoneRemap[i] == 0xFFFFu )
            BoneRemap[i] = 0u;
    }

    if( !UploadConstantBuffer( m_pSkinSectionBuffer,
                               BoneRemap,
                               sizeof(BoneRemap),
                               "skin section remap" ) )
    {
        return FALSE;
    }

    g_pd3dContext->VSSetConstantBuffers( 2, 1, &m_pSkinSectionBuffer );
    return TRUE;
}
