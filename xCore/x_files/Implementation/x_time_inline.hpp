//==============================================================================
//  
//  x_time_inline.hpp
//  
//==============================================================================

#ifndef X_TIME_INLINE_HPP
#define X_TIME_INLINE_HPP

//==============================================================================
//  EXTERNAL DECLARATIONS
//==============================================================================

#ifdef TARGET_PC
extern f64     g_PCInvFrequency;
extern f64     g_PCInvFrequency2;
#endif

//==============================================================================
//  INLINE FUNCTION IMPLEMENTATIONS
//==============================================================================

//==============================================================================

inline void xtimer::Start( void )
{
    if( !m_Running )
    {
        m_StartTime = x_GetTime();
        m_Running   = TRUE;        
        m_NSamples++;
    }
}

//==============================================================================

inline void xtimer::Reset( void )
{
    m_Running   = FALSE;
    m_StartTime = 0;
    m_TotalTime = 0;
    m_NSamples  = 0;
}

//==============================================================================

inline xtick xtimer::Stop( void )
{
    if( m_Running )
    {
        m_TotalTime += x_GetTime() - m_StartTime;
        m_Running    = FALSE;
    }
    return m_TotalTime;
}

//==============================================================================

inline f32 xtimer::StopMs( void )
{
    if( m_Running )
    {
        m_TotalTime += x_GetTime() - m_StartTime;
        m_Running    = FALSE;
    }

    return x_TicksToMs( m_TotalTime );
}

//==============================================================================

inline f32 xtimer::StopSec( void )
{
    if( m_Running )
    {
        m_TotalTime += x_GetTime() - m_StartTime;
        m_Running    = FALSE;
    }

    return (f32)x_TicksToSec( m_TotalTime );
}

//==============================================================================

inline xtick xtimer::Read( void ) const
{
    if( m_Running )  
        return m_TotalTime + (x_GetTime() - m_StartTime);
    else             
        return m_TotalTime;
}

//==============================================================================

inline f32 xtimer::ReadMs( void ) const
{
    xtick Ticks;
    if( m_Running )  
        Ticks = m_TotalTime + (x_GetTime() - m_StartTime);
    else             
        Ticks = m_TotalTime;

    return x_TicksToMs( Ticks );
}

//==============================================================================

inline f32 xtimer::ReadSec( void ) const
{
    xtick Ticks;
    if( m_Running )  
        Ticks = m_TotalTime + (x_GetTime() - m_StartTime);
    else             
        Ticks = m_TotalTime;

    return (f32)x_TicksToSec( Ticks );
}

//==============================================================================

inline xtick xtimer::Trip( void )
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
    return Ticks;
}

//==============================================================================

inline f32 xtimer::TripMs( void )
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

    return x_TicksToMs( Ticks );
}

//==============================================================================

inline f32 xtimer::TripSec( void )
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

    return (f32)x_TicksToSec( Ticks );
}

//==============================================================================

inline s32 xtimer::GetNSamples( void ) const
{
    return m_NSamples;
}

//==============================================================================

inline f32 xtimer::GetAverageMs( void ) const
{
    if( m_NSamples <= 0 ) 
        return 0.0f;

    return ReadMs() / (f32)m_NSamples;
}

//==============================================================================
#endif // X_TIME_INLINE_HPP
//==============================================================================