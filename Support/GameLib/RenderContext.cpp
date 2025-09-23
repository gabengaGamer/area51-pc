//==============================================================================
//
//  RenderContext.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "RenderContext.hpp"

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
        m_pActivePipTarget  = NULL;
        m_bPipTargetsActive = FALSE;
    }
#endif	
}

//==============================================================================

#if defined(TARGET_PC)

void render_context::SetActivePipTarget( pip_render_target_pc* pTarget )
{
    m_pActivePipTarget = pTarget;
}

//==============================================================================

pip_render_target_pc* render_context::GetActivePipTarget( void ) const
{
    return m_pActivePipTarget;
}

//==============================================================================

void render_context::MarkPipTargetsActive( xbool bActive )
{
    m_bPipTargetsActive = bActive;
}

//==============================================================================

xbool render_context::ArePipTargetsActive( void ) const
{
    return m_bPipTargetsActive;
}

#endif

//==============================================================================
