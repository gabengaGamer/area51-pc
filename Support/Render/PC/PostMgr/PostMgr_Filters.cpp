//==============================================================================
//
//  PostMgr_Filters.cpp
//
//  Screen-space post-processing module for the PC platform.
//
//==============================================================================

//==============================================================================
//  BASE INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "PostMgr.hpp"

//==============================================================================
//  FILE-LOCAL TYPES AND HELPERS
//==============================================================================

namespace
{
// Constants
static s32 const kMipRampSampleCount = 256;
static f32 const kPainBlurReferenceWidth = 256.0f;
static f32 const kPainBlurReferenceHeight = 192.0f;
static f32 const s_ClearColorTransparent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

// Constant buffer layout
struct PostFilterConstants
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
static void SetClearColor( f32 pDst[4], f32 const pSrc[4] )
{
    pDst[0] = pSrc[0];
    pDst[1] = pSrc[1];
    pDst[2] = pSrc[2];
    pDst[3] = pSrc[3];
}

static void ReleaseFilterTarget( rtarget& target )
{
    rtarget_EndPass();
    rtarget_Destroy( target );
    target = rtarget();
}

static xbool HasTextureAndSRV( rtarget const& target )
{
    return rtarget_HasTexture( target ) && rtarget_HasShaderResource( target );
}

static xbool IsFilterTargetValid( rtarget const& target, u32 width, u32 height, rtarget_format format )
{
    return rtarget_HasRenderTarget( target ) && rtarget_HasShaderResource( target ) && ( target.Desc.Width == width ) &&
           ( target.Desc.Height == height ) && ( target.Desc.Format == format );
}

static xbool CreateFilterTarget( rtarget& target, u32 width, u32 height, rtarget_format format, char const* pDebugName )
{
    if ( IsFilterTargetValid( target, width, height, format ) )
    {
        return TRUE;
    }

    rtarget_EndPass();
    rtarget_Destroy( target );

    rtarget_desc desc;
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.SampleCount = 1;
    desc.SampleQuality = 0;
    desc.bBindAsTexture = TRUE;
    desc.pDebugName = pDebugName;
    SetClearColor( desc.ClearColor, s_ClearColorTransparent );

    return rtarget_Create( target, desc );
}

static xbool EnsureFilterTarget( rtarget& target, rtarget const* pSourceTarget, char const* pDebugName )
{
    if ( !pSourceTarget || !pSourceTarget->Desc.Width || !pSourceTarget->Desc.Height )
    {
        return FALSE;
    }

    return CreateFilterTarget( target, pSourceTarget->Desc.Width, pSourceTarget->Desc.Height,
                               pSourceTarget->Desc.Format, pDebugName );
}

static xbool EnsureAbsoluteFilterTarget( rtarget& target, u32 width, u32 height, rtarget_format format,
                                         char const* pDebugName )
{
    if ( !width || !height )
    {
        return FALSE;
    }

    return CreateFilterTarget( target, width, height, format, pDebugName );
}

static rtarget_color_attachment_desc BuildColorAttachment( rtarget const* pTarget, rtarget_load_op loadOp )
{
    rtarget_color_attachment_desc color;
    color.pTarget = pTarget;
    color.LoadOp = loadOp;
    color.StoreOp = RTARGET_STORE_STORE;
    SetClearColor( color.ClearColor, s_ClearColorTransparent );
    return color;
}

static rtarget_depth_attachment_desc BuildDepthAttachment( rtarget const* pTarget, rtarget_load_op loadOp )
{
    rtarget_depth_attachment_desc depth;
    depth.pTarget        = pTarget;
    depth.DepthLoadOp    = loadOp;
    depth.DepthStoreOp   = RTARGET_STORE_STORE;
    depth.StencilLoadOp  = loadOp;
    depth.StencilStoreOp = RTARGET_STORE_STORE;
    depth.ClearDepth     = 1.0f;
    depth.ClearStencil   = 0;
    return depth;
}

static xbool BeginFilterColorPass( rtarget const* pTarget, rtarget_load_op loadOp )
{
    if ( !pTarget || !rtarget_HasRenderTarget( *pTarget ) )
    {
        return FALSE;
    }

    rtarget_color_attachment_desc color = BuildColorAttachment( pTarget, loadOp );

    rtarget_EndPass();
    return rtarget_BeginPass( &color, 1, NULL );
}

static xbool BeginFilterColorDepthPass( rtarget const* pTarget, rtarget const* pDepthTarget,
                                        rtarget_load_op colorLoadOp, rtarget_load_op depthLoadOp )
{
    if ( !pTarget || !rtarget_HasRenderTarget( *pTarget ) )
    {
        return FALSE;
    }

    rtarget_color_attachment_desc color = BuildColorAttachment( pTarget, colorLoadOp );

    rtarget_depth_attachment_desc        depth;
    rtarget_depth_attachment_desc const* pDepth = NULL;
    if ( pDepthTarget )
    {
        if ( !rtarget_HasDepthStencil( *pDepthTarget ) )
        {
            return FALSE;
        }

        depth = BuildDepthAttachment( pDepthTarget, depthLoadOp );
        pDepth = &depth;
    }

    rtarget_EndPass();
    return rtarget_BeginPass( &color, 1, pDepth );
}

static xbool CopyRenderTargetToTarget( rtarget& destination, rtarget const* pSourceTarget )
{
    if ( !pSourceTarget )
    {
        return FALSE;
    }

    rtarget_EndPass();
    return rtarget_Copy( destination, *pSourceTarget );
}

static rtarget const* GetCurrentPostTarget( void )
{
    rtarget const* pTarget = rtarget_GetCurrentTarget( 0 );
    if ( pTarget )
    {
        return pTarget;
    }

    frame_render_targets targets;
    return g_GBufferMgr.GetFrameTargets( targets ) ? targets.pSceneColor : NULL;
}

static xbool BindFilterConstants( shader const& shader, PostFilterConstants const& constants )
{
    return shader_PushUniformData( shader, SHADER_STAGE_PIXEL, "FilterParams", &constants, sizeof( constants ) );
}

static xbool BindFilterSampler( shader const& shader, char const* pName, shader_resource const* pResource,
                                rstate_sampler const& sampler )
{
    return shader_BindSampler( shader, SHADER_STAGE_PIXEL, pName, pResource, &sampler );
}

static void BuildFilterConstants( PostFilterConstants& constants, f32 motionIntensity, f32 zoom, radian angle,
                                  f32 alphaSub, f32 alphaScale, f32 centerU, f32 centerV )
{
    x_memset( &constants, 0, sizeof( constants ) );

    constants.Params0.Set( x_clamp( motionIntensity, 0.0f, 1.0f ), zoom, x_sin( angle ), x_cos( angle ) );
    constants.Params1.Set( alphaSub / 255.0f, alphaScale / 255.0f, 0.0f, 0.0f );
    constants.Params6.Set( centerU, centerV, 0.0f, 0.0f );
}

static void BuildScreenWarpConstants( PostFilterConstants& constants, PostScreenWarpParams const& screenWarp,
                                      u32 copyWidth, u32 copyHeight )
{
    x_memset( &constants, 0, sizeof( constants ) );

    constants.Params2.Set( 0.0f, ( copyWidth > 0 ) ? ( 1.0f / static_cast<f32>( copyWidth ) ) : 0.0f,
                           ( copyHeight > 0 ) ? ( 1.0f / static_cast<f32>( copyHeight ) ) : 0.0f, 0.0f );

    view const* pView = eng_GetView();
    if ( pView )
    {
        s32 warpCount = 0;
        for ( s32 i = 0; ( i < screenWarp.Count ) && ( warpCount < MAX_POST_SCREEN_WARPS ); ++i )
        {
            if ( pView->SphereInView( screenWarp.WorldPos[i], screenWarp.Radius[i], view::WORLD ) ==
                 view::VISIBLE_NONE )
            {
                continue;
            }

            vector3 const screenPos = pView->PointToScreen( screenWarp.WorldPos[i], view::WORLD );
            f32 const     screenRadius =
                pView->CalcScreenSize( screenWarp.WorldPos[i], screenWarp.Radius[i], view::WORLD ) * 0.5f;
            if ( screenRadius <= 0.0f )
            {
                continue;
            }

            constants.ScreenWarps[warpCount].Set( screenPos.GetX(), screenPos.GetY(), screenRadius,
                                                  MAX( screenWarp.Amount[i], 0.001f ) );
            warpCount++;
        }

        constants.Params2.GetX() = static_cast<f32>( warpCount );
    }
}
} // namespace

