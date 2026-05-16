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
#include "VertexMgr.hpp"

#include "Entropy/D3DEngine/d3deng_composite.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11DeviceContext* g_pd3dContext;
extern ID3D11Device*        g_pd3dDevice;

//==============================================================================
//  FILE-LOCAL HELPERS
//==============================================================================

namespace
{
    enum
    {
        SHADOW_CASTER_NONE  = -1,
        SHADOW_CASTER_RIGID = 0,
        SHADOW_CASTER_SKIN  = 1,
    };

    static const s32 kShadowDepthBias            = 16;
    static const f32 kShadowSlopeScaledDepthBias = 2.0f;
    static const f32 kShadowDepthBiasClamp       = 0.0f;

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
    m_bRasterizerSaved         ( FALSE ),
    m_SavedViewportCount       ( 0 ),
    m_pSavedRasterizerState    ( NULL ),
    m_CurrentSource            ( -1 ),
    m_CurrentCasterShader      ( SHADOW_CASTER_NONE ),
    m_ShadowAtlasSize          ( 0 ),
    m_pRigidVertexShader       ( NULL ),
    m_pSkinVertexShader        ( NULL ),
    m_pMomentPixelShader       ( NULL ),
    m_pBlurHPixelShader        ( NULL ),
    m_pBlurVPixelShader        ( NULL ),
    m_pRigidInputLayout        ( NULL ),
    m_pSkinInputLayout         ( NULL ),
    m_pShadowCastBuffer        ( NULL ),
    m_pShadowBlurBuffer        ( NULL ),
    m_pShadowCasterRasterizer  ( NULL ),
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

    D3D11_INPUT_ELEMENT_DESC RigidLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    D3D11_INPUT_ELEMENT_DESC SkinLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    m_pRigidVertexShader = shader_CompileVertexFromFileWithLayout( "a51_shadow_cast_rigid.hlsl",
                                                                   &m_pRigidInputLayout,
                                                                   RigidLayout,
                                                                   ARRAYSIZE(RigidLayout),
                                                                   "VSMain",
                                                                   "vs_5_0" );
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

    if( g_pd3dDevice )
    {
        D3D11_RASTERIZER_DESC RasterDesc;
        x_memset( &RasterDesc, 0, sizeof(RasterDesc) );
        RasterDesc.FillMode              = D3D11_FILL_SOLID;
        RasterDesc.CullMode              = D3D11_CULL_NONE;
        RasterDesc.FrontCounterClockwise = FALSE;
        RasterDesc.DepthBias             = kShadowDepthBias;
        RasterDesc.DepthBiasClamp        = kShadowDepthBiasClamp;
        RasterDesc.SlopeScaledDepthBias  = kShadowSlopeScaledDepthBias;
        RasterDesc.DepthClipEnable       = TRUE;
        RasterDesc.ScissorEnable         = FALSE;
        RasterDesc.MultisampleEnable     = FALSE;
        RasterDesc.AntialiasedLineEnable = FALSE;

        HRESULT hr = g_pd3dDevice->CreateRasterizerState( &RasterDesc, &m_pShadowCasterRasterizer );
        if( FAILED(hr) )
        {
            x_DebugMsg( "ShadowMgr: failed to create shadow rasterizer state, HRESULT = 0x%08X\n", hr );
        }
    }

    const xbool bHasRigidCaster = ( m_pRigidVertexShader && m_pRigidInputLayout );
    const xbool bHasSkinCaster  = ( m_pSkinVertexShader && m_pSkinInputLayout );

    if( !bHasRigidCaster )
    {
        ReleaseCOM( m_pRigidInputLayout );
        ReleaseCOM( m_pRigidVertexShader );
    }

    if( !bHasSkinCaster )
    {
        ReleaseCOM( m_pSkinInputLayout );
        ReleaseCOM( m_pSkinVertexShader );
    }

