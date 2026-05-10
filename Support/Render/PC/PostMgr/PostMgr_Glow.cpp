//==============================================================================
// 
//  PostMgr_Glow.cpp
// 
//  Glow post-processing module for the PC platform.
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
    static f32  s_GlowScale = 1.0000f;
    static f32  s_GlowBeg   = 0.6800f;
    static f32  s_GlowEnd   = 0.7500f;

    // Constant buffer layout
    struct cb_post_glow
    {
        vector4 Params0;
        vector4 Params1;
    };

    // Helper functions
    static
    void ReleaseGlowTarget( rtarget& Target )
    {
        rtarget_Unregister( Target );
        rtarget_Destroy( Target );
        Target = rtarget();
    }
}

//==============================================================================
//  GLOW RESOURCE MANAGEMENT
//==============================================================================

post_mgr::glow_resources::glow_resources()
{
    Downsample[0] = rtarget();
    Downsample[1] = rtarget();
    Downsample[2] = rtarget();
    Blur[0]    = rtarget();
    Blur[1]    = rtarget();
    Composite  = rtarget();
    Accum      = rtarget();
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
    pAccumulatePS = NULL;
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
    x_sprintf( shaderPath, "post_glow.hlsl" );

    char* pSource = shader_LoadSourceFromFile( shaderPath );
    if( !pSource )
        return;

    pDownsamplePS   = shader_CompilePixel( pSource, "PS_Downsample", "ps_5_0", shaderPath );
    pBlurHPS        = shader_CompilePixel( pSource, "PS_BlurHorizontal", "ps_5_0", shaderPath );
    pBlurVPS        = shader_CompilePixel( pSource, "PS_BlurVertical", "ps_5_0", shaderPath );
    pCombinePS      = shader_CompilePixel( pSource, "PS_Combine", "ps_5_0", shaderPath );
    pCompositePS    = shader_CompilePixel( pSource, "PS_Composite", "ps_5_0", shaderPath );
    pAccumulatePS   = shader_CompilePixel( pSource, "PS_Accumulate", "ps_5_0", shaderPath );
    pConstantBuffer = shader_CreateConstantBuffer( sizeof(cb_post_glow), CB_TYPE_DYNAMIC );

    x_free( pSource );

    if( !pDownsamplePS || !pBlurHPS || !pBlurVPS || !pCombinePS ||
        !pCompositePS || !pAccumulatePS || !pConstantBuffer )
    {
        x_DebugMsg( "PostMgr: WARNING - Failed to initialize glow shaders\n" );
    }
}

//==============================================================================

void post_mgr::glow_resources::Shutdown( void )
{
    ReleaseGlowTarget( Downsample[0] );
    ReleaseGlowTarget( Downsample[1] );
    ReleaseGlowTarget( Downsample[2] );
    ReleaseGlowTarget( Blur[0] );
    ReleaseGlowTarget( Blur[1] );
    ReleaseGlowTarget( Composite );
    ReleaseGlowTarget( Accum );
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

    if( pAccumulatePS )
    {
        pAccumulatePS->Release();
        pAccumulatePS = NULL;
    }
}

//==============================================================================

void post_mgr::glow_resources::ResetFrame( void )
{
    ActiveResult = NULL;
    bPendingComposite = FALSE;
}

//==============================================================================

