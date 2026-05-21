//==============================================================================
//
//  PostMgr_Filters.cpp
//
//  Screen-space post-processing module for the PC platform.
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

#include "PostMgr.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dContext;

//==============================================================================
//  FILE-LOCAL TYPES AND HELPERS
//==============================================================================

namespace
{
    // Constants
    static const s32 kMipRampSampleCount = 256;

    // Constant buffer layout
    struct cb_post_filters
    {
        vector4 Params0;
        vector4 Params1;
        vector4 Params2;
        vector4 Params3;
        vector4 Params4;
        vector4 Params5;
        vector4 Params6;
        vector4 ScreenWarps[MAX_POST_SCREEN_WARPS];
    };

    // Helper functions
    static
    void ReleaseFilterTarget( rtarget& Target )
    {
        rtarget_Unregister( Target );
        rtarget_Destroy( Target );
        Target = rtarget();
    }

    static
    void ReleaseFilterTexture( ID3D11Texture2D*& pTexture, ID3D11ShaderResourceView*& pSRV )
    {
        if( pSRV )
        {
            pSRV->Release();
            pSRV = NULL;
        }

        if( pTexture )
        {
            pTexture->Release();
            pTexture = NULL;
        }
    }

    static
    xbool EnsureFilterTarget( rtarget& Target, const rtarget* pSourceTarget )
    {
        if( !pSourceTarget || !pSourceTarget->Desc.Width || !pSourceTarget->Desc.Height )
            return FALSE;

        rtarget_registration reg;
        reg.Policy         = RTARGET_SIZE_ABSOLUTE;
        reg.BaseWidth      = pSourceTarget->Desc.Width;
        reg.BaseHeight     = pSourceTarget->Desc.Height;
        reg.Format         = pSourceTarget->Desc.Format;
        reg.SampleCount    = 1;
        reg.SampleQuality  = 0;
        reg.bBindAsTexture = TRUE;

        return rtarget_GetOrCreate( Target, reg );
    }

    static
    xbool EnsureAbsoluteFilterTarget( rtarget& Target, u32 Width, u32 Height, rtarget_format Format )
    {
        if( !Width || !Height )
            return FALSE;

        rtarget_registration reg;
        reg.Policy         = RTARGET_SIZE_ABSOLUTE;
        reg.BaseWidth      = Width;
        reg.BaseHeight     = Height;
        reg.Format         = Format;
        reg.SampleCount    = 1;
        reg.SampleQuality  = 0;
        reg.bBindAsTexture = TRUE;

        return rtarget_GetOrCreate( Target, reg );
    }

    static
    xbool CopyRenderTargetToTarget( rtarget& Destination, const rtarget* pSourceTarget )
    {
        if( !g_pd3dContext || !Destination.pTexture || !pSourceTarget || !pSourceTarget->pTexture )
            return FALSE;

        D3D11_TEXTURE2D_DESC sourceDesc;
        pSourceTarget->pTexture->GetDesc( &sourceDesc );

        if( sourceDesc.SampleDesc.Count > 1 )
        {
            g_pd3dContext->ResolveSubresource( Destination.pTexture,
                                               0,
                                               pSourceTarget->pTexture,
                                               0,
                                               sourceDesc.Format );
        }
        else
        {
            g_pd3dContext->CopyResource( Destination.pTexture, pSourceTarget->pTexture );
        }

        return TRUE;
    }

    static
    const rtarget* GetCurrentPostTarget( void )
    {
        const rtarget* pTarget = rtarget_GetCurrentTarget( 0 );
        if( !pTarget )
            pTarget = g_GBufferMgr.GetGBufferTarget( GBUFFER_FINAL_COLOR );
        if( !pTarget )
            pTarget = rtarget_GetBackBuffer();
        return pTarget;
    }
}

//==============================================================================
//  FILTER RESOURCE MANAGEMENT
//==============================================================================

