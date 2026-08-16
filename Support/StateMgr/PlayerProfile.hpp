//==============================================================================
//  
//  PlayerProfile.hpp
//  
//==============================================================================

#ifndef PLAYER_PROFILE_HPP
#define PLAYER_PROFILE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "GlobalSettings.hpp"
#include "CheckPointMgr/CheckPointMgr.hpp"
#include "LoreList.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define MAX_SAVED_LEVELS 20

//==============================================================================
//  CONTROL SENSITIVITY LIMITS
//==============================================================================

constexpr u32 MOUSE_SENSITIVITY_DEFAULT   = 16;
constexpr u32 MOUSE_SENSITIVITY_MAX       = 64;
constexpr u32 GAMEPAD_SENSITIVITY_DEFAULT = 50;
constexpr u32 GAMEPAD_SENSITIVITY_MAX     = 200;

//==============================================================================
//  ENUMS
//==============================================================================

enum profile_states
{
    PROFILE_OK,
    PROFILE_CORRUPT,
};

//------------------------------------------------------------------------------

enum difficulty_level
{
    DIFFICULTY_EASY,
    DIFFICULTY_MEDIUM,
    DIFFICULTY_HARD,
};

//------------------------------------------------------------------------------

enum class profile_control_device
{
    Mouse,
    Gamepad,
};

//------------------------------------------------------------------------------

enum class profile_control_axis
{
    X,
    Y,
};

//==============================================================================
//  PLAYER PROFILE
//==============================================================================

class player_profile
{
    friend class save_data_codec;
    template<class SERIALIZER>
    friend void save_data_serialize_profile( SERIALIZER&, player_profile& );

public:
                        player_profile          ( void );
    void                Reset                   ( void );

    void                RestoreControlDefaults  ( void );
    void                RestoreMouseControlDefaults
                                                ( void );
    void                RestoreGamepadControlDefaults
                                                ( void );
    void                RestoreCommonControlDefaults
                                                ( void );

    void                SetProfileName          ( char const* pProfileName );
    char const*         GetProfileName          ( void ) const;

    void                SetHash                 ( void );
    u32                 GetHash                 ( void ) const;

    u32                 GetSensitivity          ( profile_control_device Device,
                                                  profile_control_axis   Axis ) const;
    void                SetSensitivity          ( profile_control_device Device,
                                                  profile_control_axis   Axis,
                                                  u32                    Sensitivity );
    xbool               IsAxisInverted          ( profile_control_device Device,
                                                  profile_control_axis   Axis ) const;
    void                SetAxisInverted         ( profile_control_device Device,
                                                  profile_control_axis   Axis,
                                                  xbool                  IsInverted );
    xbool               IsAimToggleEnabled      ( void ) const;
    void                SetAimToggleEnabled     ( xbool IsEnabled );

    u8                  GetVolume               ( u32 Index ) const;
    void                SetVolume               ( u32 Index, u8 Volume );

    s32                 GetAvatarID             ( void ) const;
    void                SetAvatarID             ( s32 AvatarID );

    xbool               GetLoreAcquired         ( u32 Vault, s32 Index ) const;
    void                SetLoreAcquired         ( u32 Vault, u32 Index );

    void                AcquireSecret           ( void );

    u32                 GetTotalLoreAcquired    ( void ) const;
    xbool               IsNewLoreUnlocked       ( void ) const;
    void                ClearNewLoreUnlocked    ( void );

    u32                 GetNumSecretsUnlocked   ( void ) const;
    xbool               IsNewSecretUnlocked     ( void ) const;
    void                ClearNewSecretUnlocked  ( void );
    
    void                SetUniqueId             ( byte const* pUniqueId, s32 IdLength );
    byte const*         GetUniqueId             ( s32& IdLength ) const;

    xbool               DisplayCinemaMutatedMsg ( void );

    void                SetPlayerIsMutated      ( xbool IsMutated );
    xbool               IsPlayerMutated         ( void ) const;

    void                SetDifficultyLevel      ( u8 Difficulty );
    u8                  GetDifficultyLevel      ( void ) const;

    void                SetWeaponAutoSwitch     ( xbool AutoSwitch );
    xbool               GetWeaponAutoSwitch     ( void ) const;

    void                SetLookspringOn         ( xbool LookspringOn );
    xbool               GetLookspringOn         ( void ) const;
    void                SetCrouchOn             ( xbool CrouchOn );
    xbool               GetCrouchOn             ( void ) const;
    void                SetVibration            ( xbool Vibration );
    xbool               GetVibration            ( void ) const;
    void                SetVisibleOnline        ( xbool VisibleOnline );
    xbool               GetVisibleOnline        ( void ) const;
    void                SetAutosaveOn           ( xbool AutosaveOn );
    xbool               GetAutosaveOn           ( void ) const;
    void                SetAlienAvatarsOn       ( xbool AlienAvatarsOn );
    xbool               GetAlienAvatarsOn       ( void ) const;
    void                SetHardUnlocked         ( xbool HardUnlocked );
    xbool               GetHardUnlocked         ( void ) const;
    void                SetDifficultyChanged    ( xbool DifficultyChanged );
    xbool               GetDifficultyChanged    ( void ) const;
    void                SetAgeVerified          ( xbool AgeVerified );
    xbool               GetAgeVerified          ( void ) const;
    void                SetGameFinished         ( xbool GameFinished );
    xbool               GetGameFinished         ( void ) const;
 
