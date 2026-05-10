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
    x_sprintf( shaderPath, "a51_rigid_simple.hlsl" );

    m_pRigidVertexShader = shader_CompileVertexFromFileWithLayout( shaderPath,
                                                                  &m_pRigidInputLayout,
                                                                  rigidLayout,
                                                                  ARRAYSIZE(rigidLayout),
                                                                  "VSMain",
                                                                  "vs_5_0" );

    m_pRigidPixelShader   = shader_CompilePixelFromFile( shaderPath, "PSMain", "ps_5_0" );

    m_pRigidFrameBuffer   = shader_CreateConstantBuffer( sizeof(cb_geom_frame), CB_TYPE_DYNAMIC );
    m_pRigidObjectBuffer  = shader_CreateConstantBuffer( sizeof(cb_geom_object), CB_TYPE_DYNAMIC );
    m_pRigidLightBuffer   = shader_CreateConstantBuffer( sizeof(cb_lighting), CB_TYPE_DYNAMIC );

    x_DebugMsg( "GeomMgr: Rigid shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void geom_mgr::KillRigidShaders( void )
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

    x_DebugMsg( "GeomMgr: Rigid shaders released\n" );
}

//==============================================================================

void geom_mgr::SetRigidMaterial( const matrix4*      pL2W,
                                     const bbox*         pBBox,
                                     const d3d_lighting* pLighting,
                                     const material*     pMaterial,
                                     u32                 RenderFlags,
                                     u8                  UOffset,
                                     u8                  VOffset, 
                                     u8                  Alpha,
                                     u8                  OverrideMat )
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

    if( !UpdateRigidConstants( pL2W, pMaterial, RenderFlags, pLighting, UOffset, VOffset, Alpha, OverrideMat ) )
    {
        x_DebugMsg( "GeomMgr: Failed to update rigid constants\n" );
        return;
    }
    
    ApplyRenderStates( pMaterial, RenderFlags, OverrideMat );

    if( OverrideMat )
    {
        ResetProjTextures();
        ResetShadowMaps();
        return;
    }

    if( pL2W && pBBox )
    {
        const xbool bCanReceiveProjTextures = !pMaterial || g_ProjTextureMgr.CanReceiveProjTexture( *pMaterial );
        const xbool bCanReceiveShadowMaps   = !pMaterial || g_ShadowMapMgr.CanReceiveShadowMap( *pMaterial );

        if( bCanReceiveProjTextures )
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

        if( bCanReceiveShadowMaps )
        {
            UpdateShadowMaps( *pL2W, *pBBox );
        }
        else
        {
            ResetShadowMaps();
        }
    }
    else
    {
        ResetShadowMaps();
    }
}

//==============================================================================

xbool geom_mgr::UpdateRigidConstants( const matrix4*      pL2W,
                                      const material*     pMaterial,
                                      u32                 RenderFlags,
                                      const d3d_lighting* pLighting,
                                      u8                  UOffset,
                                      u8                  VOffset,
                                      u8                  Alpha,
                                      u8                  OverrideMat )
{
    if( !m_pRigidFrameBuffer || !m_pRigidObjectBuffer || !m_pRigidLightBuffer )
    {
        x_DebugMsg( "GeomMgr: UpdateRigidConstants missing buffers (frame=%d object=%d light=%d)\n",
                    m_pRigidFrameBuffer  ? 1 : 0,
                    m_pRigidObjectBuffer ? 1 : 0,
                    m_pRigidLightBuffer  ? 1 : 0 );
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

    cb_geom_object objectData;
    if( pL2W )
        objectData.World = *pL2W;
    else
        objectData.World.Identity();

    const cb_geom_frame frameData = BuildFrameConstants( *pView,
                                                         pMaterial,
                                                         RenderFlags,
                                                         UOffset,
                                                         VOffset,
                                                         Alpha,
                                                         TRUE,
                                                         OverrideMat );
    const cb_lighting lightData = BuildLightingConstants( pLighting );

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

    const xbool bObjectChanged = ( m_bRigidObjectDirty ||
                                   x_memcmp( &m_CachedRigidObject,
                                             &objectData,
                                             sizeof(cb_geom_object) ) != 0 );

    if( bObjectChanged )
    {
        if( !UploadConstantBuffer( m_pRigidObjectBuffer,
                                   &objectData,
                                   sizeof(cb_geom_object),
                                   "rigid object" ) )
            return FALSE;

        m_CachedRigidObject  = objectData;
        m_bRigidObjectDirty  = FALSE;
    }

    ID3D11Buffer* pRigidVSBuffers[] =
    {
        m_pRigidFrameBuffer,
        m_pRigidObjectBuffer
    };

    g_pd3dContext->VSSetConstantBuffers( 0, ARRAYSIZE(pRigidVSBuffers), pRigidVSBuffers );
    g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pRigidFrameBuffer );

    const xbool bLightingChanged = ( m_bRigidLightingDirty ||
                                     x_memcmp( &m_CachedRigidLighting,
                                               &lightData,
                                               sizeof(cb_lighting) ) != 0 );

    if( bLightingChanged )
    {
        if( !UploadConstantBuffer( m_pRigidLightBuffer,
                                   &lightData,
                                   sizeof(cb_lighting),
                                   "rigid lighting" ) )
            return FALSE;

        m_CachedRigidLighting = lightData;
        m_bRigidLightingDirty = FALSE;
    }

    g_pd3dContext->PSSetConstantBuffers( 3, 1, &m_pRigidLightBuffer );

    return TRUE;
}
