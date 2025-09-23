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
#include "Entropy/D3DEngine/d3deng_rtarget.hpp"

//==============================================================================
//  TYPES
//==============================================================================

#if defined(TARGET_PC)
struct pip_render_target_pc
{
    rtarget ColorTarget;
    rtarget DepthTarget;
    s32     VRAMID;
    s32     Width;
    s32     Height;
    xbool   bValid;

    pip_render_target_pc()
    {
        x_memset( this, 0, sizeof(pip_render_target_pc) );
    }
};
#endif // defined(TARGET_PC)

//---------------------------------------------------------------------

struct render_context
{
    s32     LocalPlayerIndex;   //  0 -  3
    s32     NetPlayerSlot;      //  0 - 31
    u32     TeamBits;

    // Boolean Flags
    u32     m_bIsMutated;
    u32     m_bIsPipRender;

#if defined(TARGET_PC)
    pip_render_target_pc*  m_pActivePipTarget;
    xbool                  m_bPipTargetsActive;
#endif

    void    Set( s32    LocalPlayerIndex, 
                 s32    NetPlayerSlot,
                 u32    TeamBits, 
                 xbool  bIsMutated,
                 xbool  bIsPipRender );

    void    SetPipRender( xbool bIsPipRender );
	
#if defined(TARGET_PC)
    void    SetActivePipTarget   ( pip_render_target_pc* pTarget );
    pip_render_target_pc* GetActivePipTarget( void ) const;
    void    MarkPipTargetsActive ( xbool bActive );
    xbool   ArePipTargetsActive  ( void ) const;
#endif
};

//==============================================================================
//  STORAGE
//==============================================================================

extern render_context g_RenderContext;

//==============================================================================
#endif // RENDER_CONTEXT_HPP
//==============================================================================
