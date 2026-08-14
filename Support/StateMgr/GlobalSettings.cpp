//==============================================================================
//  
//  GlobalSettings.cpp
//  
//==============================================================================

//==============================================================================
// INCLUDES 
//==============================================================================

#include "GlobalSettings.hpp"
#include "Configuration/GameConfig.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "StateMgr.hpp"
#include "NetworkMgr/Voice/VoiceMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "MoviePlayer/MoviePlayer.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_renderer.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "Objects/Player/Player.hpp"
#include "Objects/Render/PostEffectMgr.hpp"
#include "Render/ShadowMapMgr.hpp"

//==============================================================================
// CONST 
//==============================================================================

static x_language s_DefaultLocalizationLanguage = XL_LANG_ENGLISH;

//==============================================================================
// IMPLEMENTATION 
//==============================================================================

global_settings::global_settings( void )
{
    Reset( RESET_ALL );
}

//==============================================================================

void global_settings::Reset( s32 ResetFlags )
{
    if( ResetFlags == RESET_ALL )
    {
        x_memset( this, 0, sizeof(*this) );

        ResetMultiplayerDefaults();
        ResetHostDefaults();
        ResetJoinDefaults();
    }

    if( ResetFlags & RESET_HEADSET )
    {
        ResetHeadsetDefaults();
    }

    if( ResetFlags & RESET_AUDIO )
    {
        ResetAudioDefaults();
    }

    if( ResetFlags & RESET_LOCALIZATION )
    {
        ResetLocalizationDefaults();
    }

    if( ResetFlags & RESET_GRAPHICS )
    {
        ResetGraphicsDefaults();
    }

    if( ResetFlags & RESET_DISPLAY )
    {
        ResetDisplayDefaults();
    }

    if( ResetFlags == RESET_ALL )
    {
        m_Checksum = 0;
        m_Checksum = x_chksum( this, sizeof(*this) );
    }
}

//==============================================================================

void global_settings::ResetMapSettings( map_settings& Settings )
{
    Settings.m_bUseDefault   = TRUE;
    Settings.m_MapCycleCount = 0;
    Settings.m_MapCycleIdx   = 0;

    for( s32 i = 0; i < MAP_CYCLE_SIZE; i++ )
    {
        Settings.m_MapCycle[i] = -1;
    }
}

//==============================================================================

void global_settings::ResetMultiplayerDefaults( void )
{
    m_MultiplayerSettings.m_GameTypeID   = GAME_DM;
    m_MultiplayerSettings.m_ScoreLimit   = -1;
    m_MultiplayerSettings.m_TimeLimit    = -1;
    m_MultiplayerSettings.m_MutationMode = MUTATE_CHANGE_AT_WILL;
    ResetMapSettings( m_MultiplayerSettings.m_MapSettings );
}

//==============================================================================

void global_settings::ResetHostDefaults( void )
{
    x_wstrcpy( m_HostSettings.m_ServerName, g_StringTableMgr( "ui", "IDS_HOST_SERVER_NAME" ) );
    x_strcpy( m_HostSettings.m_Password, "" );

    m_HostSettings.m_GameTypeID   = GAME_DM;
    m_HostSettings.m_ScoreLimit   = -1;
#ifdef LAN_PARTY_BUILD
    m_HostSettings.m_TimeLimit    = 600;
    m_HostSettings.m_MaxPlayers   = 8;
#else
    m_HostSettings.m_TimeLimit    = -1;
    m_HostSettings.m_MaxPlayers   = 16;
#endif
    m_HostSettings.m_VotePassPct  = 50;
    m_HostSettings.m_Flags        = SERVER_ENABLE_MAP_SCALING | SERVER_VOICE_ENABLED;
    m_HostSettings.m_MutationMode = MUTATE_CHANGE_AT_WILL;
    ResetMapSettings( m_HostSettings.m_MapSettings );
}

//==============================================================================

void global_settings::ResetJoinDefaults( void )
{
    // GAME_MP as a game type ID means any multiplayer game.
    m_JoinSettings.m_GameTypeID      = GAME_MP;
    m_JoinSettings.m_MinPlayers      = -1;
    m_JoinSettings.m_MutationMode    = -1;
    m_JoinSettings.m_PasswordEnabled = -1;
    m_JoinSettings.m_VoiceEnabled    = -1;
}

