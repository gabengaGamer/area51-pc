//==============================================================================
// 
//  PostMgr_Glow.cpp
// 
//  Glow stage utilities for the PC post-processing manager
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
//  CONSTANTS
//==============================================================================

static const f32 kGlowIntensityScale = 0.5f;

//==============================================================================
//  CONSTANT BUFFER LAYOUT
//==============================================================================

struct cb_post_glow
{
    vector4 Params0;
    vector4 Params1;
};

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static void ReleaseGlowTarget( rtarget& Target )
{
    rtarget_Destroy( Target );
    Target = rtarget();
}

//==============================================================================
//  GLOW RESOURCE MANAGEMENT
//==============================================================================

post_mgr::glow_resources::glow_resources()
{
    Downsample = rtarget();
    Blur[0]    = rtarget();
    Blur[1]    = rtarget();
    Composite  = rtarget();
    History    = rtarget();
    ActiveResult = NULL;
    BufferWidth = 0;
    BufferHeight = 0;
    bResourcesValid = FALSE;
    bPendingComposite = FALSE;
    pDownsamplePS = NULL;
    pBlurHPS = NULL;
    pBlurVPS = NULL;
    pCombinePS = NULL;
    pCompositePS = NULL;
    pConstantBuffer = NULL;
}

//==============================================================================

void post_mgr::glow_resources::Initialize( void )
{
    Shutdown();
    ResetFrame();

    if( !g_pd3dDevice )
        return;

    char shaderPath[256];
    x_sprintf( shaderPath, "%spost_glow.hlsl", SHADER_PATH );

    char* pSource = shader_LoadSourceFromFile( shaderPath ); // GS: TODO: Maybe should this check be added to the shader manager itself?
    if( !pSource )
        return;

    pDownsamplePS = shader_CompilePixel( pSource, "PS_Downsample", "ps_5_0", shaderPath );
    pBlurHPS      = shader_CompilePixel( pSource, "PS_BlurHorizontal", "ps_5_0", shaderPath );
    pBlurVPS      = shader_CompilePixel( pSource, "PS_BlurVertical", "ps_5_0", shaderPath );
    pCombinePS    = shader_CompilePixel( pSource, "PS_Combine", "ps_5_0", shaderPath );
    pCompositePS  = shader_CompilePixel( pSource, "PS_Composite", "ps_5_0", shaderPath );
    pConstantBuffer = shader_CreateConstantBuffer( sizeof(cb_post_glow) );

    x_free( pSource );

    if( !pDownsamplePS || !pBlurHPS || !pBlurVPS || !pCombinePS ||
        !pCompositePS || !pConstantBuffer )
    {
        x_DebugMsg( "PostMgr: WARNING - Failed to initialize glow shaders\n" );
    }
}

//==============================================================================

void post_mgr::glow_resources::Shutdown( void )
{
    ReleaseGlowTarget( Downsample );
    ReleaseGlowTarget( Blur[0] );
    ReleaseGlowTarget( Blur[1] );
    ReleaseGlowTarget( Composite );
    ReleaseGlowTarget( History );

    BufferWidth = 0;
    BufferHeight = 0;
    bResourcesValid = FALSE;
    ResetFrame();

    if( pConstantBuffer )
    {
        pConstantBuffer->Release();
        pConstantBuffer = NULL;
    }

    if( pDownsamplePS )
    {
        pDownsamplePS->Release();
        pDownsamplePS = NULL;
    }

    if( pBlurHPS )
    {
        pBlurHPS->Release();
        pBlurHPS = NULL;
    }

    if( pBlurVPS )
    {
        pBlurVPS->Release();
        pBlurVPS = NULL;
    }

    if( pCombinePS )
    {
        pCombinePS->Release();
        pCombinePS = NULL;
    }

    if( pCompositePS )
    {
        pCompositePS->Release();
        pCompositePS = NULL;
    }
}

//==============================================================================

void post_mgr::glow_resources::ResetFrame( void )
{
    ActiveResult = NULL;
    bPendingComposite = FALSE;
}

//==============================================================================

static xbool CreateGlowTarget( rtarget& Target, const rtarget_desc& Desc )
{
    if( rtarget_Create( Target, Desc ) )
        return TRUE;

    ReleaseGlowTarget( Target );
    return FALSE;
}

//==============================================================================

