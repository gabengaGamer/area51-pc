//=========================================================================
//
//  MoviePlayer.cpp
//
//  This file contains the generic code for the movie player regardless of what
//  backend is present for it.
//
//=========================================================================

//=========================================================================
// INCLUDES
//=========================================================================

#include "Entropy.hpp"
#include "MoviePlayer.hpp"
#include "StateMgr\StateMgr.hpp"
#include "InputMgr\InputMgr.hpp"

#ifdef TARGET_PS2
#include "ps2\ps2_misc.hpp"
#endif

//=========================================================================
// DEVICE MANAGEMENT WRAPPERS (PC/DIRECTX9) 
//
// This is D3DENG property !!!  
//=========================================================================

#ifdef TARGET_PC

void movie_OnDeviceLost( void )
{
    Movie.OnDeviceLost();
}

//=========================================================================

void movie_OnDeviceReset( void )
{
    Movie.OnDeviceReset();
}

#endif

//=========================================================================
// MOVIE PLAYER IMPLEMENTATION
//=========================================================================

movie_player::movie_player( void )
{
    m_Shutdown      = FALSE;
    m_pMovieBuffer  = NULL;
    m_IsLooped      = FALSE;
    m_Finished      = FALSE;
    m_FramePending  = FALSE;
    m_pBitmaps      = NULL;
}

//=========================================================================

movie_player::~movie_player( void )
{
    Kill();
}

//=========================================================================

void movie_player::Init( void )
{
    // TODO: CTetrick - I'll clean this up... 
    // in a hurry now - I think PS2 needs the lang set prior to init.
    // will verify and clean up ASAP.    
    m_Private.Init();
    SetLanguage( x_GetLocale() );
}

//=========================================================================
	
xbool movie_player::Open( const char* pFilename, xbool IsResident, xbool IsLooped )
{
#ifdef TARGET_XBOX
    // Force non-resident loading on Xbox
    IsResident = FALSE;
#endif

    // NOTE: The second parameter is whether or not the movie is loaded in to memory.
    // not used right now but will be when we have front-end load hiding.
    if( !m_Private.Open( pFilename, IsResident, IsLooped ) )
    {
        Kill();
        m_Finished = TRUE;
        return FALSE;
    }

    // Initialize playback state
    m_IsLooped  = IsLooped;
    m_Shutdown  = FALSE;
    m_Finished  = FALSE;
    m_pBitmaps  = NULL;

    // If we're using a modal version of the movie playback, we make the priority of the main thread
    // higher than the decoder so that it gets a chance to render the resulting bitmap within sufficient time
    // without being locked out due to the decode thread using all CPU time.
    s32 Priority = 0;
    
    return TRUE;
}

//=========================================================================

void movie_player::Close( void )
{
    if( !m_Private.IsRunning() )
    {
        return;
    }
    
    ASSERT( !m_Shutdown );

    // Release any pending bitmaps
    if( m_pBitmaps )
    {
        m_Private.ReleaseBitmap( m_pBitmaps );
        m_pBitmaps = NULL;
    }

    m_Private.Close();
}

//=========================================================================

void movie_player::Kill( void )
{
    m_Private.Kill();
    m_Shutdown = TRUE;
}

//=========================================================================