//==============================================================================

void global_settings::ResetAudioDefaults( void )
{
    SetVolume( VOLUME_SFX, 100 );
    SetVolume( VOLUME_MUSIC, 75 );
    SetVolume( VOLUME_SPEECH, 100 );
    SetVideoVolume( 100 );
    SetSpeakerSet( SPEAKERS_STEREO );
}

//==============================================================================

void global_settings::ResetHeadsetDefaults( void )
{
    SetVolume( VOLUME_MIC, 50 );
    SetVolume( VOLUME_SPEAKER, 100 );
    SetHeadsetMode( HEADSET_HEADSET_ONLY );
}

//==============================================================================

void global_settings::ResetLocalizationDefaults( void )
{
    SetTextLanguage ( s_DefaultLocalizationLanguage );
    SetAudioLanguage( s_DefaultLocalizationLanguage );
    SetVideoLanguage( s_DefaultLocalizationLanguage );
}

//==============================================================================

void global_settings::SetDefaultLocalizationLanguage( x_language Language )
{
    ASSERT( (Language >= XL_LANG_ENGLISH) && (Language < XL_NUM_LANGUAGES) );
    s_DefaultLocalizationLanguage = Language;
}

//==============================================================================

x_language global_settings::GetDefaultLocalizationLanguage( void )
{
    return s_DefaultLocalizationLanguage;
}

//==============================================================================

void global_settings::ResetGraphicsDefaults( void )
{
    SetFieldOfView( FIELD_OF_VIEW_DEFAULT_DEGREES );
    SetFilmGrainStrength( FILM_GRAIN_DEFAULT_STRENGTH );
    SetBackgroundBlurEnabled( POST_EFFECT_BACKGROUND_BLUR_DEFAULT );
    SetAntiAliasingType( ANTI_ALIASING_TYPE_DEFAULT );
    SetDynamicShadowsEnabled( DYNAMIC_SHADOWS_DEFAULT );
    SetShadowFilterType( SHADOW_FILTER_TYPE_DEFAULT );
}

//==============================================================================

void global_settings::ResetDisplayDefaults( void )
{
    SetUIScale( UI_SCALE_DEFAULT_PERCENT );
    SetHUDScale( HUD_SCALE_DEFAULT_PERCENT );
    SetDisplayResolution( 0, 0 );
    SetDisplayMode( ENG_DISPLAY_BORDERLESS );
    SetPresentMode( ENG_PRESENT_VSYNC );
    SetFrameRateLimit( FRAME_RATE_LIMIT_DEFAULT );
}

//==============================================================================

void global_settings::Commit( void )
{
    // Commit any settings required while hosting a game.
    g_PendingConfig.SetServerName( m_HostSettings.m_ServerName );

    CommitAudio();
    CommitLocalization();
    CommitGraphics();

    if( g_NetworkMgr.IsServer() )
    {
        g_VoiceMgr.SetIsGameVoiceEnabled( m_HostSettings.m_Flags & SERVER_VOICE_ENABLED );
    }

    UpdateHeadsetMode( GetHeadsetMode() );

    g_MatchMgr.SetLocalManifestVersion( m_ContentVersion );
    Checksum();
}

//==============================================================================

void global_settings::CommitStartup( void )
{
    NormalizeGraphicsSettings();

    x_SetLocale( GetTextLanguage() );
    Movie.SetLanguage( GetVideoLanguage() );
    Movie.SetVolume( static_cast<f32>( GetVideoVolume() ) / 100.0f );

    CommitDisplaySettings();
}

//==============================================================================

