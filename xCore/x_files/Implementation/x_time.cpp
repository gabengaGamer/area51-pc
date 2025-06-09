//==============================================================================
//  
//  x_time.cpp
//
//  TODO - Make nice and inline happy for performance.
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

#define XTICKS_PER_MS       s_PCFrequency
#define XTICKS_PER_SECOND   s_PCFrequency2

static f64 s_PCFrequency;
static f64 s_PCFrequency2;
static xtick s_BaseTimeTick;

//==============================================================================

void x_TimeInit( void )
{    
    LARGE_INTEGER C;

    QueryPerformanceCounter( &C );

    LARGE_INTEGER F;
    QueryPerformanceFrequency( &F );
    s_PCFrequency  = (f64)F.QuadPart / 1000.0;
    s_PCFrequency2 = (f64)F.QuadPart;
    s_BaseTimeTick = (xtick)C.QuadPart;
}

//==============================================================================

void x_TimeKill( void )
{    
}

//==============================================================================

xtick x_GetTime( void )
{
    static xtick LastTicks = 0;
    
    xtick Ticks;
    LARGE_INTEGER C;

    QueryPerformanceCounter( &C );
    Ticks = (xtick)(C.QuadPart)-s_BaseTimeTick;

    if( Ticks < LastTicks )     
        Ticks = LastTicks + 1;

    LastTicks = Ticks;

    return( Ticks );
}

//==============================================================================

//==============================================================================
#endif // TARGET_PC
//******************************************************************************

//==============================================================================
//  PLATFORM INDEPENDENT CODE
//==============================================================================

#define ONE_HOUR ((s64)XTICKS_PER_MS * 1000 * 60 * 60)
#define ONE_DAY  ((s64)XTICKS_PER_MS * 1000 * 60 * 60 * 24)

//
// This is so we can see these values in the debugger
//

xtick s_XTICKS_PER_MS   = (xtick)XTICKS_PER_MS;
xtick s_XTICKS_PER_DAY  = (xtick)ONE_DAY;
xtick s_XTICKS_PER_HOUR = (xtick)ONE_HOUR;

//==============================================================================

s64 x_GetTicksPerMs ( void )
{
    return( (s64)XTICKS_PER_MS );
}

//==============================================================================

s64 x_GetTicksPerSecond ( void )
{
    return( (s64)XTICKS_PER_SECOND );
}

//==============================================================================

f32 x_TicksToMs( xtick Ticks )
{
    #ifndef X_EDITOR
        ASSERT( Ticks < ONE_DAY );
    #endif
    // We do the multiple casting here to try and preserve as much accuracy as possible
    //return( (f32)(     Ticks) / (f32)XTICKS_PER_MS );
    return f32(f64(Ticks)/f64(s_PCFrequency2)*1000);
}

//==============================================================================

f64 x_TicksToSec( xtick Ticks )
{
    return( ((f64)Ticks) / ((f64)(XTICKS_PER_MS * 1000)) );
}

//==============================================================================

f64 x_GetTimeSec( void )
{
    return( x_TicksToSec( x_GetTime() ) );
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

void xtimer::Start( void )
{
    //CONTEXT( "xtimer::Start" );
    if( !m_Running )
    {
        m_StartTime = x_GetTime();
        m_Running   = TRUE;        
        m_NSamples++;
    }
}

//==============================================================================

void xtimer::Reset( void )
{
    //CONTEXT( "xtimer::Reset" );
    m_Running   = FALSE;
    m_StartTime = 0;
    m_TotalTime = 0;
    m_NSamples  = 0;
}

//==============================================================================

xtick xtimer::Stop( void )
{
    //CONTEXT( "xtimer::Stop" );
    if( m_Running )
    {
        m_TotalTime += x_GetTime() - m_StartTime;
        m_Running    = FALSE;
    }
    return( m_TotalTime );
}

//==============================================================================

f32 xtimer::StopMs( void )
{
    //CONTEXT( "xtimer::StopMs" );
    if( m_Running )
    {
        m_TotalTime += x_GetTime() - m_StartTime;
        m_Running    = FALSE;
    }

    #ifndef X_EDITOR
        ASSERT( m_TotalTime < ONE_DAY );
    #endif
    return( (f32)(m_TotalTime) / (f32)XTICKS_PER_MS );
}

//==============================================================================

f32 xtimer::StopSec( void )
{
    //CONTEXT( "xtimer::StopSec" );
    if( m_Running )
    {
        m_TotalTime += x_GetTime() - m_StartTime;
        m_Running    = FALSE;
    }

    #ifndef X_EDITOR
        ASSERT( m_TotalTime < ONE_DAY );
    #endif
    return( (f32)(m_TotalTime) / ((f32)XTICKS_PER_MS * 1000.0f) );
}

//==============================================================================

xtick xtimer::Read( void ) const
{
    if( m_Running )  return( m_TotalTime + (x_GetTime() - m_StartTime) );
    else             return( m_TotalTime );
}

//==============================================================================

f32 xtimer::ReadMs( void ) const
{
    xtick Ticks;
    if( m_Running )  Ticks = m_TotalTime + (x_GetTime() - m_StartTime);
    else             Ticks = m_TotalTime;

    #ifndef X_EDITOR
        ASSERT( Ticks < ONE_DAY );
    #endif
    return( (f32)(Ticks) / (f32)XTICKS_PER_MS );
}

//==============================================================================

f32 xtimer::ReadSec( void ) const
{
    xtick Ticks;
    if( m_Running )  Ticks = m_TotalTime + (x_GetTime() - m_StartTime);
    else             Ticks = m_TotalTime;

    return( (f32)(Ticks) / ((f32)XTICKS_PER_MS * 1000.0f) );
}

//==============================================================================

xtick xtimer::Trip( void )
{
    xtick Ticks = 0;
    if( m_Running )
    {
        xtick Now = x_GetTime();
        Ticks = m_TotalTime + (Now - m_StartTime);
        m_TotalTime = 0;
        m_StartTime = Now;
        m_NSamples++;
    }
    return( Ticks );
}

//==============================================================================

f32 xtimer::TripMs( void )
{
    xtick Ticks = 0;
    if( m_Running )
    {
        xtick Now = x_GetTime();
        Ticks = m_TotalTime + (Now - m_StartTime);
        m_TotalTime = 0;
        m_StartTime = Now;
        m_NSamples++;
    }

    #ifndef X_EDITOR
        ASSERT( Ticks < ONE_DAY );
    #endif
    return( (f32)(Ticks) / (f32)XTICKS_PER_MS );
}

//==============================================================================

f32 xtimer::TripSec( void )
{
    xtick Ticks = 0;
    if( m_Running )
    {
        xtick Now = x_GetTime();
        Ticks = m_TotalTime + (Now - m_StartTime);
        m_TotalTime = 0;
        m_StartTime = Now;
        m_NSamples++;
    }

    #ifndef X_EDITOR
        ASSERT( Ticks < ONE_DAY );
    #endif
    return( (f32)(Ticks) / ((f32)XTICKS_PER_MS * 1000.0f) );
}

//==============================================================================

s32 xtimer::GetNSamples( void ) const
{
    return( m_NSamples );
}

//==============================================================================

f32 xtimer::GetAverageMs( void ) const
{
    if( m_NSamples <= 0 ) 
        return( 0 );

    return( ReadMs() / m_NSamples );
}

//==============================================================================
