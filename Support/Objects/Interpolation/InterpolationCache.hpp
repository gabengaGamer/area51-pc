//==============================================================================
//
//  InterpolationCache.hpp
//
//==============================================================================

#ifndef INTERPOLATION_CACHE_HPP
#define INTERPOLATION_CACHE_HPP

//==============================================================================
//  STRUCTS
//==============================================================================

template< class interp_state >
struct interp_cache
{
    interp_state Prev;
    interp_state Curr;
    interp_state Interp;
    xbool        Active;
};

//------------------------------------------------------------------------------

enum interp_capture_status
{
    INTERP_CAPTURE_UNCHANGED,
    INTERP_CAPTURE_CHANGED,
    INTERP_CAPTURE_SNAPPED
};

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

// interp_state must expose a Valid flag used by the generic cache helpers.

template< class interp_state >
inline 
void InitInterpCache( interp_cache<interp_state>& Cache,
                             const interp_state&         InitialState )
{
    Cache.Prev   = InitialState;
    Cache.Curr   = InitialState;
    Cache.Interp = InitialState;
    Cache.Active = FALSE;
}

//==============================================================================

template< class interp_state >
inline
interp_state& BeginCaptureInterpCache( interp_cache<interp_state>& Cache )
{
    if( Cache.Curr.Valid )
        Cache.Prev = Cache.Curr;
    else
        Cache.Prev.Valid = FALSE;

    return Cache.Curr;
}

//==============================================================================

template< class interp_state, class should_snap_fn >
inline
interp_capture_status FinishCaptureInterpCache( interp_cache<interp_state>& Cache,
                                                       should_snap_fn             ShouldSnap )
{
    if( !Cache.Curr.Valid )
        return INTERP_CAPTURE_UNCHANGED;

    if( !Cache.Prev.Valid )
    {
        Cache.Prev = Cache.Curr;
        return INTERP_CAPTURE_UNCHANGED;
    }

    if( ShouldSnap( Cache.Prev, Cache.Curr ) )
    {
        Cache.Prev = Cache.Curr;
        return INTERP_CAPTURE_SNAPPED;
    }

    if( x_memcmp( &Cache.Prev, &Cache.Curr, sizeof( interp_state ) ) == 0 )
        return INTERP_CAPTURE_UNCHANGED;

    return INTERP_CAPTURE_CHANGED;
}

//==============================================================================

template< class interp_state, class should_snap_fn >
inline
interp_capture_status CaptureInterpCache( interp_cache<interp_state>& Cache,
                                                 const interp_state&        Snapshot,
                                                 should_snap_fn             ShouldSnap )
{
    interp_state& Curr = BeginCaptureInterpCache( Cache );
    Curr = Snapshot;
    return FinishCaptureInterpCache( Cache, ShouldSnap );
}

//==============================================================================

template< class interp_state, class update_fn >
inline 
void UpdateInterpCache( interp_cache<interp_state>& Cache,
                               f32                         Alpha,
                               update_fn                   Update )
{
    Cache.Active = FALSE;

    if( !Cache.Curr.Valid )
        return;

    Update( Cache.Prev.Valid ? Cache.Prev : Cache.Curr,
            Cache.Curr,
            Cache.Interp,
            Alpha );
    Cache.Active = TRUE;
}

//==============================================================================

template< class interp_state >
inline 
void ClearInterpCache( interp_cache<interp_state>& Cache )
{
    Cache.Active = FALSE;
}

//==============================================================================

template< class interp_state >
inline
void InvalidateInterpCache( interp_cache<interp_state>& Cache )
{
    Cache.Prev.Valid   = FALSE;
    Cache.Curr.Valid   = FALSE;
    Cache.Interp.Valid = FALSE;
    Cache.Active       = FALSE;
}

//==============================================================================

template< class interp_state >
inline
void SnapInterpCache( interp_cache<interp_state>& Cache,
                             const interp_state&        Snapshot )
{
    Cache.Prev   = Snapshot;
    Cache.Curr   = Snapshot;
    Cache.Interp = Snapshot;
    Cache.Active = FALSE;
}

//==============================================================================

template< class interp_state >
inline 
xbool HasInterpCache( const interp_cache<interp_state>& Cache )
{
    return Cache.Active && Cache.Interp.Valid;
}

//==============================================================================
#endif //INTERPOLATION_CACHE_HPP
//==============================================================================
