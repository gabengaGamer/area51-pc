//=========================================================================
//
// d3deng_video.hpp
//
//=========================================================================

#ifndef D3DENG_VIDEO_HPP
#define D3DENG_VIDEO_HPP

//=========================================================================
// FUNCTIONS
//=========================================================================

void    video_Init              ( void );
void    video_Kill              ( void );

void    video_OnDeviceLost      ( void );
void    video_OnDeviceReset     ( void );

void    video_SetBrightness     ( f32 Brightness );
f32     video_GetBrightness     ( void );

void    video_SetContrast       ( f32 Contrast );
f32     video_GetContrast       ( void );

void    video_SetGamma          ( f32 Gamma );
f32     video_GetGamma          ( void );

void    video_ResetDefaults     ( void );

void    video_ApplyPostEffect   ( void );

xbool   video_IsEnabled         ( void );

//=========================================================================
// RES
//=========================================================================

void    video_GetDesktopResolution     ( s32& Width, s32& Height );
s32     video_GetSupportedModeCount    ( void );
xbool   video_GetSupportedMode         ( s32 Index, s32& Width, s32& Height, s32& RefreshRate );
s32     video_FindResolutionMode       ( s32 Width, s32 Height );
void    video_EnumerateDisplayModes    ( void );
xbool   video_ChangeResolution         ( s32 NewWidth, s32 NewHeight, xbool bFullscreen );
xbool   video_ToggleFullscreen         ( void );
void    video_SetResolutionMode        ( s32 ModeIndex, xbool bFullscreen );
s32     video_GetCurrentModeIndex      ( void );

#endif // D3DENG_VIDEO_HPP