post_mgr::filter_resources::filter_resources()
{
    History           = rtarget();
    Post[0]           = rtarget();
    Post[1]           = rtarget();
    for( s32 i = 0; i < MAX_POST_MIPS; ++i )
        Mip[i] = rtarget();
    ActiveSourceIndex = 0;
    ActiveTargetIndex = 1;
    bPostChainActive  = FALSE;
    pMotionBlurPS      = NULL;
    pMipCompositePS    = NULL;
    pRadialBlurPS      = NULL;
    pScreenWarpPS      = NULL;
    pNoisePS           = NULL;
    pConstantBuffer    = NULL;
    pMipRampTexture    = NULL;
    pMipRampSRV        = NULL;
    CopyWidth          = 0;
    CopyHeight         = 0;
    CopyFormat         = RTARGET_FORMAT_COUNT;
    MipSourceWidth     = 0;
    MipSourceHeight    = 0;
    MipSourceFormat    = RTARGET_FORMAT_COUNT;
    bHistoryValid      = FALSE;
}

//==============================================================================

void post_mgr::filter_resources::Initialize( void )
{
    Shutdown();

    if( !g_pd3dDevice )
        return;

    char shaderPath[256];
    x_sprintf( shaderPath, "a51_post_filters.hlsl" );

    char* pSource = shader_LoadSourceFromFile( shaderPath );
    if( !pSource )
        return;

    pMotionBlurPS   = shader_CompilePixel( pSource, "PS_MotionBlur", "ps_5_0", shaderPath );
    pMipCompositePS = shader_CompilePixel( pSource, "PS_MipComposite", "ps_5_0", shaderPath );
    pRadialBlurPS   = shader_CompilePixel( pSource, "PS_RadialBlur", "ps_5_0", shaderPath );
    pScreenWarpPS   = shader_CompilePixel( pSource, "PS_ScreenWarp", "ps_5_0", shaderPath );
    pNoisePS        = shader_CompilePixel( pSource, "PS_Noise", "ps_5_0", shaderPath );
    pConstantBuffer = shader_CreateConstantBuffer( sizeof(cb_post_filters), CB_TYPE_DYNAMIC );

    x_free( pSource );
}

//==============================================================================

void post_mgr::filter_resources::Shutdown( void )
{
    ReleaseFilterTarget( History );
    ReleaseFilterTarget( Post[0] );
    ReleaseFilterTarget( Post[1] );
    for( s32 i = 0; i < MAX_POST_MIPS; ++i )
        ReleaseFilterTarget( Mip[i] );
    ReleaseFilterTexture( pMipRampTexture, pMipRampSRV );

    if( pMotionBlurPS )
    {
        pMotionBlurPS->Release();
        pMotionBlurPS = NULL;
    }

    if( pMipCompositePS )
    {
        pMipCompositePS->Release();
        pMipCompositePS = NULL;
    }

    if( pRadialBlurPS )
    {
        pRadialBlurPS->Release();
        pRadialBlurPS = NULL;
    }

    if( pScreenWarpPS )
    {
        pScreenWarpPS->Release();
        pScreenWarpPS = NULL;
    }

    if( pNoisePS )
    {
        pNoisePS->Release();
        pNoisePS = NULL;
    }

    if( pConstantBuffer )
    {
        pConstantBuffer->Release();
        pConstantBuffer = NULL;
    }

    CopyWidth     = 0;
    CopyHeight    = 0;
    CopyFormat    = RTARGET_FORMAT_COUNT;
    MipSourceWidth  = 0;
    MipSourceHeight = 0;
    MipSourceFormat = RTARGET_FORMAT_COUNT;
    bHistoryValid = FALSE;
    ResetPostChain();
}

//==============================================================================

