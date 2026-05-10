#ifndef FLASHLIGHT_HPP
#define FLASHLIGHT_HPP

#include "x_math.hpp"

class player;

xbool flashlight_CalcTransform( player& Player, matrix4& L2W );
void  flashlight_Register      ( player& Player );

#endif
