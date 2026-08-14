//==============================================================================
//
//  FrameTiming.cpp
//
//==============================================================================

#include "FrameTiming.hpp"
#include "x_files/x_math.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

FrameTiming::FrameTiming( f32 MaxDeltaSeconds )
:   m_MaxDeltaSeconds( MaxDeltaSeconds )
{
    ASSERT( x_isvalid( MaxDeltaSeconds ) );
    ASSERT( MaxDeltaSeconds > 0.0f );
}

//==============================================================================

void FrameTiming::Start( void )
{
    m_Clock.Start();
}

//==============================================================================

void FrameTiming::Restart( void )
{
    m_Clock.Reset();
    m_Clock.Start();
}

//==============================================================================

FrameTimingSample FrameTiming::Sample( void )
{
    ASSERT( m_Clock.IsRunning() );
    return Classify( m_Clock.TripSec(), m_MaxDeltaSeconds );
}

//==============================================================================

FrameTimingSample FrameTiming::Classify( f32 RawDeltaSeconds, f32 MaxDeltaSeconds )
{
    FrameTimingSample Sample;
    Sample.Status                = FrameTimingStatus::InvalidValue;
    Sample.RawDeltaSeconds       = RawDeltaSeconds;
    Sample.AcceptedDeltaSeconds = 0.0f;

    if( !x_isvalid( RawDeltaSeconds ) || (RawDeltaSeconds <= 0.0f) )
    {
        return Sample;
    }

    if( RawDeltaSeconds > MaxDeltaSeconds )
    {
        Sample.Status = FrameTimingStatus::Hitch;
        return Sample;
    }

    Sample.Status                = FrameTimingStatus::Valid;
    Sample.AcceptedDeltaSeconds = RawDeltaSeconds;
    return Sample;
}

//==============================================================================
