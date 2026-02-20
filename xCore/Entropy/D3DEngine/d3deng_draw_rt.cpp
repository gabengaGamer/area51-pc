//==============================================================================
//
//  d3deng_draw_rt.cpp
//
//  Shared render target management for (draw) passes.
//
//==============================================================================

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

#include "e_Engine.hpp"
#include "d3deng_rtarget.hpp"
#include "d3deng_draw_rt.hpp"

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

static xbool draw_rt_CreateTarget( rtarget& Target, xbool& bValid, const char* pDebugName );

//==============================================================================
//  UI TARGET STATE
//==============================================================================

static rtarget  s_UITarget;
static xbool    s_bUITargetValid = FALSE;

static xbool draw_rt_CreateUITarget( void );
static void  draw_rt_DestroyUITarget( void );

//==============================================================================
//  PRIMITIVE TARGET STATE
//==============================================================================

static rtarget  s_PrimitiveTarget;
static xbool    s_bPrimitiveTargetValid = FALSE;

static xbool draw_rt_CreatePrimitiveTarget( void );
static void  draw_rt_DestroyPrimitiveTarget( void );

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
xbool draw_rt_CreateTarget( rtarget& Target, xbool& bValid, const char* pDebugName )
{
    if( !g_pd3dDevice )
        return FALSE;

    u32 prevWidth = Target.Desc.Width;
    u32 prevHeight = Target.Desc.Height;
    rtarget_format prevFormat = Target.Desc.Format;
    xbool bHadTexture = (Target.pTexture != NULL);

    rtarget_registration Reg;
    Reg.Policy = RTARGET_SIZE_RELATIVE_TO_VIEW;
    Reg.ScaleX = 1.0f;
    Reg.ScaleY = 1.0f;
    Reg.Format = RTARGET_FORMAT_RGBA8;
    Reg.SampleCount = 1;
    Reg.SampleQuality = 0;
    Reg.bBindAsTexture = TRUE;
    bValid = rtarget_GetOrCreate( Target, Reg );

    if( bValid )
    {
        if( !bHadTexture ||
            Target.Desc.Width != prevWidth ||
            Target.Desc.Height != prevHeight ||
            Target.Desc.Format != prevFormat )
        {
            x_DebugMsg( "Draw: %s target created %dx%d\n", pDebugName, Target.Desc.Width, Target.Desc.Height );
        }
        return TRUE;
    }

    x_DebugMsg( "Draw: Failed to create %s target\n", pDebugName );
    return FALSE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

void draw_rt_Init( void )
{
    s_bUITargetValid = FALSE;
    s_bPrimitiveTargetValid = FALSE;

    if( g_pd3dDevice )
    {
        draw_rt_CreateUITarget();
        draw_rt_CreatePrimitiveTarget();
    }
}

//==============================================================================

void draw_rt_Kill( void )
{
    draw_rt_DestroyUITarget();
    draw_rt_DestroyPrimitiveTarget();
}

//==============================================================================
//  UI TARGET
//==============================================================================

static
xbool draw_rt_CreateUITarget( void )
{
    return draw_rt_CreateTarget( s_UITarget, s_bUITargetValid, "UI" );
}

//==============================================================================

static
void draw_rt_DestroyUITarget( void )
{
    if( s_bUITargetValid )
    {
        rtarget_Unregister( s_UITarget );
        rtarget_Destroy( s_UITarget );
        s_bUITargetValid = FALSE;
    }
}

//==============================================================================

xbool draw_rt_BeginUI( void )
{
    if( !draw_rt_CreateUITarget() )
        return FALSE;

    rtarget_PushTargets();
    rtarget_SetTargets( &s_UITarget, 1, NULL );

    return TRUE;
}

//==============================================================================

void draw_rt_EndUI( void )
{
    if( s_bUITargetValid )
    {
        rtarget_PopTargets();
    }
}

//==============================================================================
//  PRIMITIVE RTARGET
//==============================================================================

static
xbool draw_rt_CreatePrimitiveTarget( void )
{
    return draw_rt_CreateTarget( s_PrimitiveTarget, s_bPrimitiveTargetValid, "Primitive" );
}

//==============================================================================

static
void draw_rt_DestroyPrimitiveTarget( void )
{
    if( s_bPrimitiveTargetValid )
    {
        rtarget_Unregister( s_PrimitiveTarget );
        rtarget_Destroy( s_PrimitiveTarget );
        s_bPrimitiveTargetValid = FALSE;
    }
}

//==============================================================================

xbool draw_rt_BeginPrimitive( void )
{
    if( !draw_rt_CreatePrimitiveTarget() )
        return FALSE;

    rtarget_PushTargets();
    rtarget_SetTargets( &s_PrimitiveTarget, 1, NULL );

    return TRUE;
}

//==============================================================================

void draw_rt_EndPrimitive( void )
{
    if( s_bPrimitiveTargetValid )
    {
        rtarget_PopTargets();
    }
}

//==============================================================================
//  UI RTARGET
//==============================================================================

const rtarget* draw_GetUITarget( void )
{
    return s_bUITargetValid ? &s_UITarget : NULL;
}

//==============================================================================

xbool draw_HasValidUITarget( void )
{
    return s_bUITargetValid;
}

//==============================================================================
//  PRIMITIVE RTARGET
//==============================================================================

const rtarget* draw_GetPrimitiveTarget( void )
{
    return s_bPrimitiveTargetValid ? &s_PrimitiveTarget : NULL;
}

//==============================================================================

xbool draw_HasValidPrimitiveTarget( void )
{
    return s_bPrimitiveTargetValid;
}

//==============================================================================