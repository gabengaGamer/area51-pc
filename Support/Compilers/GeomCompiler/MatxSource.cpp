#include "GeomSource.hpp"

namespace
{

xbool LoadMatxMesh( const char* pFileName, rawmesh2& Mesh )
{
    return Mesh.Load( pFileName );
}

xbool LoadMatxAnimation( const char* pFileName, rawanim& Animation )
{
    return Animation.Load( pFileName );
}

struct matx_loader_registration
{
    matx_loader_registration( void )
    {
        geom_source::loader Loader;
        Loader.pName         = "MATX";
        Loader.pExtension    = ".matx";
        Loader.LoadMesh      = LoadMatxMesh;
        Loader.LoadAnimation = LoadMatxAnimation;
        VERIFY( geom_source::RegisterLoader( Loader ) );
    }
};

matx_loader_registration s_MatxLoaderRegistration;

} // namespace
