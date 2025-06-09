//=========================================================================
//
//  MoviePlayer_Bink.cpp
//
//  Bink video codec implementation for PC (DirectX9) and XBOX platforms
//
//=========================================================================

// UPD 04.06.2025
// GS: Maybe the current system for checking for lost devices is too ( no, really ) overloaded, 
// but I'll leave it as is for now. At least it works.

//=========================================================================
// INCLUDES
//=========================================================================

#include "x_target.hpp"

#if !defined(TARGET_XBOX) && !defined(TARGET_PC)
#error This file should not be included for your platform. Please check your exclusions on your project spec.
#endif

#include "x_files.hpp"
#include "x_threads.hpp"
#include "Entropy.hpp"
#include "Audio/audio_hardware.hpp"
#include "MoviePlayer.hpp"

//=========================================================================
// PLATFORM SPECIFIC SETUP
//=========================================================================

#ifdef TARGET_XBOX
#include "Entropy/xbox/xbox_private.hpp"
#define NUM_BUFFERS 1
#endif

#ifdef TARGET_PC
#define NUM_BUFFERS 1
#endif

//=========================================================================
// GLOBAL INSTANCES
//=========================================================================

movie_player Movie;

//=========================================================================
// MEMORY MANAGEMENT CALLBACKS
//=========================================================================

void* RADLINK s_malloc( U32 bytes )
{
    return x_malloc( bytes );
}

//=========================================================================

void RADLINK s_free( void* ptr )
{
    x_free( ptr );
}

//=========================================================================
// MOVIE PRIVATE IMPLEMENTATION
//=========================================================================

movie_private::movie_private( void )
{
    m_pqFrameAvail  = NULL;
    m_IsRunning     = FALSE;
    m_IsFinished    = FALSE;
    m_IsLooped      = FALSE;
    m_nMaxBitmaps   = 0;
    m_nBitmaps      = 0;
    m_Width         = 0;
    m_Height        = 0;
    m_CurrentFrame  = 0;
    m_FrameCount    = 0;
    m_Volume        = 1.0f;
    m_Language      = XL_LANG_ENGLISH;
    m_Handle        = NULL;
    
    // Initialize platform specific members
#ifdef TARGET_PC
    m_Surface = NULL;
    m_RenderSize.Set( 640.0f, 480.0f );     // Default render state
    m_bForceStretch = TRUE;                 // Scaling video by screen size
    m_bDeviceLost = FALSE;                  // Initialize device state
#endif
}

//=========================================================================

void movie_private::Init( void )
{
    RADSetMemory( s_malloc, s_free );
    m_pqFrameAvail = new xmesgq( 2 );

    m_nMaxBitmaps = MOVIE_FIXED_WIDTH / MOVIE_STRIP_WIDTH;
    m_Language = XL_LANG_ENGLISH;
    m_Volume = 1.0f;
    
#ifdef TARGET_PC
    BinkSoundUseDirectSound( NULL );
#endif
}

//=========================================================================
// DEVICE MANAGEMENT (PC/DIRECTX9)
//=========================================================================

#ifdef TARGET_PC

void movie_private::OnDeviceLost( void )
{
    // Release surface on device lost
    if( m_Surface ) 
    {
        m_Surface->Release();
        m_Surface = NULL;
    }
    m_bDeviceLost = TRUE;
}

//=========================================================================

void movie_private::OnDeviceReset( void )
{
    m_bDeviceLost = FALSE;
    
    // Recreate surface if we have a valid handle
    if( m_Handle && g_pd3dDevice && !m_Surface ) 
    {
        HRESULT hr = g_pd3dDevice->CreateOffscreenPlainSurface(
            m_Width, m_Height,
            D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT,
            &m_Surface,
            NULL );
            
        if( FAILED( hr ) ) 
        {
            // Surface creation failed, mark as device lost again
            m_bDeviceLost = TRUE;
        }
    }
}