xbool post_mgr::filter_resources::EnsureCopyTargets( const rtarget* pSourceTarget )
{
    if( !pSourceTarget || !pSourceTarget->pTexture )
        return FALSE;

    if( History.pTexture &&
        History.pShaderResourceView &&
        Post[0].pTexture &&
        Post[0].pShaderResourceView &&
        Post[1].pTexture &&
        Post[1].pShaderResourceView &&
        (CopyWidth  == pSourceTarget->Desc.Width) &&
        (CopyHeight == pSourceTarget->Desc.Height) &&
        (CopyFormat == pSourceTarget->Desc.Format) )
    {
        return TRUE;
    }

    ReleaseFilterTarget( History );
    ReleaseFilterTarget( Post[0] );
    ReleaseFilterTarget( Post[1] );
    CopyWidth     = 0;
    CopyHeight    = 0;
    CopyFormat    = RTARGET_FORMAT_COUNT;
    bHistoryValid = FALSE;
    ResetPostChain();

    if( !EnsureFilterTarget( History, pSourceTarget ) )
        return FALSE;

    if( !EnsureFilterTarget( Post[0], pSourceTarget ) )
    {
        ReleaseFilterTarget( History );
        return FALSE;
    }

    if( !EnsureFilterTarget( Post[1], pSourceTarget ) )
    {
        ReleaseFilterTarget( History );
        ReleaseFilterTarget( Post[0] );
        return FALSE;
    }

    CopyWidth  = pSourceTarget->Desc.Width;
    CopyHeight = pSourceTarget->Desc.Height;
    CopyFormat = pSourceTarget->Desc.Format;
    return TRUE;
}

//==============================================================================

xbool post_mgr::filter_resources::EnsureMipTargets( const rtarget* pSourceTarget )
{
    if( !pSourceTarget || !pSourceTarget->pTexture )
        return FALSE;

    xbool bTargetsValid = TRUE;
    for( s32 i = 0; i < MAX_POST_MIPS; ++i )
    {
        if( !Mip[i].pTexture || !Mip[i].pShaderResourceView )
        {
            bTargetsValid = FALSE;
            break;
        }
    }

    if( bTargetsValid &&
        (MipSourceWidth  == pSourceTarget->Desc.Width) &&
        (MipSourceHeight == pSourceTarget->Desc.Height) &&
        (MipSourceFormat == pSourceTarget->Desc.Format) )
    {
        return TRUE;
    }

    for( s32 i = 0; i < MAX_POST_MIPS; ++i )
        ReleaseFilterTarget( Mip[i] );

    MipSourceWidth  = 0;
    MipSourceHeight = 0;
    MipSourceFormat = RTARGET_FORMAT_COUNT;

    u32 Width  = MAX( pSourceTarget->Desc.Width  >> 1, 1u );
    u32 Height = MAX( pSourceTarget->Desc.Height >> 1, 1u );
    for( s32 i = 0; i < MAX_POST_MIPS; ++i )
    {
        if( !EnsureAbsoluteFilterTarget( Mip[i], Width, Height, pSourceTarget->Desc.Format ) )
        {
            for( s32 j = 0; j <= i; ++j )
                ReleaseFilterTarget( Mip[j] );
            return FALSE;
        }

        Width  = MAX( Width  >> 1, 1u );
        Height = MAX( Height >> 1, 1u );
    }

    MipSourceWidth  = pSourceTarget->Desc.Width;
    MipSourceHeight = pSourceTarget->Desc.Height;
    MipSourceFormat = pSourceTarget->Desc.Format;
    return TRUE;
}

//==============================================================================

xbool post_mgr::filter_resources::BeginPostChain( const rtarget* pSourceTarget )
{
    if( !EnsureCopyTargets( pSourceTarget ) )
        return FALSE;

    if( !CopyRenderTargetToTarget( Post[0], pSourceTarget ) )
        return FALSE;

    ActiveSourceIndex = 0;
    ActiveTargetIndex = 1;
    bPostChainActive  = TRUE;
    return TRUE;
}

//==============================================================================

xbool post_mgr::filter_resources::CaptureHistory( const rtarget* pSourceTarget )
{
    if( !EnsureCopyTargets( pSourceTarget ) )
        return FALSE;

    if( !CopyRenderTargetToTarget( History, pSourceTarget ) )
        return FALSE;

    bHistoryValid = TRUE;
    return TRUE;
}

//==============================================================================

void post_mgr::filter_resources::InvalidateHistory( void )
{
    bHistoryValid = FALSE;
    ResetPostChain();
}

