//==============================================================================
//
//  RenderContext.hpp
//
//==============================================================================

#ifndef RENDER_CONTEXT_HPP
#define RENDER_CONTEXT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "e_Engine.hpp"

//==============================================================================
//  TYPES
//==============================================================================

struct pip_render_target
{
    rtarget ColorTarget;
    rtarget DepthTarget;
    s32     Width;
    s32     Height;
    xbool   bValid;

    pip_render_target() :
        Width  ( 0 ),
        Height ( 0 ),
        bValid ( FALSE )
    {
    }

    xbool Create   ( s32 Width, s32 Height );
    void  Destroy  ( void );
};

//---------------------------------------------------------------------

struct render_context
{
    s32     LocalPlayerIndex;   //  0 -  3
    s32     NetPlayerSlot;      //  0 - 31
    u32     TeamBits;

    // Boolean Flags
    u32     m_bIsMutated;
    u32     m_bIsPipRender;

    xbool                  m_bPipTargetsActive;

    void    Set( s32    LocalPlayerIndex, 
                 s32    NetPlayerSlot,
                 u32    TeamBits, 
                 xbool  bIsMutated,
                 xbool  bIsPipRender );

    xbool   BeginPipRender       ( pip_render_target* pTarget );
    void    EndPipRender         ( void );
};

//==============================================================================
//  STORAGE
//==============================================================================

extern render_context g_RenderContext;

//==============================================================================
#endif // RENDER_CONTEXT_HPP
//==============================================================================
