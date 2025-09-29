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
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dContext;

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
    m_pRigidConstantBuffer  = NULL;
    m_pRigidLightBuffer     = NULL;
    m_bRigidMatricesDirty   = TRUE;
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

    m_pRigidPixelShader = shader_CompilePixelFromFile( shaderPath, "PSMain", "ps_5_0" );
    m_pRigidConstantBuffer = shader_CreateConstantBuffer( sizeof(cb_rigid_matrices) );
    m_pRigidLightBuffer    = shader_CreateConstantBuffer( sizeof(cb_lighting) );

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

    if( m_pRigidConstantBuffer )
    {
        m_pRigidConstantBuffer->Release();
        m_pRigidConstantBuffer = NULL;
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
                                     u32                 RenderFlags )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    g_pd3dContext->IASetInputLayout( m_pRigidInputLayout );
    g_pd3dContext->VSSetShader( m_pRigidVertexShader, NULL, 0 );
    g_pd3dContext->PSSetShader( m_pRigidPixelShader, NULL, 0 );

    state_SetState( STATE_TYPE_BLEND, STATE_BLEND_NONE );
    state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_NORMAL );
    state_SetState( STATE_TYPE_RASTERIZER, STATE_RASTER_SOLID );
    state_SetState( STATE_TYPE_SAMPLER, STATE_SAMPLER_LINEAR_WRAP );
    g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    if( !UpdateRigidConstants( pL2W, pMaterial, RenderFlags, pLighting ) )
    {
        x_DebugMsg( "MaterialMgr: Failed to update rigid constants\n" );
        return;
    }

    if( pL2W && pBBox )
        UpdateProjTextures( *pL2W, *pBBox, 1, RenderFlags );
}

//==============================================================================

xbool material_mgr::UpdateRigidConstants( const matrix4*      pL2W,
                                          const material*     pMaterial,
                                          u32                 RenderFlags,
                                          const d3d_lighting* pLighting )
{
    if( !m_pRigidConstantBuffer || !pLighting || !g_pd3dDevice || !g_pd3dContext )
        return FALSE;

    const view* pView = eng_GetView();
    if( !pView )
        return FALSE;

    f32 nearZ = 0.0f;
    f32 farZ  = 0.0f;
    pView->GetZLimits( nearZ, farZ );

    cb_rigid_matrices rigidMatrices;

    if( pL2W )
        rigidMatrices.World = *pL2W;
    else
        rigidMatrices.World.Identity();

    rigidMatrices.View           = pView->GetW2V();
    rigidMatrices.Projection     = pView->GetV2C();
    rigidMatrices.CameraPosition = pView->GetPosition();
    rigidMatrices.DepthParams[0] = nearZ;
    rigidMatrices.DepthParams[1] = farZ;

    const material_constants constants = BuildMaterialFlags( pMaterial, RenderFlags, TRUE, TRUE );
    rigidMatrices.MaterialFlags = constants.Flags;
    rigidMatrices.AlphaRef      = constants.AlphaRef;

    if( m_bRigidMatricesDirty || x_memcmp( &m_CachedRigidMatrices, &rigidMatrices, sizeof(cb_rigid_matrices) ) != 0 )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pRigidConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map rigid constant buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        x_memcpy( mappedResource.pData, &rigidMatrices, sizeof(cb_rigid_matrices) );
        g_pd3dContext->Unmap( m_pRigidConstantBuffer, 0 );

        m_CachedRigidMatrices = rigidMatrices;
        m_bRigidMatricesDirty = FALSE;

        g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pRigidConstantBuffer );
        g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pRigidConstantBuffer );
    }

    if( m_pRigidLightBuffer )
    {
        const vector4 ambientBias( 0.0f, 0.0f, 0.0f, 0.0f );
        const cb_lighting lightMatrices = BuildLightingConstants( pLighting, ambientBias );

        if( m_bRigidLightingDirty || x_memcmp( &m_CachedRigidLighting, &lightMatrices, sizeof(cb_lighting) ) != 0 )
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

            g_pd3dContext->PSSetConstantBuffers( 2, 1, &m_pRigidLightBuffer );
        }
    }

    return TRUE;
}
