//==========================================================================
//
//  e_Platform.cpp
//
//==========================================================================

//==========================================================================
//  INCLUDES
//==========================================================================

#include "Entropy.hpp"

#if defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)
#include "SDL3/SDL_misc.h"
#endif

//==========================================================================
//  FUNCTIONS
//==========================================================================

xbool eng_OpenURL( const char* pURL )
{
#if defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)
    if( !pURL || !pURL[0] )
    {
        return FALSE;
    }

    return SDL_OpenURL( pURL ) ? TRUE : FALSE;
#else
    (void)pURL;
    return FALSE;
#endif
}
