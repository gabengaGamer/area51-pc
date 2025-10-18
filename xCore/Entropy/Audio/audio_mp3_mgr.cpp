//==============================================================================
//
//  audio_mp3_mgr.cpp
//
//  dr_lib based mp3 decoder.
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

// Let it be only for PC, for now...

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "audio_stream_mgr.hpp"
#include "audio_channel_mgr.hpp"
#include "audio_hardware.hpp"
#include "audio_mp3_mgr.hpp"
#include "x_bytestream.hpp"
#include "x_log.hpp"

//==============================================================================
//  DR_MP3 INCLUDES
//==============================================================================

#define DRMP3_ASSERT ASSERT
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO

#include "dr_libs\dr_mp3.h"

//==============================================================================
//  DEFINES
//==============================================================================

#define VALID_STREAM( pStream ) ((pStream >= &g_AudioStreamMgr.m_AudioStreams[0]) && (pStream <= &g_AudioStreamMgr.m_AudioStreams[MAX_AUDIO_STREAMS-1]))

//==============================================================================
//  STRUCTURES
//==============================================================================

struct mp3_decoder_context
{
    drmp3               Decoder;
    audio_stream*       pStream;
    xbool               Initialized;
};

//------------------------------------------------------------------------------

struct mp3_stream_state
{
                        mp3_stream_state    ( void )
                        : BufferedBytes      ( 0 )
    {
    }

    void                Reset               ( void )
    {
        Mutex.Enter();
        BufferedBytes = 0;
        Mutex.Exit();
    }

    xmutex              Mutex;
    s32                 BufferedBytes;
};

//------------------------------------------------------------------------------

static s16              s_DecodeBuffer[1024];
static mp3_stream_state s_StreamStates[MAX_AUDIO_STREAMS];
static xbool            s_Initialized   = FALSE;

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

audio_mp3_mgr g_AudioMP3Mgr;

//==============================================================================
// FUNCTIONS
//==============================================================================

static void* mp3_malloc( usize nBytes, void* pUserData )
{
    (void)pUserData;
    return x_malloc( (s32)nBytes );
}

//==============================================================================

static void* mp3_realloc( void* pMemory, usize nBytes, void* pUserData )
{
    (void)pUserData;
    return x_realloc( pMemory, (s32)nBytes );
}

//==============================================================================

static void mp3_free( void* pMemory, void* pUserData )
{
    (void)pUserData;
    if( pMemory )
        x_free( pMemory );
}

//==============================================================================

static const drmp3_allocation_callbacks s_DrMp3AllocCallbacks =
{
    NULL,
    mp3_malloc,
    mp3_realloc,
    mp3_free
};

//==============================================================================

static usize mp3_stream_read( void* pUserData, void* pBufferOut, usize bytesToRead )
{
    mp3_decoder_context* pContext  = (mp3_decoder_context*)pUserData;
    audio_stream*        pStream   = pContext ? pContext->pStream : NULL;
    u8*                  pDst      = (u8*)pBufferOut;
    usize                TotalRead = 0;

    if( (pStream == NULL) || (pDst == NULL) || (bytesToRead == 0) )
        return 0;

    ASSERT( VALID_STREAM( pStream ) );
    ASSERT( pStream->Index >= 0 );
    ASSERT( pStream->Index < MAX_AUDIO_STREAMS );

    mp3_stream_state& State = s_StreamStates[ pStream->Index ];

    while( bytesToRead )
    {
        State.Mutex.Enter();
        s32 Buffered = State.BufferedBytes;
        s32 Cursor   = pStream->CursorMP3;

        if( Buffered <= 0 )
        {
            State.Mutex.Exit();
            break;
        }

        ASSERT( Cursor >= 0 );
        ASSERT( Cursor < (MP3_BUFFER_SIZE*2) );

        s32 RegionRemaining;
        if( Cursor < MP3_BUFFER_SIZE )
            RegionRemaining = MP3_BUFFER_SIZE - Cursor;
        else
            RegionRemaining = (MP3_BUFFER_SIZE*2) - Cursor;

        s32 Request = (s32)((bytesToRead < (usize)RegionRemaining) ? bytesToRead : (usize)RegionRemaining);
        Request = x_min( Request, Buffered );

        if( Request <= 0 )
        {
            State.Mutex.Exit();
            break;
        }

        u8* pBase = (u8*)pStream->MainRAM[0];
        u8* pSrc  = pBase + Cursor;
        x_memcpy( pDst, pSrc, Request );
        x_memset( pSrc, 0, Request );

        Buffered -= Request;
        State.BufferedBytes = Buffered;

        s32 Previous = Cursor;
        Cursor      += Request;
        if( Cursor >= (MP3_BUFFER_SIZE*2) )
            Cursor -= (MP3_BUFFER_SIZE*2);
        pStream->CursorMP3 = Cursor;

        State.Mutex.Exit();

        bytesToRead -= Request;
        TotalRead   += Request;
        pDst        += Request;

        xbool bTransition;
        if( Previous < MP3_BUFFER_SIZE )
            bTransition = (Cursor >= MP3_BUFFER_SIZE);
        else
            bTransition = (Cursor < MP3_BUFFER_SIZE);

        if( bTransition && !pStream->StreamDone )
        {
            // Fill the read buffer.
            g_AudioStreamMgr.ReadStream( pStream );
        }
    }

    return TotalRead;
}

