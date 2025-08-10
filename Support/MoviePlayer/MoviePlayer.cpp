//==============================================================================
//
//  MoviePlayer.cpp
//
//==============================================================================

#include "Entropy.hpp"
#include "movieplayer.hpp"
#include "StateMgr\StateMgr.hpp"
#include "inputmgr\inputmgr.hpp"

//==============================================================================

movie_player::movie_player(void)
{
    m_IsLooped = FALSE;
    m_Finished = FALSE;
}

//==============================================================================

movie_player::~movie_player(void)
{
	Kill();
}

//==============================================================================

void movie_player::Init(void)
{
    m_Private.Init();
}

//==============================================================================

xbool movie_player::Open(const char* pFilename, xbool IsResident, xbool IsLooped)
{
    if ( !m_Private.Open(pFilename, IsResident, IsLooped) )
    {
        Kill();
        m_Finished = TRUE;
        return FALSE;
    }

    m_IsLooped = IsLooped;
    m_Finished = FALSE;
    
    return TRUE;
}

//==============================================================================

void movie_player::Close(void)
{
    if (!m_Private.IsRunning())
    {
        return;
    }
    
    m_Private.Close();
}

//==============================================================================

void movie_player::Kill(void)
{
    m_Private.Kill();
}

//==============================================================================

void movie_player::Render(xbool InRenderLoop)
{
#ifdef TARGET_PC	   
    if (!InRenderLoop)
    {
        if (eng_Begin("Movie"))
        {
            m_Private.Decode();
            eng_End();
        }
    }
    else
    {
        m_Private.Decode();
    }
#endif	
}

//==============================================================================

s32 PlaySimpleMovie(const char* movieName)
{
    xbool ret = FALSE;
    
    Movie.Init();
    global_settings& Settings = g_StateMgr.GetActiveSettings();
    Movie.SetVolume( Settings.GetVolume( VOLUME_SPEECH ) / 100.0f );
 
    ret = Movie.Open(movieName, FALSE, FALSE);

    if( ret )
    {   
        xbool done = FALSE;

        while(!done)
        {
            if (ret)
            {
                if (!Movie.IsPlaying())
                    break;
                    
                g_InputMgr.Update  ( 1.0f / 60.0f );
                g_NetworkMgr.Update( 1.0f / 60.0f );
                
                Movie.Render();
                eng_PageFlip();

#if defined(TARGET_PC)
                if( input_WasPressed( INPUT_KBD_RETURN, 0 ) || input_WasPressed( INPUT_KBD_ESCAPE, 0 ) || input_WasPressed( INPUT_KBD_SPACE, 0 ) )  
#else
                if( input_WasPressed( INPUT_KBD_RETURN, 0 ) || input_WasPressed( INPUT_KBD_ESCAPE, 0 ) || input_WasPressed( INPUT_KBD_SPACE, 0 ) ) 
#endif
                {
                    done = TRUE;
                }
            }
        }
        eng_PageFlip();
        Movie.Close();
    }
    Movie.Kill();
    return(ret);
}