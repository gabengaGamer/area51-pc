//==============================================================================
//
//  d3deng_draw_rt.hpp
//
//  Shared render target management for (draw) passes.
//
//==============================================================================

#ifndef D3DENG_DRAW_RT_HPP
#define D3DENG_DRAW_RT_HPP

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"

struct rtarget;

//==============================================================================
//  LIFETIME
//==============================================================================

void            draw_rt_Init( void );
void            draw_rt_Kill( void );

//==============================================================================
//  UI RTARGET
//==============================================================================

xbool           draw_rt_BeginUI( void );
void            draw_rt_EndUI( void );
const rtarget*  draw_GetUITarget( void );
xbool           draw_HasValidUITarget( void );

//==============================================================================
//  PRIMITIVE RTARGET
//==============================================================================

xbool           draw_rt_BeginPrimitive( void );
void            draw_rt_EndPrimitive( void );
const rtarget*  draw_GetPrimitiveTarget( void );
xbool           draw_HasValidPrimitiveTarget( void );

#endif // D3DENG_DRAW_RT_HPP