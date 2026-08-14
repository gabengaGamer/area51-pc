//=============================================================================
//
//  FrameRenderContext.hpp
//
//=============================================================================

#ifndef FRAME_RENDER_CONTEXT_HPP
#define FRAME_RENDER_CONTEXT_HPP

//=============================================================================
//  INCLUDES
//=============================================================================

#include "e_RenderTarget.hpp"

//=============================================================================

struct frame_render_targets
{
    rtarget const* pSceneColor;
    rtarget const* pSceneDepth;
    rtarget const* pGlow;
    xbool           IsTargetOverride;

    frame_render_targets ( void )
        : pSceneColor( NULL ), pSceneDepth( NULL ), pGlow( NULL ), IsTargetOverride( FALSE )
    {
    }
};

//=============================================================================
#endif // FRAME_RENDER_CONTEXT_HPP
//=============================================================================
