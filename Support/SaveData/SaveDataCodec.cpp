//==============================================================================
//
//  SaveDataCodec.cpp
//
//==============================================================================

#include "SaveDataCodec.hpp"
#include "Auxiliary/MiscUtils/BitseryIO.hpp"

#include <memory>

namespace
{

const bitsery_io::file_format PROFILE_FILE_FORMAT =
{
    { 'A', '5', '1', 'P' }, // Signature
    1,                      // Version
};

//------------------------------------------------------------------------------

const bitsery_io::file_format SETTINGS_FILE_FORMAT =
{
    { 'A', '5', '1', 'S' }, // Signature
    3,                      // Version
};

//==============================================================================

struct profile_save_record
{
    player_profile Profile;
};

//------------------------------------------------------------------------------

struct settings_save_record
{
    global_settings Settings;
};

//==============================================================================

template<class SERIALIZER>
void SerializeMapSettings( SERIALIZER& Serializer, map_settings& Value )
{
    for( s32 i = 0; i < MAP_CYCLE_SIZE; i++ )
    {
        Serializer.value4b( Value.m_MapCycle[i] );
    }
    Serializer.value4b( Value.m_MapCycleIdx );
    Serializer.value4b( Value.m_MapCycleCount );
    Serializer.value4b( Value.m_bUseDefault );
}

//==============================================================================

template<class SERIALIZER>
void SerializeMultiSettings( SERIALIZER& Serializer, multi_settings& Value )
{
    Serializer.value4b( Value.m_GameTypeID );
    Serializer.value4b( Value.m_ScoreLimit );
    Serializer.value4b( Value.m_TimeLimit );
    s32 MutationMode = (s32)Value.m_MutationMode;
    Serializer.value4b( MutationMode );
    Value.m_MutationMode = (mutation_mode)MutationMode;
    SerializeMapSettings( Serializer, Value.m_MapSettings );
}

//==============================================================================

template<class SERIALIZER>
void SerializeHostSettings( SERIALIZER& Serializer, host_settings& Value )
{
    for( s32 i = 0; i < NET_SERVER_NAME_LENGTH; i++ )
    {
        Serializer.value2b( Value.m_ServerName[i] );
    }
    for( s32 i = 0; i < NET_PASSWORD_LENGTH; i++ )
    {
        Serializer.value1b( Value.m_Password[i] );
    }
    Serializer.value4b( Value.m_GameTypeID );
    Serializer.value4b( Value.m_ScoreLimit );
    Serializer.value4b( Value.m_TimeLimit );
    Serializer.value4b( Value.m_MaxPlayers );
    Serializer.value4b( Value.m_VotePassPct );
    Serializer.value4b( Value.m_FFirePct );
    Serializer.value4b( Value.m_Flags );

    s32 MutationMode = (s32)Value.m_MutationMode;
    Serializer.value4b( MutationMode );
    Value.m_MutationMode = (mutation_mode)MutationMode;
    SerializeMapSettings( Serializer, Value.m_MapSettings );

    s32 SkillLevel = (s32)Value.m_SkillLevel;
    Serializer.value4b( SkillLevel );
    Value.m_SkillLevel = (skill_level)SkillLevel;
}

//==============================================================================

template<class SERIALIZER>
void SerializeJoinSettings( SERIALIZER& Serializer, join_settings& Value )
{
    Serializer.value4b( Value.m_GameTypeID );
    Serializer.value4b( Value.m_MinPlayers );
    Serializer.value4b( Value.m_MutationMode );
    Serializer.value4b( Value.m_PasswordEnabled );
    Serializer.value4b( Value.m_VoiceEnabled );
}

//==============================================================================

template<class SERIALIZER>
void SerializeCheckPoint( SERIALIZER& Serializer, check_point& Value )
{
    Serializer.value4b( Value.bIsValid );
    Serializer.value4b( Value.TableName );
    Serializer.value4b( Value.TitleName );

    u64 RespawnGUID = Value.RespawnGUID.Guid;
    u64 AdvanceGUID = Value.AdvanceGUID.Guid;
    Serializer.value8b( RespawnGUID );
    Serializer.value8b( AdvanceGUID );
    Value.RespawnGUID = guid( RespawnGUID );
    Value.AdvanceGUID = guid( AdvanceGUID );

    Serializer.value4b( Value.CurrWeapon );
    Serializer.value4b( Value.PrevWeapon );
    Serializer.value4b( Value.NextWeapon );
    Serializer.value4b( Value.MutantMelee );
    Serializer.value4b( Value.MutantPrimary );
    Serializer.value4b( Value.MutantSecondary );
    Serializer.value4b( Value.Mutagen );
    Serializer.value4b( Value.Health );
    Serializer.value4b( Value.MaxHealth );

    for( s32 i = 0; i < INVEN_COUNT; i++ )
    {
        Serializer.value4b( Value.Inventory[i] );
    }
    for( s32 i = 0; i < INVEN_NUM_WEAPONS * 2; i++ )
    {
        Serializer.value4b( Value.Ammo[i].Amount );
        Serializer.value4b( Value.Ammo[i].CurrentClip );
    }
}

//==============================================================================

template<class SERIALIZER>
void SerializeLevelCheckPoints( SERIALIZER& Serializer, level_check_points& Value )
{
    Serializer.value4b( Value.MapID );
    Serializer.value4b( Value.nValidCheckPoints );
    Serializer.value4b( Value.iCurrentCheckPoint );
    for( s32 i = 0; i < MAX_CHECKPOINTS; i++ )
    {
        SerializeCheckPoint( Serializer, Value.CheckPoints[i] );
    }
}

//==============================================================================

xbool HasNullTerminator( const char* pText, s32 Capacity )
{
    for( s32 i = 0; i < Capacity; i++ )
    {
        if( pText[i] == '\0' )
        {
            return TRUE;
        }
    }
    return FALSE;
}

} // namespace

