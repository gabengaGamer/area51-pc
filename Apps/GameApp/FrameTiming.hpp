//==============================================================================
//
//  FrameTiming.hpp
//
//==============================================================================

#ifndef FRAME_TIMING_HPP
#define FRAME_TIMING_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files/x_time.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum class FrameTimingStatus
{
    Valid,
    InvalidValue,
    Hitch
};

//------------------------------------------------------------------------------

struct FrameTimingSample
{
    FrameTimingStatus Status;
    f32               RawDeltaSeconds;
    f32               AcceptedDeltaSeconds;
};

//==============================================================================
//  FRAME TIMING
//==============================================================================

class FrameTiming
{
public:
    static constexpr f32 DEFAULT_MAX_DELTA_SECONDS = 0.1f;

                        FrameTiming ( f32 MaxDeltaSeconds = DEFAULT_MAX_DELTA_SECONDS );

    void                Start       ( void );
    void                Restart     ( void );
    FrameTimingSample   Sample      ( void );

private:
    static FrameTimingSample Classify( f32 RawDeltaSeconds, f32 MaxDeltaSeconds );

    xtimer              m_Clock;
    f32                 m_MaxDeltaSeconds;
};

//==============================================================================
#endif // FRAME_TIMING_HPP
//==============================================================================
