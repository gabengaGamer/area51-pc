//==============================================================================
//
//  GameAppPlatform.hpp
//
//==============================================================================

#ifndef GAME_APP_PLATFORM_HPP
#define GAME_APP_PLATFORM_HPP

#include "x_types.hpp"

xbool GameAppGetExecutableDirectory( char* pBuffer, s32 BufferSize );
xbool GameAppGetDataDirectory      ( char* pBuffer, s32 BufferSize );
void  GameAppSetWindowIcon         ( void );

//==============================================================================
#endif // GAME_APP_PLATFORM_HPP
//==============================================================================
