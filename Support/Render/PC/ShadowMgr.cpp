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
    static const f32 kEVSMPositiveExponent      = 5.0f;
    static const f32 kEVSMNegativeExponent      = 5.0f;

    struct cb_shadow_blur
    {
        vector4 BlurParams;
    };

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

    static
    vector4 GetEVSMClearValue( void )
    {
        const f32 Positive = x_exp( kEVSMPositiveExponent );
        const f32 Negative = -x_exp( -kEVSMNegativeExponent );
        return vector4( Positive,
                        Positive * Positive,
                        Negative,
                        Negative * Negative );
    }

    static
    s32 GetPointCubeFaceIndex( s32 LegacyFaceIndex )
    {
        ASSERT( ( LegacyFaceIndex >= 0 ) && ( LegacyFaceIndex < POINT_SHADOW_FACE_COUNT ) );

        // Obj_Mgr still submits point faces in legacy order:
        // +Y, -Y, +Z, -Z, +X, -X.
        // D3D cube arrays are addressed in hardware order:
        // +X, -X, +Y, -Y, +Z, -Z.
        static const s32 kPointCubeFaceRemap[POINT_SHADOW_FACE_COUNT] =
        {
            2,  // +Y
            3,  // -Y
            4,  // +Z
            5,  // -Z
            0,  // +X
            1,  // -X
        };

        return kPointCubeFaceRemap[LegacyFaceIndex];
    }

    static
    s32 GetPointShadowBucketIndex( s32 ShadowMapResolution )
    {
        switch( ShadowMapResolution )
        {
            case 256:  return 0;
            case 512:  return 1;
            case 1024: return 2;
            case 2048: return 3;
        }

        return -1;
    }

    static
    s32 GetPointShadowBucketResolution( s32 BucketIndex )
    {
        static const s32 kPointShadowBucketResolution[POINT_SHADOW_BUCKET_COUNT] =
        {
            256,
            512,
            1024,
            2048
        };

        ASSERT( ( BucketIndex >= 0 ) && ( BucketIndex < POINT_SHADOW_BUCKET_COUNT ) );
        return kPointShadowBucketResolution[BucketIndex];
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
    m_ShadowFilterRadius       ( 15.0f ),
    m_ShadowMinVariance        ( 0.0001f ),
    m_ShadowLightBleedReduction( 0.20f )
{
    x_memset( &m_SavedViewport, 0, sizeof(m_SavedViewport) );
    x_memset( m_SourceCleared, 0, sizeof(m_SourceCleared) );
    x_memset( m_PointShadowBuckets, 0, sizeof(m_PointShadowBuckets) );

    for( s32 i = 0; i < MAX_SHADOW_LIGHTS; i++ )
    {
        m_PointLightBindings[i].BucketIndex     = -1;
        m_PointLightBindings[i].LocalLightIndex = -1;
    }
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
    m_pBlurHPixelShader  = shader_CompilePixelFromFile( "a51_shadow_evsm.hlsl",
                                                        "PSBlurHorizontal",
                                                        "ps_5_0" );
    m_pBlurVPixelShader  = shader_CompilePixelFromFile( "a51_shadow_evsm.hlsl",
                                                        "PSBlurVertical",
                                                        "ps_5_0" );
    m_pShadowCastBuffer = shader_CreateConstantBuffer( sizeof(cb_shadow_cast), CB_TYPE_DYNAMIC );
    m_pShadowBlurBuffer = shader_CreateConstantBuffer( sizeof(cb_shadow_blur), CB_TYPE_DYNAMIC );

    if( !m_pSkinVertexShader || !m_pMomentPixelShader || !m_pBlurHPixelShader ||
        !m_pBlurVPixelShader || !m_pSkinInputLayout || !m_pShadowCastBuffer ||
        !m_pShadowBlurBuffer )
    {
        if( m_pShadowBlurBuffer )
        {
            m_pShadowBlurBuffer->Release();
            m_pShadowBlurBuffer = NULL;
        }

        if( m_pShadowCastBuffer )
        {
            m_pShadowCastBuffer->Release();
            m_pShadowCastBuffer = NULL;
        }

        if( m_pBlurVPixelShader )
        {
            m_pBlurVPixelShader->Release();
            m_pBlurVPixelShader = NULL;
        }

        if( m_pBlurHPixelShader )
        {
            m_pBlurHPixelShader->Release();
            m_pBlurHPixelShader = NULL;
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

    for( s32 iBucket = 0; iBucket < POINT_SHADOW_BUCKET_COUNT; iBucket++ )
        ReleasePointShadowBucket( iBucket );

    m_bTargetsPushed      = FALSE;
    m_bViewportSaved      = FALSE;
    m_SavedViewportCount  = 0;
    m_CurrentSource       = -1;
    m_ShadowAtlasSize     = 0;
    m_bInitialized        = FALSE;

    for( s32 i = 0; i < MAX_SHADOW_LIGHTS; i++ )
    {
        m_PointLightBindings[i].BucketIndex     = -1;
        m_PointLightBindings[i].LocalLightIndex = -1;
    }
}

//==============================================================================
//  SOURCE MANAGEMENT
//==============================================================================

void shadow_mgr::EnsureAtlas( void )
{
    s32 ShadowAtlasSize = g_ShadowMapMgr.GetSpotAtlasSize();
    if( ShadowAtlasSize <= 0 )
        ShadowAtlasSize = SHADOW_ATLAS_SIZE;

    if( m_ShadowAtlas.pTexture &&
        ( m_ShadowAtlasSize == ShadowAtlasSize ) )
    {
        if( !m_ShadowBlurAtlas.pTexture || !m_ShadowDepthAtlas.pTexture )
        {
            ReleaseShadowTarget( m_ShadowAtlas );
            ReleaseShadowTarget( m_ShadowBlurAtlas );
            ReleaseShadowTarget( m_ShadowDepthAtlas );
        }
        else
        {
            return;
        }
    }

    m_ShadowAtlasSize = 0;

    rtarget_registration ColorReg;
    ColorReg.Policy         = RTARGET_SIZE_ABSOLUTE;
    ColorReg.BaseWidth      = ShadowAtlasSize;
    ColorReg.BaseHeight     = ShadowAtlasSize;
    ColorReg.Format         = RTARGET_FORMAT_RGBA16F;
    ColorReg.SampleCount    = 1;
    ColorReg.SampleQuality  = 0;
    ColorReg.bBindAsTexture = TRUE;

    if( !rtarget_GetOrCreate( m_ShadowAtlas, ColorReg ) )
    {
        x_DebugMsg( "ShadowMgr: failed to create EVSM atlas\n" );
        return;
    }

    if( !rtarget_GetOrCreate( m_ShadowBlurAtlas, ColorReg ) )
    {
        x_DebugMsg( "ShadowMgr: failed to create EVSM blur atlas\n" );
        ReleaseShadowTarget( m_ShadowAtlas );
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
        x_DebugMsg( "ShadowMgr: failed to create EVSM depth atlas\n" );
        ReleaseShadowTarget( m_ShadowAtlas );
        ReleaseShadowTarget( m_ShadowBlurAtlas );
        return;
    }

    m_ShadowAtlasSize = ShadowAtlasSize;
}

//==============================================================================

void shadow_mgr::EnsurePointShadows( void )
{
    s32 DesiredBucketLightCounts[POINT_SHADOW_BUCKET_COUNT];
    x_memset( DesiredBucketLightCounts, 0, sizeof(DesiredBucketLightCounts) );

    for( s32 iLight = 0; iLight < MAX_SHADOW_LIGHTS; iLight++ )
    {
        m_PointLightBindings[iLight].BucketIndex     = -1;
        m_PointLightBindings[iLight].LocalLightIndex = -1;
    }

    const s32 nSources = g_ShadowMapMgr.GetSourceCount();
    for( s32 i = 0; i < nSources; i++ )
    {
        const shadow_map_mgr::shadow_source& Source = g_ShadowMapMgr.GetSource( i );
        if( Source.Type != shadow_map_mgr::SHADOW_SOURCE_POINT_FACE )
            continue;

        if( ( Source.PointLightIndex < 0 ) || ( Source.PointLightIndex >= MAX_SHADOW_LIGHTS ) )
            continue;

        if( m_PointLightBindings[Source.PointLightIndex].BucketIndex >= 0 )
            continue;

        const s32 BucketIndex = GetPointShadowBucketIndex( Source.RequestedResolution );
        if( BucketIndex < 0 )
            continue;

        m_PointLightBindings[Source.PointLightIndex].BucketIndex     = BucketIndex;
        m_PointLightBindings[Source.PointLightIndex].LocalLightIndex = DesiredBucketLightCounts[BucketIndex];
        DesiredBucketLightCounts[BucketIndex]++;
    }

    for( s32 iBucket = 0; iBucket < POINT_SHADOW_BUCKET_COUNT; iBucket++ )
    {
        point_shadow_bucket& Bucket = m_PointShadowBuckets[iBucket];
        const s32            FaceSize = GetPointShadowBucketResolution( iBucket );
        const s32            LightCount = DesiredBucketLightCounts[iBucket];
        const s32            SliceCount = LightCount * POINT_SHADOW_FACE_COUNT;

        if( LightCount <= 0 )
        {
            ReleasePointShadowBucket( iBucket );
            continue;
        }

        if( Bucket.pTexture &&
            Bucket.pSRV &&
            ( Bucket.FaceSize == FaceSize ) &&
            ( Bucket.LightCount == LightCount ) )
        {
            xbool bBucketReady = TRUE;
            for( s32 iSlice = 0; iSlice < SliceCount; iSlice++ )
            {
                if( !Bucket.pDSV[iSlice] )
                {
                    bBucketReady = FALSE;
                    break;
                }
            }

            if( bBucketReady )
                continue;
        }

        ReleasePointShadowBucket( iBucket );

        if( !g_pd3dDevice )
            continue;

        D3D11_TEXTURE2D_DESC Desc;
        x_memset( &Desc, 0, sizeof(Desc) );
        Desc.Width              = FaceSize;
        Desc.Height             = FaceSize;
        Desc.MipLevels          = 1;
        Desc.ArraySize          = SliceCount;
        Desc.Format             = DXGI_FORMAT_R32_TYPELESS;
        Desc.SampleDesc.Count   = 1;
        Desc.SampleDesc.Quality = 0;
        Desc.Usage              = D3D11_USAGE_DEFAULT;
        Desc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        Desc.CPUAccessFlags     = 0;
        Desc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE;

        HRESULT hr = g_pd3dDevice->CreateTexture2D( &Desc, NULL, &Bucket.pTexture );
        if( FAILED(hr) )
        {
            x_DebugMsg( "ShadowMgr: failed to create point shadow cube array bucket %d (HRESULT 0x%08X)\n",
                        iBucket, hr );
            ReleasePointShadowBucket( iBucket );
            continue;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        x_memset( &SRVDesc, 0, sizeof(SRVDesc) );
        SRVDesc.Format                             = DXGI_FORMAT_R32_FLOAT;
        SRVDesc.ViewDimension                      = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
        SRVDesc.TextureCubeArray.MostDetailedMip  = 0;
        SRVDesc.TextureCubeArray.MipLevels        = 1;
        SRVDesc.TextureCubeArray.First2DArrayFace = 0;
        SRVDesc.TextureCubeArray.NumCubes         = LightCount;

        hr = g_pd3dDevice->CreateShaderResourceView( Bucket.pTexture, &SRVDesc, &Bucket.pSRV );
        if( FAILED(hr) )
        {
            x_DebugMsg( "ShadowMgr: failed to create point shadow cube SRV bucket %d (HRESULT 0x%08X)\n",
                        iBucket, hr );
            ReleasePointShadowBucket( iBucket );
            continue;
        }

        xbool bBucketReady = TRUE;
        for( s32 iSlice = 0; iSlice < SliceCount; iSlice++ )
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
            x_memset( &DSVDesc, 0, sizeof(DSVDesc) );
            DSVDesc.Format                         = DXGI_FORMAT_D32_FLOAT;
            DSVDesc.ViewDimension                  = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            DSVDesc.Texture2DArray.MipSlice        = 0;
            DSVDesc.Texture2DArray.FirstArraySlice = iSlice;
            DSVDesc.Texture2DArray.ArraySize       = 1;

            hr = g_pd3dDevice->CreateDepthStencilView( Bucket.pTexture,
                                                       &DSVDesc,
                                                       &Bucket.pDSV[iSlice] );
            if( FAILED(hr) )
            {
                x_DebugMsg( "ShadowMgr: failed to create point shadow face DSV bucket %d slice %d (HRESULT 0x%08X)\n",
                            iBucket, iSlice, hr );
                ReleasePointShadowBucket( iBucket );
                bBucketReady = FALSE;
                break;
            }
        }

        if( bBucketReady )
        {
            Bucket.FaceSize   = FaceSize;
            Bucket.LightCount = LightCount;
        }
    }
}

//==============================================================================

void shadow_mgr::ReleasePointShadowBucket( s32 BucketIndex )
{
    ASSERT( ( BucketIndex >= 0 ) && ( BucketIndex < POINT_SHADOW_BUCKET_COUNT ) );

    point_shadow_bucket& Bucket = m_PointShadowBuckets[BucketIndex];

    for( s32 i = 0; i < ARRAYSIZE(Bucket.pDSV); i++ )
        ReleaseCOM( Bucket.pDSV[i] );

    ReleaseCOM( Bucket.pSRV );
    ReleaseCOM( Bucket.pTexture );

    Bucket.FaceSize   = 0;
    Bucket.LightCount = 0;
}

//==============================================================================

//==============================================================================
//  SHADOW CASTER PIPELINE
//==============================================================================

void shadow_mgr::UnbindShadowSRVs( void )
{
    if( !g_pd3dContext )
        return;

    ID3D11ShaderResourceView* pNullSRV[PC_POINT_SHADOW_TEX_COUNT + 1] = { NULL };
    g_pd3dContext->PSSetShaderResources( PC_POINT_SHADOW_TEX_SLOT, ARRAYSIZE(pNullSRV), pNullSRV );
}

//==============================================================================

void shadow_mgr::BeginShadowShaders( void )
{
    if( !m_bInitialized || !g_ShadowMapMgr.HasActiveSources() || !g_pd3dContext )
        return;

    if( g_ShadowMapMgr.GetSpotSourceCount() > 0 )
    {
        EnsureAtlas();
        if( !m_ShadowAtlas.pRenderTargetView || !m_ShadowDepthAtlas.pDepthStencilView )
            return;
    }

    if( g_ShadowMapMgr.GetPointLightCount() > 0 )
    {
        EnsurePointShadows();
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

    if( g_ShadowMapMgr.GetSpotSourceCount() > 0 )
    {
        EnsureAtlas();
        if( !m_ShadowAtlas.pRenderTargetView || !m_ShadowDepthAtlas.pDepthStencilView )
            return;

        const vector4 ClearMoments = GetEVSMClearValue();
        const f32 ClearColor[4]    = { ClearMoments.GetX(), ClearMoments.GetY(), ClearMoments.GetZ(), ClearMoments.GetW() };

        rtarget_SetTargets( &m_ShadowAtlas, 1, &m_ShadowDepthAtlas );
        rtarget_ClearColor( m_ShadowAtlas, ClearColor );
        rtarget_ClearDepthStencil( m_ShadowDepthAtlas, RTARGET_CLEAR_DEPTH, 1.0f, 0 );
    }

    if( g_ShadowMapMgr.GetPointLightCount() > 0 )
    {
        EnsurePointShadows();
    }

    state_SetBlend( STATE_BLEND_NONE );
    state_SetDepth( STATE_DEPTH_NORMAL );
    state_SetRasterizer( STATE_RASTER_SOLID_NO_CULL );
    x_memset( m_SourceCleared, 0, sizeof(m_SourceCleared) );

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
    BlurAtlas();
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

    if( Source.Type == shadow_map_mgr::SHADOW_SOURCE_POINT_FACE )
    {
        s32 BucketIndex = -1;
        s32 LocalLightIndex = -1;
        if( !GetPointShadowBinding( Source.PointLightIndex, BucketIndex, LocalLightIndex ) )
            return;

        point_shadow_bucket& Bucket = m_PointShadowBuckets[BucketIndex];
        const s32 CubeFaceIndex = GetPointCubeFaceIndex( Source.FaceIndex );
        const s32 SliceIndex    = LocalLightIndex * POINT_SHADOW_FACE_COUNT + CubeFaceIndex;
        if( ( SliceIndex < 0 ) || ( SliceIndex >= ARRAYSIZE(Bucket.pDSV) ) || !Bucket.pDSV[SliceIndex] )
            return;

        Viewport.TopLeftX = 0.0f;
        Viewport.TopLeftY = 0.0f;
        Viewport.Width    = (FLOAT)Bucket.FaceSize;
        Viewport.Height   = (FLOAT)Bucket.FaceSize;
        g_pd3dContext->RSSetViewports( 1, &Viewport );

        g_pd3dContext->OMSetRenderTargets( 0, NULL, Bucket.pDSV[SliceIndex] );
        if( !m_SourceCleared[SourceIndex] )
        {
            g_pd3dContext->ClearDepthStencilView( Bucket.pDSV[SliceIndex], D3D11_CLEAR_DEPTH, 1.0f, 0 );
            m_SourceCleared[SourceIndex] = TRUE;
        }

        Pass.pPixelShader = NULL;
    }
    else
    {
        Viewport.TopLeftX = (FLOAT)Source.AtlasX;
        Viewport.TopLeftY = (FLOAT)Source.AtlasY;
        Viewport.Width    = (FLOAT)Source.AtlasWidth;
        Viewport.Height   = (FLOAT)Source.AtlasHeight;
        g_pd3dContext->RSSetViewports( 1, &Viewport );

        rtarget_SetTargets( &m_ShadowAtlas, 1, &m_ShadowDepthAtlas );
        Pass.pPixelShader = m_pMomentPixelShader;
    }

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
    if( g_ShadowMapMgr.GetSpotSourceCount() <= 0 )
        return;

    if( !g_pd3dContext ||
        !m_ShadowAtlas.pShaderResourceView ||
        !m_ShadowBlurAtlas.pRenderTargetView ||
        !m_ShadowBlurAtlas.pShaderResourceView ||
        !m_pBlurHPixelShader ||
        !m_pBlurVPixelShader ||
        !m_pShadowBlurBuffer )
    {
        return;
    }

    cb_shadow_blur BlurCB;
    const f32 TexelStep = GetAtlasTexelSize() * MAX( m_ShadowFilterRadius, 0.0f );

    BlurCB.BlurParams = vector4( TexelStep, 0.0f, 0.0f, 0.0f );
    shader_UpdateConstantBuffer( m_pShadowBlurBuffer, &BlurCB, sizeof(BlurCB) );
    g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pShadowBlurBuffer );

    rtarget_SetTargets( &m_ShadowBlurAtlas, 1, NULL );
    composite_Blit( m_ShadowAtlas,
                    COMPOSITE_BLEND_COPY,
                    1.0f,
                    m_pBlurHPixelShader,
                    STATE_SAMPLER_LINEAR_CLAMP );

    BlurCB.BlurParams = vector4( 0.0f, TexelStep, 0.0f, 0.0f );
    shader_UpdateConstantBuffer( m_pShadowBlurBuffer, &BlurCB, sizeof(BlurCB) );
    g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pShadowBlurBuffer );

    rtarget_SetTargets( &m_ShadowAtlas, 1, NULL );
    composite_Blit( m_ShadowBlurAtlas,
                    COMPOSITE_BLEND_COPY,
                    1.0f,
                    m_pBlurVPixelShader,
                    STATE_SAMPLER_LINEAR_CLAMP );
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

ID3D11ShaderResourceView* shadow_mgr::GetPointShadowSRV( s32 BucketIndex ) const
{
    if( ( BucketIndex < 0 ) || ( BucketIndex >= POINT_SHADOW_BUCKET_COUNT ) )
        return NULL;

    const point_shadow_bucket& Bucket = m_PointShadowBuckets[BucketIndex];
    if( Bucket.LightCount <= 0 )
        return NULL;

    return Bucket.pSRV;
}

//==============================================================================

xbool shadow_mgr::GetPointShadowBinding( s32 PointLightIndex,
                                         s32& BucketIndex,
                                         s32& LocalLightIndex ) const
{
    BucketIndex     = -1;
    LocalLightIndex = -1;

    if( ( PointLightIndex < 0 ) || ( PointLightIndex >= MAX_SHADOW_LIGHTS ) )
        return FALSE;

    const point_shadow_binding& Binding = m_PointLightBindings[PointLightIndex];
    if( ( Binding.BucketIndex < 0 ) || ( Binding.BucketIndex >= POINT_SHADOW_BUCKET_COUNT ) )
        return FALSE;

    const point_shadow_bucket& Bucket = m_PointShadowBuckets[Binding.BucketIndex];
    if( !Bucket.pTexture || !Bucket.pSRV )
        return FALSE;

    if( ( Binding.LocalLightIndex < 0 ) || ( Binding.LocalLightIndex >= Bucket.LightCount ) )
        return FALSE;

    BucketIndex     = Binding.BucketIndex;
    LocalLightIndex = Binding.LocalLightIndex;
    return TRUE;
}

//==============================================================================

ID3D11ShaderResourceView* shadow_mgr::GetSpotShadowSRV( void ) const
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
