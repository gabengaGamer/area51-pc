//=========================================================================
//
//  PlayerCombat.hpp
//
//=========================================================================

#ifndef PLAYER_COMBAT_HPP
#define PLAYER_COMBAT_HPP

#include "PlayerInput.hpp"
#include "PlayerLook.hpp"

//=========================================================================
//  DATA CONTRACTS
//=========================================================================

struct PlayerCombatInput
{
                    PlayerCombatInput           ( void );

    xbool           PrimaryHeld;
    xbool           PrimaryPressed;
    xbool           SecondaryHeld;
    xbool           SecondaryPressed;
    xbool           ReloadPressed;
    xbool           MeleeHeld;
    xbool           GrenadeHeld;
    xbool           UsesGamepad;
};

//=========================================================================
//  PLAYER COMBAT
//=========================================================================

class PlayerCombat
{
public:

    PlayerCombatInput BuildInput                 ( const PlayerInputState& Input,
                                                    xbool IsMutated ) const;
    xbool             IsAimAssistActive          ( const PlayerLookSample& Look,
                                                    const PlayerInputState& Input,
                                                    xbool IsMutated ) const;
    void              ApplyAimAssistDampening    ( PlayerLookSample& Look,
                                                    f32 Multiplier ) const;
};

//=========================================================================
#endif // PLAYER_COMBAT_HPP
