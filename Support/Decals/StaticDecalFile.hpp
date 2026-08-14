//==============================================================================
//
//  StaticDecalFile.hpp
//
//==============================================================================

#ifndef STATIC_DECAL_FILE_HPP
#define STATIC_DECAL_FILE_HPP

#include "DecalMgr.hpp"

namespace static_decal_file
{
    xbool Load( X_FILE*                 pFile,
                decal_mgr::static_data*& pData,
                xstring&                Error );

    xbool Save( X_FILE*                     pFile,
                const decal_mgr::static_data& Data,
                xstring&                    Error );

    xbool Save( const char*                  pFileName,
                const decal_mgr::static_data& Data,
                xstring&                    Error );
}

//==============================================================================
#endif // STATIC_DECAL_FILE_HPP
//==============================================================================
