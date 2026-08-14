//=========================================================================
//
//  GeomFile.hpp
//
//=========================================================================

#ifndef GEOM_FILE_HPP
#define GEOM_FILE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

struct rigid_geom;
struct skin_geom;

//=========================================================================
//  GEOMETRY FILE
//=========================================================================

namespace geom_file
{

enum
{
    VERSION = 42,
};

xbool LoadRigid( X_FILE* pFile, rigid_geom*& pGeom, xstring& error );
xbool LoadSkin( X_FILE* pFile, skin_geom*& pGeom, xstring& error );
xbool SaveRigid( X_FILE* pFile, rigid_geom const& geom, xstring& error );
xbool SaveSkin( X_FILE* pFile, skin_geom const& geom, xstring& error );
xbool SaveRigid( char const* pFileName, rigid_geom const& geom, xstring& error );
xbool SaveSkin( char const* pFileName, skin_geom const& geom, xstring& error );
xbool Validate( rigid_geom const& geom, xstring& error );
xbool Validate( skin_geom const& geom, xstring& error );

} // namespace geom_file

//=========================================================================
#endif // GEOM_FILE_HPP
//=========================================================================
