#ifndef GEOM_SOURCE_HPP
#define GEOM_SOURCE_HPP

#include "x_files.hpp"
#include "RawMesh2.hpp"
#include "RawAnim.hpp"

// Source adapters convert authoring formats into the compiler's canonical
// import model. Runtime geometry builders never depend on a file format.
namespace geom_source
{

struct loader
{
    const char* pName;
    const char* pExtension;
    xbool (*LoadMesh)     ( const char* pFileName, rawmesh2& Mesh );
    xbool (*LoadAnimation)( const char* pFileName, rawanim&  Animation );
};

xbool RegisterLoader( const loader& Loader );
xbool IsSourceFile  ( const char* pFileName );
xbool LoadMesh      ( const char* pFileName, rawmesh2& Mesh, xstring& Error );
xbool LoadAnimation ( const char* pFileName, rawanim& Animation, xstring& Error );

} // namespace geom_source

#endif
