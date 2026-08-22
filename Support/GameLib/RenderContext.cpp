//==============================================================================
//
//  RenderContext.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "RenderContext.hpp"

#include "Render/PC/GBufferMgr.hpp"

//==============================================================================
//  STORAGE
//==============================================================================

render_context g_RenderContext;

//==============================================================================
//  FUNCTIONS
//==============================================================================

static
xbool BeginPipPass( pip_render_target& Target )
{
    static const f32 ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    if( !rtarget_HasRenderTarget( Target.ColorTarget ) ||
        !rtarget_HasDepthStencil( Target.DepthTarget ) )
    {
        return FALSE;
    }

    rtarget_color_attachment_desc Color;
    Color.pTarget       = &Target.ColorTarget;
    Color.LoadOp        = RTARGET_LOAD_CLEAR;
    Color.StoreOp       = RTARGET_STORE_STORE;
    Color.ClearColor[0] = ClearColor[0];
    Color.ClearColor[1] = ClearColor[1];
    Color.ClearColor[2] = ClearColor[2];
    Color.ClearColor[3] = ClearColor[3];

    rtarget_depth_attachment_desc Depth;
    Depth.pTarget        = &Target.DepthTarget;
    Depth.DepthLoadOp    = RTARGET_LOAD_CLEAR;
    Depth.DepthStoreOp   = RTARGET_STORE_STORE;
    Depth.StencilLoadOp  = RTARGET_LOAD_CLEAR;
    Depth.StencilStoreOp = RTARGET_STORE_STORE;
    Depth.ClearDepth     = 1.0f;
    Depth.ClearStencil   = 0;

    rtarget_EndPass();
    return rtarget_BeginPass( &Color, 1, &Depth );
}

//==============================================================================

void render_context::Set( s32   aLocalPlayerIndex, 
                          s32   aNetPlayerSlot,
                          u32   aTeamBits, 
                          xbool bIsMutated,
                          xbool bIsPipRender )
{
    LocalPlayerIndex = aLocalPlayerIndex;
    NetPlayerSlot    = aNetPlayerSlot;
    TeamBits         = aTeamBits;
    m_bIsMutated     = bIsMutated;
    if( m_bPipTargetsActive )
    {
        EndPipRender();
    }

    m_bPipTargetsActive = FALSE;
    m_bIsPipRender      = bIsPipRender;
}

//==============================================================================

xbool pip_render_target::Create( s32 TargetWidth, s32 TargetHeight )
{
    Destroy();

    Width  = TargetWidth;
    Height = TargetHeight;

    rtarget_desc colorDesc;
    colorDesc.Width          = (u32)Width;
    colorDesc.Height         = (u32)Height;
    colorDesc.Format         = RTARGET_FORMAT_RGBA8;
    colorDesc.SampleCount    = 1;
    colorDesc.SampleQuality  = 0;
    colorDesc.bBindAsTexture = TRUE;

    if( !rtarget_Create( ColorTarget, colorDesc ) )
    {
        Destroy();
        return FALSE;
    }

    rtarget_desc depthDesc = colorDesc;
    depthDesc.Format         = RTARGET_FORMAT_DEPTH_STENCIL;
    depthDesc.bBindAsTexture = FALSE;

    if( !rtarget_Create( DepthTarget, depthDesc ) )
    {
        Destroy();
        return FALSE;
    }

    bValid = TRUE;
    return TRUE;
}

//==============================================================================

void pip_render_target::Destroy( void )
{
    if( rtarget_HasTexture( ColorTarget ) )
        rtarget_Destroy( ColorTarget );

    if( rtarget_HasTexture( DepthTarget ) )
        rtarget_Destroy( DepthTarget );

    Width  = 0;
    Height = 0;
    bValid = FALSE;
}

//==============================================================================

xbool render_context::BeginPipRender( pip_render_target* pTarget )
{
    if( m_bPipTargetsActive )
        EndPipRender();

    if( !pTarget || !pTarget->bValid )
        return FALSE;

    m_bIsPipRender      = TRUE;

    if( !BeginPipPass( *pTarget ) )
    {
        EndPipRender();
        return FALSE;
    }

    const rtarget* pPipDepth = ( pTarget->DepthTarget.bIsDepthTarget &&
                                 rtarget_HasDepthStencil( pTarget->DepthTarget ) )
                               ? &pTarget->DepthTarget : NULL;

    g_GBufferMgr.SetTargetOverride( &pTarget->ColorTarget, pPipDepth );

    m_bPipTargetsActive = TRUE;
    return TRUE;
}

//==============================================================================

void render_context::EndPipRender( void )
{
    if( m_bPipTargetsActive )
    {
        rtarget_EndPass();
        m_bPipTargetsActive = FALSE;
    }

    g_GBufferMgr.SetTargetOverride( NULL, NULL );

    m_bIsPipRender     = FALSE;
}

//==============================================================================