//==============================================================================

xbool save_data_codec::ProfileFieldsAreValid( const player_profile& Profile )
{
    return HasNullTerminator( Profile.m_pProfileName,
                              (s32)sizeof(Profile.m_pProfileName) ) &&
           (Profile.m_MouseSensitivity[0] <= MOUSE_SENSITIVITY_MAX) &&
           (Profile.m_MouseSensitivity[1] <= MOUSE_SENSITIVITY_MAX) &&
           (Profile.m_GamepadSensitivity[0] <= GAMEPAD_SENSITIVITY_MAX) &&
           (Profile.m_GamepadSensitivity[1] <= GAMEPAD_SENSITIVITY_MAX) &&
           (Profile.m_UniqueIdLength >= 0) &&
           (Profile.m_UniqueIdLength <= (s32)sizeof(Profile.m_UniqueId)) &&
           (Profile.m_DifficultyLevel <= DIFFICULTY_HARD);
}

//==============================================================================

xbool save_data_codec::SettingsFieldsAreValid( const global_settings& Settings )
{
    return (Settings.m_PatchLength >= 0) &&
           (Settings.m_PatchLength <= NET_MAX_PATCH_SIZE) &&
           (Settings.m_TextLanguage >= XL_LANG_ENGLISH) &&
           (Settings.m_TextLanguage < XL_NUM_LANGUAGES) &&
           (Settings.m_AudioLanguage >= XL_LANG_ENGLISH) &&
           (Settings.m_AudioLanguage < XL_NUM_LANGUAGES) &&
           (Settings.m_VideoLanguage >= XL_LANG_ENGLISH) &&
           (Settings.m_VideoLanguage < XL_NUM_LANGUAGES) &&
           (Settings.m_UIScale >= UI_SCALE_MIN_PERCENT) &&
           (Settings.m_UIScale <= UI_SCALE_MAX_PERCENT) &&
           (Settings.m_HUDScale >= HUD_SCALE_MIN_PERCENT) &&
           (Settings.m_HUDScale <= HUD_SCALE_MAX_PERCENT) &&
           ((Settings.m_DisplayMode == ENG_DISPLAY_WINDOWED) ||
            (Settings.m_DisplayMode == ENG_DISPLAY_BORDERLESS)) &&
           ((Settings.m_PresentMode == ENG_PRESENT_VSYNC) ||
            (Settings.m_PresentMode == ENG_PRESENT_MAILBOX) ||
            (Settings.m_PresentMode == ENG_PRESENT_IMMEDIATE)) &&
           global_settings::IsFrameRateLimitValid( Settings.m_FrameRateLimit ) &&
           ((Settings.m_PresentMode == ENG_PRESENT_IMMEDIATE) ||
            (Settings.m_FrameRateLimit == FrameRateLimit::Auto)) &&
           ((Settings.m_DynamicShadowsEnabled == FALSE) ||
            (Settings.m_DynamicShadowsEnabled == TRUE)) &&
           ((Settings.m_ShadowFilterType == ShadowFilterType::Hard) ||
            (Settings.m_ShadowFilterType == ShadowFilterType::Evsm)) &&
           ((Settings.m_BackgroundBlurEnabled == FALSE) ||
            (Settings.m_BackgroundBlurEnabled == TRUE)) &&
           ((Settings.m_AntiAliasingType == AntiAliasingType::None) ||
            (Settings.m_AntiAliasingType == AntiAliasingType::Cmaa2));
}

