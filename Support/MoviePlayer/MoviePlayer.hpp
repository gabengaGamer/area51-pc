//=========================================================================
//
//  MoviePlayer.hpp
//
//=========================================================================

#ifndef MOVIEPLAYER_HPP
#define MOVIEPLAYER_HPP

//=========================================================================
// INCLUDES
//=========================================================================

#include "x_target.hpp"
#include "x_types.hpp"
#include "x_math.hpp"

//=========================================================================
// LANGUAGE LIST
//=========================================================================

// The audio stream should use THIS order when adding the languages to the movie stream.
// This order is defined in x_locale.hpp
// XL_LANG_ENGLISH = 0,
// XL_LANG_FRENCH,
// XL_LANG_GERMAN,
// XL_LANG_ITALIAN,
// XL_LANG_SPANISH,
// XL_LANG_DUTCH,
// XL_LANG_JAPANESE,
// XL_LANG_KOREAN,
// XL_LANG_PORTUGUESE,
// XL_LANG_TCHINESE,

// TODO: ADD THIS STUFF:
// XL_LANG_GOIDA

//=========================================================================
// GLOBAL FUNCTIONS
//=========================================================================

s32 PlaySimpleMovie( const char* movieName );

//=========================================================================
// PLATFORM SPECIFIC INCLUDES
//=========================================================================

#if defined(TARGET_XBOX) || defined(TARGET_PC)
    #include "MoviePlayer_Bink.hpp"
#elif defined(TARGET_PS2)
    #include "MoviePlayer_PS2.hpp"
#else 
	// Nothing here for now...
#endif


//=========================================================================
// MOVIE PLAYER CLASS
//=========================================================================

class movie_player
{
public:
                        movie_player    ( void );
                       ~movie_player    ( void );

    // Core functionality
    void                Init            ( void );
    void                Kill            ( void );
    xbool               Open            ( const char* pFilename, xbool PlayResident, xbool IsLooped );
    void                Close           ( void );

    // Playback control
    void                Play            ( void );
    void                Stop            ( void );
    void                Pause           ( void )                            { m_Private.Pause();                    }
    void                Resume          ( void )                            { m_Private.Resume();                   }

    // Rendering
    void                Render          ( const vector2& Pos, const vector2& Size, xbool InRenderLoop = FALSE );

    // Audio control
    void                SetVolume       ( f32 Volume )                      { m_Private.SetVolume(Volume);          }
    void                SetLanguage     ( const s32 Language );

    // Status queries
    xbool               IsPlaying       ( void )                            { return !m_Private.IsFinished();       }
    s32                 GetWidth        ( void )                            { return m_Private.GetWidth();          }
    s32                 GetHeight       ( void )                            { return m_Private.GetHeight();         }
    xbool               CachingComplete ( void )                            { return m_Private.CachingComplete();   }

#ifdef TARGET_PC
    // Device management for PC/DirectX9
    void                OnDeviceLost    ( void )                            { m_Private.OnDeviceLost();             }
    void                OnDeviceReset   ( void )                            { m_Private.OnDeviceReset();            }
#endif

private:
    movie_private       m_Private;          // Platform-specific implementation
    xbool               m_Shutdown;         // Shutdown flag
    byte*               m_pMovieBuffer;     // Movie data buffer
    xbool               m_IsLooped;         // Loop playback flag
    xbool               m_Finished;         // Playback finished flag
    xbool               m_FramePending;     // Frame pending flag
    xbitmap*            m_pBitmaps;         // Bitmap storage

    friend  void        s_MoviePlayerThread( void* );
};

//=========================================================================
// GLOBAL INSTANCES
//=========================================================================

extern movie_player Movie;

//=========================================================================
// GLOBAL DEVICE MANAGEMENT (PC/DIRECTX9)
//=========================================================================

#ifdef TARGET_PC
void movie_OnDeviceLost ( void );
void movie_OnDeviceReset( void );
#endif

//=========================================================================
// CONVENIENCE FUNCTIONS
//=========================================================================

xbool       movie_Play          ( const char* pFilename, const xbool IsLooped = FALSE, const xbool Modal = TRUE );
void        movie_Stop          ( void );
void        movie_Pause         ( void );
xbitmap*    movie_GetBitmap     ( void );
xbool       movie_IsPlaying     ( void );
xbool       movie_LockBitmap    ( void );
void        movie_UnlockBitmap  ( void );

//=========================================================================
#endif // MOVIEPLAYER_HPP
//=========================================================================