xbool post_mgr::glow_resources::ResizeIfNeeded( u32 SourceWidth, u32 SourceHeight )
{
    if( SourceWidth == 0 || SourceHeight == 0 )
    {
        ReleaseGlowTarget( Downsample );
        ReleaseGlowTarget( Blur[0] );
        ReleaseGlowTarget( Blur[1] );
        ReleaseGlowTarget( Composite );
        ReleaseGlowTarget( History );
        BufferWidth = 0;
        BufferHeight = 0;
        bResourcesValid = FALSE;
        ResetFrame();
        return FALSE;
    }

    u32 width  = (SourceWidth  > 1) ? (SourceWidth  / 2) : SourceWidth;
    u32 height = (SourceHeight > 1) ? (SourceHeight / 2) : SourceHeight;

    if( bResourcesValid && (BufferWidth == width) && (BufferHeight == height) )
        return TRUE;

    ReleaseGlowTarget( Downsample );
    ReleaseGlowTarget( Blur[0] );
    ReleaseGlowTarget( Blur[1] );
    ReleaseGlowTarget( Composite );
    ReleaseGlowTarget( History );

    rtarget_desc desc;
    desc.Width = width;
    desc.Height = height;
    desc.Format = RTARGET_FORMAT_RGBA16F;
    desc.SampleCount = 1;
    desc.SampleQuality = 0;
    desc.bBindAsTexture = TRUE;

    if( !CreateGlowTarget( Downsample, desc ) ||
        !CreateGlowTarget( Blur[0], desc ) ||
        !CreateGlowTarget( Blur[1], desc ) ||
        !CreateGlowTarget( Composite, desc ) ||
        !CreateGlowTarget( History, desc ) )
    {
        ResizeIfNeeded( 0, 0 );
        return FALSE;
    }

    if( g_pd3dContext )
    {
        const f32 clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dContext->ClearRenderTargetView( Downsample.pRenderTargetView, clearColor );
        g_pd3dContext->ClearRenderTargetView( Blur[0].pRenderTargetView, clearColor );
        g_pd3dContext->ClearRenderTargetView( Blur[1].pRenderTargetView, clearColor );
        g_pd3dContext->ClearRenderTargetView( Composite.pRenderTargetView, clearColor );
        g_pd3dContext->ClearRenderTargetView( History.pRenderTargetView, clearColor );
    }

    BufferWidth = width;
    BufferHeight = height;
    bResourcesValid = TRUE;
    ResetFrame();

    return TRUE;
}

//==============================================================================

const rtarget* post_mgr::glow_resources::BindForComposite( void ) const
{
    if( !bPendingComposite || !ActiveResult )
        return NULL;

    if( !ActiveResult->pShaderResourceView )
        return NULL;

    if( !g_pd3dContext )
        return NULL;

    rtarget_SetBackBuffer();
    return ActiveResult;
}

//==============================================================================

void post_mgr::glow_resources::FinalizeComposite( void )
{
    ResetFrame();
}

//==============================================================================

void post_mgr::glow_resources::UpdateConstants( f32 Cutoff, f32 IntensityScale, f32 MotionBlend, f32 StepX, f32 StepY )
{
    if( !pConstantBuffer || !g_pd3dContext )
        return;

    cb_post_glow cbData;
    cbData.Params0.Set( Cutoff, IntensityScale, MotionBlend, 0.0f );
    cbData.Params1.Set( StepX, StepY, 0.0f, 0.0f );

    shader_UpdateConstantBuffer( pConstantBuffer, &cbData, sizeof(cb_post_glow) );
    g_pd3dContext->PSSetConstantBuffers( 4, 1, &pConstantBuffer );
}

//==============================================================================

void post_mgr::glow_resources::SetPendingResult( const rtarget* pResult )
{
    ActiveResult = pResult;
    bPendingComposite = (pResult != NULL);
}

//==============================================================================
//  GLOW PROCESSING
//==============================================================================