//==============================================================================
//  FILTER RESOURCE MANAGEMENT
//==============================================================================

PostMgr::FilterResources::FilterResources()
{
    History = rtarget();
    Post[0] = rtarget();
    Post[1] = rtarget();
    for ( s32 i = 0; i < MAX_POST_MIPS; ++i )
    {
        Mip[i] = rtarget();
    }
    Pain = rtarget();
    PainScratch = rtarget();
    ActiveSourceIndex    = 0;
    ActiveTargetIndex    = 1;
    bPostChainActive     = FALSE;
    MotionBlurPS         = shader();
    MipDownsamplePS      = shader();
    PainBlurPS           = shader();
    MipCompositePS       = shader();
    MipCompositeCustomPS = shader();
    RadialBlurPS         = shader();
    ScreenWarpPS         = shader();
    NoisePS              = shader();
    ScreenFadePS         = shader();
    MipRampTexture       = vram_texture();
    LinearSampler        = rstate_sampler();
    PointSampler         = rstate_sampler();
    CopyWidth            = 0;
    CopyHeight           = 0;
    CopyFormat           = RTARGET_FORMAT_COUNT;
    MipSourceWidth       = 0;
    MipSourceHeight      = 0;
    MipSourceFormat      = RTARGET_FORMAT_COUNT;
    bHistoryValid        = FALSE;
}

//==============================================================================