    level_check_points& GetCheckpoint           ( s32 Index );
    level_check_points* GetCheckpointByMapID    ( s32 MapID );

    void                Checksum                ( void );
    xbool               HasChanged              ( void );
    void                MarkDirty               ( void );

#ifndef CONFIG_RETAIL
    void                UnlockAll               ( void );
#endif

private:
    s32                 m_Checksum;                                                     // CRC32 of the profile
    char                m_pProfileName[32];                                             // the nickname (MUST BE FIRST)
    u32                 m_HashString;                                                   // hash of the profile name and the time created
    s32                 m_AvatarID;                                                     // avatar
    u8                  m_MouseSensitivity[2];
    u8                  m_GamepadSensitivity[2];
    u8                  m_Volume[5];                                                    // volume controls
    u8                  m_Lore[NUM_VAULTS];                                             // lore acquired flags
    u32                 m_LoreTotal;                                                    // total lore acquired
    xbool               m_bNewLoreUnlocked;                                             // flag set when new piece of lore is unlocked
    u32                 m_NumSecretsUnlocked;                                           // number of secrets unlocked
    xbool               m_bNewSecretUnlocked;                                           // flag set when a new secret is unlocked
    byte                m_UniqueId[64];
    s32                 m_UniqueIdLength;
    xbool               m_bIsMutated;                                                   // is the player currently in mutant form
    s8                  m_CinemaMutatedMsgCount;                                        // counts how many times we should display a msg
    u8                  m_DifficultyLevel;                                              // campaign game difficulty level
    xbool               m_bWeaponAutoSwitch;                                            // if on/true, will auto-switch to a weapon with a > rating

    level_check_points  m_Checkpoints[ MAX_SAVED_LEVELS ];

    u32                 m_bLookspringOn         : 1;  // toggle lookspring
    u32                 m_bCrouchOn             : 1;  // toggle crouch
    u32                 m_bIsActive             : 1;  // is currently active 
    u32                 m_bGamepadInvertY       : 1;  // invert gamepad Y axis
    u32                 m_bVibration            : 1;  // Force feedback/vibration enabled
    u32                 m_bIsVisibleOnline      : 1;  // Report full status when online
    u32                 m_bAutosaveOn           : 1;  // Autosave is enabled
    u32                 m_bAlienAvatarsOn       : 1;  // Alien avatars are selectable
    u32                 m_bSecretAwarded        : 1;  // Give a secret away after deep underground
    u32                 m_bHardUnlocked         : 1;  // Difficulty level hard is available
    u32                 m_bDifficultyChanged    : 1;  // Was the difficulty level changed during a campaign
    u32                 m_bAgeVerified          : 1;  // The player's age has been verified for this profile (COPA requirement)
    u32                 m_bGameFinished         : 1;  // Player has finished the game.
    u32                 m_bMouseInvertX         : 1;  // invert mouse X axis
    u32                 m_bMouseInvertY         : 1;  // invert mouse Y axis
    u32                 m_bGamepadInvertX       : 1;  // invert gamepad X axis
    u32                 m_bAimToggleOn          : 1;  // toggle aim instead of holding
};

//==============================================================================
//  PROFILE INFO
//==============================================================================

struct profile_info
{
    xbool       bDamaged;
    s32         ProfileID;
    xwstring    Name;
    u32         Hash;           // unique identifier for this profile
    datestamp   CreationDate;   // in platform specific format
    datestamp   ModifiedDate;   // in platform specific format
};

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline char const* player_profile::GetProfileName( void ) const
{
    return m_pProfileName;
}

//==============================================================================

inline u32 player_profile::GetHash( void ) const
{
    return m_HashString;
}

//==============================================================================

inline u8 player_profile::GetVolume( u32 Index ) const
{
    ASSERT( Index < 5 );
    return m_Volume[Index];
}

//==============================================================================

inline void player_profile::SetVolume( u32 Index, u8 Volume )
{
    ASSERT( Index < 5 );
    m_Volume[Index] = static_cast<u8>( x_clamp( static_cast<s32>( Volume ), VOLUME_MIN_PERCENT, VOLUME_MAX_PERCENT ) );
}

//==============================================================================

inline s32 player_profile::GetAvatarID( void ) const
{
    return m_AvatarID;
}

//==============================================================================

inline void player_profile::SetAvatarID( s32 AvatarID )
{
    m_AvatarID = AvatarID;
}

