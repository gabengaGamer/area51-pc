//=========================================================================
//
//  PlayerStatus.hpp
//
//=========================================================================

#ifndef PLAYER_STATUS_HPP
#define PLAYER_STATUS_HPP

#include "x_types.hpp"

//=========================================================================

enum class PlayerLifePhase
{
    Dead,
    Alive,
    Removed
};

//=========================================================================
//  PLAYER STATUS / LIFECYCLE
//=========================================================================

class PlayerStatus
{
public:

                    PlayerStatus                 ( void );

    void            SetLifePhase                 ( PlayerLifePhase Phase );
    PlayerLifePhase GetLifePhase                 ( void ) const;

    xbool           ShouldRefreshSafeSpot        ( f32 DeltaTime,
                                                    f32 RefreshInterval );

private:

    PlayerLifePhase m_LifePhase;
    f32             m_SafeSpotElapsed;
};

//=========================================================================
#endif // PLAYER_STATUS_HPP
