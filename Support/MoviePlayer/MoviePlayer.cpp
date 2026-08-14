//==============================================================================
//
//  MoviePlayer.cpp
//
//==============================================================================

#include "Entropy.hpp"
#include "MoviePlayer.hpp"
#include "StateMgr/StateMgr.hpp"
#include "InputMgr/GamePad.hpp"

//=========================================================================
// FUNCTIONS
//=========================================================================

static xbool movieplayer_ClearBackBuffer( void )
{
    rtarget_backbuffer_pass_desc PassDesc;
    PassDesc.bUseDepth = FALSE;
    if( !rtarget_BeginBackBufferPass( PassDesc ) )
    {
        x_DebugMsg( "MoviePlayer: failed to begin clear-only backbuffer pass\n" );
        eng_ResetAfterException();
        return FALSE;
    }

    rtarget_EndPass();
    return TRUE;
}

//==============================================================================

static xbool movieplayer_BeginTimedFrame( xtimer& FrameTimer, f32& DeltaTime )
{
    static const f32 k_MaxFrameDeltaSeconds = 0.1f;

    while( eng_BeginFrame() )
    {
        DeltaTime = FrameTimer.TripSec();
        if( x_isvalid( DeltaTime ) &&
            (DeltaTime > 0.0f) &&
            (DeltaTime <= k_MaxFrameDeltaSeconds) )
        {
            return TRUE;
        }

        x_DebugMsg( "MoviePlayer: discarding frame with invalid delta %.6f\n", DeltaTime );
        if( !movieplayer_ClearBackBuffer() || !eng_EndFrame() )
        {
            return FALSE;
        }
    }

    return FALSE;
}

movie_player::movie_player(void)
{
    m_IsLooped = FALSE;
    m_Finished = FALSE;
    m_Volume   = 1.0f;
    m_Language = XL_LANG_ENGLISH;
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
    m_Private.SetVolume( m_Volume );

    if ( !m_Private.Open(pFilename, IsResident, IsLooped, m_Language) )
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

void movie_player::SetVolume( f32 Volume )
{
    m_Volume = x_clamp( Volume, 0.0f, 1.0f );
    m_Private.SetVolume( m_Volume );
}

//==============================================================================

void movie_player::SetLanguage( x_language Language )
{
    ASSERT( (Language >= XL_LANG_ENGLISH) && (Language < XL_NUM_LANGUAGES) );
    m_Language = Language;
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
    ret = Movie.Open(movieName, FALSE, FALSE);

    if( ret )
    {   
        xbool done = FALSE;
        xbool allowSkip = FALSE;
        xtimer FrameTimer;
        FrameTimer.Start();

        while( !done )
        {
            if( !Movie.IsPlaying() )
            {
                break;
            }

            f32 DeltaTime = 0.0f;
            if( !movieplayer_BeginTimedFrame( FrameTimer, DeltaTime ) )
            {
                break;
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.Update" );

                g_FrontendInput.Update( DeltaTime );
                g_NetworkMgr.UpdateFrame( DeltaTime );

                // Wait until the user releases all keys, otherwise a movie can
                // be skipped by the input that opened it.
                xbool anyHeld = FALSE;
                for( s32 i = 0; i < frontend_input::MAX_CONTROLLERS; i++ )
                {
                    const frontend_pad& Pad = g_FrontendInput.GetPad( i );
                    if( (Pad.GetFrameLogical( frontend_pad::UI_SELECT   ).GetIsValue() > 0.25f) ||
                        (Pad.GetFrameLogical( frontend_pad::UI_BACK     ).GetIsValue() > 0.25f) ||
                        (Pad.GetFrameLogical( frontend_pad::UI_ACTIVATE ).GetIsValue() > 0.25f) )
                    {
                        anyHeld = TRUE;
                    }

                    if( allowSkip )
                    {
                        if( (Pad.GetFrameLogical( frontend_pad::UI_SELECT   ).GetWasValue() > 0.25f) ||
                            (Pad.GetFrameLogical( frontend_pad::UI_BACK     ).GetWasValue() > 0.25f) ||
                            (Pad.GetFrameLogical( frontend_pad::UI_ACTIVATE ).GetWasValue() > 0.25f) )
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

            if( !movieplayer_ClearBackBuffer() )
            {
                break;
            }

            Movie.Render();
            if( !eng_EndFrame() )
            {
                break;
            }
        }

        if( eng_BeginFrame() )
        {
            if( movieplayer_ClearBackBuffer() )
            {
                eng_EndFrame();
            }
        }
        Movie.Close();
    }
    Movie.Kill();
    return(ret);
}