#endif

//=========================================================================

xbool movie_private::Open( const char* pFilename, xbool PlayResident, xbool IsLooped )
{
    m_IsLooped = IsLooped;
    
    // Set up audio track selection
#ifdef TARGET_PC
    BinkSetSoundTrack( 1, (unsigned long*)&m_Language );
    
    u32 Flags = BINKNOSKIP | BINKSNDTRACK;
    if( PlayResident ) 
    {
        Flags |= BINKPRELOADALL;
    }
    m_Handle = BinkOpen( xfs( "C:\\GameData\\A51\\Release\\PC\\%s.bik", pFilename ), Flags );
#endif

#ifdef TARGET_XBOX
    #if CONFIG_IS_DEMO
        m_Handle = BinkOpen(xfs("D:\\Area51\\movies\\%s.bik",pFilename), BINKNOSKIP|BINKSNDTRACK);
    #else
        u32 Flags = BINKNOSKIP|BINKSNDTRACK;
        if( PlayResident )
		{	
            Flags |= BINKPRELOADALL;
		}
        m_Handle = BinkOpen(xfs("D:\\movies\\%s.bik",pFilename), Flags);
    #endif
    // Load convertors
    BinkLoadConverter( BINKSURFACE32 );
    
    // Set up Xbox audio mixing
    U32 bins[2];
    bins[0] = DSMIXBIN_3D_FRONT_LEFT;
    bins[1] = DSMIXBIN_3D_FRONT_RIGHT;
    
    BinkSetMixBins( m_Handle, m_Language, bins, 2 );
#endif

    // Check if file opened successfully
    if( !m_Handle ) 
    {
        m_IsFinished = TRUE;
        return FALSE;
    }
    
    // Set initial volume and get video dimensions
    BinkSetVolume( m_Handle, m_Language, (s32)(m_Volume * 32767) );
    m_Width = m_Handle->Width;
    m_Height = m_Handle->Height;
    m_nBitmaps = 1;

    ASSERTS( m_nBitmaps <= m_nMaxBitmaps, "Movie size is larger than decode buffer size" );

#ifdef TARGET_PC
    m_bDeviceLost = FALSE;
    
    // Only create surface if device is available and not lost
    if( g_pd3dDevice && !m_Surface ) 
    {
        // Check device state before creating surface
        HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
        if( SUCCEEDED( hr ) )
        {
            hr = g_pd3dDevice->CreateOffscreenPlainSurface(
                m_Width, m_Height,
                D3DFMT_A8R8G8B8,
                D3DPOOL_DEFAULT,
                &m_Surface,
                NULL );
                
            if( FAILED( hr ) ) 
            {
                m_IsFinished = TRUE;
                return FALSE;
            }
        }
        else
        {
            // Device is lost, defer surface creation
            m_bDeviceLost = TRUE;
        }
    }
#endif

    m_IsFinished = FALSE;
    m_IsRunning = TRUE;
    
    return TRUE;
}

//=========================================================================

void movie_private::Close( void )
{
    if( !m_Handle )
        return;
        
#ifdef TARGET_PC
    BinkSetSoundOnOff( m_Handle, 0 );
#endif

    BinkClose( m_Handle );
    m_Handle = NULL;
    
#ifdef TARGET_PC
    m_bDeviceLost = FALSE;
#endif
    
    Kill();
}

//=========================================================================

void movie_private::SetVolume( f32 Volume )
{
    m_Volume = Volume;
    if( m_Handle )
    {
        BinkSetVolume( m_Handle, m_Language, (s32)(m_Volume * 32767) );
    }
}

//=========================================================================

void movie_private::Pause( void )
{
    if( Movie.IsPlaying() && m_Handle )
    {
        BinkPause( m_Handle, 1 );
    }
}

//=========================================================================

void movie_private::Resume( void )
{
    if( Movie.IsPlaying() && m_Handle )
    {
        BinkPause( m_Handle, 0 );
    }
}

