//==============================================================================
//
//  RenderContext.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "RenderContext.hpp"

#if defined(TARGET_PC)
#include "Render\PC\GBufferMgr.hpp"
#include "Entropy\e_VRAM.hpp"
#endif

//==============================================================================
//  STORAGE
//==============================================================================

render_context g_RenderContext;

//==============================================================================
//  FUNCTIONS
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
    m_bIsPipRender   = bIsPipRender;
#if defined(TARGET_PC)
    m_pActivePipTarget = NULL;
    m_bPipTargetsActive = FALSE;
#endif	
}

//==============================================================================

void render_context::SetPipRender( xbool bIsPipRender )
{
    m_bIsPipRender = bIsPipRender;
#if defined(TARGET_PC)
    if( bIsPipRender == FALSE )
    {
        EndPipRender();
    }
#endif	
}

//==============================================================================

#if defined(TARGET_PC)

xbool pip_render_target_pc::Create( s32 TargetWidth, s32 TargetHeight )
{
    Destroy();

    Width  = TargetWidth;
    Height = TargetHeight;

    rtarget_desc colorDesc;
    x_memset( &colorDesc, 0, sizeof(colorDesc) );
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
    depthDesc.Format         = RTARGET_FORMAT_DEPTH24_STENCIL8;
    depthDesc.bBindAsTexture = FALSE;

    if( !rtarget_Create( DepthTarget, depthDesc ) )
    {
        Destroy();
        return FALSE;
    }

    VRAMID = vram_Register( ColorTarget.pTexture );
    if( !VRAMID )
    {
        Destroy();
        return FALSE;
    }

    bValid = TRUE;
    return TRUE;
}

//==============================================================================

void pip_render_target_pc::Destroy( void )
{
    if( VRAMID )
        vram_Unregister( VRAMID );

    if( ColorTarget.pTexture )
        rtarget_Destroy( ColorTarget );

    if( DepthTarget.pTexture )
        rtarget_Destroy( DepthTarget );

    x_memset( this, 0, sizeof(pip_render_target_pc) );
}

//==============================================================================

xbool render_context::BeginPipRender( pip_render_target_pc* pTarget )
{
    if( m_bPipTargetsActive || m_pActivePipTarget )
        EndPipRender();

    if( !pTarget || !pTarget->bValid )
        return FALSE;

    m_bIsPipRender      = TRUE;
    m_pActivePipTarget = pTarget;

    if( !rtarget_PushTargets() )
    {
        EndPipRender();
        return FALSE;
    }

    if( !rtarget_SetTargets( &pTarget->ColorTarget, 1, &pTarget->DepthTarget ) )
    {
        rtarget_PopTargets();
        EndPipRender();
        return FALSE;
    }

    const rtarget* pPipDepth = ( pTarget->DepthTarget.bIsDepthTarget &&
                                 pTarget->DepthTarget.pDepthStencilView )
                               ? &pTarget->DepthTarget : NULL;

    g_GBufferMgr.SetTargetOverride( &pTarget->ColorTarget, pPipDepth );

    f32 ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    rtarget_Clear( RTARGET_CLEAR_COLOR | RTARGET_CLEAR_DEPTH, ClearColor, 1.0f, 0 );

    m_bPipTargetsActive = TRUE;
    return TRUE;
}

//==============================================================================

void render_context::EndPipRender( void )
{
    if( m_bPipTargetsActive )
    {
        rtarget_PopTargets();
        m_bPipTargetsActive = FALSE;
    }

    g_GBufferMgr.SetTargetOverride( NULL, NULL );

    m_pActivePipTarget = NULL;
    m_bIsPipRender     = FALSE;
}

//==============================================================================

pip_render_target_pc* render_context::GetActivePipTarget( void ) const
{
    return m_pActivePipTarget;
}

//==============================================================================

xbool render_context::ArePipTargetsActive( void ) const
{
    return m_bPipTargetsActive;
}

#endif

//==============================================================================