void PostMgr::FilterResources::Initialize( void )
{
    Shutdown();

    shader_LoadFromEcs( MotionBlurPS, "post_filters_motion_blur_ps.ps.ecs" );
    shader_LoadFromEcs( MipDownsamplePS, "post_filters_mip_downsample_ps.ps.ecs" );
    shader_LoadFromEcs( PainBlurPS, "post_filters_pain_blur_ps.ps.ecs" );
    shader_LoadFromEcs( MipCompositePS, "post_filters_mip_composite_ps.ps.ecs" );
    shader_LoadFromEcs( MipCompositeCustomPS, "post_filters_mip_composite_custom_ps.ps.ecs" );
    shader_LoadFromEcs( RadialBlurPS, "post_filters_radial_blur_ps.ps.ecs" );
    shader_LoadFromEcs( ScreenWarpPS, "post_filters_screen_warp_ps.ps.ecs" );
    shader_LoadFromEcs( NoisePS, "post_filters_noise_ps.ps.ecs" );
    shader_LoadFromEcs( ScreenFadePS, "post_filters_screen_fade_ps.ps.ecs" );
    rstate_CreateSampler( LinearSampler, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostFilterLinear" );
    rstate_CreateSampler( PointSampler, RSTATE_SAMPLER_PRESET_POINT_CLAMP, "PostFilterPoint" );

    if ( !MotionBlurPS || !MipDownsamplePS || !PainBlurPS || !MipCompositePS || !MipCompositeCustomPS ||
         !RadialBlurPS || !ScreenWarpPS || !NoisePS || !ScreenFadePS || !LinearSampler || !PointSampler )
    {
        x_DebugMsg( "PostMgr: WARNING - Failed to initialize filter resources\n" );
    }
}

//==============================================================================

void PostMgr::FilterResources::Shutdown( void )
{
    ReleaseFilterTarget( History );
    ReleaseFilterTarget( Post[0] );
    ReleaseFilterTarget( Post[1] );
    for ( s32 i = 0; i < MAX_POST_MIPS; ++i )
    {
        ReleaseFilterTarget( Mip[i] );
    }
    ReleaseFilterTarget( Pain );
    ReleaseFilterTarget( PainScratch );
    vram_DestroyTexture( MipRampTexture );
    rstate_DestroySampler( LinearSampler );
    rstate_DestroySampler( PointSampler );

    shader_Destroy( MotionBlurPS );
    shader_Destroy( MipDownsamplePS );
    shader_Destroy( PainBlurPS );
    shader_Destroy( MipCompositePS );
    shader_Destroy( MipCompositeCustomPS );
    shader_Destroy( RadialBlurPS );
    shader_Destroy( ScreenWarpPS );
    shader_Destroy( NoisePS );
    shader_Destroy( ScreenFadePS );

    CopyWidth = 0;
    CopyHeight = 0;
    CopyFormat = RTARGET_FORMAT_COUNT;
    MipSourceWidth = 0;
    MipSourceHeight = 0;
    MipSourceFormat = RTARGET_FORMAT_COUNT;
    bHistoryValid = FALSE;
    ResetPostChain();
}

//==============================================================================

xbool PostMgr::FilterResources::EnsureCopyTargets( rtarget const* pSourceTarget )
{
    if ( !pSourceTarget || !rtarget_HasTexture( *pSourceTarget ) )
    {
        return FALSE;
    }

    if ( HasTextureAndSRV( History ) && HasTextureAndSRV( Post[0] ) && HasTextureAndSRV( Post[1] ) &&
         ( CopyWidth == pSourceTarget->Desc.Width ) && ( CopyHeight == pSourceTarget->Desc.Height ) &&
         ( CopyFormat == pSourceTarget->Desc.Format ) )
    {
        return TRUE;
    }

    ReleaseFilterTarget( History );
    ReleaseFilterTarget( Post[0] );
    ReleaseFilterTarget( Post[1] );
    CopyWidth = 0;
    CopyHeight = 0;
    CopyFormat = RTARGET_FORMAT_COUNT;
    bHistoryValid = FALSE;
    ResetPostChain();

    if ( !EnsureFilterTarget( History, pSourceTarget, "PostFilterHistory" ) )
    {
        return FALSE;
    }

    if ( !EnsureFilterTarget( Post[0], pSourceTarget, "PostFilterPost0" ) )
    {
        ReleaseFilterTarget( History );
        return FALSE;
    }

    if ( !EnsureFilterTarget( Post[1], pSourceTarget, "PostFilterPost1" ) )
    {
        ReleaseFilterTarget( History );
        ReleaseFilterTarget( Post[0] );
        return FALSE;
    }

    CopyWidth = pSourceTarget->Desc.Width;
    CopyHeight = pSourceTarget->Desc.Height;
    CopyFormat = pSourceTarget->Desc.Format;
    return TRUE;
}

//==============================================================================

xbool PostMgr::FilterResources::EnsureMipTargets( rtarget const* pSourceTarget )
{
    if ( !pSourceTarget || !rtarget_HasTexture( *pSourceTarget ) )
    {
        return FALSE;
    }

    xbool bTargetsValid = TRUE;
    for ( s32 i = 0; i < MAX_POST_MIPS; ++i )
    {
        if ( !HasTextureAndSRV( Mip[i] ) )
        {
            bTargetsValid = FALSE;
            break;
        }
    }

    if ( bTargetsValid && ( MipSourceWidth == pSourceTarget->Desc.Width ) &&
         ( MipSourceHeight == pSourceTarget->Desc.Height ) && ( MipSourceFormat == pSourceTarget->Desc.Format ) )
    {
        return TRUE;
    }

    for ( s32 i = 0; i < MAX_POST_MIPS; ++i )
    {
        ReleaseFilterTarget( Mip[i] );
    }

    MipSourceWidth = 0;
    MipSourceHeight = 0;
    MipSourceFormat = RTARGET_FORMAT_COUNT;

    u32                width = MAX( ( pSourceTarget->Desc.Width * 2u + 2u ) / 5u, 1u );
    u32                height = MAX( ( pSourceTarget->Desc.Height * 2u + 2u ) / 5u, 1u );
    static char const* sMipTargetNames[MAX_POST_MIPS] = { "PostFilterMip0", "PostFilterMip1", "PostFilterMip2" };

    for ( s32 i = 0; i < MAX_POST_MIPS; ++i )
    {
        if ( !EnsureAbsoluteFilterTarget( Mip[i], width, height, pSourceTarget->Desc.Format, sMipTargetNames[i] ) )
        {
            for ( s32 j = 0; j <= i; ++j )
            {
                ReleaseFilterTarget( Mip[j] );
            }
            return FALSE;
        }

        width = MAX( width >> 1, 1u );
        height = MAX( height >> 1, 1u );
    }

    MipSourceWidth = pSourceTarget->Desc.Width;
    MipSourceHeight = pSourceTarget->Desc.Height;
    MipSourceFormat = pSourceTarget->Desc.Format;
    return TRUE;
}

//==============================================================================

xbool PostMgr::FilterResources::BeginPostChain( rtarget const* pSourceTarget )
{
    if ( !EnsureCopyTargets( pSourceTarget ) )
    {
        return FALSE;
    }

    if ( !CopyRenderTargetToTarget( Post[0], pSourceTarget ) )
    {
        return FALSE;
    }

    ActiveSourceIndex = 0;
    ActiveTargetIndex = 1;
    bPostChainActive = TRUE;
    return TRUE;
}

//==============================================================================

xbool PostMgr::FilterResources::CaptureHistory( rtarget const* pSourceTarget )
{
    if ( !EnsureCopyTargets( pSourceTarget ) )
    {
        return FALSE;
    }

    if ( !CopyRenderTargetToTarget( History, pSourceTarget ) )
    {
        return FALSE;
    }

    bHistoryValid = TRUE;
    return TRUE;
}

//==============================================================================

void PostMgr::FilterResources::InvalidateHistory( void )
{
    bHistoryValid = FALSE;
    ResetPostChain();
}

//==============================================================================

rtarget const* PostMgr::FilterResources::GetPostSource( void ) const
{
    if ( !bPostChainActive )
    {
        return NULL;
    }

    return &Post[ActiveSourceIndex];
}

//==============================================================================

rtarget const* PostMgr::FilterResources::GetMipTarget( s32 index ) const
{
    if ( ( index < 0 ) || ( index >= MAX_POST_MIPS ) )
    {
        return NULL;
    }

    if ( !HasTextureAndSRV( Mip[index] ) )
    {
        return NULL;
    }

    return &Mip[index];
}

//==============================================================================

xbool PostMgr::FilterResources::BindPostTarget( void )
{
    if ( !bPostChainActive )
    {
        return FALSE;
    }

    return BeginFilterColorPass( &Post[ActiveTargetIndex], RTARGET_LOAD_CLEAR );
}

//==============================================================================

xbool PostMgr::FilterResources::PrimePostTarget( void )
{
    rtarget const* pSource = GetPostSource();
    if ( !pSource || !BindPostTarget() )
    {
        return FALSE;
    }

    composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, NULL, RSTATE_SAMPLER_PRESET_POINT_CLAMP );
    return TRUE;
}