//==============================================================================

inline u32 player_profile::GetTotalLoreAcquired( void ) const
{
    return m_LoreTotal;
}

//==============================================================================

inline xbool player_profile::IsNewLoreUnlocked( void ) const
{
    return m_bNewLoreUnlocked;
}

//==============================================================================

inline void player_profile::ClearNewLoreUnlocked( void )
{
    m_bNewLoreUnlocked = FALSE;
}

//==============================================================================

inline u32 player_profile::GetNumSecretsUnlocked( void ) const
{
    return m_NumSecretsUnlocked;
}

//==============================================================================

inline xbool player_profile::IsNewSecretUnlocked( void ) const
{
    return m_bNewSecretUnlocked;
}

//==============================================================================

inline void player_profile::ClearNewSecretUnlocked( void )
{
    m_bNewSecretUnlocked = FALSE;
}

//==============================================================================

inline void player_profile::SetPlayerIsMutated( xbool IsMutated )
{
    m_bIsMutated = IsMutated;
}

//==============================================================================

inline xbool player_profile::IsPlayerMutated( void ) const
{
    return m_bIsMutated;
}

//==============================================================================

inline void player_profile::SetDifficultyLevel( u8 Difficulty )
{
    m_DifficultyLevel = Difficulty;
}

//==============================================================================

inline u8 player_profile::GetDifficultyLevel( void ) const
{
    return m_DifficultyLevel;
}

//==============================================================================

inline void player_profile::SetWeaponAutoSwitch( xbool AutoSwitch )
{
    m_bWeaponAutoSwitch = AutoSwitch;
}

//==============================================================================

inline xbool player_profile::GetWeaponAutoSwitch( void ) const
{
    return m_bWeaponAutoSwitch;
}

//==============================================================================

inline void player_profile::SetLookspringOn( xbool LookspringOn )
{
    m_bLookspringOn = LookspringOn;
}

//==============================================================================

inline xbool player_profile::GetLookspringOn( void ) const
{
    return m_bLookspringOn;
}

//==============================================================================

inline void player_profile::SetCrouchOn( xbool CrouchOn )
{
    m_bCrouchOn = CrouchOn;
}

//==============================================================================

inline xbool player_profile::GetCrouchOn( void ) const
{
    return m_bCrouchOn;
}

//==============================================================================

inline void player_profile::SetVibration( xbool Vibration )
{
    m_bVibration = Vibration;
}

//==============================================================================

inline xbool player_profile::GetVibration( void ) const
{
    return m_bVibration;
}

//==============================================================================

inline void player_profile::SetVisibleOnline( xbool VisibleOnline )
{
    m_bIsVisibleOnline = VisibleOnline;
}

//==============================================================================

inline xbool player_profile::GetVisibleOnline( void ) const
{
    return m_bIsVisibleOnline;
}

//==============================================================================

inline void player_profile::SetAutosaveOn( xbool AutosaveOn )
{
    m_bAutosaveOn = AutosaveOn;
}

//==============================================================================

inline xbool player_profile::GetAutosaveOn( void ) const
{
    return m_bAutosaveOn;
}

//==============================================================================

inline void player_profile::SetAlienAvatarsOn( xbool AlienAvatarsOn )
{
    m_bAlienAvatarsOn = AlienAvatarsOn;
}

//==============================================================================

inline xbool player_profile::GetAlienAvatarsOn( void ) const
{
    return m_bAlienAvatarsOn;
}

//==============================================================================

inline void player_profile::SetHardUnlocked( xbool HardUnlocked )
{
    m_bHardUnlocked = HardUnlocked;
}

//==============================================================================

inline xbool player_profile::GetHardUnlocked( void ) const
{
    return m_bHardUnlocked;
}

//==============================================================================

inline void player_profile::SetDifficultyChanged( xbool DifficultyChanged )
{
    m_bDifficultyChanged = DifficultyChanged;
}

//==============================================================================

inline xbool player_profile::GetDifficultyChanged( void ) const
{
    return m_bDifficultyChanged;
}

//==============================================================================

inline void player_profile::SetAgeVerified( xbool AgeVerified )
{
    m_bAgeVerified = AgeVerified;
}

//==============================================================================

inline xbool player_profile::GetAgeVerified( void ) const
{
    return m_bAgeVerified;
}

//==============================================================================

inline void player_profile::SetGameFinished( xbool GameFinished )
{
    m_bGameFinished = GameFinished;
}

//==============================================================================

inline xbool player_profile::GetGameFinished( void ) const
{
    return m_bGameFinished;
}

//==============================================================================

inline level_check_points& player_profile::GetCheckpoint( s32 Index )
{
    ASSERT( (Index >= 0) && (Index < MAX_SAVED_LEVELS) );
    return m_Checkpoints[Index];
}

//==============================================================================
#endif // PLAYER_PROFILE_HPP
//==============================================================================

