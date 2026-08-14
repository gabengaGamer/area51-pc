//=========================================================================
//
//  CinemaPlayback.hpp
//
//=========================================================================

#ifndef CINEMA_PLAYBACK_HPP
#define CINEMA_PLAYBACK_HPP

#include "x_math.hpp"

enum class CinemaPlaybackFinishReason
{
    Completed,
    Deactivated,
    Error
};

class CinemaPlayback
{
public:

                    CinemaPlayback              ( void );

    void            Begin                       ( void );
    void            AdvanceTo                   ( f32 Time );
    void            Finish                      ( CinemaPlaybackFinishReason Reason );

    xbool           Crossed                     ( f32 MarkerTime ) const;
    xbool           IsPast                      ( f32 MarkerTime ) const;
    xbool           IsActive                    ( void ) const;
    xbool           IsDone                      ( void ) const;
    f32             GetPreviousTime             ( void ) const;
    f32             GetTime                     ( void ) const;
    CinemaPlaybackFinishReason GetFinishReason  ( void ) const;

private:

    f32                         m_PreviousTime;
    f32                         m_CurrentTime;
    CinemaPlaybackFinishReason  m_FinishReason;
    xbool                       m_IsActive;
    xbool                       m_IsDone;
    xbool                       m_HasAdvanced;
};

#endif // CINEMA_PLAYBACK_HPP
