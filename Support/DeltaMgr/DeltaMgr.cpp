//==============================================================================
//
//  DeltaMgr.cpp
//
//==============================================================================

//==============================================================================
//
// Fixed timestep handling was based on
// https://gafferongames.com/post/fix_your_timestep/
//
// Thanks to Glenn Fiedler for the great article.
//
//==============================================================================

// TODO: f64 m_RealTime

//==============================================================================
//  INCLUDES
//==============================================================================

#include "DeltaMgr.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

const f32 delta_mgr::DEFAULT_FIXED_DELTA_TIME           = 1.0f / 60.0f;
const f32 delta_mgr::DEFAULT_MAX_FRAME_DELTA_TIME       = 0.25f;
const s32 delta_mgr::DEFAULT_MAX_FIXED_STEPS_PER_FRAME  = 12;

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

delta_mgr g_DeltaMgr;

//==============================================================================
//  FUNCTIONS
//==============================================================================

delta_mgr::delta_mgr( void ) :
    m_RawDeltaTime         ( 0.0f ),
    m_UnscaledDeltaTime    ( 0.0f ),
    m_DeltaTime            ( 0.0f ),
    m_RealTime             ( 0.0f ),
    m_Accumulator          ( 0.0f ),
    m_TimeScale            ( 1.0f ),
    m_FixedDeltaTime       ( DEFAULT_FIXED_DELTA_TIME ),
    m_FallbackDeltaTime    ( DEFAULT_FIXED_DELTA_TIME ),
    m_MaxFrameDeltaTime    ( DEFAULT_MAX_FRAME_DELTA_TIME ),
    m_FixedStepCount       ( 0 ),
    m_MaxFixedStepsPerFrame( DEFAULT_MAX_FIXED_STEPS_PER_FRAME )
{
}

//==============================================================================

delta_mgr::~delta_mgr( void )
{
}

//==============================================================================

void delta_mgr::Reset( void )
{
	// Reset frame/runtime state.
    m_RawDeltaTime      = 0.0f;
    m_UnscaledDeltaTime = 0.0f;
    m_DeltaTime         = 0.0f;
    m_RealTime          = 0.0f;
    m_Accumulator       = 0.0f;
    m_TimeScale         = 1.0f;
    m_FixedStepCount    = 0;
}

//==============================================================================

void delta_mgr::Tick( f32 RawDeltaTime )
{
    ASSERT( x_isvalid( RawDeltaTime ) );

    m_RawDeltaTime      = RawDeltaTime;
    m_UnscaledDeltaTime = ClampDeltaTimeToMax( RawDeltaTime );
    m_DeltaTime         = m_UnscaledDeltaTime * m_TimeScale;

    if( RawDeltaTime > 0.0f )
        m_RealTime += RawDeltaTime;

    m_FixedStepCount = 0;
    m_Accumulator   += m_DeltaTime;
}