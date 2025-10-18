//==============================================================================
//
//  MoviePlayer.hpp
//
//==============================================================================

#ifndef MOVIEPLAYER_HPP
#define MOVIEPLAYER_HPP

#include "x_target.hpp"
#include "x_types.hpp"
#include "x_math.hpp"

s32 PlaySimpleMovie( const char* movieName );

#ifdef TARGET_PC
#include "MoviePlayer_WebM/MoviePlayer_WebM_Private.hpp"
#endif

//==============================================================================
// MOVIE PLAYER CLASS
//==============================================================================

class movie_player
{
public:
                movie_player    (void);
               ~movie_player    (void);
    xbool       Open            (const char* pFilename, xbool PlayResident, xbool IsLooped);
    void        Close           (void);
    void        Init            (void);
    void        Kill            (void);
    void        Play            (void);
    void        Stop            (void);
    void        Render          (xbool InRenderLoop = FALSE);
    void        SetVolume       (f32 Volume)                        { m_Private.SetVolume(Volume);          };
    void        Pause           (void)                              { m_Private.Pause();                    };
    void        Resume          (void)                              { m_Private.Resume();                   };

    xbool       IsPlaying       (void)                              { return !m_Private.IsFinished();       };
    s32         GetWidth        (void)                              { return m_Private.GetWidth();          };
    s32         GetHeight       (void)                              { return m_Private.GetHeight();         };

private:
    movie_private   m_Private;
    xbool           m_IsLooped;
    xbool           m_Finished;
    friend  void    s_MoviePlayerThread(void*);
};

extern movie_player Movie;

//==============================================================================

xbool       movie_Play          (const char* pFilename,const xbool IsLooped=FALSE,const xbool Modal=TRUE);
void        movie_Stop          (void);
void        movie_Pause         (void);
xbitmap*    movie_GetBitmap     (void);
xbool       movie_IsPlaying     (void);
xbool       movie_LockBitmap    (void);
void        movie_UnlockBitmap  (void);

#endif // MOVIEPLAYER_HPP