void post_mgr::ExecuteSelfIllumGlow( void )
{
    if( !g_pd3dContext )
        return;

    if( !m_GlowResources.pDownsamplePS || !m_GlowResources.pBlurHPS ||
        !m_GlowResources.pBlurVPS || !m_GlowResources.pCombinePS )
    {
        x_DebugMsg( "PostMgr: Glow resources missing, skipping glow stage\n" );
        return;
    }

    const rtarget* pGlowSource = g_GBufferMgr.GetGBufferTarget( GBUFFER_GLOW );
    if( !pGlowSource || !pGlowSource->pShaderResourceView )
        return;

    const u32 sourceWidth  = pGlowSource->Desc.Width;
    const u32 sourceHeight = pGlowSource->Desc.Height;
    if( sourceWidth == 0 || sourceHeight == 0 )
        return;

    if( !m_GlowResources.ResizeIfNeeded( sourceWidth, sourceHeight ) )
        return;

    const f32 cutoff       = (m_Glow.Cutoff >= 255) ? 0.0f : (f32)m_Glow.Cutoff / 255.0f;
    const f32 motionBlend  = x_clamp( m_Glow.MotionBlurIntensity, 0.0f, 1.0f );
    const f32 intensity    = kGlowIntensityScale;
    const f32 clearColor[4]= { 0.0f, 0.0f, 0.0f, 0.0f };

    rtarget_SetTargets( &m_GlowResources.Downsample, 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    m_GlowResources.UpdateConstants( cutoff, intensity, motionBlend, 1.0f / (f32)sourceWidth, 1.0f / (f32)sourceHeight );
    composite_Blit( *pGlowSource, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pDownsamplePS );

    rtarget_SetTargets( &m_GlowResources.Blur[0], 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    m_GlowResources.UpdateConstants( cutoff, intensity, motionBlend, 1.0f / (f32)m_GlowResources.BufferWidth, 0.0f );
    composite_Blit( m_GlowResources.Downsample, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pBlurHPS, STATE_SAMPLER_LINEAR_CLAMP );

    rtarget_SetTargets( &m_GlowResources.Blur[1], 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    m_GlowResources.UpdateConstants( cutoff, intensity, motionBlend, 0.0f, 1.0f / (f32)m_GlowResources.BufferHeight );
    composite_Blit( m_GlowResources.Blur[0], COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pBlurVPS, STATE_SAMPLER_LINEAR_CLAMP );

    const rtarget* pBlurredResult = &m_GlowResources.Blur[1];

    if( motionBlend > 0.0f && m_GlowResources.History.pShaderResourceView )
    {
        rtarget_SetTargets( &m_GlowResources.Composite, 1, NULL );
        rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
        m_GlowResources.UpdateConstants( cutoff, intensity, motionBlend, 0.0f, 0.0f );

        ID3D11ShaderResourceView* pHistorySRV = m_GlowResources.History.pShaderResourceView;
        g_pd3dContext->PSSetShaderResources( 1, 1, &pHistorySRV );

        composite_Blit( *pBlurredResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pCombinePS, STATE_SAMPLER_LINEAR_CLAMP );

        ID3D11ShaderResourceView* pNullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( 1, 1, &pNullSRV );

        pBlurredResult = &m_GlowResources.Composite;

        if( m_GlowResources.History.pTexture && pBlurredResult->pTexture )
            g_pd3dContext->CopyResource( m_GlowResources.History.pTexture, pBlurredResult->pTexture );
    }
    else if( m_GlowResources.History.pTexture && pBlurredResult->pTexture )
    {
        g_pd3dContext->CopyResource( m_GlowResources.History.pTexture, pBlurredResult->pTexture );
    }

    m_GlowResources.SetPendingResult( pBlurredResult );
}

//==============================================================================

void post_mgr::UpdateGlowStageBegin( void )
{
    m_GlowResources.ResetFrame();

    if( !g_GBufferMgr.IsGBufferEnabled() )
    {
        m_GlowResources.ResizeIfNeeded( 0, 0 );
        return;
    }

    u32 width = 0;
    u32 height = 0;
    g_GBufferMgr.GetGBufferSize( width, height );

    if( (width == 0) || (height == 0) )
    {
        m_GlowResources.ResizeIfNeeded( 0, 0 );
        return;
    }

    const u32 targetWidth  = (width  > 1) ? (width  / 2) : width;
    const u32 targetHeight = (height > 1) ? (height / 2) : height;

    if( m_GlowResources.bResourcesValid &&
        ((m_GlowResources.BufferWidth != targetWidth) || (m_GlowResources.BufferHeight != targetHeight)) )
    {
        m_GlowResources.ResizeIfNeeded( 0, 0 );
    }
}

//==============================================================================

void post_mgr::CompositePendingGlow( void )
{
    const rtarget* pResult = m_GlowResources.BindForComposite();
    if( !pResult )
        return;

    if( m_GlowResources.pCompositePS && m_GlowResources.pConstantBuffer )
    {
        m_GlowResources.UpdateConstants( 0.0f, kGlowIntensityScale, 0.0f, 0.0f, 0.0f );
        composite_Blit( *pResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pCompositePS, STATE_SAMPLER_LINEAR_CLAMP );
    }
    else
    {
        composite_Blit( *pResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, NULL, STATE_SAMPLER_LINEAR_CLAMP );
    }

    m_GlowResources.FinalizeComposite();
}