//==============================================================================

const rtarget* post_mgr::filter_resources::GetPostSource( void ) const
{
    if( !bPostChainActive )
        return NULL;

    return &Post[ActiveSourceIndex];
}

//==============================================================================

const rtarget* post_mgr::filter_resources::GetMipTarget( s32 Index ) const
{
    if( (Index < 0) || (Index >= MAX_POST_MIPS) )
        return NULL;

    if( !Mip[Index].pTexture || !Mip[Index].pShaderResourceView )
        return NULL;

    return &Mip[Index];
}

//==============================================================================

xbool post_mgr::filter_resources::BindPostTarget( void )
{
    if( !bPostChainActive )
        return FALSE;

    return rtarget_SetTargets( &Post[ActiveTargetIndex], 1, NULL );
}

//==============================================================================

xbool post_mgr::filter_resources::PrimePostTarget( void )
{
    const rtarget* pSource = GetPostSource();
    if( !pSource || !BindPostTarget() )
        return FALSE;

    composite_Blit( *pSource,
                    COMPOSITE_BLEND_COPY,
                    1.0f,
                    NULL,
                    STATE_SAMPLER_POINT_CLAMP );
    return TRUE;
}

//==============================================================================

void post_mgr::filter_resources::SwapPostTargets( void )
{
    if( !bPostChainActive )
        return;

    s32 index         = ActiveSourceIndex;
    ActiveSourceIndex = ActiveTargetIndex;
    ActiveTargetIndex = index;
}

//==============================================================================

xbool post_mgr::filter_resources::ResolvePostChain( void )
{
    const rtarget* pSource = GetPostSource();
    if( !pSource )
        return FALSE;

    composite_Blit( *pSource,
                    COMPOSITE_BLEND_COPY,
                    1.0f,
                    NULL,
                    STATE_SAMPLER_POINT_CLAMP );

    ResetPostChain();
    return TRUE;
}

//==============================================================================

void post_mgr::filter_resources::ResetPostChain( void )
{
    ActiveSourceIndex = 0;
    ActiveTargetIndex = 1;
    bPostChainActive  = FALSE;
}

//==============================================================================

xbool post_mgr::filter_resources::IsPostChainActive( void ) const
{
    return bPostChainActive;
}

//==============================================================================

