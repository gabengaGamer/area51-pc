//==============================================================================
//  
//  x_time.hpp
//
//==============================================================================

#ifndef X_TIME_HPP
#define X_TIME_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_PLUS_HPP
#include "x_types.hpp"
#endif

//==============================================================================
//  TYPES
//==============================================================================

typedef s64 xtick;

class xtimer
{
protected:
    
    xtick   m_StartTime;
    xtick   m_TotalTime;
    xbool   m_Running;
    s32     m_NSamples;

public:

            xtimer          ( void );

    void    Start           ( void );
    void    Reset           ( void );

    xtick   Stop            ( void );
    f32     StopMs          ( void );
    f32     StopSec         ( void );

    xtick   Read            ( void ) const;
    f32     ReadMs          ( void ) const;
    f32     ReadSec         ( void ) const;

    xtick   Trip            ( void );   // Similar to Stop, Reset, Start
    f32     TripMs          ( void );
    f32     TripSec         ( void );

    s32     GetNSamples     ( void ) const;
    f32     GetAverageMs    ( void ) const;
    xbool   IsRunning       ( void ) const { return m_Running; } 
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

xtick   x_GetTime           ( void );
f64     x_GetTimeSec        ( void );

f32     x_TicksToMs         ( xtick Ticks );
f64     x_TicksToSec        ( xtick Ticks );

s64     x_GetTicksPerMs     ( void );
s64     x_GetTicksPerSecond ( void );

//==============================================================================
//  PRIVATE FUNCTIONS
//==============================================================================

void    x_TimeInit          ( void );
void    x_TimeKill          ( void );    

#ifdef X_DEBUG
void    x_TimeUpdateDebugVars( void );
#endif

//==============================================================================
//  INLINE IMPLEMENTATIONS
//==============================================================================

#include "Implementation/x_time_inline.hpp"

//==============================================================================
#endif // X_TIME_HPP
//==============================================================================