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
static xbool draw_rt_ValidateTarget( rtarget& Target, xbool& bValid, const char* pDebugName );

//==============================================================================
//  UI TARGET STATE
//==============================================================================

static rtarget  s_UITarget;
static xbool    s_bUITargetValid = FALSE;

static xbool draw_rt_CreateUITarget( void );
static xbool draw_rt_ValidateUITarget( void );
static void  draw_rt_DestroyUITarget( void );

//==============================================================================
//  PRIMITIVE TARGET STATE
//==============================================================================

static rtarget  s_PrimitiveTarget;
static xbool    s_bPrimitiveTargetValid = FALSE;

static xbool draw_rt_CreatePrimitiveTarget( void );
static xbool draw_rt_ValidatePrimitiveTarget( void );
static void  draw_rt_DestroyPrimitiveTarget( void );

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
xbool draw_rt_CreateTarget( rtarget& Target, xbool& bValid, const char* pDebugName )
{
    if( !g_pd3dDevice )
        return FALSE;

    // Get current resolution
    s32 xRes, yRes;
    eng_GetRes( xRes, yRes );

    // Validate resolution
    if( xRes <= 0 || yRes <= 0 )
    {
        x_DebugMsg( "Draw: Invalid resolution %dx%d for %s target\n", xRes, yRes, pDebugName );
        return FALSE;
    }

    // Clean up existing target
    if( bValid )
    {
        rtarget_Destroy( Target );
        bValid = FALSE;
    }

    // Create render target with current resolution
    rtarget_desc Desc;
    Desc.Width = xRes;
    Desc.Height = yRes;
    Desc.Format = RTARGET_FORMAT_RGBA8;
    Desc.SampleCount = 1;
    Desc.SampleQuality = 0;
    Desc.bBindAsTexture = TRUE;

    bValid = rtarget_Create( Target, Desc );

    if( bValid )
    {
        x_DebugMsg( "Draw: %s target created with resolution %dx%d\n", pDebugName, xRes, yRes );
    }
    else
    {
        x_DebugMsg( "Draw: Failed to create %s target with resolution %dx%d\n", pDebugName, xRes, yRes );
    }

    return bValid;
}

//==============================================================================

static
xbool draw_rt_ValidateTarget( rtarget& Target, xbool& bValid, const char* pDebugName )
{
    if( !bValid )
    {
        return draw_rt_CreateTarget( Target, bValid, pDebugName );
    }

    s32 xRes, yRes;
    eng_GetRes( xRes, yRes );

    if( Target.Desc.Width != (u32)xRes || Target.Desc.Height != (u32)yRes )
    {
        x_DebugMsg( "Draw: %s target resolution changed from %dx%d to %dx%d, recreating\n",
                    pDebugName,
                    Target.Desc.Width,
                    Target.Desc.Height,
                    xRes,
                    yRes );
        return draw_rt_CreateTarget( Target, bValid, pDebugName );
    }

    return TRUE;
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
xbool draw_rt_ValidateUITarget( void )
{
    return draw_rt_ValidateTarget( s_UITarget, s_bUITargetValid, "UI" );
}

//==============================================================================

static
void draw_rt_DestroyUITarget( void )
{
    if( s_bUITargetValid )
    {
        rtarget_Destroy( s_UITarget );
        s_bUITargetValid = FALSE;
    }
}

//==============================================================================

xbool draw_rt_BeginUI( void )
{
    if( !draw_rt_ValidateUITarget() )
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
xbool draw_rt_ValidatePrimitiveTarget( void )
{
    return draw_rt_ValidateTarget( s_PrimitiveTarget, s_bPrimitiveTargetValid, "Primitive" );
}

//==============================================================================

static
void draw_rt_DestroyPrimitiveTarget( void )
{
    if( s_bPrimitiveTargetValid )
    {
        rtarget_Destroy( s_PrimitiveTarget );
        s_bPrimitiveTargetValid = FALSE;
    }
}

//==============================================================================

xbool draw_rt_BeginPrimitive( void )
{
    if( !draw_rt_ValidatePrimitiveTarget() )
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