xbool post_mgr::filter_resources::UpdateMipRampTexture( const xbitmap* pBitmap )
{
    if( !g_pd3dDevice || !g_pd3dContext || !pBitmap )
        return FALSE;

    if( !pMipRampTexture || !pMipRampSRV )
    {
        ReleaseFilterTexture( pMipRampTexture, pMipRampSRV );

        D3D11_TEXTURE2D_DESC desc;
        x_memset( &desc, 0, sizeof(desc) );
        desc.Width              = kMipRampSampleCount;
        desc.Height             = 1;
        desc.MipLevels          = 1;
        desc.ArraySize          = 1;
        desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count   = 1;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = g_pd3dDevice->CreateTexture2D( &desc, NULL, &pMipRampTexture );
        if( FAILED(hr) || !pMipRampTexture )
        {
            ReleaseFilterTexture( pMipRampTexture, pMipRampSRV );
            return FALSE;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        x_memset( &srvDesc, 0, sizeof(srvDesc) );
        srvDesc.Format                    = desc.Format;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels       = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        hr = g_pd3dDevice->CreateShaderResourceView( pMipRampTexture, &srvDesc, &pMipRampSRV );
        if( FAILED(hr) || !pMipRampSRV )
        {
            ReleaseFilterTexture( pMipRampTexture, pMipRampSRV );
            return FALSE;
        }
    }

    const s32 Width  = pBitmap->GetWidth();
    const s32 Height = pBitmap->GetHeight();
    const s32 Count  = Width * Height;
    if( Count <= 0 )
        return FALSE;

    u8 Ramp[kMipRampSampleCount * 4];
    x_memset( Ramp, 0, sizeof(Ramp) );

    for( s32 i = 0; i < kMipRampSampleCount; ++i )
    {
        const f32 SampleT  = (kMipRampSampleCount > 1) ? ((f32)i / (f32)(kMipRampSampleCount - 1)) : 0.0f;
        const s32 SourceIX = MIN( (s32)(SampleT * (f32)(Count - 1) + 0.5f), Count - 1 );
        const s32 SourceX  = SourceIX % Width;
        const s32 SourceY  = SourceIX / Width;
        const xcolor Color = pBitmap->GetPixelColor( SourceX, SourceY );

        Ramp[i * 4 + 0] = Color.R;
        Ramp[i * 4 + 1] = Color.G;
        Ramp[i * 4 + 2] = Color.B;
        Ramp[i * 4 + 3] = Color.A;
    }

    g_pd3dContext->UpdateSubresource( pMipRampTexture, 0, NULL, Ramp, kMipRampSampleCount * 4, 0 );
    return TRUE;
}

//==============================================================================

void post_mgr::filter_resources::UpdateConstants( f32 MotionIntensity, f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale, f32 CenterU, f32 CenterV )
{
    if( !pConstantBuffer || !g_pd3dContext )
        return;

    cb_post_filters cbData;
    x_memset( &cbData, 0, sizeof(cbData) );
    cbData.Params0.Set( x_clamp( MotionIntensity, 0.0f, 1.0f ),
                        Zoom,
                        x_sin( Angle ),
                        x_cos( Angle ) );
    cbData.Params1.Set( AlphaSub / 255.0f,
                        AlphaScale / 255.0f,
                        0.0f,
                        0.0f );
    cbData.Params6.Set( CenterU,
                        CenterV,
                        0.0f,
                        0.0f );

    shader_UpdateConstantBuffer( pConstantBuffer, &cbData, sizeof(cb_post_filters) );
    g_pd3dContext->PSSetConstantBuffers( 4, 1, &pConstantBuffer );
}

//==============================================================================

void post_mgr::filter_resources::UpdateScreenWarpConstants( const post_screen_warp_params& ScreenWarp )
{
    if( !pConstantBuffer || !g_pd3dContext )
        return;

    cb_post_filters cbData;
    x_memset( &cbData, 0, sizeof(cbData) );

    cbData.Params2.Set( 0.0f,
                        (CopyWidth  > 0) ? (1.0f / (f32)CopyWidth)  : 0.0f,
                        (CopyHeight > 0) ? (1.0f / (f32)CopyHeight) : 0.0f,
                        0.0f );

    const view* pView = eng_GetView();
    if( pView )
    {
        s32 WarpCount = 0;
        for( s32 i = 0; (i < ScreenWarp.Count) && (WarpCount < MAX_POST_SCREEN_WARPS); ++i )
        {
            if( pView->SphereInView( ScreenWarp.WorldPos[i], ScreenWarp.Radius[i], view::WORLD ) == view::VISIBLE_NONE )
                continue;

            const vector3 ScreenPos = pView->PointToScreen( ScreenWarp.WorldPos[i], view::WORLD );
            const f32 ScreenRadius  = pView->CalcScreenSize( ScreenWarp.WorldPos[i], ScreenWarp.Radius[i], view::WORLD ) * 0.5f;
            if( ScreenRadius <= 0.0f )
                continue;

            cbData.ScreenWarps[WarpCount].Set( ScreenPos.GetX(),
                                               ScreenPos.GetY(),
                                               ScreenRadius,
                                               MAX( ScreenWarp.Amount[i], 0.001f ) );
            WarpCount++;
        }

        cbData.Params2.GetX() = (f32)WarpCount;
    }

    shader_UpdateConstantBuffer( pConstantBuffer, &cbData, sizeof(cb_post_filters) );
    g_pd3dContext->PSSetConstantBuffers( 4, 1, &pConstantBuffer );
}

//==============================================================================
//  EFFECT IMPLEMENTATIONS
//==============================================================================

void post_mgr::ExecuteMotionBlur( void )
{
    if( !g_pd3dContext || !m_FilterResources.pMotionBlurPS || !m_FilterResources.bHistoryValid || !m_FilterResources.History.pShaderResourceView )
        return;

    if( !GetCurrentPostTarget() )
        return;

    PrepareFullscreenQuad();
    m_FilterResources.UpdateConstants( m_MotionBlur.Intensity, 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f );

    composite_Blit( m_FilterResources.History,
                    COMPOSITE_BLEND_ALPHA,
                    1.0f,
                    m_FilterResources.pMotionBlurPS,
                    STATE_SAMPLER_LINEAR_CLAMP );

    ID3D11ShaderResourceView* pNullSRV = NULL;
    g_pd3dContext->PSSetShaderResources( 0, 1, &pNullSRV );
}

//==============================================================================

void post_mgr::ExecuteRadialBlur( void )
{
    const rtarget* pSource = m_FilterResources.GetPostSource();
    if( !g_pd3dContext || !m_FilterResources.pRadialBlurPS || !pSource )
        return;

    BuildScreenMips( 1 );

    const rtarget* pBlurSource = m_FilterResources.GetMipTarget( 0 );
    if( !pBlurSource || !pBlurSource->pShaderResourceView )
        pBlurSource = pSource;

    if( !m_FilterResources.PrimePostTarget() )
        return;

    PrepareFullscreenQuad();

    const f32 CenterU = (pSource->Desc.Width  > 0) ? x_clamp( ((f32)(m_PostViewL + m_PostViewR) * 0.5f) / (f32)pSource->Desc.Width,  0.0f, 1.0f ) : 0.5f;
    const f32 CenterV = (pSource->Desc.Height > 0) ? x_clamp( ((f32)(m_PostViewT + m_PostViewB) * 0.5f) / (f32)pSource->Desc.Height, 0.0f, 1.0f ) : 0.5f;

    m_FilterResources.UpdateConstants( 0.0f,
                                       m_RadialBlur.Zoom,
                                       m_RadialBlur.Angle,
                                       m_RadialBlur.AlphaSub,
                                       m_RadialBlur.AlphaScale,
                                       CenterU,
                                       CenterV );
    composite_Blit( *pBlurSource,
                    COMPOSITE_BLEND_ALPHA,
                    1.0f,
                    m_FilterResources.pRadialBlurPS,
                    STATE_SAMPLER_LINEAR_CLAMP );

    m_FilterResources.UpdateConstants( 0.0f,
                                       m_RadialBlur.Zoom,
                                       -m_RadialBlur.Angle,
                                       m_RadialBlur.AlphaSub,
                                       m_RadialBlur.AlphaScale,
                                       CenterU,
                                       CenterV );
    composite_Blit( *pBlurSource,
                    COMPOSITE_BLEND_ALPHA,
                    1.0f,
                    m_FilterResources.pRadialBlurPS,
                    STATE_SAMPLER_LINEAR_CLAMP );

    ID3D11ShaderResourceView* pNullSRV = NULL;
    g_pd3dContext->PSSetShaderResources( 0, 1, &pNullSRV );
    m_FilterResources.SwapPostTargets();
}

//==============================================================================

void post_mgr::ExecuteScreenWarps( void )
{
    const rtarget* pSource = m_FilterResources.GetPostSource();
    if( !g_pd3dContext || !m_FilterResources.pScreenWarpPS || !pSource || (m_ScreenWarp.Count <= 0) )
        return;

    if( !m_FilterResources.BindPostTarget() )
        return;

    PrepareFullscreenQuad();
    m_FilterResources.UpdateScreenWarpConstants( m_ScreenWarp );

    ID3D11ShaderResourceView* pWarpBase = pSource->pShaderResourceView;
    g_pd3dContext->PSSetShaderResources( 1, 1, &pWarpBase );

    composite_Blit( *pSource,
                    COMPOSITE_BLEND_COPY,
                    1.0f,
                    m_FilterResources.pScreenWarpPS,
                    STATE_SAMPLER_LINEAR_CLAMP );

    ID3D11ShaderResourceView* pNullSRVs[2] = { NULL, NULL };
    g_pd3dContext->PSSetShaderResources( 0, 2, pNullSRVs );
    m_FilterResources.SwapPostTargets();
}

//==============================================================================

void post_mgr::ExecuteMipFilter( void )
{
    const s32 palette = m_MipFilter.PaletteIndex;
    if( !g_pd3dContext || !m_FilterResources.pMipCompositePS || !m_FilterResources.pConstantBuffer )
        return;

    if( (palette < 0) || (palette >= 4) )
        return;

    const s32 count = MIN( m_MipFilter.Count[palette], MAX_POST_MIPS );
    if( count <= 0 )
        return;

    const rtarget* pTarget = GetCurrentPostTarget();
    const rtarget* pDepthTarget = rtarget_GetCurrentDepth();
    const rtarget* pLinearDepthTarget = g_GBufferMgr.GetGBufferTarget( GBUFFER_LINEAR_DEPTH );
    if( !pTarget || !pLinearDepthTarget || !pLinearDepthTarget->pShaderResourceView )
        return;

    BuildScreenMips( count );

    const rtarget* pMipTarget = m_FilterResources.GetMipTarget( count - 1 );
    if( !pMipTarget || !pMipTarget->pShaderResourceView )
        return;

    if( !rtarget_SetTargets( pTarget, 1, pDepthTarget ) )
        return;

    const xbool bUseCustom = (m_MipFilter.Fn[palette] == render::FALLOFF_CUSTOM);
    if( bUseCustom )
    {
        if( !m_pMipTexture || !m_FilterResources.UpdateMipRampTexture( m_pMipTexture ) )
            return;
    }

    cb_post_filters cbData;
    x_memset( &cbData, 0, sizeof(cbData) );
    cbData.Params3.Set( m_MipFilter.Color[palette].R / 128.0f,
                        m_MipFilter.Color[palette].G / 128.0f,
                        m_MipFilter.Color[palette].B / 128.0f,
                        m_MipFilter.Color[palette].A / 128.0f );
    cbData.Params4.Set( (f32)m_MipFilter.Fn[palette],
                        m_MipFilter.Param1[palette],
                        m_MipFilter.Param2[palette],
                        m_MipFilter.Offset[palette] );
    cbData.Params5.Set( m_PostNearZ,
                        m_PostFarZ,
                        (pTarget->Desc.Width  > 0) ? (1.0f / (f32)pTarget->Desc.Width)  : 0.0f,
                        (pTarget->Desc.Height > 0) ? (1.0f / (f32)pTarget->Desc.Height) : 0.0f );
    cbData.Params6.Set( bUseCustom ? 1.0f : 0.0f,
                        0.0f,
                        0.0f,
                        0.0f );

    shader_UpdateConstantBuffer( m_FilterResources.pConstantBuffer, &cbData, sizeof(cb_post_filters) );
    g_pd3dContext->PSSetConstantBuffers( 4, 1, &m_FilterResources.pConstantBuffer );

    ID3D11ShaderResourceView* pResources[2] = { pLinearDepthTarget->pShaderResourceView, NULL };
    if( bUseCustom )
        pResources[1] = m_FilterResources.pMipRampSRV;
    g_pd3dContext->PSSetShaderResources( 1, 2, pResources );

    PrepareFullscreenQuad();
    composite_Blit( *pMipTarget,
                    COMPOSITE_BLEND_ALPHA,
                    1.0f,
                    m_FilterResources.pMipCompositePS,
                    STATE_SAMPLER_LINEAR_CLAMP );

    ID3D11ShaderResourceView* pNullResources[2] = { NULL, NULL };
    g_pd3dContext->PSSetShaderResources( 1, 2, pNullResources );
}

//==============================================================================

void post_mgr::ExecuteNoiseFilter( void )
{
    if( !g_pd3dContext || !m_FilterResources.pNoisePS || !m_FilterResources.pConstantBuffer )
        return;
    const rtarget* pLinearDepthTarget = g_GBufferMgr.GetGBufferTarget( GBUFFER_LINEAR_DEPTH );
    if( !pLinearDepthTarget || !pLinearDepthTarget->pShaderResourceView )
        return;

    cb_post_filters cbData;
    x_memset( &cbData, 0, sizeof(cbData) );

    const f32 aNorm = m_Simple.NoiseColor.A / 255.0f;
    cbData.Params3.Set( m_Simple.NoiseColor.R / 255.0f,
                        m_Simple.NoiseColor.G / 255.0f,
                        m_Simple.NoiseColor.B / 255.0f,
                        aNorm * aNorm * aNorm );

    cbData.Params6.Set( (f32)x_rand(),
                        (f32)x_rand(),
                        0.0f,
                        0.0f );

    shader_UpdateConstantBuffer( m_FilterResources.pConstantBuffer, &cbData, sizeof(cb_post_filters) );
    g_pd3dContext->PSSetConstantBuffers( 4, 1, &m_FilterResources.pConstantBuffer );
    PrepareFullscreenQuad();
    composite_Blit( *pLinearDepthTarget,
                    COMPOSITE_BLEND_ALPHA,
                    1.0f,
                    m_FilterResources.pNoisePS,
                    STATE_SAMPLER_POINT_CLAMP );
}

//==============================================================================

void post_mgr::ExecuteScreenFade( void )
{
    if( !g_pd3dContext )
        return;

    PrepareFullscreenQuad();

    irect Rect;
    Rect.l = m_PostViewL;
    Rect.t = m_PostViewT;
    Rect.r = m_PostViewR;
    Rect.b = m_PostViewB;

    state_SetBlend( STATE_BLEND_ALPHA );
    draw_Rect( Rect, m_Simple.FadeColor, FALSE, DRAW_UI_RTARGET );
}

//==============================================================================
//  SUPPORT FUNCTIONS
//==============================================================================

void post_mgr::BuildMipPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex )
{
    if( (m_MipFilter.Fn[PaletteIndex] == Fn) &&
        (m_MipFilter.Param1[PaletteIndex] == Param1) &&
        (m_MipFilter.Param2[PaletteIndex] == Param2) &&
        (m_MipFilter.Color[PaletteIndex] == Color) )
    {
        return;
    }

    m_MipFilter.Fn[PaletteIndex] = Fn;
    m_MipFilter.Param1[PaletteIndex] = Param1;
    m_MipFilter.Param2[PaletteIndex] = Param2;
    m_MipFilter.Color[PaletteIndex] = Color;
    m_MipFilter.PaletteIndex = PaletteIndex;
}

