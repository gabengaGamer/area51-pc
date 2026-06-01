//==============================================================================
//
//  DeltaMgr.hpp
//
//==============================================================================

#ifndef DELTA_MGR_HPP
#define DELTA_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_math.hpp"
#include "x_time.hpp"

//==============================================================================
//  DELTA MGR CLASS
//==============================================================================

class delta_mgr
{
public:
                    delta_mgr                   ( void );
                   ~delta_mgr                   ( void );

    void            Reset                       ( void );
    void            Tick                        ( f32 RawDeltaTime );
    void            SetTimeScale                ( f32 TimeScale );
    void            SetFixedUpdateDeltaTime     ( f32 DeltaTime );
    void            SetFallbackDeltaTime        ( f32 DeltaTime );
    void            SetMaxFrameDeltaTime        ( f32 DeltaTime );
    void            SetMaxFixedStepsPerFrame    ( s32 StepCount );

    f32             GetRawDeltaTime             ( void ) const;
    f32             GetUnscaledDeltaTime        ( void ) const;
    f32             GetDeltaTime                ( void ) const;
    f32             GetFixedUpdateDeltaTime     ( void ) const;
    f32             GetInterpolationAlpha       ( void ) const;
    f32             GetTimeScale                ( void ) const;
    f32             GetRealTime                 ( void ) const;
    s32             GetFixedStepCount           ( void ) const;
    s32             GetPendingFixedStepCount    ( void ) const;

    xbool           HasFixedUpdate              ( void ) const;
    void            AdvanceFixedUpdate          ( void );

    f32             ClampDeltaTimeToMax         ( f32 DeltaTime ) const;
    f32             ClampDeltaTimeToFallback    ( f32 DeltaTime ) const;
    f32             ReadTimerDeltaToFallback    ( xtimer& Timer ) const;

private:
    static const f32    DEFAULT_FIXED_DELTA_TIME;
    static const f32    DEFAULT_MAX_FRAME_DELTA_TIME;
    static const s32    DEFAULT_MAX_FIXED_STEPS_PER_FRAME;

    f32             m_RawDeltaTime;
    f32             m_UnscaledDeltaTime;
    f32             m_DeltaTime;
    f32             m_RealTime;
    f32             m_Accumulator;
    f32             m_TimeScale;
    f32             m_FixedDeltaTime;
    f32             m_FallbackDeltaTime;
    f32             m_MaxFrameDeltaTime;
    s32             m_FixedStepCount;
    s32             m_MaxFixedStepsPerFrame;
};

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline 
void delta_mgr::SetTimeScale( f32 TimeScale )
{
    ASSERT( x_isvalid( TimeScale ) );

    if( TimeScale < 0.0f )
        TimeScale = 0.0f;

    m_TimeScale = TimeScale;
}

//==============================================================================

inline 
void delta_mgr::SetFixedUpdateDeltaTime( f32 DeltaTime )
{
    ASSERT( x_isvalid( DeltaTime ) );

    if( DeltaTime <= 0.0f )
        DeltaTime = DEFAULT_FIXED_DELTA_TIME;

    m_FixedDeltaTime = DeltaTime;
    m_Accumulator    = 0.0f;
}

//==============================================================================

inline 
void delta_mgr::SetFallbackDeltaTime( f32 DeltaTime )
{
    ASSERT( x_isvalid( DeltaTime ) );

    if( DeltaTime <= 0.0f )
        DeltaTime = DEFAULT_FIXED_DELTA_TIME;

    m_FallbackDeltaTime = DeltaTime;
}

//==============================================================================

inline 
void delta_mgr::SetMaxFrameDeltaTime( f32 DeltaTime )
{
    ASSERT( x_isvalid( DeltaTime ) );

    if( DeltaTime <= 0.0f )
        DeltaTime = DEFAULT_MAX_FRAME_DELTA_TIME;

    m_MaxFrameDeltaTime = DeltaTime;
}

//==============================================================================

inline 
void delta_mgr::SetMaxFixedStepsPerFrame( s32 StepCount )
{
    if( StepCount <= 0 )
        StepCount = DEFAULT_MAX_FIXED_STEPS_PER_FRAME;

    m_MaxFixedStepsPerFrame = StepCount;
}

