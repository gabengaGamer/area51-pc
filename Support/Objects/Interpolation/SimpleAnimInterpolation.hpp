//==============================================================================
//
//  SimpleAnimInterpolation.hpp
//
//==============================================================================

#ifndef SIMPLE_ANIM_INTERPOLATION_HPP
#define SIMPLE_ANIM_INTERPOLATION_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "InterpolationCache.hpp"
#include "SkinnedInterpolation.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct simple_anim_interp_state : public skinned_interp_state
{
};

//------------------------------------------------------------------------------

typedef interp_cache<simple_anim_interp_state> simple_anim_interp_cache;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

//==============================================================================
//  STATE
//==============================================================================

inline 
void InitSimpleAnimInterpState( simple_anim_interp_state& State )
{
    InitSkinnedInterpState( State );
}

//==============================================================================

inline 
void CaptureSimpleAnimInterpState( simple_anim_interp_state& Snapshot,
                                          const matrix4&           L2W,
                                          simple_anim_player&      AnimPlayer )
{
    CaptureSkinnedInterpState( Snapshot, L2W );

    const anim_group* pAnimGroup = AnimPlayer.GetAnimGroup();
    if( !pAnimGroup )
        return;

    if( AnimPlayer.GetAnimIndex() < 0 )
        return;

    const s32 nBones = MIN( pAnimGroup->GetNBones(), MAX_ANIM_BONES );
    if( nBones <= 0 )
        return;

    const matrix4* pBones = AnimPlayer.GetBoneL2Ws( FALSE );
    if( !pBones )
        return;

    SetSkinnedInterpStateBones( Snapshot, pBones, nBones );
}

//==============================================================================

inline 
xbool ShouldSnapSimpleAnimInterpState( const simple_anim_interp_state& Prev,
                                              const simple_anim_interp_state& Curr )
{
    return ShouldSnapSkinnedInterpState( Prev, Curr );
}

//==============================================================================

inline 
void UpdateSimpleAnimInterpState( const simple_anim_interp_state& Prev,
                                         const simple_anim_interp_state& Curr,
                                               simple_anim_interp_state& Interp,
                                               f32                       Alpha )
{
    UpdateSkinnedInterpState( Prev, Curr, Interp, Alpha );
}

//==============================================================================

inline 
xbool GetSimpleAnimInterpBoneL2W( const simple_anim_interp_state& State,
                                         s32                             iBone,
                                         matrix4&                        L2W )
{
    return GetSkinnedInterpBoneL2W( State, iBone, L2W );
}

//==============================================================================

inline 
const matrix4* BuildSimpleAnimInterpMatrices( const simple_anim_interp_state& State,
                                                     const anim_group&               AnimGroup,
                                                     s32                             nBones )
{
    return BuildSkinnedInterpMatrices( State, AnimGroup, nBones );
}

//==============================================================================
//  CACHE
//==============================================================================

inline 
void InitSimpleAnimInterpCache( simple_anim_interp_cache& Cache )
{
    simple_anim_interp_state InitialState;
    InitSimpleAnimInterpState( InitialState );
    InitInterpCache( Cache, InitialState );
}

//==============================================================================

inline
interp_capture_status CaptureSimpleAnimInterpCache( simple_anim_interp_cache& Cache,
                                                           const matrix4&            L2W,
                                                           simple_anim_player&       AnimPlayer )
{
    simple_anim_interp_state& Snapshot = BeginCaptureInterpCache( Cache );
    CaptureSimpleAnimInterpState( Snapshot, L2W, AnimPlayer );
    return FinishCaptureInterpCache( Cache, ShouldSnapSimpleAnimInterpState );
}

//==============================================================================

inline 
void UpdateSimpleAnimInterpCache( simple_anim_interp_cache& Cache,
                                         f32                      Alpha )
{
    UpdateInterpCache( Cache, Alpha, UpdateSimpleAnimInterpState );
}

//==============================================================================

inline 
void ClearSimpleAnimInterpCache( simple_anim_interp_cache& Cache )
{
    ClearInterpCache( Cache );
}

//==============================================================================

inline
void InvalidateSimpleAnimInterpCache( simple_anim_interp_cache& Cache )
{
    InvalidateInterpCache( Cache );
}

//==============================================================================

inline
void SnapSimpleAnimInterpCache( simple_anim_interp_cache& Cache,
                                       const matrix4&            L2W,
                                       simple_anim_player&       AnimPlayer )
{
    simple_anim_interp_state Snapshot;
    CaptureSimpleAnimInterpState( Snapshot, L2W, AnimPlayer );
    SnapInterpCache( Cache, Snapshot );
}

//==============================================================================

inline 
xbool HasSimpleAnimInterpCache( const simple_anim_interp_cache& Cache )
{
    return HasInterpCache( Cache );
}

//==============================================================================

inline 
xbool GetSimpleAnimInterpCacheBoneL2W( const simple_anim_interp_cache& Cache,
                                              s32                             iBone,
                                              matrix4&                        L2W )
{
    if( !HasSimpleAnimInterpCache( Cache ) )
        return FALSE;

    return GetSimpleAnimInterpBoneL2W( Cache.Interp, iBone, L2W );
}

//==============================================================================

inline 
const matrix4& GetSimpleAnimInterpCacheL2W( const simple_anim_interp_cache& Cache,
                                                   const matrix4&                  FallbackL2W )
{
    if( HasSimpleAnimInterpCache( Cache ) )
        return Cache.Interp.L2W;

    return FallbackL2W;
}

//==============================================================================

inline 
const matrix4* BuildSimpleAnimInterpCacheMatrices( const simple_anim_interp_cache& Cache,
                                                          const anim_group&               AnimGroup,
                                                          s32                             nBones )
{
    if( !HasSimpleAnimInterpCache( Cache ) )
        return NULL;

    return BuildSimpleAnimInterpMatrices( Cache.Interp, AnimGroup, nBones );
}

//==============================================================================
#endif //SIMPLE_ANIM_INTERPOLATION_HPP
//==============================================================================