void global_settings::CommitAudio( void )
{
    // set up audio controls
    g_AudioMgr.SetSFXVolume  ( static_cast<f32>( GetVolume( VOLUME_SFX    ) ) / 100.0f );
    g_AudioMgr.SetMusicVolume( static_cast<f32>( GetVolume( VOLUME_MUSIC  ) ) / 100.0f );
    g_AudioMgr.SetVoiceVolume( static_cast<f32>( GetVolume( VOLUME_SPEECH ) ) / 100.0f );
    Movie.SetVolume( static_cast<f32>( GetVideoVolume() ) / 100.0f );

    // Setup headset volumes
    f32 const HeadsetVolume    = static_cast<f32>( GetVolume( VOLUME_SPEAKER ) ) / 100.0f;
    f32 const MicrophoneVolume = static_cast<f32>( GetVolume( VOLUME_MIC     ) ) / 100.0f;
    g_VoiceMgr.GetHeadset().SetVolume( HeadsetVolume, MicrophoneVolume );
}

//==============================================================================

void global_settings::CommitLocalization( void )
{
    x_language const PreviousTextLanguage  = x_GetLocale();
    x_language const PreviousAudioLanguage = g_AudioMgr.GetLanguage();

    x_SetLocale( GetTextLanguage() );
    g_AudioMgr.SetLanguage( GetAudioLanguage() );
    Movie.SetLanguage( GetVideoLanguage() );

    if( PreviousTextLanguage != GetTextLanguage() )
    {
        if( !g_StringTableMgr.ReloadLocalizedTables() )
        {
            LOG_ERROR( "global_settings::CommitLocalization",
                       "Unable to reload localized string tables." );
        }
    }

    if( PreviousAudioLanguage != GetAudioLanguage() )
    {
        xarray<xstring> Packages;
        g_AudioMgr.GetLoadedPackageLookupNames( Packages );

        for( s32 i = 0; i < Packages.GetCount(); i++ )
        {
            char const* pPackageName = Packages[i];
            char const* pFilename = pPackageName;

            for( char const* p = pPackageName; *p; p++ )
            {
                if( (*p == '\\') || (*p == '/') )
                {
                    pFilename = p + 1;
                }
            }

            if( x_strncmp( pFilename, "DX_", 3 ) == 0 )
            {
                g_RscMgr.Refresh( pPackageName );
            }
        }
    }
}

//==============================================================================

void global_settings::CommitGraphics( void )
{
    NormalizeGraphicsSettings();
    CommitInterfaceSettings();
    CommitRenderSettings();
    CommitDisplaySettings();
}

//==============================================================================

void global_settings::NormalizeGraphicsSettings( void )
{
    SetUIScale( m_UIScale );
    SetHUDScale( m_HUDScale );
    SetFieldOfView( m_FieldOfView );
    SetFilmGrainStrength( m_FilmGrainStrength );
    SetBackgroundBlurEnabled( m_BackgroundBlurEnabled );

    if( (m_AntiAliasingType != AntiAliasingType::None) &&
        (m_AntiAliasingType != AntiAliasingType::Cmaa2) )
    {
        m_AntiAliasingType = ANTI_ALIASING_TYPE_DEFAULT;
    }

    if( ((m_ResolutionWidth <= 0) && (m_ResolutionHeight > 0)) ||
        ((m_ResolutionWidth > 0) && (m_ResolutionHeight <= 0)) )
    {
        m_ResolutionWidth  = 0;
        m_ResolutionHeight = 0;
    }

    if( (m_DisplayMode != ENG_DISPLAY_WINDOWED) &&
        (m_DisplayMode != ENG_DISPLAY_BORDERLESS) )
    {
        m_DisplayMode = ENG_DISPLAY_BORDERLESS;
    }

    if( (m_PresentMode != ENG_PRESENT_VSYNC) &&
        (m_PresentMode != ENG_PRESENT_MAILBOX) &&
        (m_PresentMode != ENG_PRESENT_IMMEDIATE) )
    {
        m_PresentMode = ENG_PRESENT_VSYNC;
    }

    if( !IsFrameRateLimitValid( m_FrameRateLimit ) ||
        (m_PresentMode != ENG_PRESENT_IMMEDIATE) )
    {
        m_FrameRateLimit = FrameRateLimit::Auto;
    }

    if( (m_ShadowFilterType != ShadowFilterType::Hard) &&
        (m_ShadowFilterType != ShadowFilterType::Evsm) )
    {
        m_ShadowFilterType = SHADOW_FILTER_TYPE_DEFAULT;
    }
}

//==============================================================================