xbool post_mgr::glow_resources::ResizeIfNeeded( u32 SourceWidth, u32 SourceHeight )
{
    if( SourceWidth == 0 || SourceHeight == 0 )
    {
        ReleaseGlowTarget( Downsample[0] );
        ReleaseGlowTarget( Downsample[1] );
        ReleaseGlowTarget( Downsample[2] );
        ReleaseGlowTarget( Blur[0] );
        ReleaseGlowTarget( Blur[1] );
        ReleaseGlowTarget( Composite );
        ReleaseGlowTarget( Accum );
        ReleaseGlowTarget( History );
        BufferWidth = 0;
        BufferHeight = 0;
        bResourcesValid = FALSE;
        ResetFrame();
        return FALSE;
    }

    u32 halfW  = (SourceWidth  > 1) ? (SourceWidth  / 2) : SourceWidth;
    u32 halfH  = (SourceHeight > 1) ? (SourceHeight / 2) : SourceHeight;
    u32 qW     = (halfW  > 1) ? (halfW  / 2) : halfW;
    u32 qH     = (halfH  > 1) ? (halfH  / 2) : halfH;
    u32 eW     = (qW     > 1) ? (qW     / 2) : qW;
    u32 eH     = (qH     > 1) ? (qH     / 2) : qH;

    rtarget_registration regHalf;
    regHalf.Policy = RTARGET_SIZE_ABSOLUTE;
    regHalf.BaseWidth = halfW;
    regHalf.BaseHeight = halfH;
    regHalf.Format = RTARGET_FORMAT_RGBA16F;
    regHalf.SampleCount = 1;
    regHalf.SampleQuality = 0;
    regHalf.bBindAsTexture = TRUE;

    rtarget_registration regQuarter = regHalf;
    regQuarter.BaseWidth = qW;
    regQuarter.BaseHeight = qH;

    rtarget_registration regEighth = regHalf;
    regEighth.BaseWidth = eW;
    regEighth.BaseHeight = eH;

    if( !rtarget_GetOrCreate( Downsample[0], regHalf ) ||
        !rtarget_GetOrCreate( Downsample[1], regQuarter ) ||
        !rtarget_GetOrCreate( Downsample[2], regEighth ) ||
        !rtarget_GetOrCreate( Blur[0], regEighth ) ||
        !rtarget_GetOrCreate( Blur[1], regEighth ) ||
        !rtarget_GetOrCreate( Composite, regEighth ) ||
        !rtarget_GetOrCreate( Accum, regEighth ) ||
        !rtarget_GetOrCreate( History, regEighth ) )
    {
        ResizeIfNeeded( 0, 0 );
        return FALSE;
    }

    if( g_pd3dContext )
    {
        static const f32 clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };	
        rtarget_ClearColor( Downsample[0], clearColor );
        rtarget_ClearColor( Downsample[1], clearColor );
        rtarget_ClearColor( Downsample[2], clearColor );
        rtarget_ClearColor( Blur[0], clearColor );
        rtarget_ClearColor( Blur[1], clearColor );
        rtarget_ClearColor( Composite, clearColor );
        rtarget_ClearColor( Accum, clearColor );
        rtarget_ClearColor( History, clearColor );
    }

    BufferWidth = halfW;
    BufferHeight = halfH;
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

    g_GBufferMgr.SetFinalColorTarget();
    return ActiveResult;
}

//==============================================================================

void post_mgr::glow_resources::FinalizeComposite( void )
{
    if( g_pd3dContext &&
        ActiveResult &&
        ActiveResult->pShaderResourceView &&
        History.pRenderTargetView )
    {
        static const f32 clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        rtarget_SetTargets( &History, 1, NULL );
        rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
        composite_Blit( *ActiveResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, NULL, STATE_SAMPLER_LINEAR_CLAMP );
        g_GBufferMgr.SetFinalColorTarget();
    }

    ResetFrame();
}

//==============================================================================