//==============================================================================

template<class SERIALIZER>
void save_data_serialize_profile( SERIALIZER& Serializer, player_profile& Value )
{
    for( s32 i = 0; i < (s32)sizeof(Value.m_pProfileName); i++ )
    {
        Serializer.value1b( Value.m_pProfileName[i] );
    }
    Serializer.value4b( Value.m_HashString );
    Serializer.value4b( Value.m_AvatarID );
    for( s32 i = 0; i < 2; i++ )
    {
        Serializer.value1b( Value.m_MouseSensitivity[i] );
        Serializer.value1b( Value.m_GamepadSensitivity[i] );
    }
    for( s32 i = 0; i < 5; i++ )
    {
        Serializer.value1b( Value.m_Volume[i] );
    }
    for( s32 i = 0; i < NUM_VAULTS; i++ )
    {
        Serializer.value1b( Value.m_Lore[i] );
    }
    Serializer.value4b( Value.m_LoreTotal );
    Serializer.value4b( Value.m_bNewLoreUnlocked );
    Serializer.value4b( Value.m_NumSecretsUnlocked );
    Serializer.value4b( Value.m_bNewSecretUnlocked );
    for( s32 i = 0; i < (s32)sizeof(Value.m_UniqueId); i++ )
    {
        Serializer.value1b( Value.m_UniqueId[i] );
    }
    Serializer.value4b( Value.m_UniqueIdLength );
    Serializer.value4b( Value.m_bIsMutated );
    Serializer.value1b( Value.m_CinemaMutatedMsgCount );
    Serializer.value1b( Value.m_DifficultyLevel );
    Serializer.value4b( Value.m_bWeaponAutoSwitch );
    for( s32 i = 0; i < MAX_SAVED_LEVELS; i++ )
    {
        SerializeLevelCheckPoints( Serializer, Value.m_Checkpoints[i] );
    }

    u32 Flags = 0;
    Flags |= Value.m_bLookspringOn      << 0;
    Flags |= Value.m_bCrouchOn          << 1;
    Flags |= Value.m_bIsActive          << 2;
    Flags |= Value.m_bGamepadInvertY    << 3;
    Flags |= Value.m_bVibration         << 4;
    Flags |= Value.m_bIsVisibleOnline   << 5;
    Flags |= Value.m_bAutosaveOn        << 6;
    Flags |= Value.m_bAlienAvatarsOn    << 7;
    Flags |= Value.m_bSecretAwarded     << 8;
    Flags |= Value.m_bHardUnlocked      << 9;
    Flags |= Value.m_bDifficultyChanged << 10;
    Flags |= Value.m_bAgeVerified       << 11;
    Flags |= Value.m_bGameFinished      << 12;
    Flags |= Value.m_bMouseInvertX      << 13;
    Flags |= Value.m_bMouseInvertY      << 14;
    Flags |= Value.m_bGamepadInvertX    << 15;
    Flags |= Value.m_bAimToggleOn       << 16;
    Serializer.value4b( Flags );

    Value.m_bLookspringOn      = (Flags >> 0)  & 1;
    Value.m_bCrouchOn          = (Flags >> 1)  & 1;
    Value.m_bIsActive          = (Flags >> 2)  & 1;
    Value.m_bGamepadInvertY    = (Flags >> 3)  & 1;
    Value.m_bVibration         = (Flags >> 4)  & 1;
    Value.m_bIsVisibleOnline   = (Flags >> 5)  & 1;
    Value.m_bAutosaveOn        = (Flags >> 6)  & 1;
    Value.m_bAlienAvatarsOn    = (Flags >> 7)  & 1;
    Value.m_bSecretAwarded     = (Flags >> 8)  & 1;
    Value.m_bHardUnlocked      = (Flags >> 9)  & 1;
    Value.m_bDifficultyChanged = (Flags >> 10) & 1;
    Value.m_bAgeVerified       = (Flags >> 11) & 1;
    Value.m_bGameFinished      = (Flags >> 12) & 1;
    Value.m_bMouseInvertX      = (Flags >> 13) & 1;
    Value.m_bMouseInvertY      = (Flags >> 14) & 1;
    Value.m_bGamepadInvertX    = (Flags >> 15) & 1;
    Value.m_bAimToggleOn       = (Flags >> 16) & 1;
}

