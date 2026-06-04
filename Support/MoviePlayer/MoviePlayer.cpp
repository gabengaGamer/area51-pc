//==============================================================================
//
//  MoviePlayer.cpp
//
//==============================================================================

#include "Entropy.hpp"
#include "movieplayer.hpp"
#include "StateMgr\StateMgr.hpp"
#include "inputmgr\gamepad.hpp"
#include "DeltaMgr\DeltaMgr.hpp"

//=========================================================================
// FUNCTIONS
//=========================================================================

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
    if (!InRenderLoop)
    {
        if (eng_Begin("Movie"))
        {
            m_Private.Render();
            eng_End();
        }
    }
    else
    {
        m_Private.Render();
    }
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
        xbool allowSkip = FALSE;

        while( !done )
        {
            if( !Movie.IsPlaying() )
            {
                break;
            }
        
            f32 DeltaTime = g_DeltaMgr.GetFixedUpdateDeltaTime();
            g_Input.SampleActionMaps( g_IngamePad, DeltaTime, FRONTEND_CONTEXT );
            g_Input.CommitActionMapsFrame( g_IngamePad );
            g_Input.ClearActionMapsFixed( g_IngamePad );
            g_NetworkMgr.Update( DeltaTime );
        
            Movie.Render();
            eng_PageFlip();
        
            // Wait until the user release all keys,
            // because if not do this, some videos to be accidentally skipped		
            xbool anyHeld = FALSE;
            for( s32 i = 0; i < MAX_LOCAL_PLAYERS; i++ )
            {
                const auto& pad = g_IngamePad[i];
                if( (pad.GetLogical( ingame_pad::UI_SELECT   ).GetIsValue() > 0.25f) ||
                    (pad.GetLogical( ingame_pad::UI_BACK     ).GetIsValue() > 0.25f) ||
                    (pad.GetLogical( ingame_pad::UI_ACTIVATE ).GetIsValue() > 0.25f) )
                {
                    anyHeld = TRUE;
                }
        
                if( allowSkip )
                {
                    if( (pad.GetLogical( ingame_pad::UI_SELECT   ).GetWasValue() > 0.25f) ||
                        (pad.GetLogical( ingame_pad::UI_BACK     ).GetWasValue() > 0.25f) ||
                        (pad.GetLogical( ingame_pad::UI_ACTIVATE ).GetWasValue() > 0.25f) )
                    {
                        done = TRUE;
                    }
                }
            }
        
            if( !anyHeld )
            {
                allowSkip = TRUE;
            }
        }
        eng_PageFlip();
        Movie.Close();
    }
    Movie.Kill();
    return(ret);
}