void post_mgr::glow_resources::UpdateConstants( f32 Cutoff, f32 IntensityScale, f32 MotionBlend, f32 StepX, f32 StepY, f32 CompositeWeight )
{
    if( !pConstantBuffer || !g_pd3dContext )
        return;

    cb_post_glow cbData;
    cbData.Params0.Set( Cutoff, IntensityScale, MotionBlend, 0.0f );
    cbData.Params1.Set( StepX, StepY, CompositeWeight, 0.0f );

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
        !m_GlowResources.pBlurVPS || !m_GlowResources.pAccumulatePS )
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

    const f32 cutoff            = (m_Glow.Cutoff >= 255) ? -0.5f : ((f32)m_Glow.Cutoff / 255.0f) - 0.5f;
    const f32 motionBlend       = x_clamp( m_Glow.MotionBlurIntensity, 0.0f, 1.0f );
    const xbool bMutantAccum    = (motionBlend > 0.0f);
    const f32 accumWeight       = 1.0f;
    const f32 clearColor[4]     = { 0.0f, 0.0f, 0.0f, 0.0f };
    f32       mutantTrailScale  = 0.0f;

    // Animate the trail intensity while temporal accumulation is active.
    static f32 s_GlowInc   = 0.005f;
    if( bMutantAccum )
    {
        s_GlowScale += s_GlowInc;
        if( s_GlowScale >= s_GlowEnd ){ s_GlowScale = s_GlowEnd; s_GlowInc *= -1.0f; }
        if( s_GlowScale <= s_GlowBeg ){ s_GlowScale = s_GlowBeg; s_GlowInc *= -1.0f; }
        mutantTrailScale = s_GlowScale;
    }

    // Downsample chain: 1/2, 1/4, 1/8
    rtarget_SetTargets( &m_GlowResources.Downsample[0], 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    m_GlowResources.UpdateConstants( cutoff, accumWeight, motionBlend, 1.0f / (f32)sourceWidth, 1.0f / (f32)sourceHeight );
    composite_Blit( *pGlowSource, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pDownsamplePS );

    rtarget_SetTargets( &m_GlowResources.Downsample[1], 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    m_GlowResources.UpdateConstants( cutoff, accumWeight, motionBlend, 1.0f / (f32)m_GlowResources.Downsample[0].Desc.Width, 1.0f / (f32)m_GlowResources.Downsample[0].Desc.Height );
    composite_Blit( m_GlowResources.Downsample[0], COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pDownsamplePS );

    rtarget_SetTargets( &m_GlowResources.Downsample[2], 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    m_GlowResources.UpdateConstants( cutoff, accumWeight, motionBlend, 1.0f / (f32)m_GlowResources.Downsample[1].Desc.Width, 1.0f / (f32)m_GlowResources.Downsample[1].Desc.Height );
    composite_Blit( m_GlowResources.Downsample[1], COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pDownsamplePS );

    // Apply the separable blur on the 1/8 working buffer.
    rtarget_SetTargets( &m_GlowResources.Blur[0], 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    m_GlowResources.UpdateConstants( cutoff, accumWeight, motionBlend, 1.0f / (f32)m_GlowResources.Downsample[2].Desc.Width, 0.0f );
    composite_Blit( m_GlowResources.Downsample[2], COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pBlurHPS, STATE_SAMPLER_LINEAR_CLAMP );

    rtarget_SetTargets( &m_GlowResources.Blur[1], 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    m_GlowResources.UpdateConstants( cutoff, accumWeight, motionBlend, 0.0f, 1.0f / (f32)m_GlowResources.Downsample[2].Desc.Height );
    composite_Blit( m_GlowResources.Blur[0], COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pBlurVPS, STATE_SAMPLER_LINEAR_CLAMP );

    // Accumulate the blurred glow into a single working buffer.
    rtarget_SetTargets( &m_GlowResources.Accum, 1, NULL );
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );
    m_GlowResources.UpdateConstants( cutoff, accumWeight, motionBlend, 1.0f / (f32)m_GlowResources.Blur[1].Desc.Width, 1.0f / (f32)m_GlowResources.Blur[1].Desc.Height, 1.0f );
    composite_Blit( m_GlowResources.Blur[1], COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pAccumulatePS, STATE_SAMPLER_LINEAR_CLAMP );

    if( !bMutantAccum )
    {
        // Add a small reinforcement pass when temporal accumulation is disabled.
        m_GlowResources.UpdateConstants( cutoff, 0.0125f, motionBlend, 1.0f / (f32)m_GlowResources.Blur[1].Desc.Width, 1.0f / (f32)m_GlowResources.Blur[1].Desc.Height, 1.0f );
        composite_Blit( m_GlowResources.Blur[1], COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pAccumulatePS, STATE_SAMPLER_LINEAR_CLAMP );
    }

    if( m_GlowResources.pCombinePS &&
        m_GlowResources.History.pShaderResourceView )
    {
        const f32 currentBlend = bMutantAccum ? motionBlend : 1.0f;
        const f32 historyBlend = bMutantAccum ? mutantTrailScale : 0.0f;

        rtarget_SetTargets( &m_GlowResources.Composite, 1, NULL );
        rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );

        ID3D11ShaderResourceView* pHistorySRV = m_GlowResources.History.pShaderResourceView;
        g_pd3dContext->PSSetShaderResources( 1, 1, &pHistorySRV );

        m_GlowResources.UpdateConstants( cutoff, currentBlend, historyBlend, 0.0f, 0.0f, 1.0f );
        composite_Blit( m_GlowResources.Accum, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pCombinePS, STATE_SAMPLER_LINEAR_CLAMP );

        ID3D11ShaderResourceView* pNullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( 1, 1, &pNullSRV );

        m_GlowResources.SetPendingResult( &m_GlowResources.Composite );
    }
    else
    {
        m_GlowResources.SetPendingResult( &m_GlowResources.Accum );
    }

    // Glow leaves an internal 1/8 HDR target bound; restore the final scene
    // target so any subsequent post effects in EndPostEffects render to the
    // lit scene color buffer.
    g_GBufferMgr.SetFinalColorTarget();
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
        ID3D11ShaderResourceView* pAuxSRV = NULL;
        const rtarget* pGlowMask = g_GBufferMgr.GetGBufferTarget( GBUFFER_GLOW );
        if( pGlowMask )
            pAuxSRV = pGlowMask->pShaderResourceView;

        if( g_pd3dContext )
            g_pd3dContext->PSSetShaderResources( 1, 1, &pAuxSRV );

        const f32 cutoff = (m_Glow.Cutoff >= 255) ? -0.5f : ((f32)m_Glow.Cutoff / 255.0f) - 0.5f;
        m_GlowResources.UpdateConstants( cutoff, 1.0f, 0.0f, 0.0f, 0.0f );
        composite_Blit( *pResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, m_GlowResources.pCompositePS, STATE_SAMPLER_LINEAR_CLAMP );

        if( g_pd3dContext )
        {
            ID3D11ShaderResourceView* pNullSRV = NULL;
            g_pd3dContext->PSSetShaderResources( 1, 1, &pNullSRV );
        }
    }
    else
    {
        composite_Blit( *pResult, COMPOSITE_BLEND_ADDITIVE, 1.0f, NULL, STATE_SAMPLER_LINEAR_CLAMP );
    }

    m_GlowResources.FinalizeComposite();
}