xbool global_settings::IsFrameRateLimitValid( FrameRateLimit Limit )
{
    return (Limit == FrameRateLimit::Auto)   ||
           (Limit == FrameRateLimit::Fps30)  ||
           (Limit == FrameRateLimit::Fps60)  ||
           (Limit == FrameRateLimit::Fps90)  ||
           (Limit == FrameRateLimit::Fps120) ||
           (Limit == FrameRateLimit::Fps144) ||
           (Limit == FrameRateLimit::Fps165) ||
           (Limit == FrameRateLimit::Fps240);
}

//==============================================================================

void global_settings::CommitInterfaceSettings( void )
{
    f32 const UIScale  = static_cast<f32>( GetUIScale() ) / 100.0f;
    f32 const HUDScale = static_cast<f32>( GetHUDScale() ) / 100.0f;

    if( g_UiMgr )
    {
        g_UiMgr->SetUserScale( UIScale );
    }
    g_UIRenderer.SetHudUserScale( HUDScale );
}

//==============================================================================

void global_settings::CommitRenderSettings( void )
{
    g_PostEffectMgr.SetFilmGrainStrength( GetFilmGrainStrength() );
    g_PostEffectMgr.SetBackgroundBlurEnabled( GetBackgroundBlurEnabled() );
    g_ShadowMapMgr.SetShadowFilterType( GetShadowFilterType() );
    g_ShadowMapMgr.SetEnabled( GetDynamicShadowsEnabled() );

    radian const FieldOfView = DEG_TO_RAD( static_cast<f32>( GetFieldOfView() ) );
    for( slot_id PlayerSlot = g_ObjMgr.GetFirst( object::TYPE_PLAYER );
         PlayerSlot != SLOT_NULL;
         PlayerSlot = g_ObjMgr.GetNext( PlayerSlot ) )
    {
        object* pObject = g_ObjMgr.GetObjectBySlot( PlayerSlot );
        if( pObject )
        {
            static_cast<player*>( pObject )->SetFieldOfView( FieldOfView );
        }
    }
}

//==============================================================================

void global_settings::CommitDisplaySettings( void )
{
    s32 CurrentWidth  = 0;
    s32 CurrentHeight = 0;
    eng_GetRes( CurrentWidth, CurrentHeight );

    if( (m_ResolutionWidth <= 0) || (m_ResolutionHeight <= 0) )
    {
        CaptureGraphics();
        return;
    }

    eng_display_mode const CurrentDisplayMode = eng_GetDisplayMode();

    if( (m_ResolutionWidth != CurrentWidth) ||
        (m_ResolutionHeight != CurrentHeight) )
    {
        eng_SetResolution( m_ResolutionWidth, m_ResolutionHeight );
    }

    if( m_DisplayMode != CurrentDisplayMode )
    {
        eng_SetDisplayMode( m_DisplayMode );
    }

    if( (m_PresentMode != eng_GetPresentMode()) &&
        !eng_SetPresentMode( m_PresentMode ) )
    {
        x_DebugMsg( "Global settings: failed to apply present mode\n" );
    }
}

//==============================================================================

void global_settings::CaptureGraphics( void )
{
    s32 Width  = 0;
    s32 Height = 0;
    eng_GetRes( Width, Height );
    SetDisplayResolution( Width, Height );
    SetDisplayMode( eng_GetDisplayMode() );
    SetPresentMode( eng_GetPresentMode() );
}

//==============================================================================

void global_settings::SetUIScale( s32 Percent )
{
    m_UIScale = x_clamp( Percent, UI_SCALE_MIN_PERCENT, UI_SCALE_MAX_PERCENT );
}

//==============================================================================

void global_settings::SetHUDScale( s32 Percent )
{
    m_HUDScale = x_clamp( Percent, HUD_SCALE_MIN_PERCENT, HUD_SCALE_MAX_PERCENT );
}

//==============================================================================

void global_settings::SetDisplayResolution( s32 Width, s32 Height )
{
    m_ResolutionWidth  = Width;
    m_ResolutionHeight = Height;
}

//==============================================================================

