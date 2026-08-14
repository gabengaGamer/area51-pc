//==============================================================================
//
//  SaveDataTypes.hpp
//
//==============================================================================

#ifndef SAVE_DATA_TYPES_HPP
#define SAVE_DATA_TYPES_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  DATA
//==============================================================================

enum class save_data_operation
{
    None,
    RefreshProfiles,
    CreateProfile,
    LoadProfile,
    SaveProfile,
    DeleteProfile,
    SaveSettings,
};

//------------------------------------------------------------------------------

enum class save_data_status
{
    Success,
    NotFound,
    Corrupt,
    NoSpace,
    AccessDenied,
    IoError,
    Busy,
};

//------------------------------------------------------------------------------

struct save_data_result
{
    save_data_result( void ) :
        Operation ( save_data_operation::None ),
        Status    ( save_data_status::Success ),
        ProfileID( -1 )
    {
    }

    xbool Succeeded( void ) const
    {
        return( Status == save_data_status::Success );
    }

    save_data_operation Operation;
    save_data_status    Status;
    s32                 ProfileID;
};

//==============================================================================
#endif // SAVE_DATA_TYPES_HPP
//==============================================================================