//==============================================================================

void post_mgr::CopyBackBuffer( void )
{
    const rtarget* pTarget = GetCurrentPostTarget();
    if( !pTarget )
        return;

    m_FilterResources.BeginPostChain( pTarget );
}

//==============================================================================

void post_mgr::BuildScreenMips( s32 nMips )
{
    if( nMips <= 0 )
        return;

    const rtarget* pSourceTarget = GetCurrentPostTarget();
    if( !pSourceTarget || !pSourceTarget->pShaderResourceView )
        return;

    nMips = MIN( nMips, MAX_POST_MIPS );
    if( !m_FilterResources.EnsureMipTargets( pSourceTarget ) )
        return;

    const rtarget* pMipSource = pSourceTarget;
    for( s32 i = 0; i < nMips; ++i )
    {
        const rtarget* pMipTarget = m_FilterResources.GetMipTarget( i );
        if( !pMipTarget || !rtarget_SetTargets( pMipTarget, 1, NULL ) )
            return;

        composite_Blit( *pMipSource,
                        COMPOSITE_BLEND_COPY,
                        1.0f,
                        NULL,
                        STATE_SAMPLER_LINEAR_CLAMP );

        pMipSource = pMipTarget;
    }
}

//==============================================================================

void post_mgr::UpdateFilterHistoryBeforePresent( void )
{
    const rtarget* pTarget = GetCurrentPostTarget();
    if( !pTarget )
        return;

    m_FilterResources.CaptureHistory( pTarget );
}
