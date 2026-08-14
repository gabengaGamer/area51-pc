//==============================================================================
//
//  FramePacer.hpp
//
//==============================================================================

#ifndef FRAME_PACER_HPP
#define FRAME_PACER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "StateMgr/GlobalSettings.hpp"

//==============================================================================
//  FRAME PACER
//==============================================================================

class FramePacer
{
public:
                    FramePacer        ( void );

    void            Configure         ( FrameRateLimit Limit, eng_present_mode PresentMode );
    void            WaitForNextFrame  ( void );
    void            Reset             ( void );

private:
    static s32      GetFramesPerSecond( FrameRateLimit Limit );

    FrameRateLimit  m_Limit;
    f64             m_FramePeriodTicks;
    f64             m_NextDeadlineTicks;
    xbool           m_HasDeadline;
};

//==============================================================================
#endif // FRAME_PACER_HPP
//==============================================================================