//=========================================================================

void movie_private::Kill( void )
{
    ASSERT( !m_Handle );
    
    if( m_pqFrameAvail )
    {
        delete m_pqFrameAvail;
        m_pqFrameAvail = NULL;
    }
    
#ifdef TARGET_PC
    if( m_Surface ) 
    {
        m_Surface->Release();
        m_Surface = NULL;
    }
#endif

#ifdef TARGET_XBOX   
    ASSERT( m_pBitmaps );
    for( s32 i = 0; i < NUM_BUFFERS; i++ )
    {
        for( s32 j = 0; j < m_nMaxBitmaps; j++ )
        {
            vram_Unregister( m_pBitmaps[i][j] );
        }
        delete[] m_pBitmaps[i];
        m_pBitmaps[i] = NULL;
    }
#endif

#ifdef TARGET_PC
    for( s32 i = 0; i < NUM_BUFFERS; i++ )
    {
        if( m_pBitmaps[i] )
        {
            vram_Unregister( *m_pBitmaps[i] );
            m_pBitmaps[i]->DePreregister();
            delete m_pBitmaps[i];
            m_pBitmaps[i] = NULL;
        }
    }
#endif

    m_IsRunning = FALSE;
}

//=========================================================================

void movie_private::ReleaseBitmap( xbitmap* pBitmap )
{
    ASSERT( !m_pqFrameAvail->IsFull() );
    m_pqFrameAvail->Send( pBitmap, MQ_BLOCK );
}

//=========================================================================

xbitmap* movie_private::Decode( void )
{
    ASSERT( !m_IsFinished );
    
    // Wait for frame to be ready
    while( BinkWait( m_Handle ) ) 
    {
        x_DelayThread( 1 );
    }
    
    BinkDoFrame( m_Handle );

#ifdef TARGET_PC  
    // Skip rendering if device is lost or no device available
    if( !g_pd3dDevice || m_bDeviceLost )
    {
        BinkNextFrame( m_Handle );
        
        if( m_Handle->FrameNum >= (m_Handle->Frames - 1) )
        {
            if( m_IsLooped )
                BinkGoto( m_Handle, 1, 0 );
            else
                m_IsFinished = TRUE;
        }
        return NULL;
    }
    
    // Check if device is still valid
    HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
    if( FAILED( hr ) )
    {
        m_bDeviceLost = TRUE;
        BinkNextFrame( m_Handle );
        
        if( m_Handle->FrameNum >= (m_Handle->Frames - 1) )
        {
            if( m_IsLooped )
                BinkGoto( m_Handle, 1, 0 );
            else
                m_IsFinished = TRUE;
        }
        return NULL;
    }
    
    // Recreate surface if needed
    if( !m_Surface && !m_bDeviceLost ) 
    {
        hr = g_pd3dDevice->CreateOffscreenPlainSurface(
            m_Width, m_Height,
            D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT,
            &m_Surface,
            NULL );
            
        if( FAILED( hr ) ) 
        {
            m_bDeviceLost = TRUE;
        }
    }
    
    // Get screen dimensions for rendering
    s32 WindowWidth, WindowHeight;
    eng_GetRes( WindowWidth, WindowHeight );
    
    // Render to surface and copy to back buffer
    if( m_Surface && !m_bDeviceLost ) 
    {
        D3DLOCKED_RECT lockRect;
        hr = m_Surface->LockRect( &lockRect, NULL, 0 );
        
        if( SUCCEEDED( hr ) ) 
        {
            // Copy Bink frame to surface
            BinkCopyToBuffer(
                m_Handle,
                lockRect.pBits,
                lockRect.Pitch,
                m_Height,
                0, 0,
                BINK_BITMAP_FORMAT | BINKCOPYALL );
            
            m_Surface->UnlockRect();
            
            // Get back buffer and copy surface to it
            IDirect3DSurface9* pBackBuffer = NULL;
            hr = g_pd3dDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
            
			//TODO: GS: Maybe it's worth moving this code into a separate function? It's worth doing this later.
			
            if( SUCCEEDED( hr ) && pBackBuffer ) 
            {
                RECT destRect;
                
                // Calculate destination rectangle
                if( m_bForceStretch ) 
                {
                    destRect.left = 0;
                    destRect.top = 0;
                    destRect.right = WindowWidth;
                    destRect.bottom = WindowHeight;
                } 
                else 
                {
                    // Maintain aspect ratio
                    float aspectRatio = (float)m_Width / (float)m_Height;
                    float screenRatio = (float)WindowWidth / (float)WindowHeight;
                    
                    if( screenRatio > aspectRatio ) 
                    {
                        int targetWidth = (int)(WindowHeight * aspectRatio);
                        destRect.left = (WindowWidth - targetWidth) / 2;
                        destRect.top = 0;
                        destRect.right = destRect.left + targetWidth;
                        destRect.bottom = WindowHeight;
                    } 
                    else 
                    {
                        int targetHeight = (int)(WindowWidth / aspectRatio);
                        destRect.left = 0;
                        destRect.top = (WindowHeight - targetHeight) / 2;
                        destRect.right = WindowWidth;
                        destRect.bottom = destRect.top + targetHeight;
                    }
                }
                
                // Clear and stretch blit
                g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 0, 0 );
                
                RECT srcRect = { 0, 0, m_Width, m_Height };
                hr = g_pd3dDevice->StretchRect( m_Surface, &srcRect, pBackBuffer, &destRect, D3DTEXF_LINEAR );
                
                if( FAILED( hr ) )
                {
                    // StretchRect failed, possibly due to device loss
                    m_bDeviceLost = TRUE;
                }
                
                pBackBuffer->Release();
            }
            else
            {
                // Failed to get back buffer
                m_bDeviceLost = TRUE;
            }
        }
        else
        {
            // Lock failed, surface may be lost
            m_bDeviceLost = TRUE;
        }
    }
