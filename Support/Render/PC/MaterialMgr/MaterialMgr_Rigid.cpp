//==============================================================================
//
//  MaterialMgr_Rigid.cpp
//
//  Rigid material handling for the PC material manager
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

#include "MaterialMgr.hpp"

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool material_mgr::InitRigidShaders( void )
{
    x_DebugMsg( "MaterialMgr: Initializing rigid shaders\n" );

    // Initialize member variables
    m_pRigidVertexShader    = NULL;
    m_pRigidPixelShader     = NULL;
    m_pRigidInputLayout     = NULL;
    m_pRigidFrameBuffer     = NULL;
    m_pRigidObjectBuffer    = NULL;
    m_pRigidLightBuffer     = NULL;
    m_bRigidFrameDirty      = TRUE;
    m_bRigidObjectDirty     = TRUE;
    m_bRigidLightingDirty   = TRUE;

    D3D11_INPUT_ELEMENT_DESC rigidLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    char shaderPath[256];
    x_sprintf( shaderPath, "%sa51_rigid_simple.hlsl", SHADER_PATH );

    m_pRigidVertexShader = shader_CompileVertexFromFileWithLayout( shaderPath,
                                                                  &m_pRigidInputLayout,
                                                                  rigidLayout,
                                                                  ARRAYSIZE(rigidLayout),
                                                                  "VSMain",
                                                                  "vs_5_0" );

    m_pRigidPixelShader   = shader_CompilePixelFromFile( shaderPath, "PSMain", "ps_5_0" );
    m_pRigidFrameBuffer   = shader_CreateConstantBuffer( sizeof(cb_geom_frame) );
    m_pRigidObjectBuffer  = shader_CreateConstantBuffer( sizeof(cb_geom_object) );
    m_pRigidLightBuffer   = shader_CreateConstantBuffer( sizeof(cb_lighting) );

    x_DebugMsg( "MaterialMgr: Rigid shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void material_mgr::KillRigidShaders( void )
{
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

    if( m_pRigidObjectBuffer )
    {
        m_pRigidObjectBuffer->Release();
        m_pRigidObjectBuffer = NULL;
    }

    if( m_pRigidLightBuffer )
    {
        m_pRigidLightBuffer->Release();
        m_pRigidLightBuffer = NULL;
    }

    x_DebugMsg( "MaterialMgr: Rigid shaders released\n" );
}

//==============================================================================

void material_mgr::SetRigidMaterial( const matrix4*      pL2W,
                                     const bbox*         pBBox,
                                     const d3d_lighting* pLighting,
                                     const material*     pMaterial,
                                     u32                 RenderFlags,
                                     u8                  UOffset,
                                     u8                  VOffset )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    g_pd3dContext->IASetInputLayout( m_pRigidInputLayout );
    g_pd3dContext->VSSetShader( m_pRigidVertexShader, NULL, 0 );
    g_pd3dContext->PSSetShader( m_pRigidPixelShader, NULL, 0 );
    g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    if( !UpdateRigidConstants( pL2W, pMaterial, RenderFlags, pLighting, UOffset, VOffset ) )
    {
        x_DebugMsg( "MaterialMgr: Failed to update rigid constants\n" );
        return;
    }
	
	ApplyRenderStates( pMaterial, RenderFlags );

    if( pL2W && pBBox )
    {
        if( !pMaterial || g_ProjTextureMgr.CanReceiveProjTexture( *pMaterial ) )
        {
            UpdateProjTextures( *pL2W, *pBBox, 4, RenderFlags );
        }
        else
        {
            UpdateProjTextures( *pL2W,
                                *pBBox,
                                4,
                                RenderFlags | render::DISABLE_SPOTLIGHT | render::DISABLE_PROJ_SHADOWS );
        }
    }
}

//==============================================================================

xbool material_mgr::UpdateRigidConstants( const matrix4*      pL2W,
                                          const material*     pMaterial,
                                          u32                 RenderFlags,
                                          const d3d_lighting* pLighting,
                                          u8                  UOffset,
                                          u8                  VOffset )
{
    if( !m_pRigidFrameBuffer || !m_pRigidObjectBuffer || !pLighting || !g_pd3dDevice || !g_pd3dContext )
        return FALSE;

    const view* pView = eng_GetView();
    if( !pView )
        return FALSE;

    f32 nearZ = 0.0f;
    f32 farZ  = 0.0f;
    pView->GetZLimits( nearZ, farZ );

    cb_geom_frame  frameData;
    cb_geom_object objectData;
    x_memset( &frameData,  0, sizeof(cb_geom_frame) );
    x_memset( &objectData, 0, sizeof(cb_geom_object) );

    if( pL2W )
        objectData.World = *pL2W;
    else
        objectData.World.Identity();

    frameData.View       = pView->GetW2V();
    frameData.Projection = pView->GetV2C();
    const vector3& camPos = pView->GetPosition();
    frameData.CameraPosition.Set( camPos.GetX(),
                                  camPos.GetY(),
                                  camPos.GetZ(),
                                  1.0f );

    const f32 invByte = 1.0f / 255.0f;
    frameData.UVAnim.Set( (f32)UOffset * invByte,
                          (f32)VOffset * invByte,
                          0.0f,
                          0.0f );

    const material_constants constants = BuildMaterialFlags( pMaterial, RenderFlags, TRUE );
    frameData.MaterialParams.Set( (f32)constants.Flags,
                                  constants.AlphaRef,
                                  nearZ,
                                  farZ );
    f32 fixedAlpha = pMaterial ? pMaterial->m_FixedAlpha : 0.0f;
    const f32 cubeIntensity = ComputeCubeMapIntensity( pMaterial );
    frameData.EnvParams.Set( fixedAlpha, cubeIntensity, 0.0f, 0.0f );

    const xbool bFrameChanged = ( m_bRigidFrameDirty ||
                                  x_memcmp( &m_CachedRigidFrame,
                                            &frameData,
                                            sizeof(cb_geom_frame) ) != 0 );

    if( bFrameChanged )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pRigidFrameBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map rigid frame buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        x_memcpy( mappedResource.pData, &frameData, sizeof(cb_geom_frame) );
        g_pd3dContext->Unmap( m_pRigidFrameBuffer, 0 );

        m_CachedRigidFrame  = frameData;
        m_bRigidFrameDirty  = FALSE;
    }

    const xbool bObjectChanged = ( m_bRigidObjectDirty ||
                                    x_memcmp( &m_CachedRigidObject,
                                              &objectData,
                                              sizeof(cb_geom_object) ) != 0 );

    if( bObjectChanged )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pRigidObjectBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map rigid object buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        x_memcpy( mappedResource.pData, &objectData, sizeof(cb_geom_object) );
        g_pd3dContext->Unmap( m_pRigidObjectBuffer, 0 );

        m_CachedRigidObject  = objectData;
        m_bRigidObjectDirty  = FALSE;
    }

    g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pRigidFrameBuffer );
    g_pd3dContext->VSSetConstantBuffers( 1, 1, &m_pRigidObjectBuffer );
    g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pRigidFrameBuffer );

    if( m_pRigidLightBuffer )
    {
        const vector4 ambientFloor( 0.0f, 0.0f, 0.0f, 0.0f );
        const cb_lighting lightMatrices = BuildLightingConstants( pLighting, ambientFloor );

        const xbool bLightingChanged = ( m_bRigidLightingDirty ||
                                         x_memcmp( &m_CachedRigidLighting,
                                                   &lightMatrices,
                                                   sizeof(cb_lighting) ) != 0 );

        if( bLightingChanged )
        {
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            HRESULT hr = g_pd3dContext->Map( m_pRigidLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
            if( FAILED(hr) )
            {
                x_DebugMsg( "MaterialMgr: Failed to map rigid light buffer, HRESULT 0x%08X\n", hr );
                return FALSE;
            }

            x_memcpy( mappedResource.pData, &lightMatrices, sizeof(cb_lighting) );
            g_pd3dContext->Unmap( m_pRigidLightBuffer, 0 );

            m_CachedRigidLighting = lightMatrices;
            m_bRigidLightingDirty = FALSE;
        }

        g_pd3dContext->PSSetConstantBuffers( 3, 1, &m_pRigidLightBuffer );
    }

    return TRUE;
}