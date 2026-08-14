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
//  SAVE DATA BACKEND
//==============================================================================

class save_data_backend
{
public:
    save_data_status Init        ( void );
    void             Kill        ( void );
    save_data_status List        ( xarray<save_data_file_info>& Files );
    save_data_status Read        ( const char* pName, xarray<u8>& Bytes );
    save_data_status WriteAtomic ( const char* pName, const xarray<u8>& Bytes );
    save_data_status Delete      ( const char* pName );
};

//==============================================================================
#endif // SAVE_DATA_BACKEND_HPP
//==============================================================================
