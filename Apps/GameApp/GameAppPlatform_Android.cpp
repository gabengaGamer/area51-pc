//==============================================================================
//
//  GameAppPlatform_Android.cpp
//
//==============================================================================

#include "GameAppPlatform.hpp"

#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_system.h"

#include <cstring>
#include <unistd.h>

//==============================================================================

static
xbool CopyPath( char* pBuffer, s32 BufferSize, const char* pPath )
{
    if( (pBuffer == NULL) || (BufferSize <= 0) || (pPath == NULL) )
        return FALSE;

    size_t const Length = std::strlen( pPath );
    if( Length >= (size_t)BufferSize )
    {
        pBuffer[0] = '\0';
        return FALSE;
    }

    std::memcpy( pBuffer, pPath, Length + 1 );
    return TRUE;
}

//==============================================================================

xbool GameAppGetExecutableDirectory( char* pBuffer, s32 BufferSize )
{
    return CopyPath( pBuffer, BufferSize, SDL_GetBasePath() );
}

//==============================================================================

xbool GameAppGetDataDirectory( char* pBuffer, s32 BufferSize )
{
    const char* pExternalPath = SDL_GetAndroidExternalStoragePath();
    if( pExternalPath && pExternalPath[0] )
    {
        if( access( pExternalPath, R_OK | X_OK ) == 0 )
            return CopyPath( pBuffer, BufferSize, pExternalPath );
    }

    return CopyPath( pBuffer, BufferSize, "." );
}

//==============================================================================

xbool GameAppGetSaveDirectory( char* pBuffer, s32 BufferSize )
{
    char* pSavePath = SDL_GetPrefPath( "Area51", "Area51" );
    if( !pSavePath )
        return FALSE;

    const xbool Result = CopyPath( pBuffer, BufferSize, pSavePath );
    SDL_free( pSavePath );
    return Result;
}

//==============================================================================

void GameAppSetWindowIcon( void )
{
}
