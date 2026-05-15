//==============================================================================
//
//  ShadowMgr.cpp
//
//  Shadow-map manager implementation for the PC platform.
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

#include "ShadowMgr.hpp"

#include "GeomMgr/GeomMgr.hpp"
#include "SoftVertexMgr.hpp"

#include "Entropy/D3DEngine/d3deng_composite.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11DeviceContext* g_pd3dContext;

//==============================================================================
//  FILE-LOCAL HELPERS
//==============================================================================

namespace
{
    static
    void ReleaseShadowTarget( rtarget& Target )
    {
        rtarget_Unregister( Target );
        rtarget_Destroy( Target );
        Target = rtarget();
    }

    template< typename T >
    static
    void ReleaseCOM( T*& pResource )
    {
        if( pResource )
        {
            pResource->Release();
            pResource = NULL;
        }
    }

}

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

shadow_mgr g_ShadowMgr;

//==============================================================================
//  MANAGER LIFETIME
//==============================================================================

shadow_mgr::shadow_mgr( void ) :
    m_bInitialized             ( FALSE ),
    m_bTargetsPushed           ( FALSE ),
    m_bViewportSaved           ( FALSE ),
    m_SavedViewportCount       ( 0 ),
    m_CurrentSource            ( -1 ),
    m_ShadowAtlasSize          ( 0 ),
    m_pSkinVertexShader        ( NULL ),
    m_pMomentPixelShader       ( NULL ),
    m_pBlurHPixelShader        ( NULL ),
    m_pBlurVPixelShader        ( NULL ),
    m_pSkinInputLayout         ( NULL ),
    m_pShadowCastBuffer        ( NULL ),
    m_pShadowBlurBuffer        ( NULL ),
    m_ShadowBias               ( 0.0025f ),
    m_ShadowStrength           ( 0.32f ),
    m_ShadowFilterRadius       ( 2.0f ),
    m_ShadowMinVariance        ( 0.0001f ),
    m_ShadowLightBleedReduction( 0.20f )
{
    x_memset( &m_SavedViewport, 0, sizeof(m_SavedViewport) );
}

//==============================================================================

shadow_mgr::~shadow_mgr( void )
{
    Kill();
}

//==============================================================================

void shadow_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    D3D11_INPUT_ELEMENT_DESC SkinLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    m_pSkinVertexShader = shader_CompileVertexFromFileWithLayout( "a51_shadow_cast_skin.hlsl",
                                                                  &m_pSkinInputLayout,
                                                                  SkinLayout,
                                                                  ARRAYSIZE(SkinLayout),
                                                                  "VSMain",
                                                                  "vs_5_0" );
    m_pMomentPixelShader = shader_CompilePixelFromFile( "a51_shadow_evsm.hlsl",
                                                        "PSCastMoments",
                                                        "ps_5_0" );
    m_pShadowCastBuffer = shader_CreateConstantBuffer( sizeof(cb_shadow_cast), CB_TYPE_DYNAMIC );
    m_pShadowBlurBuffer = NULL;

    if( !m_pSkinVertexShader || !m_pMomentPixelShader || !m_pSkinInputLayout ||
        !m_pShadowCastBuffer )
    {
        if( m_pShadowCastBuffer )
        {
            m_pShadowCastBuffer->Release();
            m_pShadowCastBuffer = NULL;
        }

        if( m_pMomentPixelShader )
        {
            m_pMomentPixelShader->Release();
            m_pMomentPixelShader = NULL;
        }

        if( m_pSkinInputLayout )
        {
            m_pSkinInputLayout->Release();
            m_pSkinInputLayout = NULL;
        }

        if( m_pSkinVertexShader )
        {
            m_pSkinVertexShader->Release();
            m_pSkinVertexShader = NULL;
        }

        return;
    }

    m_bInitialized = TRUE;
}

//==============================================================================

void shadow_mgr::Kill( void )
{
    ReleaseCOM( m_pShadowBlurBuffer );
    ReleaseCOM( m_pShadowCastBuffer );
    ReleaseCOM( m_pBlurVPixelShader );
    ReleaseCOM( m_pBlurHPixelShader );
    ReleaseCOM( m_pMomentPixelShader );
    ReleaseCOM( m_pSkinInputLayout );
    ReleaseCOM( m_pSkinVertexShader );

    ReleaseShadowTarget( m_ShadowAtlas );
    ReleaseShadowTarget( m_ShadowBlurAtlas );
    ReleaseShadowTarget( m_ShadowDepthAtlas );

    m_bTargetsPushed      = FALSE;
    m_bViewportSaved      = FALSE;
    m_SavedViewportCount  = 0;
    m_CurrentSource       = -1;
    m_ShadowAtlasSize     = 0;
    m_bInitialized        = FALSE;
}

//==============================================================================
//  SOURCE MANAGEMENT
//==============================================================================

