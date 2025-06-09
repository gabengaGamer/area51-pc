//=========================================================================
//
//  MoviePlayer_Bink.hpp
//
//=========================================================================

#ifndef MOVIEPLAYER_BINK_HPP
#define MOVIEPLAYER_BINK_HPP

//=========================================================================
// INCLUDES
//=========================================================================

#include "x_target.hpp"

#if !defined(TARGET_XBOX) && !defined(TARGET_PC)
#error This file should not be included for your platform. Please check your exclusions on your project spec.
#endif

#include "x_files.hpp"
#include "Entropy.hpp"
#include "audio/audio_hardware.hpp"

//=========================================================================
// XBOX PLATFORM DEFINITIONS
//=========================================================================

#ifdef TARGET_XBOX
#define MOVIE_FIXED_WIDTH       640
#define MOVIE_STRIP_WIDTH       640
#define MOVIE_STRIP_HEIGHT      512

#define MOVIE_BITMAP_FORMAT     (xbitmap::FMT_32_ARGB_8888)
#define BINK_BITMAP_FORMAT      BINKSURFACE32
#define XBITMAP_FLAGS           (0)
#define __RADXBOX__

#include <3rdParty\BinkXbox\Include\bink.h>
#endif

//=========================================================================
// PC PLATFORM DEFINITIONS
//=========================================================================

#ifdef TARGET_PC
#define MOVIE_FIXED_WIDTH       640
#define MOVIE_STRIP_WIDTH       640

#define MOVIE_BITMAP_FORMAT     (xbitmap::FMT_32_URGB_8888)
#define BINK_BITMAP_FORMAT      BINKSURFACE32
#define XBITMAP_FLAGS           (0)
#define __RADWIN__

// Actually Bink XBOX is multi-platform, so we can use its libraries for PC,
// except binkw32.lib and binkw32.dll
#include <3rdParty\BinkXBOX\Include\bink.h>
#endif

//=========================================================================
// BINK MOVIE PRIVATE IMPLEMENTATION
//=========================================================================

class movie_private
{
public:
                        movie_private   ( void );

    // Core functionality
    void                Init            ( void );
    void                Kill            ( void );
    void                Shutdown        ( void );
    xbool               Open            ( const char* pFilename, xbool PlayResident, xbool IsLooped );
    void                Close           ( void );

    // Playback control
    void                Pause           ( void );
    void                Resume          ( void );

    // Audio control
    void                SetVolume       ( f32 Volume );
    void                SetLanguage     ( s32 Language )        { m_Language = Language; }

    // Bitmap management
    xbitmap*            Decode          ( void );
    void                ReleaseBitmap   ( xbitmap* pBitmap );
    s32                 GetBitmapCount  ( void )                { return m_nBitmaps; }

    // Status queries
    s32                 GetWidth        ( void )                { return m_Width; }
    s32                 GetHeight       ( void )                { return m_Height; }
    xbool               IsRunning       ( void )                { return m_IsRunning; }
    xbool               IsFinished      ( void )                { return m_IsFinished; }
    s32                 GetCurrentFrame ( void )                { return m_CurrentFrame; }
    s32                 GetFrameCount   ( void )                { return m_FrameCount; }
    xbool               CachingComplete ( void )                { return TRUE; }

#ifdef TARGET_PC     
    // DirectX9 device management
    void                OnDeviceLost    ( void );
    void                OnDeviceReset   ( void );
    
    // Rendering control
    void                SetRenderSize   ( const vector2& Size ) { m_RenderSize = Size; }
    void                SetRenderPos    ( const vector2& Pos )  { m_RenderPos = Pos; }
    void                SetForceStretch ( xbool bForceStretch ) { m_bForceStretch = bForceStretch; }
    xbool               IsForceStretch  ( void )                { return m_bForceStretch; }
#endif

#ifdef TARGET_XBOX
    void                SetPos          ( vector3& Pos )        { m_Pos = Pos; }
#endif

private:
    // Core state
    xbool               m_IsRunning;            // Player background thread is running
    xbool               m_IsFinished;           // Movie is complete
    xbool               m_IsLooped;             // Loop playback
    
    // Bitmap management
    xbitmap*            m_pBitmaps[2];          // Double buffered bitmaps
    xmesgq*             m_pqFrameAvail;         // Frame availability queue
    s32                 m_nMaxBitmaps;          // Number of bitmaps allocated (for 640 wide)
    s32                 m_nBitmaps;             // Number of bitmaps used
    
    // Video properties
    s32                 m_Width;                // Width in pixels
    s32                 m_Height;               // Height in pixels
    s32                 m_CurrentFrame;         // Current frame number
    s32                 m_FrameCount;           // Total frame count
    
    // Audio properties
    f32                 m_Volume;               // Audio volume
    s32                 m_Language;             // Audio language track
    
    // Bink handle
    HBINK               m_Handle;               // Bink video handle
    
#ifdef TARGET_PC 
    vector2             m_RenderSize;           // Render target size
    vector2             m_RenderPos;            // Render position
    xbool               m_bForceStretch;        // Force stretch to fill screen
    xbool               m_bDeviceLost;          // Track DirectX device state
    
    IDirect3DSurface9*  m_Surface;              // DirectX9 surface for video
#endif

#ifdef TARGET_XBOX
    vector3             m_Pos;                  // 3D position for Xbox
#endif
};

//=========================================================================
#endif // MOVIEPLAYER_BINK_HPP
//=======================================================================