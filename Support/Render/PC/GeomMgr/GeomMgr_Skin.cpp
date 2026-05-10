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
    m_pSkinLightBuffer    = NULL;
    m_bSkinFrameDirty     = TRUE;
    m_bSkinLightingDirty  = TRUE;

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
    m_pSkinLightBuffer  = shader_CreateConstantBuffer( sizeof(cb_lighting), CB_TYPE_DYNAMIC );

    x_DebugMsg( "GeomMgr: Skin shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void geom_mgr::KillSkinShaders( void )
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

    if( m_pSkinLightBuffer )
    {
        m_pSkinLightBuffer->Release();
        m_pSkinLightBuffer = NULL;
    }

    x_DebugMsg( "GeomMgr: Skin shaders released\n" );
}

//==============================================================================

void geom_mgr::SetSkinMaterial( const matrix4*      pL2W,
                                    const bbox*         pBBox,
                                    const d3d_lighting* pLighting,
                                    const material*     pMaterial,
                                    u32                 RenderFlags,
                                    u8                  UOffset,
                                    u8                  VOffset,
                                    u8                  Alpha,
                                    u8                  OverrideMat )
{
    ResetShadowMaps();

    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    shader_pass Pass;
    Pass.pInputLayout    = m_pSkinInputLayout;
    Pass.pVertexShader   = m_pSkinVertexShader;
    Pass.pPixelShader    = m_pSkinPixelShader;
    Pass.pGeometryShader = NULL;
    Pass.Topology        = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    shader_ApplyPass( Pass );

    if( !UpdateSkinConstants( pLighting, pMaterial, RenderFlags, UOffset, VOffset, Alpha, OverrideMat ) )
    {
        x_DebugMsg( "GeomMgr: Failed to update skin constants\n" );
        return;
    }
    
    ApplyRenderStates( pMaterial, RenderFlags, OverrideMat );

    if( OverrideMat )
    {
        ResetProjTextures();
        return;
    }

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

xbool geom_mgr::UpdateSkinConstants( const d3d_lighting* pLighting,
                                     const material*     pMaterial,
                                     u32                 RenderFlags,
                                     u8                  UOffset,
                                     u8                  VOffset,
                                     u8                  Alpha,
                                     u8                  OverrideMat )
{
    if( !m_pSkinFrameBuffer || !m_pSkinBoneBuffer || !m_pSkinLightBuffer )
    {
        x_DebugMsg( "GeomMgr: UpdateSkinConstants missing buffers (frame=%d bones=%d light=%d)\n",
                    m_pSkinFrameBuffer ? 1 : 0,
                    m_pSkinBoneBuffer  ? 1 : 0,
                    m_pSkinLightBuffer ? 1 : 0 );
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
                                                         RenderFlags,
                                                         UOffset,
                                                         VOffset,
                                                         Alpha,
                                                         FALSE,
                                                         OverrideMat );
    const cb_lighting lightData = BuildLightingConstants( pLighting );

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
    g_pd3dContext->VSSetConstantBuffers( 2, 1, &m_pSkinBoneBuffer );

    const xbool bLightingChanged = ( m_bSkinLightingDirty ||
                                     x_memcmp( &m_CachedSkinLighting,
                                               &lightData,
                                               sizeof(cb_lighting) ) != 0 );

    if( bLightingChanged )
    {
        if( !UploadConstantBuffer( m_pSkinLightBuffer,
                                   &lightData,
                                   sizeof(cb_lighting),
                                   "skin lighting" ) )
            return FALSE;

        m_CachedSkinLighting = lightData;
        m_bSkinLightingDirty = FALSE;
    }

    g_pd3dContext->PSSetConstantBuffers( 3, 1, &m_pSkinLightBuffer );

    return TRUE;
}
