//==============================================================================
//
//  MaterialMgr_Skin.cpp
//
//  Skin material handling for the PC material manager
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

xbool material_mgr::InitSkinShaders( void )
{
    x_DebugMsg( "MaterialMgr: Initializing skin shaders\n" );

    // Initialize member variables
    m_pSkinVertexShader     = NULL;
    m_pSkinPixelShader      = NULL;
    m_pSkinInputLayout      = NULL;
    m_pSkinVSConstBuffer    = NULL;
    m_pSkinLightBuffer      = NULL;
    m_pSkinBoneBuffer       = NULL;
    m_bSkinMatricesDirty    = TRUE;
    m_bSkinLightingDirty    = TRUE;

    D3D11_INPUT_ELEMENT_DESC skinLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    char shaderPath[256];
    x_sprintf( shaderPath, "%sa51_skin_simple.hlsl", SHADER_PATH );

    m_pSkinVertexShader = shader_CompileVertexFromFileWithLayout( shaderPath,
                                                                 &m_pSkinInputLayout,
                                                                 skinLayout,
                                                                 ARRAYSIZE(skinLayout),
                                                                 "VSMain",
                                                                 "vs_5_0" );

    m_pSkinPixelShader = shader_CompilePixelFromFile( shaderPath, "PSMain", "ps_5_0" );
    m_pSkinVSConstBuffer = shader_CreateConstantBuffer( sizeof(cb_skin_matrices) );
    m_pSkinLightBuffer   = shader_CreateConstantBuffer( sizeof(cb_lighting) );
    m_pSkinBoneBuffer    = shader_CreateConstantBuffer( sizeof(cb_skin_bone) * MAX_SKIN_BONES );

    x_DebugMsg( "MaterialMgr: Skin shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void material_mgr::KillSkinShaders( void )
{
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

    if( m_pSkinVSConstBuffer )
    {
        m_pSkinVSConstBuffer->Release();
        m_pSkinVSConstBuffer = NULL;
    }

    if( m_pSkinLightBuffer )
    {
        m_pSkinLightBuffer->Release();
        m_pSkinLightBuffer = NULL;
    }

    if( m_pSkinBoneBuffer )
    {
        m_pSkinBoneBuffer->Release();
        m_pSkinBoneBuffer = NULL;
    }

    x_DebugMsg( "MaterialMgr: Skin shaders released\n" );
}

//==============================================================================

void material_mgr::SetSkinMaterial( const matrix4*      pL2W,
                                    const bbox*         pBBox,
                                    const d3d_lighting* pLighting,
                                    const material*     pMaterial,
                                    u32                 RenderFlags,
                                    u8                  UOffset,
                                    u8                  VOffset )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    g_pd3dContext->IASetInputLayout( m_pSkinInputLayout );
    g_pd3dContext->VSSetShader( m_pSkinVertexShader, NULL, 0 );
    g_pd3dContext->PSSetShader( m_pSkinPixelShader, NULL, 0 );

    state_SetState( STATE_TYPE_BLEND, STATE_BLEND_NONE );
    state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_NORMAL );
    state_SetState( STATE_TYPE_RASTERIZER, STATE_RASTER_SOLID );
    state_SetState( STATE_TYPE_SAMPLER, STATE_SAMPLER_LINEAR_WRAP );
    g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    if( !UpdateSkinConstants( pLighting, pMaterial, RenderFlags, UOffset, VOffset ) )
    {
        x_DebugMsg( "MaterialMgr: Failed to update skin constants\n" );
        return;
    }

    if( pL2W && pBBox )
        UpdateProjTextures( *pL2W, *pBBox, 1, RenderFlags );
}

//==============================================================================

xbool material_mgr::UpdateSkinConstants( const d3d_lighting* pLighting,
                                         const material*     pMaterial,
                                         u32                 RenderFlags,
                                         u8                  UOffset,
                                         u8                  VOffset )
{
    if( !m_pSkinVSConstBuffer || !m_pSkinLightBuffer || !pLighting || !g_pd3dDevice || !g_pd3dContext )
        return FALSE;

    const view* pView = eng_GetView();
    if( !pView )
        return FALSE;

    f32 nearZ = 0.0f;
    f32 farZ  = 0.0f;
    pView->GetZLimits( nearZ, farZ );

    cb_skin_matrices skinMatrices;
    skinMatrices.View        = pView->GetW2V();
    skinMatrices.Projection  = pView->GetV2C();
    skinMatrices.DepthParams[0] = nearZ;
    skinMatrices.DepthParams[1] = farZ;

    const f32 invByte = 1.0f / 255.0f;
    skinMatrices.UVAnim.Set( (f32)UOffset * invByte,
                             (f32)VOffset * invByte,
                             0.0f,
                             0.0f );

    const material_constants constants = BuildMaterialFlags( pMaterial, RenderFlags, FALSE, FALSE );
    skinMatrices.MaterialFlags = constants.Flags;
    skinMatrices.AlphaRef      = constants.AlphaRef;

    const vector4 BaseBrightness( 0.16f, 0.16f, 0.16f, 0.0f ); // Maintain minimum ambient lighting.
    const cb_lighting lightMatrices = BuildLightingConstants( pLighting, BaseBrightness );

    if( m_bSkinMatricesDirty || x_memcmp( &m_CachedSkinMatrices, &skinMatrices, sizeof(cb_skin_matrices) ) != 0 )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pSkinVSConstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map skin VS constant buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        x_memcpy( mappedResource.pData, &skinMatrices, sizeof(cb_skin_matrices) );
        g_pd3dContext->Unmap( m_pSkinVSConstBuffer, 0 );

        m_CachedSkinMatrices = skinMatrices;
        m_bSkinMatricesDirty = FALSE;

        g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pSkinVSConstBuffer );
        g_pd3dContext->VSSetConstantBuffers( 2, 1, &m_pSkinBoneBuffer );
        g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pSkinVSConstBuffer );
    }

    if( m_pSkinLightBuffer )
    {
        if( m_bSkinLightingDirty || x_memcmp( &m_CachedSkinLighting, &lightMatrices, sizeof(cb_lighting) ) != 0 )
        {
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            HRESULT hr = g_pd3dContext->Map( m_pSkinLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
            if( FAILED(hr) )
            {
                x_DebugMsg( "MaterialMgr: Failed to map skin light buffer, HRESULT 0x%08X\n", hr );
                return FALSE;
            }

            x_memcpy( mappedResource.pData, &lightMatrices, sizeof(cb_lighting) );
            g_pd3dContext->Unmap( m_pSkinLightBuffer, 0 );

            m_CachedSkinLighting = lightMatrices;
            m_bSkinLightingDirty = FALSE;

            g_pd3dContext->PSSetConstantBuffers( 2, 1, &m_pSkinLightBuffer );
        }
    }

    return TRUE;
}