    if( !m_pMomentPixelShader || !m_pShadowCastBuffer || !m_pShadowCasterRasterizer || !( bHasRigidCaster || bHasSkinCaster ) )
    {
        if( m_pShadowCasterRasterizer )
        {
            m_pShadowCasterRasterizer->Release();
            m_pShadowCasterRasterizer = NULL;
        }

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

        if( m_pRigidInputLayout )
        {
            m_pRigidInputLayout->Release();
            m_pRigidInputLayout = NULL;
        }

        if( m_pSkinInputLayout )
        {
            m_pSkinInputLayout->Release();
            m_pSkinInputLayout = NULL;
        }

        if( m_pRigidVertexShader )
        {
            m_pRigidVertexShader->Release();
            m_pRigidVertexShader = NULL;
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
    ReleaseCOM( m_pSavedRasterizerState );
    ReleaseCOM( m_pShadowCasterRasterizer );
    ReleaseCOM( m_pShadowBlurBuffer );
    ReleaseCOM( m_pShadowCastBuffer );
    ReleaseCOM( m_pBlurVPixelShader );
    ReleaseCOM( m_pBlurHPixelShader );
    ReleaseCOM( m_pMomentPixelShader );
    ReleaseCOM( m_pRigidInputLayout );
    ReleaseCOM( m_pSkinInputLayout );
    ReleaseCOM( m_pRigidVertexShader );
    ReleaseCOM( m_pSkinVertexShader );

    ReleaseShadowTarget( m_ShadowAtlas );
    ReleaseShadowTarget( m_ShadowBlurAtlas );
    ReleaseShadowTarget( m_ShadowDepthAtlas );

    m_bTargetsPushed      = FALSE;
    m_bViewportSaved      = FALSE;
    m_bRasterizerSaved    = FALSE;
    m_SavedViewportCount  = 0;
    m_CurrentSource       = -1;
    m_CurrentCasterShader = SHADOW_CASTER_NONE;
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
        !m_pMomentPixelShader ||
        !m_pShadowCastBuffer ||
        !m_pShadowCasterRasterizer )
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

    if( !m_bRasterizerSaved )
    {
        g_pd3dContext->RSGetState( &m_pSavedRasterizerState );
        m_bRasterizerSaved = TRUE;
    }
    g_pd3dContext->RSSetState( m_pShadowCasterRasterizer );

    ID3D11Buffer* pBoneBuffer = g_GeomMgr.GetSkinBoneBuffer();
    if( pBoneBuffer )
    {
        g_pd3dContext->VSSetConstantBuffers( 2, 1, &pBoneBuffer );
    }

    g_RigidVertMgr.BeginRender();
    g_SkinVertMgr.BeginRender();
    m_CurrentSource       = -1;
    m_CurrentCasterShader = SHADOW_CASTER_NONE;
}

//==============================================================================

void shadow_mgr::EndCastPass( void )
{
    m_CurrentSource       = -1;
    m_CurrentCasterShader = SHADOW_CASTER_NONE;
}

//==============================================================================

xbool shadow_mgr::SetShadowCastConstants( const matrix4& ShadowViewProjection,
                                          const matrix4* pWorld )
{
    if( !g_pd3dContext || !m_pShadowCastBuffer )
        return FALSE;

    cb_shadow_cast CBData;
    CBData.ShadowViewProjection = ShadowViewProjection;
    CBData.World.Identity();
    if( pWorld )
        CBData.World = *pWorld;

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    if( FAILED( g_pd3dContext->Map( m_pShadowCastBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) ) )
        return FALSE;

    x_memcpy( MappedResource.pData, &CBData, sizeof(CBData) );
    g_pd3dContext->Unmap( m_pShadowCastBuffer, 0 );
    g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pShadowCastBuffer );
    return TRUE;
}

//==============================================================================

void shadow_mgr::ApplySource( s32 SourceIndex, s32 CasterShader )
{
    ASSERT( SourceIndex >= 0 );
    ASSERT( SourceIndex < g_ShadowMapMgr.GetSourceCount() );

    if( ( SourceIndex == m_CurrentSource ) && ( CasterShader == m_CurrentCasterShader ) )
        return;

    const shadow_map_mgr::shadow_source& Source = g_ShadowMapMgr.GetSource( SourceIndex );

    ID3D11InputLayout*  pInputLayout  = NULL;
    ID3D11VertexShader* pVertexShader = NULL;

    if( CasterShader == SHADOW_CASTER_RIGID )
    {
        pInputLayout  = m_pRigidInputLayout;
        pVertexShader = m_pRigidVertexShader;
    }
    else if( CasterShader == SHADOW_CASTER_SKIN )
    {
        pInputLayout  = m_pSkinInputLayout;
        pVertexShader = m_pSkinVertexShader;
    }

    if( !pInputLayout || !pVertexShader || !m_pMomentPixelShader )
        return;

    shader_pass Pass;
    x_memset( &Pass, 0, sizeof(Pass) );
    Pass.pInputLayout  = pInputLayout;
    Pass.pVertexShader = pVertexShader;
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

    if( CasterShader == SHADOW_CASTER_SKIN )
    {
        if( !SetShadowCastConstants( Source.WorldToClip ) )
            return;
    }

    m_CurrentSource       = SourceIndex;
    m_CurrentCasterShader = CasterShader;
}

//==============================================================================

void shadow_mgr::RenderRigidCaster( xhandle hDList, const matrix4* pL2W, s32 SourceIndex )
{
    if( !m_bInitialized ||
        !g_ShadowMapMgr.HasActiveSources() ||
        !g_pd3dContext ||
        !m_pRigidVertexShader ||
        !m_pRigidInputLayout ||
        !m_pShadowCastBuffer ||
        !pL2W )
    {
        return;
    }

    if( ( SourceIndex < 0 ) || ( SourceIndex >= g_ShadowMapMgr.GetSourceCount() ) )
        return;

    ApplySource( SourceIndex, SHADOW_CASTER_RIGID );

    if( ( SourceIndex != m_CurrentSource ) || ( m_CurrentCasterShader != SHADOW_CASTER_RIGID ) )
        return;

    const shadow_map_mgr::shadow_source& Source = g_ShadowMapMgr.GetSource( SourceIndex );
    if( !SetShadowCastConstants( Source.WorldToClip, pL2W ) )
        return;

    g_RigidVertMgr.DrawDList( hDList );
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

    ApplySource( SourceIndex, SHADOW_CASTER_SKIN );

    if( ( SourceIndex != m_CurrentSource ) || ( m_CurrentCasterShader != SHADOW_CASTER_SKIN ) )
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

    if( g_pd3dContext && m_bRasterizerSaved )
    {
        g_pd3dContext->RSSetState( m_pSavedRasterizerState );
    }

    m_bViewportSaved     = FALSE;
    m_bRasterizerSaved   = FALSE;
    m_SavedViewportCount = 0;
    ReleaseCOM( m_pSavedRasterizerState );

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

    m_CurrentSource       = -1;
    m_CurrentCasterShader = SHADOW_CASTER_NONE;
}

//==============================================================================
//  RUNTIME QUERIES
//==============================================================================

ID3D11ShaderResourceView* shadow_mgr::GetShadowAtlasSRV( void ) const
{
    return m_ShadowAtlas.pShaderResourceView;
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