//==============================================================================

audio_mp3_mgr::audio_mp3_mgr( void )
{
}

//==============================================================================

audio_mp3_mgr::~audio_mp3_mgr( void )
{
}

//==============================================================================

// NOTE: GS: audio_mp3_mgr::Open already does this work, 
// it is enough to set it here s_Initialized = TRUE and thats it.

void audio_mp3_mgr::Init( void )
{
    ASSERT( !s_Initialized );
    s_Initialized = TRUE; 
}

//==============================================================================

void audio_mp3_mgr::Kill( void )
{
    ASSERT( s_Initialized );
    
    for( s32 i = 0; i < MAX_AUDIO_STREAMS; ++i )
    {
        audio_stream& Stream = g_AudioStreamMgr.m_AudioStreams[i];
        if( Stream.HandleMP3 )
        {
            Close( &Stream );
        }
        else
        {
            s_StreamStates[i].Reset();
            Stream.CursorMP3 = 0;
        }
    }
    
    s_Initialized = FALSE;
}

//==============================================================================

void audio_mp3_mgr::ResetStreamBuffer( audio_stream* pStream )
{
    ASSERT( pStream );
    ASSERT( VALID_STREAM( pStream ) );
    ASSERT( pStream->Index >= 0 );
    ASSERT( pStream->Index < MAX_AUDIO_STREAMS );

    s_StreamStates[ pStream->Index ].Reset();
    pStream->CursorMP3 = 0;
}

//==============================================================================

void audio_mp3_mgr::NotifyStreamData( audio_stream* pStream, s32 nBytes )
{
    if( (pStream == NULL) || (nBytes <= 0) )
        return;

    ASSERT( VALID_STREAM( pStream ) );
    ASSERT( pStream->Index >= 0 );
    ASSERT( pStream->Index < MAX_AUDIO_STREAMS );

    mp3_stream_state& State = s_StreamStates[ pStream->Index ];
    State.Mutex.Enter();
    State.BufferedBytes += nBytes;
    if( State.BufferedBytes > (MP3_BUFFER_SIZE * 2) )
        State.BufferedBytes = MP3_BUFFER_SIZE * 2;
    State.Mutex.Exit();
}

//==============================================================================

void audio_mp3_mgr::Open( audio_stream* pStream )
{
    ASSERT( s_Initialized );
    ASSERT( VALID_STREAM(pStream) );

    if( pStream->HandleMP3 )
        Close( pStream );

    mp3_decoder_context* pContext = new mp3_decoder_context;
    if( pContext )
    {
        x_memset( pContext, 0, sizeof( mp3_decoder_context ) );
        pContext->pStream = pStream;

        if( drmp3_init( &pContext->Decoder, mp3_stream_read, NULL, NULL, NULL, pContext, &s_DrMp3AllocCallbacks ) )
        {
            pContext->Initialized = TRUE;
            pStream->HandleMP3    = pContext;
        }
        else
        {
            x_DebugMsg( "audio_mp3_mgr::Open - Failed to initialize dr_mp3 decoder\n" );
            delete pContext;
            pStream->HandleMP3 = NULL;
        }
    }
    else
    {
        x_DebugMsg( "audio_mp3_mgr::Open - Failed to allocate decoder context\n" );
    }
}

