//==============================================================================
//  
//  x_time.cpp
//
//==============================================================================

#ifndef X_PLUS_HPP
#include "../x_time.hpp"
#endif

#ifndef X_DEBUG_HPP
#include "../x_debug.hpp"
#endif

#include <chrono>

//==============================================================================
//  TIME SOURCE
//==============================================================================

namespace
{
    using x_clock = std::chrono::steady_clock;

    // x_GetTime() is an elapsed-time API, not a wall-clock API. Keep its
    // representation identical on every desktop platform.
    constexpr s64 TICKS_PER_MS     = 1000;
    constexpr s64 TICKS_PER_SECOND = 1000000;
    constexpr xtick ONE_HOUR_TICKS = (xtick)TICKS_PER_SECOND * 60 * 60;
    constexpr xtick ONE_DAY_TICKS  = ONE_HOUR_TICKS * 24;

    x_clock::time_point g_BaseTime = x_clock::now();
}

//==============================================================================

void x_TimeInit( void )
{
    g_BaseTime = x_clock::now();

#ifdef X_DEBUG
    x_TimeUpdateDebugVars();
#endif
}

//==============================================================================

void x_TimeKill( void )
{
    g_BaseTime = x_clock::now();
}

//==============================================================================

xtick x_GetTime( void )
{
    const x_clock::duration Elapsed = x_clock::now() - g_BaseTime;
    return (xtick)std::chrono::duration_cast<std::chrono::microseconds>( Elapsed ).count();
}

//==============================================================================

// Debug variables for easier inspection
#ifdef X_DEBUG
xtick g_XTICKS_PER_MS   = 0;
xtick g_XTICKS_PER_DAY  = 0;
xtick g_XTICKS_PER_HOUR = 0;

void x_TimeUpdateDebugVars( void )
{
    g_XTICKS_PER_MS   = (xtick)TICKS_PER_MS;
    g_XTICKS_PER_DAY  = (xtick)ONE_DAY_TICKS;
    g_XTICKS_PER_HOUR = (xtick)ONE_HOUR_TICKS;
}
#endif

//==============================================================================

s64 x_GetTicksPerMs( void )
{
    return TICKS_PER_MS;
}

//==============================================================================

s64 x_GetTicksPerSecond( void )
{
    return TICKS_PER_SECOND;
}

//==============================================================================

f32 x_TicksToMs( xtick Ticks )
{
    #ifndef X_EDITOR
        ASSERT( Ticks < ONE_DAY_TICKS );
    #endif
    
    return (f32)((f64)Ticks / (f64)TICKS_PER_MS);
}

//==============================================================================

f64 x_TicksToSec( xtick Ticks )
{
    return ((f64)Ticks) / (f64)TICKS_PER_SECOND;
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
