//==============================================================================
//
//  GameAppPlatform_Linux.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GameAppPlatform.hpp"

#include <unistd.h>
#include <cstdlib>
#include <cstring>

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static 
xbool CopyPath( char* pBuffer, s32 BufferSize, const char* pPath )
{
    if( (pBuffer == NULL) || (BufferSize <= 0) || (pPath == NULL) )
        return( FALSE );

    size_t Length = std::strlen( pPath );
    if( Length >= (size_t)BufferSize )
    {
        pBuffer[0] = '\0';
        return( FALSE );
    }

    std::memcpy( pBuffer, pPath, Length + 1 );
    return( TRUE );
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

xbool GameAppGetExecutableDirectory( char* pBuffer, s32 BufferSize )
{
    if( (pBuffer == NULL) || (BufferSize <= 1) )
        return( FALSE );

    ssize_t Length = readlink( "/proc/self/exe", pBuffer, (size_t)BufferSize - 1 );
    if( (Length <= 0) || (Length >= BufferSize - 1) )
    {
        pBuffer[0] = '\0';
        return( FALSE );
    }

    pBuffer[Length] = '\0';
    char* pLastSlash = std::strrchr( pBuffer, '/' );
    if( pLastSlash )
        *pLastSlash = '\0';

    return( pBuffer[0] != '\0' );
}

//=============================================================================

xbool GameAppGetDataDirectory( char* pBuffer, s32 BufferSize )
{
    const char* pDataRoot = std::getenv( "A51_DATA_ROOT" );
    if( pDataRoot && pDataRoot[0] )
        return( CopyPath( pBuffer, BufferSize, pDataRoot ) );

    return( GameAppGetExecutableDirectory( pBuffer, BufferSize ) );
}

//=============================================================================

xbool GameAppGetSaveDirectory( char* pBuffer, s32 BufferSize )
{
    return( GameAppGetDataDirectory( pBuffer, BufferSize ) );
}

//=============================================================================

void GameAppSetWindowIcon( void )
{
}