void shadow_mgr::EnsureAtlas( void )
{
    s32 ShadowAtlasSize = g_ShadowMapMgr.GetAtlasSize();
    if( ShadowAtlasSize <= 0 )
        ShadowAtlasSize = SHADOW_ATLAS_SIZE;

    if( m_ShadowAtlas.pTexture &&
        ( m_ShadowAtlasSize == ShadowAtlasSize ) &&
        ( m_ShadowAtlas.Desc.Format == RTARGET_FORMAT_R32F ) )
    {
        if( !m_ShadowDepthAtlas.pTexture )
        {
            ReleaseShadowTarget( m_ShadowAtlas );
            ReleaseShadowTarget( m_ShadowDepthAtlas );
        }
        else
        {
            return;
        }
    }
    else if( m_ShadowAtlas.pTexture || m_ShadowBlurAtlas.pTexture || m_ShadowDepthAtlas.pTexture )
    {
        ReleaseShadowTarget( m_ShadowAtlas );
        ReleaseShadowTarget( m_ShadowBlurAtlas );
        ReleaseShadowTarget( m_ShadowDepthAtlas );
    }

    m_ShadowAtlasSize = 0;

    rtarget_registration ColorReg;
    ColorReg.Policy         = RTARGET_SIZE_ABSOLUTE;
    ColorReg.BaseWidth      = ShadowAtlasSize;
    ColorReg.BaseHeight     = ShadowAtlasSize;
    ColorReg.Format         = RTARGET_FORMAT_R32F;
    ColorReg.SampleCount    = 1;
    ColorReg.SampleQuality  = 0;
    ColorReg.bBindAsTexture = TRUE;

    if( !rtarget_GetOrCreate( m_ShadowAtlas, ColorReg ) )
    {
        x_DebugMsg( "ShadowMgr: failed to create shadow atlas\n" );
        return;
    }

    rtarget_registration DepthReg;
    DepthReg.Policy         = RTARGET_SIZE_ABSOLUTE;
    DepthReg.BaseWidth      = ShadowAtlasSize;
    DepthReg.BaseHeight     = ShadowAtlasSize;
    DepthReg.Format         = RTARGET_FORMAT_DEPTH32F;
    DepthReg.SampleCount    = 1;
    DepthReg.SampleQuality  = 0;
    DepthReg.bBindAsTexture = FALSE;

    if( !rtarget_GetOrCreate( m_ShadowDepthAtlas, DepthReg ) )
    {
        x_DebugMsg( "ShadowMgr: failed to create shadow depth atlas\n" );
        ReleaseShadowTarget( m_ShadowAtlas );
        return;
    }

    m_ShadowAtlasSize = ShadowAtlasSize;
}

//==============================================================================

//==============================================================================
//  SHADOW CASTER PIPELINE
//==============================================================================

void shadow_mgr::UnbindShadowSRVs( void )
{
    if( !g_pd3dContext )
        return;

    ID3D11ShaderResourceView* pNullSRV = NULL;
    g_pd3dContext->PSSetShaderResources( PC_SHADOW_ATLAS_TEX_SLOT, 1, &pNullSRV );
}

//==============================================================================

void shadow_mgr::BeginShadowShaders( void )
{
    if( !m_bInitialized || !g_ShadowMapMgr.HasActiveSources() || !g_pd3dContext )
        return;

    if( g_ShadowMapMgr.GetAtlasSourceCount() > 0 )
    {
        EnsureAtlas();
        if( !m_ShadowAtlas.pRenderTargetView || !m_ShadowDepthAtlas.pDepthStencilView )
            return;
    }

    UnbindShadowSRVs();

    if( !m_bTargetsPushed )
    {
        m_bTargetsPushed = rtarget_PushTargets();
    }

    if( !m_bViewportSaved )
    {
        m_SavedViewportCount = 1;
        g_pd3dContext->RSGetViewports( &m_SavedViewportCount, &m_SavedViewport );
        m_bViewportSaved = ( m_SavedViewportCount > 0 );
    }
}

//==============================================================================

void shadow_mgr::BeginCastPass( void )
{
    if( !m_bInitialized ||
        !g_ShadowMapMgr.HasActiveSources() ||
        !g_pd3dContext ||
        !m_pSkinVertexShader ||
        !m_pMomentPixelShader ||
        !m_pSkinInputLayout ||
        !m_pShadowCastBuffer )
    {
        return;
    }

    if( g_ShadowMapMgr.GetAtlasSourceCount() > 0 )
    {
        EnsureAtlas();
        if( !m_ShadowAtlas.pRenderTargetView || !m_ShadowDepthAtlas.pDepthStencilView )
            return;

        const f32 ClearColor[4]    = { 1.0f, 1.0f, 1.0f, 1.0f };

        rtarget_SetTargets( &m_ShadowAtlas, 1, &m_ShadowDepthAtlas );
        rtarget_ClearColor( m_ShadowAtlas, ClearColor );
        rtarget_ClearDepthStencil( m_ShadowDepthAtlas, RTARGET_CLEAR_DEPTH, 1.0f, 0 );
    }

    state_SetBlend( STATE_BLEND_NONE );
    state_SetDepth( STATE_DEPTH_NORMAL );
    state_SetRasterizer( STATE_RASTER_SOLID_NO_CULL );

    ID3D11Buffer* pBoneBuffer = g_GeomMgr.GetSkinBoneBuffer();
    if( pBoneBuffer )
    {
        g_pd3dContext->VSSetConstantBuffers( 2, 1, &pBoneBuffer );
    }

    g_SkinVertMgr.BeginRender();
    m_CurrentSource = -1;
}