//==============================================================================

void PostMgr::FilterResources::SwapPostTargets( void )
{
    if ( !bPostChainActive )
    {
        return;
    }

    s32 index = ActiveSourceIndex;
    ActiveSourceIndex = ActiveTargetIndex;
    ActiveTargetIndex = index;
}

//==============================================================================

xbool PostMgr::FilterResources::ResolvePostChain( void )
{
    rtarget const* pSource = GetPostSource();
    if ( !pSource )
    {
        return FALSE;
    }

    composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, NULL, RSTATE_SAMPLER_PRESET_POINT_CLAMP );

    ResetPostChain();
    return TRUE;
}

//==============================================================================

void PostMgr::FilterResources::ResetPostChain( void )
{
    ActiveSourceIndex = 0;
    ActiveTargetIndex = 1;
    bPostChainActive = FALSE;
}

//==============================================================================

xbool PostMgr::FilterResources::IsPostChainActive( void ) const
{
    return bPostChainActive;
}

//==============================================================================

xbool PostMgr::FilterResources::UpdateMipRampTexture( xbitmap const* pBitmap )
{
    if ( !pBitmap )
    {
        return FALSE;
    }

    if ( !vram_IsValid( MipRampTexture ) )
    {
        vram_texture_desc desc;
        desc.Width = kMipRampSampleCount;
        desc.Height = 1;
        desc.Format = VRAM_TEXTURE_FORMAT_RGBA8;
        desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
        desc.pDebugName = "PostFilterMipRamp";

        if ( !vram_CreateTexture( MipRampTexture, desc ) )
        {
            return FALSE;
        }
    }

    s32 const width = pBitmap->GetWidth();
    s32 const height = pBitmap->GetHeight();
    s32 const count = width * height;
    if ( count <= 0 )
    {
        return FALSE;
    }

    u8 ramp[kMipRampSampleCount * 4];
    x_memset( ramp, 0, sizeof( ramp ) );

    for ( s32 i = 0; i < kMipRampSampleCount; ++i )
    {
        f32 const    sampleT = ( kMipRampSampleCount > 1 )
                                   ? ( static_cast<f32>( i ) / static_cast<f32>( kMipRampSampleCount - 1 ) )
                                   : 0.0f;
        s32 const    sourceIx = MIN( static_cast<s32>( sampleT * static_cast<f32>( count - 1 ) + 0.5f ), count - 1 );
        s32 const    sourceX = sourceIx % width;
        s32 const    sourceY = sourceIx / width;
        xcolor const color = pBitmap->GetPixelColor( sourceX, sourceY );

        ramp[i * 4 + 0] = color.R;
        ramp[i * 4 + 1] = color.G;
        ramp[i * 4 + 2] = color.B;
        ramp[i * 4 + 3] = color.A;
    }

    vram_texture_upload_desc upload;
    upload.Region.Width = kMipRampSampleCount;
    upload.Region.Height = 1;
    upload.Region.Depth = 1;
    upload.pData = ramp;
    upload.Size = kMipRampSampleCount * 4;
    upload.RowPitch = kMipRampSampleCount * 4;
    upload.SlicePitch = kMipRampSampleCount * 4;
    upload.bCycle = TRUE;

    return vram_UploadTexture( MipRampTexture, upload );
}