void global_settings::SetFieldOfView( s32 Degrees )
{
    m_FieldOfView = x_clamp( Degrees, FIELD_OF_VIEW_MIN_DEGREES, FIELD_OF_VIEW_MAX_DEGREES );
}

//==============================================================================

void global_settings::SetFilmGrainStrength( s32 Strength )
{
    m_FilmGrainStrength = x_clamp( Strength, FILM_GRAIN_MIN_STRENGTH, FILM_GRAIN_MAX_STRENGTH );
}

//==============================================================================

void global_settings::SetBackgroundBlurEnabled( xbool Enabled )
{
    m_BackgroundBlurEnabled = Enabled;
}

//==============================================================================

xbool global_settings::GetBackgroundBlurEnabled( void ) const
{
    return m_BackgroundBlurEnabled;
}

//==============================================================================

void global_settings::SetAntiAliasingType( AntiAliasingType Type )
{
    ASSERT( (Type == AntiAliasingType::None) ||
            (Type == AntiAliasingType::Cmaa2) );

    m_AntiAliasingType = Type;
}

//==============================================================================

AntiAliasingType global_settings::GetAntiAliasingType( void ) const
{
    return m_AntiAliasingType;
}

//==============================================================================

void global_settings::SetDynamicShadowsEnabled( xbool Enabled )
{
    m_DynamicShadowsEnabled = Enabled;
}

//==============================================================================

xbool global_settings::GetDynamicShadowsEnabled( void ) const
{
    return m_DynamicShadowsEnabled;
}

//==============================================================================

void global_settings::SetShadowFilterType( ShadowFilterType Type )
{
    ASSERT( (Type == ShadowFilterType::Hard) || (Type == ShadowFilterType::Evsm) );

    m_ShadowFilterType = Type;
}

//==============================================================================

ShadowFilterType global_settings::GetShadowFilterType( void ) const
{
    return m_ShadowFilterType;
}

//==============================================================================

xbool global_settings::HasChanged( void )
{
    s32 const DesiredChecksum = m_Checksum;
    m_Checksum = 0;
    s32 const ActualChecksum = x_chksum( this, sizeof(*this) );
    m_Checksum = DesiredChecksum;
    return DesiredChecksum != ActualChecksum;
}

//==============================================================================

void global_settings::MarkDirty( void )
{
    m_Checksum = 0;
}

//==============================================================================

void global_settings::Checksum( void )
{
    m_Checksum  = 0;
    m_DateStamp = eng_GetDate();
    m_Checksum  = x_chksum( this, sizeof(*this) );
}

//==============================================================================

void global_settings::UpdateHeadsetMode( headset_mode HeadsetMode )
{
    xbool IsVoiceEnabled = TRUE;
    xbool IsVoiceAudible = TRUE;
    xbool IsThroughSpeakers = FALSE;

    switch( HeadsetMode )
    {
        case HEADSET_HEADSET_ONLY:
            IsVoiceAudible    = TRUE;
            IsThroughSpeakers = FALSE;
            IsVoiceEnabled    = TRUE;
            break;

        case HEADSET_THROUGH_SPEAKERS:
            IsVoiceAudible    = TRUE;
            IsThroughSpeakers = TRUE;
            IsVoiceEnabled    = TRUE;
            break;

        case HEADSET_DISABLED:
            IsVoiceAudible    = FALSE;
            IsThroughSpeakers = FALSE;
            IsVoiceEnabled    = FALSE;
            break;

        default:
            ASSERT( FALSE );
            break;
    }

    g_VoiceMgr.SetVoiceEnabled( IsVoiceEnabled );
    g_VoiceMgr.SetVoiceAudible( IsVoiceAudible );
    g_VoiceMgr.SetVoiceThruSpeakers( IsThroughSpeakers );

    // On the server we must update the game manager's voice peripheral status directly
    if( g_NetworkMgr.IsServer() == TRUE )
    {
        GameMgr.SetVoiceCapable( 0, IsVoiceEnabled );
    }
}

//==============================================================================

void global_settings::SetPatchData( void const* pPatchData, s32 Size, s32 Version )
{
    m_PatchVersion = Version;
    m_PatchLength  = Size;
    x_memcpy( m_PatchData, pPatchData, Size );
}
