//=============================================================================
//
//  PC Specific Routines
//
//=============================================================================

#define PLATFORM_PATH   "PC"

//extern d3deng_ToggleWindowMode();

extern void video_SetBrightness( f32 Brightness );
extern f32  video_GetBrightness( void );
extern void video_SetContrast( f32 Contrast );
extern f32  video_GetContrast( void );
extern void video_SetGamma( f32 Gamma );
extern f32  video_GetGamma( void );
extern void video_ResetDefaults( void );

extern void    video_GetDesktopResolution     ( s32& Width, s32& Height );
extern s32     video_GetSupportedModeCount    ( void );
extern xbool   video_GetSupportedMode         ( s32 Index, s32& Width, s32& Height, s32& RefreshRate );
extern s32     video_FindResolutionMode       ( s32 Width, s32 Height );
extern xbool   video_ChangeResolution         ( s32 NewWidth, s32 NewHeight, xbool bFullscreen );
extern xbool   video_ToggleFullscreen         ( void );
extern void    video_SetResolutionMode        ( s32 ModeIndex, xbool bFullscreen );
extern s32     video_GetCurrentModeIndex      ( void );
extern void    eng_GetRes                     ( s32& XRes, s32& YRes );
extern xbool   d3deng_GetWindowedState                 ( void );

//=============================================================================

void InitRenderPlatform( void )
{
    CONTEXT( "InitRenderPlatform" );
}

//=============================================================================

f32 Spd = 2000.0f;

void FreeCam( f32 DeltaTime )
{
    view& View = g_View;

    f32 Move = Spd  * DeltaTime;
    f32 Rot  = R_10 * DeltaTime;
    f32 X    = 0.0f;
    f32 Y    = 0.0f;
    f32 Z    = 0.0f;

    if( input_IsPressed( INPUT_MOUSE_BTN_L ) ) Move *= 4.0f;
    if( input_IsPressed( INPUT_MOUSE_BTN_R ) ) Move *= 0.2f;
    
    if( input_IsPressed( INPUT_KBD_A ) ) X =  Move;
    if( input_IsPressed( INPUT_KBD_D ) ) X = -Move;
    if( input_IsPressed( INPUT_KBD_Q ) ) Y =  Move;
    if( input_IsPressed( INPUT_KBD_Z ) ) Y = -Move;
    if( input_IsPressed( INPUT_KBD_W ) ) Z =  Move;
    if( input_IsPressed( INPUT_KBD_S ) ) Z = -Move;
    
    View.Translate( vector3(    X, 0.0f,    Z ), view::VIEW  );
    View.Translate( vector3( 0.0f,    Y, 0.0f ), view::WORLD );
    
    radian Pitch, Yaw;
    View.GetPitchYaw( Pitch, Yaw );
    
    Pitch += input_GetValue( INPUT_MOUSE_Y_REL ) * Rot;
    Yaw   -= input_GetValue( INPUT_MOUSE_X_REL ) * Rot;
    View.SetRotation( radian3( Pitch, Yaw, R_0 ) );
}

//=============================================================================