//==============================================================================

inline 
f32 delta_mgr::GetRawDeltaTime( void ) const
{
    return m_RawDeltaTime;
}

//==============================================================================

inline 
f32 delta_mgr::GetUnscaledDeltaTime( void ) const
{
    return m_UnscaledDeltaTime;
}

//==============================================================================

inline 
f32 delta_mgr::GetDeltaTime( void ) const
{
    return m_DeltaTime;
}

//==============================================================================

inline 
f32 delta_mgr::GetFixedUpdateDeltaTime( void ) const
{
    return m_FixedDeltaTime;
}

//==============================================================================

inline 
f32 delta_mgr::GetInterpolationAlpha( void ) const
{
    ASSERT( m_FixedDeltaTime > 0.0f );

    f32 Alpha = m_Accumulator / m_FixedDeltaTime;

    if( Alpha < 0.0f )
        return 0.0f;

    if( Alpha > 1.0f )
        return 1.0f;

    return Alpha;
}

//==============================================================================

inline 
f32 delta_mgr::GetTimeScale( void ) const
{
    return m_TimeScale;
}

//==============================================================================

inline 
f32 delta_mgr::GetRealTime( void ) const
{
    return m_RealTime;
}

//==============================================================================

inline 
s32 delta_mgr::GetFixedStepCount( void ) const
{
    return m_FixedStepCount;
}

//==============================================================================

inline 
s32 delta_mgr::GetPendingFixedStepCount( void ) const
{
    ASSERT( m_FixedDeltaTime > 0.0f );
    ASSERT( m_MaxFixedStepsPerFrame > 0 );

    s32 Count = 0;
    f32 Accumulator = m_Accumulator;

    while( (Accumulator >= m_FixedDeltaTime) &&
           ((m_FixedStepCount + Count) < m_MaxFixedStepsPerFrame) )
    {
        Accumulator -= m_FixedDeltaTime;
        Count++;
    }

    return Count;
}

//==============================================================================

inline 
xbool delta_mgr::HasFixedUpdate( void ) const
{
    ASSERT( m_FixedDeltaTime > 0.0f );
    ASSERT( m_MaxFixedStepsPerFrame > 0 );

    return ( m_Accumulator >= m_FixedDeltaTime ) &&
           ( m_FixedStepCount < m_MaxFixedStepsPerFrame );
}

//==============================================================================

inline 
void delta_mgr::AdvanceFixedUpdate( void )
{
    ASSERT( HasFixedUpdate() );

    m_Accumulator -= m_FixedDeltaTime;
    m_FixedStepCount++;
}

//==============================================================================

inline 
f32 delta_mgr::ClampDeltaTimeToMax( f32 DeltaTime ) const
{
    ASSERT( x_isvalid( DeltaTime ) );
    ASSERT( m_FallbackDeltaTime > 0.0f );
    ASSERT( m_MaxFrameDeltaTime > 0.0f );

    if( DeltaTime < 0.0f )
        return m_FallbackDeltaTime;

    if( DeltaTime > m_MaxFrameDeltaTime )
        return m_MaxFrameDeltaTime;

    return DeltaTime;
}

//==============================================================================

inline 
f32 delta_mgr::ClampDeltaTimeToFallback( f32 DeltaTime ) const
{
    ASSERT( x_isvalid( DeltaTime ) );
    ASSERT( m_FallbackDeltaTime > 0.0f );
    ASSERT( m_MaxFrameDeltaTime > 0.0f );

    if( DeltaTime < 0.0f )
        return m_FallbackDeltaTime;

    if( DeltaTime > m_MaxFrameDeltaTime )
        return m_FallbackDeltaTime;

    return DeltaTime;
}

//==============================================================================

inline 
f32 delta_mgr::ReadTimerDeltaToFallback( xtimer& Timer ) const
{
    ASSERT( Timer.IsRunning() );

    return ClampDeltaTimeToFallback( Timer.TripSec() );
}

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern delta_mgr g_DeltaMgr;

//==============================================================================
#endif // DELTA_MGR_HPP
//==============================================================================
