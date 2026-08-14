//==============================================================================
//
//  SaveDataCodec.hpp
//
//==============================================================================

#ifndef SAVE_DATA_CODEC_HPP
#define SAVE_DATA_CODEC_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "StateMgr/GlobalSettings.hpp"
#include "StateMgr/PlayerProfile.hpp"

//==============================================================================
//  SAVE DATA CODEC
//==============================================================================

class save_data_codec
{
public:
    static xbool EncodeProfile ( const player_profile&  Profile,
                                 xarray<u8>&            Bytes,
                                 xstring&               Error );
    static xbool DecodeProfile ( const xarray<u8>&      Bytes,
                                 player_profile&        Profile,
                                 xstring&               Error );
    static xbool EncodeSettings( const global_settings& Settings,
                                 xarray<u8>&            Bytes,
                                 xstring&               Error );
    static xbool DecodeSettings( const xarray<u8>&      Bytes,
                                 global_settings&       Settings,
                                 xstring&               Error );

private:
    static xbool ProfileFieldsAreValid ( const player_profile& Profile );
    static xbool SettingsFieldsAreValid( const global_settings& Settings );
};

//==============================================================================
#endif // SAVE_DATA_CODEC_HPP
//==============================================================================