//==============================================================================

//  EFFECT IMPLEMENTATIONS
//==============================================================================

void PostMgr::ExecuteMotionBlur( void )
{
    if ( !m_FilterResources.MotionBlurPS || !m_FilterResources.bHistoryValid ||
         !rtarget_HasShaderResource( m_FilterResources.History ) )
    {
        return;
    }

    if ( !GetCurrentPostTarget() )
    {
        return;
    }

    PostFilterConstants constants;
    BuildFilterConstants( constants, m_motionBlur.Intensity, 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f );
    if ( !BindFilterConstants( m_FilterResources.MotionBlurPS, constants ) )
    {
        return;
    }

    composite_Blit( m_FilterResources.History, COMPOSITE_BLEND_ALPHA, 1.0f, &m_FilterResources.MotionBlurPS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostSource" );
}

//==============================================================================

void PostMgr::ExecuteRadialBlur( void )
{
    rtarget const* pSource = m_FilterResources.GetPostSource();
    if ( !m_FilterResources.RadialBlurPS || !pSource )
    {
        return;
    }

    BuildScreenMips( 1 );

    rtarget const* pBlurSource = m_FilterResources.GetMipTarget( 0 );
    if ( !pBlurSource || !rtarget_HasShaderResource( *pBlurSource ) )
    {
        pBlurSource = pSource;
    }

    if ( !m_FilterResources.PrimePostTarget() )
    {
        return;
    }

    f32 const centerU = ( pSource->Desc.Width > 0 )
                            ? x_clamp( ( static_cast<f32>( m_postViewL + m_postViewR ) * 0.5f ) /
                                           static_cast<f32>( pSource->Desc.Width ),
                                       0.0f, 1.0f )
                            : 0.5f;
    f32 const centerV = ( pSource->Desc.Height > 0 )
                            ? x_clamp( ( static_cast<f32>( m_postViewT + m_postViewB ) * 0.5f ) /
                                           static_cast<f32>( pSource->Desc.Height ),
                                       0.0f, 1.0f )
                            : 0.5f;

    PostFilterConstants constants;
    BuildFilterConstants( constants, 0.0f, m_radialBlur.Zoom, m_radialBlur.Angle, m_radialBlur.AlphaSub,
                          m_radialBlur.AlphaScale, centerU, centerV );
    if ( !BindFilterConstants( m_FilterResources.RadialBlurPS, constants ) )
    {
        return;
    }

    composite_Blit( *pBlurSource, COMPOSITE_BLEND_ALPHA, 1.0f, &m_FilterResources.RadialBlurPS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostSource" );

    BuildFilterConstants( constants, 0.0f, m_radialBlur.Zoom, -m_radialBlur.Angle, m_radialBlur.AlphaSub,
                          m_radialBlur.AlphaScale, centerU, centerV );
    if ( !BindFilterConstants( m_FilterResources.RadialBlurPS, constants ) )
    {
        return;
    }

    composite_Blit( *pBlurSource, COMPOSITE_BLEND_ALPHA, 1.0f, &m_FilterResources.RadialBlurPS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostSource" );

    m_FilterResources.SwapPostTargets();
}

//==============================================================================

void PostMgr::ExecuteScreenWarps( void )
{
    rtarget const* pSource = m_FilterResources.GetPostSource();
    if ( !m_FilterResources.ScreenWarpPS || !pSource || ( m_ScreenWarp.Count <= 0 ) )
    {
        return;
    }

    if ( !m_FilterResources.BindPostTarget() )
    {
        return;
    }

    PostFilterConstants constants;
    BuildScreenWarpConstants( constants, m_ScreenWarp, m_FilterResources.CopyWidth, m_FilterResources.CopyHeight );
    if ( !BindFilterConstants( m_FilterResources.ScreenWarpPS, constants ) )
    {
        return;
    }

    if ( !BindFilterSampler( m_FilterResources.ScreenWarpPS, "FilterSource1", rtarget_GetShaderResource( *pSource ),
                             m_FilterResources.LinearSampler ) )
    {
        return;
    }

    composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &m_FilterResources.ScreenWarpPS,
                    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostSource" );

    m_FilterResources.SwapPostTargets();
}

//==============================================================================

void PostMgr::ExecuteMipFilter( void )
{
    s32 const palette = m_mipFilter.PaletteIndex;
    if ( ( palette < 0 ) || ( palette >= 4 ) )
    {
        return;
    }

    s32 const count = MIN( m_mipFilter.Count[palette], MAX_POST_MIPS );
    if ( count <= 0 )
    {
        return;
    }

    rtarget const* pTarget = GetCurrentPostTarget();
    if ( !pTarget )
    {
        return;
    }

    if ( m_mipFilter.Fn[palette] == render::FALLOFF_CONSTANT )
    {
        f32 const offset = m_mipFilter.Offset[palette];
        if ( ( x_abs( offset ) < 0.001f ) || !m_FilterResources.MipDownsamplePS ||
             !m_FilterResources.PainBlurPS )
        {
            return;
        }

        u32 const painWidth = pTarget->Desc.Width;
        u32 const painHeight = pTarget->Desc.Height;
        if ( !EnsureAbsoluteFilterTarget( m_FilterResources.Pain, painWidth, painHeight, pTarget->Desc.Format,
                                          "PostFilterPain" ) ||
             !EnsureAbsoluteFilterTarget( m_FilterResources.PainScratch, painWidth, painHeight,
                                          pTarget->Desc.Format, "PostFilterPainScratch" ) ||
             !BeginFilterColorPass( &m_FilterResources.Pain, RTARGET_LOAD_CLEAR ) )
        {
            return;
        }

        PostFilterConstants downsampleConstants;
        x_memset( &downsampleConstants, 0, sizeof( downsampleConstants ) );
        downsampleConstants.Params5.Set( 0.0f, 0.0f, 1.0f / static_cast<f32>( pTarget->Desc.Width ),
                                         1.0f / static_cast<f32>( pTarget->Desc.Height ) );
        if ( !BindFilterConstants( m_FilterResources.MipDownsamplePS, downsampleConstants ) )
        {
            return;
        }

        composite_Blit( *pTarget, COMPOSITE_BLEND_COPY, 1.0f, &m_FilterResources.MipDownsamplePS,
                        RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostSource" );

        xcolor const& color = m_mipFilter.Color[palette];
        f32 const     blendWeight = static_cast<f32>( color.A / 2 ) / 255.0f;

        PostFilterConstants horizontalConstants;
        x_memset( &horizontalConstants, 0, sizeof( horizontalConstants ) );
        horizontalConstants.Params3.Set( color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, blendWeight );
        horizontalConstants.Params5.Set( 0.0f, 0.0f, offset / ( kPainBlurReferenceWidth * 8.0f ), 0.0f );
        horizontalConstants.Params6.Set( 0.0f, 0.0f, 0.0f, 0.0f );

        if ( !BeginFilterColorPass( &m_FilterResources.PainScratch, RTARGET_LOAD_CLEAR ) ||
             !BindFilterConstants( m_FilterResources.PainBlurPS, horizontalConstants ) )
        {
            return;
        }

        composite_Blit( m_FilterResources.Pain, COMPOSITE_BLEND_COPY, 1.0f, &m_FilterResources.PainBlurPS,
                        RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostSource" );

        PostFilterConstants verticalConstants;
        x_memset( &verticalConstants, 0, sizeof( verticalConstants ) );
        verticalConstants.Params3.Set( color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, blendWeight );
        verticalConstants.Params5.Set( 0.0f, 0.0f, 0.0f, offset / ( kPainBlurReferenceHeight * 8.0f ) );
        verticalConstants.Params6.Set( 1.0f, 0.0f, 0.0f, 0.0f );

        if ( !BeginFilterColorPass( pTarget, RTARGET_LOAD_LOAD ) ||
             !BindFilterConstants( m_FilterResources.PainBlurPS, verticalConstants ) )
        {
            return;
        }

        composite_Blit( m_FilterResources.PainScratch, COMPOSITE_BLEND_COPY, 1.0f,
                        &m_FilterResources.PainBlurPS,
                        RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostSource" );
        return;
    }

    rtarget const* pDepthTarget = rtarget_GetCurrentDepth();
    rtarget const* pNormalDepthTarget = g_GBufferMgr.GetGBufferTarget( GBufferTarget::NormalDepth );
    if ( !pNormalDepthTarget || !rtarget_HasShaderResource( *pNormalDepthTarget ) )
    {
        return;
    }

    BuildScreenMips( count );

    rtarget const* pMipTarget = m_FilterResources.GetMipTarget( count - 1 );
    if ( !pMipTarget || !rtarget_HasShaderResource( *pMipTarget ) )
    {
        return;
    }

    if ( !BeginFilterColorDepthPass( pTarget, pDepthTarget, RTARGET_LOAD_LOAD, RTARGET_LOAD_LOAD ) )
    {
        return;
    }

    xbool const   bUseCustom = ( m_mipFilter.Fn[palette] == render::FALLOFF_CUSTOM );
    shader const& pixelShader = bUseCustom ? m_FilterResources.MipCompositeCustomPS : m_FilterResources.MipCompositePS;
    if ( !pixelShader )
    {
        return;
    }

    if ( bUseCustom )
    {
        if ( !m_pMipTexture || !m_FilterResources.UpdateMipRampTexture( m_pMipTexture ) )
        {
            return;
        }
    }

    PostFilterConstants cbData;
    x_memset( &cbData, 0, sizeof( cbData ) );
    cbData.Params3.Set( m_mipFilter.Color[palette].R / 128.0f, m_mipFilter.Color[palette].G / 128.0f,
                        m_mipFilter.Color[palette].B / 128.0f, 0.0f );
    cbData.Params4.Set( static_cast<f32>( m_mipFilter.Fn[palette] ), m_mipFilter.Param1[palette],
                        m_mipFilter.Param2[palette], m_mipFilter.Offset[palette] );
    cbData.Params5.Set( m_postNearZ, m_postFarZ,
                        ( pTarget->Desc.Width > 0 ) ? ( 1.0f / static_cast<f32>( pTarget->Desc.Width ) ) : 0.0f,
                        ( pTarget->Desc.Height > 0 ) ? ( 1.0f / static_cast<f32>( pTarget->Desc.Height ) ) : 0.0f );
    cbData.Params6.Set( static_cast<f32>( count ), 0.0f, 0.0f, 0.0f );

    if ( !BindFilterConstants( pixelShader, cbData ) )
    {
        return;
    }

    if ( !BindFilterSampler( pixelShader, "FilterSource1", rtarget_GetShaderResource( *pNormalDepthTarget ),
                             m_FilterResources.PointSampler ) )
    {
        return;
    }

    if ( bUseCustom )
    {
        if ( !BindFilterSampler( pixelShader, "FilterRamp", vram_GetShaderResource( m_FilterResources.MipRampTexture ),
                                 m_FilterResources.LinearSampler ) )
        {
            return;
        }
    }

    composite_Blit( *pMipTarget, COMPOSITE_BLEND_ALPHA, 1.0f, &pixelShader, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP,
                    "PostSource" );
}

//==============================================================================

void PostMgr::ExecuteNoiseFilter( void )
{
    if ( !m_FilterResources.NoisePS )
    {
        return;
    }
    rtarget const* pNormalDepthTarget = g_GBufferMgr.GetGBufferTarget( GBufferTarget::NormalDepth );
    if ( !pNormalDepthTarget || !rtarget_HasShaderResource( *pNormalDepthTarget ) )
    {
        return;
    }

    PostFilterConstants cbData;
    x_memset( &cbData, 0, sizeof( cbData ) );

    f32 const aNorm = m_simple.NoiseColor.A / 255.0f;
    cbData.Params3.Set( m_simple.NoiseColor.R / 255.0f, m_simple.NoiseColor.G / 255.0f, m_simple.NoiseColor.B / 255.0f,
                        aNorm * aNorm * aNorm );

    cbData.Params6.Set( static_cast<f32>( x_rand() ), static_cast<f32>( x_rand() ), 0.0f, 0.0f );

    if ( !BindFilterConstants( m_FilterResources.NoisePS, cbData ) )
    {
        return;
    }

    composite_Blit( *pNormalDepthTarget, COMPOSITE_BLEND_ALPHA, 1.0f, &m_FilterResources.NoisePS,
                    RSTATE_SAMPLER_PRESET_POINT_CLAMP );
}

//==============================================================================

void PostMgr::ExecuteScreenFadeLate( void )
{
    if ( !m_Flags.DoScreenFade || !rtarget_IsBackBufferPassActive() )
    {
        return;
    }

    rtarget const* pSourceTarget = g_GBufferMgr.GetGBufferTarget( GBufferTarget::FinalColor );
    rtarget const* pBackBuffer   = rtarget_GetCurrentTarget( 0 );
    if ( !m_FilterResources.ScreenFadePS || !pSourceTarget || !pBackBuffer ||
         ( pSourceTarget == pBackBuffer ) || !rtarget_HasShaderResource( *pSourceTarget ) ||
         !rtarget_HasRenderTarget( *pBackBuffer ) )
    {
        return;
    }

    PostFilterConstants constants;
    x_memset( &constants, 0, sizeof( constants ) );
    constants.Params3.Set( m_simple.FadeColor.R / 255.0f, m_simple.FadeColor.G / 255.0f, m_simple.FadeColor.B / 255.0f,
                           m_simple.FadeColor.A / 255.0f );
    if ( !BindFilterConstants( m_FilterResources.ScreenFadePS, constants ) )
    {
        return;
    }

    rdraw_scissor viewScissor;
    viewScissor.X = m_postViewL;
    viewScissor.Y = m_postViewT;
    viewScissor.Width = m_postViewR - m_postViewL;
    viewScissor.Height = m_postViewB - m_postViewT;
    if ( !rdraw_SetScissor( viewScissor ) )
    {
        return;
    }

    composite_Blit( *pSourceTarget, COMPOSITE_BLEND_ALPHA, 1.0f, &m_FilterResources.ScreenFadePS,
                    RSTATE_SAMPLER_PRESET_POINT_CLAMP );

    rdraw_scissor fullScissor;
    fullScissor.X = 0;
    fullScissor.Y = 0;
    fullScissor.Width = static_cast<s32>( pBackBuffer->Desc.Width );
    fullScissor.Height = static_cast<s32>( pBackBuffer->Desc.Height );
    rdraw_SetScissor( fullScissor );
}

//==============================================================================
//  SUPPORT FUNCTIONS
//==============================================================================

void PostMgr::BuildMipPalette( render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2, s32 paletteIndex )
{
    if ( ( m_mipFilter.Fn[paletteIndex] == fn ) && ( m_mipFilter.Param1[paletteIndex] == param1 ) &&
         ( m_mipFilter.Param2[paletteIndex] == param2 ) && ( m_mipFilter.Color[paletteIndex] == color ) )
    {
        return;
    }

    m_mipFilter.Fn[paletteIndex] = fn;
    m_mipFilter.Param1[paletteIndex] = param1;
    m_mipFilter.Param2[paletteIndex] = param2;
    m_mipFilter.Color[paletteIndex] = color;
    m_mipFilter.PaletteIndex = paletteIndex;
}

//==============================================================================

void PostMgr::CopyBackBuffer( void )
{
    rtarget const* pTarget = GetCurrentPostTarget();
    if ( !pTarget )
    {
        return;
    }

    m_FilterResources.BeginPostChain( pTarget );
}

//==============================================================================

void PostMgr::BuildScreenMips( s32 nMips )
{
    if ( nMips <= 0 )
    {
        return;
    }

    rtarget const* pSourceTarget = GetCurrentPostTarget();
    if ( !pSourceTarget || !rtarget_HasShaderResource( *pSourceTarget ) )
    {
        return;
    }

    nMips = MIN( nMips, MAX_POST_MIPS );
    if ( !m_FilterResources.EnsureMipTargets( pSourceTarget ) )
    {
        return;
    }

    rtarget const* pMipSource = pSourceTarget;
    for ( s32 i = 0; i < nMips; ++i )
    {
        rtarget const* pMipTarget = m_FilterResources.GetMipTarget( i );
        if ( !BeginFilterColorPass( pMipTarget, RTARGET_LOAD_CLEAR ) )
        {
            return;
        }

        shader const* pDownsampleShader = NULL;
        if ( i == 0 )
        {
            PostFilterConstants constants;
            x_memset( &constants, 0, sizeof( constants ) );
            constants.Params5.Set( 0.0f, 0.0f, 1.0f / static_cast<f32>( pMipSource->Desc.Width ),
                                   1.0f / static_cast<f32>( pMipSource->Desc.Height ) );
            if ( !m_FilterResources.MipDownsamplePS ||
                 !BindFilterConstants( m_FilterResources.MipDownsamplePS, constants ) )
            {
                return;
            }
            pDownsampleShader = &m_FilterResources.MipDownsamplePS;
        }

        composite_Blit( *pMipSource, COMPOSITE_BLEND_COPY, 1.0f, pDownsampleShader,
                        RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostSource" );

        pMipSource = pMipTarget;
    }
}

//==============================================================================

void PostMgr::UpdateFilterHistoryBeforePresent( void )
{
    rtarget const* pTarget = GetCurrentPostTarget();
    if ( !pTarget )
    {
        return;
    }

    m_FilterResources.CaptureHistory( pTarget );
}