//==============================================================================

template<class SERIALIZER>
void save_data_serialize_settings( SERIALIZER& Serializer, global_settings& Value )
{
    Serializer.value8b( Value.m_DateStamp );
    for( s32 i = 0; i < VOLUME_LAST; i++ )
    {
        Serializer.value4b( Value.m_Volume[i] );
    }
    Serializer.value4b( Value.m_SpeakerSet );
    Serializer.value4b( Value.m_ContentVersion );
    Serializer.value4b( Value.m_PatchVersion );
    Serializer.value4b( Value.m_PatchLength );
    for( s32 i = 0; i < NET_MAX_PATCH_SIZE; i++ )
    {
        Serializer.value1b( Value.m_PatchData[i] );
    }

    s32 HeadsetMode = (s32)Value.m_HeadsetMode;
    Serializer.value4b( HeadsetMode );
    Value.m_HeadsetMode = (headset_mode)HeadsetMode;

    SerializeMultiSettings( Serializer, Value.m_MultiplayerSettings );
    SerializeHostSettings( Serializer, Value.m_HostSettings );
    SerializeJoinSettings( Serializer, Value.m_JoinSettings );
    SerializeMapSettings( Serializer, Value.m_MapSettings );

    Serializer.value4b( Value.m_UIScale );
    Serializer.value4b( Value.m_HUDScale );
    Serializer.value4b( Value.m_ResolutionWidth );
    Serializer.value4b( Value.m_ResolutionHeight );

    s32 DisplayMode = (s32)Value.m_DisplayMode;
    s32 PresentMode = (s32)Value.m_PresentMode;
    Serializer.value4b( DisplayMode );
    Serializer.value4b( PresentMode );
    Value.m_DisplayMode = (eng_display_mode)DisplayMode;
    Value.m_PresentMode = (eng_present_mode)PresentMode;

    Serializer.value4b( Value.m_FieldOfView );
    Serializer.value4b( Value.m_FilmGrainStrength );
    Serializer.value4b( Value.m_DynamicShadowsEnabled );

    s32 ShadowFilter = static_cast<s32>( Value.m_ShadowFilterType );
    Serializer.value4b( ShadowFilter );
    Value.m_ShadowFilterType = static_cast<ShadowFilterType>( ShadowFilter );

    Serializer.value4b( Value.m_VideoVolume );

    s32 TextLanguage  = (s32)Value.m_TextLanguage;
    s32 AudioLanguage = (s32)Value.m_AudioLanguage;
    s32 VideoLanguage = (s32)Value.m_VideoLanguage;
    Serializer.value4b( TextLanguage );
    Serializer.value4b( AudioLanguage );
    Serializer.value4b( VideoLanguage );
    Value.m_TextLanguage  = (x_language)TextLanguage;
    Value.m_AudioLanguage = (x_language)AudioLanguage;
    Value.m_VideoLanguage = (x_language)VideoLanguage;

    s32 FrameLimit = static_cast<s32>( Value.m_FrameRateLimit );
    Serializer.value4b( FrameLimit );
    Value.m_FrameRateLimit = static_cast<FrameRateLimit>( FrameLimit );

    Serializer.value4b( Value.m_BackgroundBlurEnabled );

    s32 AntiAliasing = static_cast<s32>( Value.m_AntiAliasingType );
    Serializer.value4b( AntiAliasing );
    Value.m_AntiAliasingType = static_cast<AntiAliasingType>( AntiAliasing );
}

