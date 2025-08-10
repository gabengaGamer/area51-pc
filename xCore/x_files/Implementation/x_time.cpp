//==============================================================================
//  
//  x_time.cpp
//
//==============================================================================

#include "..\x_context.hpp"

#ifndef X_PLUS_HPP
#include "..\x_time.hpp"
#endif

#ifndef X_DEBUG_HPP
#include "..\x_debug.hpp"
#endif

//==============================================================================
//  PLATFORM SPECIFIC CODE
//==============================================================================
//
//  Must provide the following:
//    - Functions x_TimeInit(), x_TimeKill(), and x_GetTime().
//    - Definition for XTICKS_PER_MS which is one millisecond in xticks.
//  
//==============================================================================

//******************************************************************************
#ifdef TARGET_PC
//==============================================================================

#if defined(TARGET_PC)
#include <windows.h>
#else
#ifdef CONFIG_RETAIL
#   define D3DCOMPILE_PUREDEVICE 1    
#endif
#include <xtl.h>
#endif

// Global variables for time system
f64     g_PCFrequency     = 0.0;
f64     g_PCFrequency2    = 0.0;
f64     g_PCInvFrequency  = 0.0;        // Inverse frequency for faster division
f64     g_PCInvFrequency2 = 0.0;        // Inverse frequency for seconds
xtick   g_BaseTimeTick    = 0;
xtick   g_LastTicks       = 0;

#define XTICKS_PER_MS       g_PCFrequency
#define XTICKS_PER_SECOND   g_PCFrequency2

//==============================================================================

void x_TimeInit( void )
{    
    LARGE_INTEGER F, C;

    // Get frequency first
    QueryPerformanceFrequency( &F );
    g_PCFrequency2 = (f64)F.QuadPart;
    g_PCFrequency  = g_PCFrequency2 / 1000.0;
    
    // Pre-calculate inverse frequencies for faster division
    g_PCInvFrequency  = 1000.0 / g_PCFrequency2;
    g_PCInvFrequency2 = 1.0 / g_PCFrequency2;
    
    // Get base time
    QueryPerformanceCounter( &C );
    g_BaseTimeTick = (xtick)C.QuadPart;
    g_LastTicks = 0;

#ifdef X_DEBUG
    x_TimeUpdateDebugVars();
#endif
}

//==============================================================================

void x_TimeKill( void )
{    
    // Reset all global variables
    g_PCFrequency     = 0.0;
    g_PCFrequency2    = 0.0;
    g_PCInvFrequency  = 0.0;
    g_PCInvFrequency2 = 0.0;
    g_BaseTimeTick    = 0;
    g_LastTicks       = 0;
}

//==============================================================================

xtick x_GetTime( void )
{
    LARGE_INTEGER C;
    QueryPerformanceCounter( &C );
    
    xtick Ticks = (xtick)(C.QuadPart) - g_BaseTimeTick;

    // Ensure monotonic time progression
    if( Ticks < g_LastTicks )     
        Ticks = g_LastTicks + 1;

    g_LastTicks = Ticks;
    return Ticks;
}

//==============================================================================
#endif // TARGET_PC
//******************************************************************************

//==============================================================================
//  PLATFORM INDEPENDENT CODE
//==============================================================================

#define ONE_HOUR_TICKS ((s64)XTICKS_PER_MS * 1000 * 60 * 60)
#define ONE_DAY_TICKS  ((s64)XTICKS_PER_MS * 1000 * 60 * 60 * 24)

// Debug variables for easier inspection
#ifdef X_DEBUG
xtick g_XTICKS_PER_MS   = 0;
xtick g_XTICKS_PER_DAY  = 0;
xtick g_XTICKS_PER_HOUR = 0;

void x_TimeUpdateDebugVars( void )
{
    g_XTICKS_PER_MS   = (xtick)XTICKS_PER_MS;
    g_XTICKS_PER_DAY  = (xtick)ONE_DAY_TICKS;
    g_XTICKS_PER_HOUR = (xtick)ONE_HOUR_TICKS;
}
#endif

//==============================================================================

s64 x_GetTicksPerMs( void )
{
    return (s64)XTICKS_PER_MS;
}

//==============================================================================

s64 x_GetTicksPerSecond( void )
{
    return (s64)XTICKS_PER_SECOND;
}

//==============================================================================

f32 x_TicksToMs( xtick Ticks )
{
    #ifndef X_EDITOR
        ASSERT( Ticks < ONE_DAY_TICKS );
    #endif
    
    #ifdef TARGET_PC
        // Use pre-calculated inverse frequency for faster conversion
        return (f32)((f64)Ticks * g_PCInvFrequency);
    #else
        return (f32)((f64)Ticks / (f64)XTICKS_PER_MS);
    #endif
}

//==============================================================================

f64 x_TicksToSec( xtick Ticks )
{
    #ifdef TARGET_PC
        // Use pre-calculated inverse frequency for faster conversion
        return (f64)Ticks * g_PCInvFrequency2;
    #else
        return ((f64)Ticks) / ((f64)(XTICKS_PER_MS * 1000));
    #endif
}

//==============================================================================

f64 x_GetTimeSec( void )
{
    return x_TicksToSec( x_GetTime() );
}

//==============================================================================

xtimer::xtimer( void )
{
    m_Running   = FALSE;
    m_StartTime = 0;
    m_TotalTime = 0;
    m_NSamples  = 0;
}

//==============================================================================