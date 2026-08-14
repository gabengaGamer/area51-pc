//=========================================================================
//
//  CinemaPlayback.cpp
//
//=========================================================================

#include "CinemaPlayback.hpp"

CinemaPlayback::CinemaPlayback( void ) :
    m_PreviousTime ( 0.0f ),
    m_CurrentTime  ( 0.0f ),
    m_FinishReason ( CinemaPlaybackFinishReason::Completed ),
    m_IsActive     ( FALSE ),
    m_IsDone       ( FALSE ),
    m_HasAdvanced  ( FALSE )
{
}

//=========================================================================

void CinemaPlayback::Begin( void )
{
    m_PreviousTime = -F32_MAX;
    m_CurrentTime  = 0.0f;
    m_FinishReason = CinemaPlaybackFinishReason::Completed;
    m_IsActive     = TRUE;
    m_IsDone       = FALSE;
    m_HasAdvanced  = FALSE;
}

//=========================================================================

void CinemaPlayback::AdvanceTo( f32 Time )
{
    if( !m_IsActive )
    {
        return;
    }

    if( m_HasAdvanced )
    {
        m_PreviousTime = m_CurrentTime;
    }
    m_CurrentTime  = MAX( m_CurrentTime, MAX( 0.0f, Time ) );
    m_HasAdvanced  = TRUE;
}

//=========================================================================

void CinemaPlayback::Finish( CinemaPlaybackFinishReason Reason )
{
    m_FinishReason = Reason;
    m_IsActive     = FALSE;
    m_IsDone       = TRUE;
}

//=========================================================================

xbool CinemaPlayback::Crossed( f32 MarkerTime ) const
{
    return m_IsActive &&
           (MarkerTime > m_PreviousTime) &&
           (MarkerTime <= m_CurrentTime);
}

//=========================================================================

xbool CinemaPlayback::IsPast( f32 MarkerTime ) const
{
    return m_IsDone || (MarkerTime <= m_CurrentTime);
}

//=========================================================================

xbool CinemaPlayback::IsActive( void ) const
{
    return m_IsActive;
}

//=========================================================================

xbool CinemaPlayback::IsDone( void ) const
{
    return m_IsDone;
}

//=========================================================================

f32 CinemaPlayback::GetPreviousTime( void ) const
{
    return m_PreviousTime;
}

//=========================================================================

f32 CinemaPlayback::GetTime( void ) const
{
    return m_CurrentTime;
}

//=========================================================================

CinemaPlaybackFinishReason CinemaPlayback::GetFinishReason( void ) const
{
    return m_FinishReason;
}
