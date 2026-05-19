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

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

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

template< class interp_state, class should_snap_fn >
inline 
void CaptureInterpCache( interp_cache<interp_state>& Cache,
                                const interp_state&        Snapshot,
                                should_snap_fn             ShouldSnap )
{
    Cache.Prev = Cache.Curr;
    Cache.Curr = Snapshot;

    if( !Cache.Prev.Valid )
    {
        Cache.Prev = Cache.Curr;
        return;
    }

    if( ShouldSnap( Cache.Prev, Cache.Curr ) )
        Cache.Prev = Cache.Curr;
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
xbool HasInterpCache( const interp_cache<interp_state>& Cache )
{
    return Cache.Active && Cache.Interp.Valid;
}

//==============================================================================
#endif //INTERPOLATION_CACHE_HPP
//==============================================================================