//==============================================================================

namespace
{

template<class SERIALIZER>
void serialize( SERIALIZER& Serializer, profile_save_record& Value )
{
    save_data_serialize_profile( Serializer, Value.Profile );
}

//==============================================================================

template<class SERIALIZER>
void serialize( SERIALIZER& Serializer, settings_save_record& Value )
{
    save_data_serialize_settings( Serializer, Value.Settings );
}

//==============================================================================

} // namespace

//==============================================================================

xbool save_data_codec::EncodeProfile( const player_profile& Profile,
                                      xarray<u8>&           Bytes,
                                      xstring&              Error )
{
    std::unique_ptr<profile_save_record> Record( new profile_save_record );
    Record->Profile = Profile;
    return bitsery_io::EncodeFile( PROFILE_FILE_FORMAT, *Record, Bytes, Error );
}

//==============================================================================

xbool save_data_codec::DecodeProfile( const xarray<u8>& Bytes,
                                      player_profile&   Profile,
                                      xstring&          Error )
{
    std::unique_ptr<profile_save_record> Record( new profile_save_record );
    Record->Profile.Reset();
    if( !bitsery_io::DecodeFile( Bytes.GetPtr(), Bytes.GetCount(),
                                 PROFILE_FILE_FORMAT, *Record, Error ) )
    {
        return FALSE;
    }
    if( !ProfileFieldsAreValid( Record->Profile ) )
    {
        Error = "Serialized profile contains invalid fields.";
        return FALSE;
    }

    Profile = Record->Profile;
    Profile.m_Checksum = 0;
    Profile.m_Checksum = x_chksum( &Profile, sizeof(Profile) );
    return TRUE;
}

//==============================================================================

xbool save_data_codec::EncodeSettings( const global_settings& Settings,
                                       xarray<u8>&            Bytes,
                                       xstring&               Error )
{
    std::unique_ptr<settings_save_record> Record( new settings_save_record );
    Record->Settings = Settings;
    return bitsery_io::EncodeFile( SETTINGS_FILE_FORMAT, *Record, Bytes, Error );
}

//==============================================================================

xbool save_data_codec::DecodeSettings( const xarray<u8>& Bytes,
                                       global_settings&  Settings,
                                       xstring&          Error )
{
    std::unique_ptr<settings_save_record> Record( new settings_save_record );
    Record->Settings.Reset();
    if( !bitsery_io::DecodeFile( Bytes.GetPtr(), Bytes.GetCount(),
                                 SETTINGS_FILE_FORMAT, *Record, Error ) )
    {
        return FALSE;
    }
    if( !SettingsFieldsAreValid( Record->Settings ) )
    {
        Error = "Serialized settings contain invalid fields.";
        return FALSE;
    }

    Settings = Record->Settings;
    Settings.m_Checksum = 0;
    Settings.m_Checksum = x_chksum( &Settings, sizeof(Settings) );
    return TRUE;
}
