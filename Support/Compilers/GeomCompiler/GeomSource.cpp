#include "GeomSource.hpp"

namespace
{

const s32 MaxSourceLoaders = 16;
geom_source::loader s_Loaders[MaxSourceLoaders];
s32                 s_nLoaders = 0;

const geom_source::loader* FindLoader( const char* pFileName )
{
    if( !pFileName || !pFileName[0] )
        return NULL;

    char Extension[X_MAX_EXT];
    x_splitpath( pFileName, NULL, NULL, NULL, Extension );
    for( s32 i = 0; i < s_nLoaders; i++ )
    {
        if( x_stricmp( Extension, s_Loaders[i].pExtension ) == 0 )
            return &s_Loaders[i];
    }
    return NULL;
}

} // namespace

xbool geom_source::RegisterLoader( const loader& Loader )
{
    if( !Loader.pName || !Loader.pName[0] ||
        !Loader.pExtension || (Loader.pExtension[0] != '.') ||
        !Loader.LoadMesh ||
        (s_nLoaders >= MaxSourceLoaders) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < s_nLoaders; i++ )
    {
        if( x_stricmp( Loader.pExtension, s_Loaders[i].pExtension ) == 0 )
            return FALSE;
    }

    s_Loaders[s_nLoaders++] = Loader;
    return TRUE;
}

xbool geom_source::IsSourceFile( const char* pFileName )
{
    return FindLoader( pFileName ) != NULL;
}

xbool geom_source::LoadMesh( const char* pFileName,
                             rawmesh2&   Mesh,
                             xstring&    Error )
{
    const loader* pLoader = FindLoader( pFileName );
    if( !pLoader )
    {
        Error.Format( "No geometry source loader is registered for [%s]",
                      pFileName ? pFileName : "" );
        return FALSE;
    }
    if( !pLoader->LoadMesh( pFileName, Mesh ) )
    {
        Error.Format( "%s loader could not read [%s]",
                      pLoader->pName,
                      pFileName );
        return FALSE;
    }
    return TRUE;
}

xbool geom_source::LoadAnimation( const char* pFileName,
                                  rawanim&    Animation,
                                  xstring&    Error )
{
    const loader* pLoader = FindLoader( pFileName );
    if( !pLoader )
    {
        Error.Format( "No geometry source loader is registered for [%s]",
                      pFileName ? pFileName : "" );
        return FALSE;
    }
    if( !pLoader->LoadAnimation )
    {
        Error.Format( "%s loader does not provide skeleton animation",
                      pLoader->pName );
        return FALSE;
    }
    if( !pLoader->LoadAnimation( pFileName, Animation ) )
    {
        Error.Format( "%s loader could not read animation from [%s]",
                      pLoader->pName,
                      pFileName );
        return FALSE;
    }
    return TRUE;
}
