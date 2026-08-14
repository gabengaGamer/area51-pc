//==============================================================================
//
//  DecalPackageFile.hpp
//
//==============================================================================

#ifndef DECAL_PACKAGE_FILE_HPP
#define DECAL_PACKAGE_FILE_HPP

#include "x_files.hpp"

class decal_package;

namespace decal_package_file
{
    enum
    {
        MAX_GROUPS      = 4096,
        MAX_DEFINITIONS = 65536,
    };

    xbool Validate( const decal_package& Package,
                    xstring&             Error );

    xbool Load( X_FILE*         pFile,
                decal_package*& pPackage,
                xstring&        Error );

    xbool Save( X_FILE*             pFile,
                const decal_package& Package,
                xstring&            Error );

    xbool Save( const char*          pFileName,
                const decal_package& Package,
                xstring&            Error );
}

//==============================================================================
#endif // DECAL_PACKAGE_FILE_HPP
//==============================================================================
