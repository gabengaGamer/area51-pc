//=========================================================================
//
//  PlayerInventory.hpp
//
//=========================================================================

#ifndef PLAYER_INVENTORY_HPP
#define PLAYER_INVENTORY_HPP

#include "PlayerInput.hpp"

//=========================================================================
//  DATA CONTRACTS
//=========================================================================

enum class PlayerWeaponRequest
{
    None,
    CycleRight,
    CycleLeft,
    Scanner,
    Pistol,
    Smp,
    Shotgun,
    SniperRifle,
    Bbg,
    MesonCannon
};

//-------------------------------------------------------------------------

struct PlayerInventoryInput
{
                    PlayerInventoryInput        ( void );

    PlayerWeaponRequest WeaponRequest;
    xbool               UseHeld;
    xbool               UsePressed;
};

//=========================================================================
//  PLAYER INVENTORY / INTERACTION
//=========================================================================

class PlayerInventory
{
public:

    PlayerInventoryInput BuildInput             ( const PlayerInputState& Input,
                                                    xbool AllowWeaponSwitch ) const;
};

//=========================================================================
#endif // PLAYER_INVENTORY_HPP