xbool HandleInputPlatform( f32 DeltaTime )
{
    static s32 s_DisplayStats = 0;
    static s32 s_DisplayMode  = 0;
    static s32 s_DisplayVideo = 0;

    // Video settings test controls
    {
        const f32 BRIGHTNESS_STEP = 0.1f;
        const f32 CONTRAST_STEP   = 0.1f; 
        const f32 GAMMA_STEP      = 0.1f;

        // Brightness controls: 8 (decrease), 9 (increase)
        if( input_WasPressed( INPUT_KBD_8 ) )
        {
            f32 brightness = video_GetBrightness() - BRIGHTNESS_STEP;
            video_SetBrightness( brightness );
        }
        if( input_WasPressed( INPUT_KBD_9 ) )
        {
            f32 brightness = video_GetBrightness() + BRIGHTNESS_STEP;
            video_SetBrightness( brightness );
        }

        // Contrast controls: - (decrease), + (increase)
        if( input_WasPressed( INPUT_KBD_MINUS ) )
        {
            f32 contrast = video_GetContrast() - CONTRAST_STEP;
            video_SetContrast( contrast );
        }
        if( input_WasPressed( INPUT_KBD_EQUALS ) ) // + key (shift+equals)
        {
            f32 contrast = video_GetContrast() + CONTRAST_STEP;
            video_SetContrast( contrast );
        }

        // Gamma controls: [ (decrease), ] (increase)
        if( input_WasPressed( INPUT_KBD_LBRACKET ) )
        {
            f32 gamma = video_GetGamma() - GAMMA_STEP;
            video_SetGamma( gamma );
        }
        if( input_WasPressed( INPUT_KBD_RBRACKET ) )
        {
            f32 gamma = video_GetGamma() + GAMMA_STEP;
            video_SetGamma( gamma );
        }

        // Reset all video settings: 0
        if( input_WasPressed( INPUT_KBD_0 ) )
        {
            video_ResetDefaults();
        }

        // Toggle video settings display: V
        if( input_WasPressed( INPUT_KBD_V ) )
        {
            s_DisplayVideo = !s_DisplayVideo;
        }

        // Display current video settings
        if( s_DisplayVideo )
        {
            x_printfxy( 0, 10, "INTERVELOP D3DENG DEBUGGER:" );
            x_printfxy( 0, 11, "Brightness: %.2f (8-/9+)", video_GetBrightness() );
            x_printfxy( 0, 12, "Contrast:   %.2f (-/+)", video_GetContrast() );
            x_printfxy( 0, 13, "Gamma:      %.2f ([/])", video_GetGamma() );
        }
    }

    // Resolution and window mode test controls
    {
        static s32 s_CurrentModeIndex = -1;
        static s32 s_DisplayResInfo = 0;
        
        // Initialize mode index if needed
        if( s_CurrentModeIndex == -1 )
        {
            s_CurrentModeIndex = video_GetCurrentModeIndex();
            if( s_CurrentModeIndex == -1 )
                s_CurrentModeIndex = 0;
        }
        
        // Toggle fullscreen/windowed: F10
        if( input_WasPressed( INPUT_KBD_F10 ) )
        {
            if( video_ToggleFullscreen() )
            {
                x_DebugMsg( "Toggled fullscreen mode\n" );
            }
        }
        
        // Previous resolution: F7
        if( input_WasPressed( INPUT_KBD_F7 ) )
        {
            s32 modeCount = video_GetSupportedModeCount();
            if( modeCount > 0 )
            {
                s_CurrentModeIndex--;
                if( s_CurrentModeIndex < 0 )
                    s_CurrentModeIndex = modeCount - 1;
                    
                video_SetResolutionMode( s_CurrentModeIndex, !d3deng_GetWindowedState() );
            }
        }
        
        // Next resolution: F8
        if( input_WasPressed( INPUT_KBD_F8 ) )
        {
            s32 modeCount = video_GetSupportedModeCount();
            if( modeCount > 0 )
            {
                s_CurrentModeIndex++;
                if( s_CurrentModeIndex >= modeCount )
                    s_CurrentModeIndex = 0;
                    
                video_SetResolutionMode( s_CurrentModeIndex, !d3deng_GetWindowedState() );
            }
        }
        
        // Quick resolution shortcuts
        // F1: 640x480
        if( input_WasPressed( INPUT_KBD_F1 ) )
        {
            if( video_ChangeResolution( 640, 480, !d3deng_GetWindowedState() ) )
            {
                s_CurrentModeIndex = video_GetCurrentModeIndex();
            }
        }
        
        // F2: 800x600  
        if( input_WasPressed( INPUT_KBD_F2 ) )
        {
            if( video_ChangeResolution( 800, 600, !d3deng_GetWindowedState() ) )
            {
                s_CurrentModeIndex = video_GetCurrentModeIndex();
            }
        }
        
        // F3: 1024x768
        if( input_WasPressed( INPUT_KBD_F3 ) )
        {
            if( video_ChangeResolution( 1024, 768, !d3deng_GetWindowedState() ) )
            {
                s_CurrentModeIndex = video_GetCurrentModeIndex();
            }
        }
        
        // F4: 1280x1024
        if( input_WasPressed( INPUT_KBD_F4 ) )
        {
            if( video_ChangeResolution( 1280, 1024, !d3deng_GetWindowedState() ) )
            {
                s_CurrentModeIndex = video_GetCurrentModeIndex();
            }
        }
        
        // F5: Desktop resolution
        if( input_WasPressed( INPUT_KBD_F5 ) )
        {
            s32 desktopW, desktopH;
            video_GetDesktopResolution( desktopW, desktopH );
            if( video_ChangeResolution( desktopW, desktopH, !d3deng_GetWindowedState() ) )
            {
                s_CurrentModeIndex = video_GetCurrentModeIndex();
            }
        }
        
        // Toggle resolution info display: R
        if( input_WasPressed( INPUT_KBD_R ) )
        {
            s_DisplayResInfo = !s_DisplayResInfo;
        }
        
        // Display resolution and mode info
        if( s_DisplayResInfo )
        {
            s32 currentW, currentH;
            eng_GetRes( currentW, currentH );
            
            s32 desktopW, desktopH;
            video_GetDesktopResolution( desktopW, desktopH );
            
            s32 modeCount = video_GetSupportedModeCount();
            s32 currentIndex = video_GetCurrentModeIndex();
            
            x_printfxy( 0, 15, "RESOLUTION DEBUGGER:" );
            x_printfxy( 0, 16, "Current: %dx%d %s", currentW, currentH, 
                       d3deng_GetWindowedState() ? "Windowed" : "Fullscreen" );
            x_printfxy( 0, 17, "Desktop: %dx%d", desktopW, desktopH );
            x_printfxy( 0, 18, "Mode: %d/%d", currentIndex+1, modeCount );
            x_printfxy( 0, 19, "F7/F8: Prev/Next Mode" );
            x_printfxy( 0, 20, "F10: Toggle Fullscreen" );
            x_printfxy( 0, 21, "F1-F5: Quick Resolutions" );
            
            // Show current mode details
            if( currentIndex >= 0 && currentIndex < modeCount )
            {
                s32 modeW, modeH, refresh;
                if( video_GetSupportedMode( currentIndex, modeW, modeH, refresh ) )
                {
                    x_printfxy( 0, 22, "Mode %d: %dx%d @ %dHz", currentIndex, modeW, modeH, refresh );
                }
            }
            
            // Show a few neighboring modes
            x_printfxy( 0, 23, "Available modes:" );
            for( s32 i = 0; i < 5 && i < modeCount; i++ )
            {
                s32 showIndex = (currentIndex - 2 + i + modeCount) % modeCount;
                s32 modeW, modeH, refresh;
                if( video_GetSupportedMode( showIndex, modeW, modeH, refresh ) )
                {
                    char marker = (showIndex == currentIndex) ? '*' : ' ';
                    x_printfxy( 0, 24 + i, "%c%d: %dx%d", marker, showIndex, modeW, modeH );
                }
            }
        }
    }

    if( g_FreeCam == FALSE )
        return( TRUE );

    FreeCam( DeltaTime );

    if( input_IsPressed ( INPUT_KBD_LSHIFT ) &&
        input_WasPressed( INPUT_KBD_C ) )
        SaveCamera();

#if ENABLE_RENDER_STATS
    if( input_IsPressed ( INPUT_KBD_LSHIFT ) &&
        input_WasPressed( INPUT_KBD_P ) )
    {
        switch( s_DisplayStats )
        {
            case 0 : s_DisplayMode = render::stats::TO_SCREEN;
                     break;
            
            case 1 : s_DisplayMode = render::stats::TO_SCREEN | render::stats::VERBOSE;
                     break;
        
            case 2 : render::GetStats().ClearAll();
                     s_DisplayMode = 0;
                     break;
        }
        
        s_DisplayStats++;
        if( s_DisplayStats > 2 )
            s_DisplayStats = 0;
    }
    
    render::GetStats().Print( s_DisplayMode );
#endif //ENABLE_RENDER_STATS

    return( TRUE );
}

//=============================================================================

void PrintStatsPlatform( void )
{
}

//=============================================================================

void EndRenderPlatform( void )
{
    CONTEXT( "EndRenderPlatform" );
}

//=============================================================================

