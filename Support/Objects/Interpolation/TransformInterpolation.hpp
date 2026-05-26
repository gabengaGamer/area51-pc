//==============================================================================
//
//  TransformInterpolation.hpp
//
//==============================================================================

#ifndef TRANSFORM_INTERPOLATION_HPP
#define TRANSFORM_INTERPOLATION_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "InterpolationCache.hpp"
#include "InterpolationMath.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct transform_interp_state
{
    xbool   Valid;
    matrix4 L2W;
};

//------------------------------------------------------------------------------

typedef interp_cache<transform_interp_state> transform_interp_cache;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

//==============================================================================
//  STATE
//==============================================================================

inline 
void InitTransformInterpState( transform_interp_state& State )
{
    State.Valid = FALSE;
    State.L2W.Identity();
}

//==============================================================================

inline 
void CaptureTransformInterpState( transform_interp_state& Snapshot,
                                         const matrix4&          L2W )
{
    InitTransformInterpState( Snapshot );
    Snapshot.Valid = TRUE;
    Snapshot.L2W   = L2W;
}

//==============================================================================

inline 
xbool ShouldSnapTransformInterpState( const transform_interp_state& Prev,
                                             const transform_interp_state& Curr )
{
    return ShouldSnapInterpL2W( Prev.L2W, Curr.L2W );
}

//==============================================================================

inline 
void UpdateTransformInterpState( const transform_interp_state& Prev,
                                        const transform_interp_state& Curr,
                                              transform_interp_state& Interp,
                                              f32                     Alpha )
{
    Alpha = ClampInterpAlpha( Alpha );

    Interp.Valid = Curr.Valid;
    Interp.L2W = InterpMatrix( Prev.L2W, Curr.L2W, Alpha );
}

//==============================================================================
//  CACHE
//==============================================================================

inline
void InitTransformInterpCache( transform_interp_cache& Cache )
{
    transform_interp_state InitialState;
    InitTransformInterpState( InitialState );
    InitInterpCache( Cache, InitialState );
}

//==============================================================================

inline 
interp_capture_status CaptureTransformInterpCache( transform_interp_cache&       Cache,
                                                          const transform_interp_state& Snapshot )
{
    return CaptureInterpCache( Cache, Snapshot, ShouldSnapTransformInterpState );
}

//==============================================================================

inline 
void UpdateTransformInterpCache( transform_interp_cache& Cache,
                                        f32                    Alpha )
{
    UpdateInterpCache( Cache, Alpha, UpdateTransformInterpState );
}

//==============================================================================

inline 
void ClearTransformInterpCache( transform_interp_cache& Cache )
{
    ClearInterpCache( Cache );
}

//==============================================================================

inline
void InvalidateTransformInterpCache( transform_interp_cache& Cache )
{
    InvalidateInterpCache( Cache );
}

//==============================================================================

inline
void SnapTransformInterpCache( transform_interp_cache& Cache,
                                      const matrix4&          L2W )
{
    transform_interp_state Snapshot;
    CaptureTransformInterpState( Snapshot, L2W );
    SnapInterpCache( Cache, Snapshot );
}

//==============================================================================

inline 
xbool HasTransformInterpCache( const transform_interp_cache& Cache )
{
    return HasInterpCache( Cache );
}

//==============================================================================

inline
xbool HasTransformInterpCacheChange( const transform_interp_cache& Cache )
{
    return Cache.Prev.Valid &&
           Cache.Curr.Valid &&
           (x_memcmp( &Cache.Prev.L2W, &Cache.Curr.L2W, sizeof( matrix4 ) ) != 0);
}

//==============================================================================

inline 
const matrix4& GetTransformInterpCacheL2W( const transform_interp_cache& Cache,
                                                  const matrix4&                FallbackL2W )
{
    if( HasTransformInterpCache( Cache ) )
        return Cache.Interp.L2W;

    return FallbackL2W;
}

//==============================================================================
#endif //TRANSFORM_INTERPOLATION_HPP
//==============================================================================