//==============================================================================

void shadow_mgr::EndCastPass( void )
{
    m_CurrentSource = -1;
}

//==============================================================================

void shadow_mgr::ApplySource( s32 SourceIndex )
{
    ASSERT( SourceIndex >= 0 );
    ASSERT( SourceIndex < g_ShadowMapMgr.GetSourceCount() );

    if( SourceIndex == m_CurrentSource )
        return;

    const shadow_map_mgr::shadow_source& Source = g_ShadowMapMgr.GetSource( SourceIndex );

    shader_pass Pass;
    x_memset( &Pass, 0, sizeof(Pass) );
    Pass.pInputLayout  = m_pSkinInputLayout;
    Pass.pVertexShader = m_pSkinVertexShader;
    Pass.Topology      = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    D3D11_VIEWPORT Viewport;
    x_memset( &Viewport, 0, sizeof(Viewport) );
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    Viewport.TopLeftX = (FLOAT)Source.AtlasX;
    Viewport.TopLeftY = (FLOAT)Source.AtlasY;
    Viewport.Width    = (FLOAT)Source.AtlasWidth;
    Viewport.Height   = (FLOAT)Source.AtlasHeight;
    g_pd3dContext->RSSetViewports( 1, &Viewport );

    rtarget_SetTargets( &m_ShadowAtlas, 1, &m_ShadowDepthAtlas );
    Pass.pPixelShader = m_pMomentPixelShader;

    shader_ApplyPass( Pass );

    cb_shadow_cast CBData;
    CBData.ShadowViewProjection = Source.WorldToClip;

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    if( SUCCEEDED( g_pd3dContext->Map( m_pShadowCastBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) ) )
    {
        x_memcpy( MappedResource.pData, &CBData, sizeof(CBData) );
        g_pd3dContext->Unmap( m_pShadowCastBuffer, 0 );
        g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pShadowCastBuffer );
        m_CurrentSource = SourceIndex;
    }
}

//==============================================================================

void shadow_mgr::RenderSkinCaster( xhandle hDList, const matrix4* pBones, s32 SourceIndex )
{
    if( !m_bInitialized ||
        !g_ShadowMapMgr.HasActiveSources() ||
        !g_pd3dContext ||
        !m_pSkinVertexShader ||
        !m_pSkinInputLayout ||
        !m_pShadowCastBuffer )
    {
        return;
    }

    if( ( SourceIndex < 0 ) || ( SourceIndex >= g_ShadowMapMgr.GetSourceCount() ) )
        return;

    ApplySource( SourceIndex );

    if( SourceIndex != m_CurrentSource )
        return;

    g_SkinVertMgr.DrawDList( hDList, pBones );
}

//==============================================================================

void shadow_mgr::BlurAtlas( void )
{
}

//==============================================================================

void shadow_mgr::EndShadowShaders( void )
{
    if( g_pd3dContext && m_bViewportSaved )
    {
        g_pd3dContext->RSSetViewports( m_SavedViewportCount, &m_SavedViewport );
    }

    m_bViewportSaved     = FALSE;
    m_SavedViewportCount = 0;

    if( m_bTargetsPushed )
    {
        rtarget_PopTargets();
        m_bTargetsPushed = FALSE;
    }

    if( g_pd3dContext )
    {
        shader_SetVertexShader( NULL );
        shader_SetPixelShader( NULL );
    }

    m_CurrentSource = -1;
}

//==============================================================================
//  RUNTIME QUERIES
//==============================================================================

ID3D11ShaderResourceView* shadow_mgr::GetShadowAtlasSRV( void ) const
{
    return m_ShadowAtlas.pShaderResourceView;
}

//==============================================================================

f32 shadow_mgr::GetShadowBias( void ) const
{
    return m_ShadowBias;
}

//==============================================================================

f32 shadow_mgr::GetShadowStrength( void ) const
{
    return m_ShadowStrength;
}

//==============================================================================

f32 shadow_mgr::GetShadowFilterRadius( void ) const
{
    return m_ShadowFilterRadius;
}

//==============================================================================

f32 shadow_mgr::GetShadowMinVariance( void ) const
{
    return m_ShadowMinVariance;
}

//==============================================================================

f32 shadow_mgr::GetShadowLightBleedReduction( void ) const
{
    return m_ShadowLightBleedReduction;
}

//==============================================================================

f32 shadow_mgr::GetAtlasTexelSize( void ) const
{
    const s32 ShadowAtlasSize = ( m_ShadowAtlasSize > 0 ) ? m_ShadowAtlasSize : SHADOW_ATLAS_SIZE;
    return 1.0f / (f32)ShadowAtlasSize;
}