//==============================================================================

void audio_mp3_mgr::Close( audio_stream* pStream )
{
    ASSERT( s_Initialized );
    ASSERT( VALID_STREAM(pStream) );

    if( pStream->HandleMP3 )
    {
        mp3_decoder_context* pContext = (mp3_decoder_context*)pStream->HandleMP3;
        if( pContext->Initialized )
        {
            drmp3_uninit( &pContext->Decoder );
            pContext->Initialized = FALSE;
        }
        delete pContext;
        pStream->HandleMP3 = NULL;
    }

    ResetStreamBuffer( pStream );
}

//==============================================================================

void audio_mp3_mgr::Decode( audio_stream* pStream, s16* pBufferL, s16* pBufferR, s32 nSamples )
{
    ASSERT( s_Initialized );
    ASSERT( VALID_STREAM(pStream) );
    ASSERT( nSamples >= 0 );
    ASSERT( nSamples <= 512 );

    if( (nSamples <= 0) || (nSamples > 512) )
        nSamples = 512;

    // MP3 Stream closed?
    if( (pStream==NULL) || (pStream->HandleMP3 == NULL) )
    {   
        if( pBufferL )
            x_memset( pBufferL, 0, nSamples * sizeof(s16) );
        if( pBufferR )
            x_memset( pBufferR, 0, nSamples * sizeof(s16) );
        return;
    }

    mp3_decoder_context* pContext = (mp3_decoder_context*)pStream->HandleMP3;
    if( (pContext == NULL) || (pContext->Initialized == FALSE) )
    {
        if( pBufferL )
            x_memset( pBufferL, 0, nSamples * sizeof(s16) );
        if( pBufferR )
            x_memset( pBufferR, 0, nSamples * sizeof(s16) );
        return;
    }

    // Lock the audio hardware.
    g_AudioHardware.Lock();

    s16* pDecodeBuffer = s_DecodeBuffer;
    drmp3_uint32 Channels = pContext->Decoder.channels;
    if( Channels == 0 )
        Channels = (pStream->Type == STEREO_STREAM) ? 2 : 1;

    drmp3_uint64 FramesRead = drmp3_read_pcm_frames_s16( &pContext->Decoder, (drmp3_uint64)nSamples, (drmp3_int16*)pDecodeBuffer );
    if( FramesRead < (drmp3_uint64)nSamples )
    {
        s32 Start = (s32)(FramesRead * Channels);
        s32 Count = (nSamples * (s32)Channels) - Start;
        if( Count > 0 )
        {
            x_memset( pDecodeBuffer + Start, 0, Count * sizeof(s16) );
        }
    }

    if( pStream->Type == STEREO_STREAM )
    {
        ASSERT( pBufferL );
        ASSERT( pBufferR );

        s16* pSrc = pDecodeBuffer;
        for( s32 i = 0; i < nSamples; i++ )
        {
            s16 Left  = 0;
            s16 Right = 0;

            if( Channels >= 1 )
                Left = pSrc[0];
            if( Channels >= 2 )
                Right = pSrc[1];
            else
                Right = Left;

            pBufferL[i] = Left;
            pBufferR[i] = Right;

            pSrc += Channels;
        }
    }
    else
    {
        ASSERT( pBufferL );

        s16* pSrc = pDecodeBuffer;
        for( s32 i = 0; i < nSamples; i++ )
        {
            s16 Sample = 0;
            if( Channels >= 1 )
                Sample = pSrc[0];

            pBufferL[i] = Sample;
            pSrc += Channels;
        }
    }

    // Unlock it now.
    g_AudioHardware.Unlock();
}