//=========================================================================
//
//  PlayerAnimation.hpp
//
//=========================================================================

#ifndef PLAYER_ANIMATION_HPP
#define PLAYER_ANIMATION_HPP

#include "x_types.hpp"

//=========================================================================
//  TYPED, ALLOCATION-FREE TRANSITION DISPATCH
//=========================================================================

template< typename AnimationState >
struct PlayerAnimationTransition
{
    AnimationState  State;
    xbool           ShouldApply;
    xbool           WasForcedByDeath;
};

//-------------------------------------------------------------------------

template< typename AnimationState >
class PlayerAnimationDispatcher
{
public:

    PlayerAnimationTransition<AnimationState> Resolve( AnimationState Current,
                                                        AnimationState Requested,
                                                        xbool IsDead,
                                                        AnimationState Death,
                                                        AnimationState MissionFailed ) const
    {
        PlayerAnimationTransition<AnimationState> Transition;
        Transition.State            = Requested;
        Transition.ShouldApply      = Current != Requested;
        Transition.WasForcedByDeath = FALSE;

        if( Transition.ShouldApply && IsDead && (Requested != MissionFailed) )
        {
            Transition.State            = Death;
            Transition.ShouldApply      = Current != Death;
            Transition.WasForcedByDeath = Requested != Death;
        }

        return Transition;
    }
};

//=========================================================================
#endif // PLAYER_ANIMATION_HPP