void movie_player::Render( const vector2& Pos, const vector2& Size, xbool InRenderLoop )
{
    // Note: This routine is used for PS2 and Bink playback. It assumes the controlling
    // 'private' subsystem creates all bitmaps required. The PS2 requires the bitmaps to
    // be in a strip format due to crappy hardware (no surprise there) and the gcn needs
    // them to be powers of 2 in size.
    //
    // Note: The XBOX and PC doesn't have either of these limitations so (hacky hacky) I decode
    // directly to the back buffer.

#if defined(TARGET_XBOX)
    if( !InRenderLoop )
    {
        if( eng_Begin( "Movie" ) )
        {
            m_Private.SetPos( (vector3&)Pos );
            m_Private.Decode();
            eng_End();
        }
    }
    else
    {
        m_Private.SetPos( (vector3&)Pos );
        m_Private.Decode();
    }
#elif defined(TARGET_PS2)
    {
        vram_Flush();

        s32         nBitmaps;
        vector2     BitmapPos;
        vector2     BitmapSize;
        vector2     BitmapLowerRight;
        vector2     BitmapTopLeft;
        s32         i;

        if (m_pBitmaps)
        {
            m_Private.ReleaseBitmap(m_pBitmaps);
        }

        m_pBitmaps = m_Private.Decode();

        // RMB - more hack!!!
        if( !m_pBitmaps )
            return;
        ASSERT(m_pBitmaps);
        nBitmaps = m_Private.GetBitmapCount();

        // PS2 have parametric uv's
        f32 V0 = 0.0f ;
        f32 V1 = (f32)m_Private.GetHeight() / (f32)m_pBitmaps->GetHeight();

        f32 U0 = 0.0f ;
        f32 U1 = 1.0f;

        BitmapSize       = vector2((Size.X / nBitmaps), (f32)Size.Y);

        BitmapTopLeft    = vector2(U0,V0);
        BitmapLowerRight = vector2(U1,V1);

        xbool RenderMovie = TRUE;
        if( !InRenderLoop )
        {
            if( !eng_Begin( "movie_player" ) )
                RenderMovie = FALSE;
        }

        if( RenderMovie )
        {
            BitmapPos = Pos;

            for (i=0;i<nBitmaps;i++)
            {
                draw_Begin(DRAW_SPRITES,DRAW_TEXTURED|DRAW_2D|DRAW_NO_ZBUFFER);
                vram_Flush();
                draw_SetTexture(m_pBitmaps[i]);
                draw_DisableBilinear();

                draw_SpriteImmediate( BitmapPos,
                                      BitmapSize,
                                      BitmapTopLeft,
                                      BitmapLowerRight,
                                      XCOLOR_WHITE );
                BitmapPos.X += BitmapSize.X;
                draw_End();
            }
         
            if( !InRenderLoop )
            {
                eng_End();
            }
            draw_EnableBilinear();
        }
    }
#elif defined(TARGET_PC)
    m_Private.SetRenderSize( Size );
    m_Private.SetRenderPos( Pos );
    
    if( !InRenderLoop )
    {
        if( eng_Begin( "Movie" ) )
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

//=========================================================================

void movie_player::SetLanguage( const s32 Language )
{
    m_Private.SetLanguage( Language );
}

//=========================================================================

s32 PlaySimpleMovie( const char* movieName )
{
#ifndef X_EDITOR	
    view    View;
    s32     width;
    s32     height;
    vector2 Pos;
    vector2 Size;
    xbool   ret = FALSE;
    
    // Initialize movie system
    Movie.Init();
    global_settings& Settings = g_StateMgr.GetActiveSettings();
    Movie.SetVolume( Settings.GetVolume( VOLUME_SPEECH ) / 100.0f );
 
    // Attempt to open the movie
    ret = Movie.Open( movieName, FALSE, FALSE );

    if( ret )
    {	
		View.SetXFOV( R_60 );
        View.SetPosition( vector3(0,0,200) );
        View.LookAtPoint( vector3(  0,  0,  0) );
        View.SetZLimits ( 0.1f, 1000.0f );
        eng_MaximizeViewport( View );
        eng_SetView         ( View ) ;
		
        // Get screen resolution for fullscreen playback
        eng_GetRes( width, height );
        
        // Set size and position for fullscreen
#if defined(TARGET_PS2)
        Size.X = (f32)width;
        Size.Y = (f32)height;
#elif defined(TARGET_XBOX)
        Size.X = 640.0f;
        Size.Y = 480.0f;
#elif defined(TARGET_PC)
        Size.X = 0.0f;
        Size.Y = 0.0f; 
#endif

        Pos.X = (width-Size.X)/2.0f;
        Pos.Y = (height-Size.Y)/2.0f;  

        xbool done = FALSE;

        // Main playback loop
        while( !done )
        {
            if( ret )
            {
                // Check if movie finished
                if( !Movie.IsPlaying() )
                    break;
                    
                // Update input and network
                g_InputMgr.Update  ( 1.0f / 60.0f );
                g_NetworkMgr.Update( 1.0f / 60.0f );
                
                // Render movie frame
                Movie.Render( Pos, Size );
                eng_PageFlip();

                // Check for skip input
#if defined(TARGET_XBOX)
                if( input_WasPressed( INPUT_XBOX_BTN_START, -1 ) || input_WasPressed( INPUT_XBOX_BTN_A, -1 ) ) 
#elif defined(TARGET_PS2)
                if( input_WasPressed( INPUT_PS2_BTN_CROSS, 0 ) || input_WasPressed( INPUT_PS2_BTN_CROSS, 1 ) ) 
#elif defined(TARGET_PC)
                if( input_WasPressed( INPUT_KBD_RETURN, 0 ) || input_WasPressed( INPUT_KBD_ESCAPE, 0 ) || input_WasPressed( INPUT_KBD_SPACE, 0 ) ) 
#endif
                done = TRUE;
            }
        }      
        // Final page flip to clear screen
        eng_PageFlip();
        Movie.Close();
    }   
    Movie.Kill();
    return( ret );
#endif	
}