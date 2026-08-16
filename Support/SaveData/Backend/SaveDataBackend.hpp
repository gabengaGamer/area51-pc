//==============================================================================
//
//  SaveDataBackend.hpp
//
//==============================================================================

#ifndef SAVE_DATA_BACKEND_HPP
#define SAVE_DATA_BACKEND_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "SaveData/SaveDataTypes.hpp"
#include "x_files.hpp"

//==============================================================================
//  DATA
//==============================================================================

struct save_data_file_info
{
    xstring Name;
    s32     Size;
    u64     CreationDate;
    u64     ModifiedDate;
};

//==============================================================================
//  ROOT DIRECTORY
//==============================================================================

void SaveDataBackend_SetRootDirectory( const char* pRootDir );

//==============================================================================
//  SAVE DATA BACKEND
//==============================================================================

class save_data_backend
{
public:
    SaveDataStatus Init        ( void );
    void             Kill        ( void );
    SaveDataStatus List        ( xarray<save_data_file_info>& Files );
    SaveDataStatus Read        ( const char* pName, xarray<u8>& Bytes );
    SaveDataStatus WriteAtomic ( const char* pName, const xarray<u8>& Bytes );
    SaveDataStatus Delete      ( const char* pName );
};

//==============================================================================
#endif // SAVE_DATA_BACKEND_HPP
//==============================================================================