#endif

#ifdef TARGET_XBOX    
    xbitmap* pBitmap;
    pBitmap = (xbitmap*)m_pqFrameAvail->Recv( MQ_BLOCK );
    ASSERT( pBitmap );

    // Clear back buffer and get surface
    g_pd3dDevice->Clear( 0, 0, D3DCLEAR_TARGET, 0, 0, 0 );

    IDirect3DSurface8* pBackBuffer;
    VERIFY( !g_pd3dDevice->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer ) );

    D3DSURFACE_DESC desc;
    VERIFY( !pBackBuffer->GetDesc( &desc ) );

    D3DLOCKED_RECT LockedRect;
    VERIFY( !pBackBuffer->LockRect( &LockedRect, NULL, D3DLOCK_TILED ) );

    // Copy Bink frame directly to back buffer
    u8* pBuffer = (u8*)LockedRect.pBits;
    BinkCopyToBuffer(
        m_Handle, 
        pBuffer + (((desc.Height - m_Height) / 2) * LockedRect.Pitch), 
        LockedRect.Pitch, 
        GetHeight(), 
        U32((f32(g_PhysW) - 640.0f) / 2.0f), 
        U32((f32(g_PhysH) - 480.0f) / 2.0f), 
        BINK_BITMAP_FORMAT | BINKCOPYALL );

    VERIFY( !pBackBuffer->UnlockRect() );
    pBackBuffer->Release();
#endif

    // Advance to next frame
    BinkNextFrame( m_Handle );
    
    // Check for end of movie
    if( m_Handle->FrameNum >= (m_Handle->Frames - 1) )
    {
        if( m_IsLooped )
            BinkGoto( m_Handle, 1, 0 );
        else
            m_IsFinished = TRUE;
    }
    
    return NULL;
}