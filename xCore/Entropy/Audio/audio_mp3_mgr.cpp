//==============================================================================
//
//  audio_mp3_mgr.cpp
//
//  minimp3 based mp3 decoder.
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

//==============================================================================
//  MINIMP3 INCLUDES
//==============================================================================

#define MINIMP3_IMPLEMENTATION
#include "../../3rdParty/minimp3/minimp3.h"
#undef MINIMP3_IMPLEMENTATION

//==============================================================================
//  STRUCTURES
//==============================================================================

enum
{
    MP3_INPUT_BUFFER_SIZE = MP3_BUFFER_SIZE,
};

//------------------------------------------------------------------------------

struct audio_mp3_mgr::mp3_decoder_state
{
    mp3dec_t                Decoder;
    mp3dec_frame_info_t     FrameInfo;
    s32                     FileCursor;
    s32                     InputBytes;
    s32                     InputCursor;
    s32                     SamplesAvailable;
    s32                     SampleOffset;
    s32                     TotalSamplesDecoded;
    s32                     Channels;
    s32                     ExpectedChannels;
    xbool                   EndOfInput;
    xbool                   DecodeComplete;
    s16                     SamplesBuffer[MINIMP3_MAX_SAMPLES_PER_FRAME];
    u8                      InputBuffer[MP3_INPUT_BUFFER_SIZE];
};

//------------------------------------------------------------------------------

xbool audio_mp3_mgr::s_Initialized = FALSE;

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

audio_mp3_mgr g_AudioMP3Mgr;

//==============================================================================
// FUNCTIONS
//==============================================================================

xbool audio_mp3_mgr::IsValidStream( const audio_stream* pStream )
{
    if( pStream == NULL )
        return FALSE;

    return (pStream >= &g_AudioStreamMgr.m_AudioStreams[0]) &&
           (pStream <= &g_AudioStreamMgr.m_AudioStreams[MAX_AUDIO_STREAMS-1]);
}

//==============================================================================

s32 audio_mp3_mgr::mp3_stream_read( audio_stream* pStream, mp3_decoder_state& State, void* pBuffer, s32 nBytes )
{
    ASSERT( pStream );
    ASSERT( IsValidStream( pStream ) );
    ASSERT( pBuffer );
    ASSERT( nBytes >= 0 );

    if( (nBytes <= 0) || (pStream->FileHandle == NULL) )
    {
        State.EndOfInput = TRUE;
        return 0;
    }

    s32 Remaining = (s32)pStream->WaveformLength - State.FileCursor;
    if( Remaining <= 0 )
    {
        State.EndOfInput = TRUE;
        return 0;
    }

    s32 FileOffset = (s32)pStream->WaveformOffset + State.FileCursor;
    s32 FileLeft   = pStream->FileHandle->Length - FileOffset;
    if( FileLeft <= 0 )
    {
        State.EndOfInput = TRUE;
        return 0;
    }

    s32 BytesToRead = x_min( nBytes, Remaining );
    BytesToRead     = x_min( BytesToRead, FileLeft );

    pStream->FileHandle->Position = FileOffset;

    s32 BytesRead = g_IOFSMgr.Read( pStream->FileHandle, (byte*)pBuffer, BytesToRead );
    if( BytesRead < 0 )
        BytesRead = 0;

    State.FileCursor       += BytesRead;
    pStream->WaveformCursor = State.FileCursor;

    if( (BytesRead < BytesToRead) || (State.FileCursor >= (s32)pStream->WaveformLength) )
    {
        State.EndOfInput = TRUE;
    }

    return BytesRead;
}

//==============================================================================

void audio_mp3_mgr::mp3_state_reset( mp3_decoder_state& State, const audio_stream* pStream )
{
    ASSERT( pStream );

    x_memset( &State, 0, sizeof( State ) );
    mp3dec_init( &State.Decoder );

    State.Channels         = (pStream->Type == STEREO_STREAM) ? 2 : 1;
    State.ExpectedChannels = State.Channels;
    State.EndOfInput      = FALSE;
}

//==============================================================================

void audio_mp3_mgr::mp3_state_compact( mp3_decoder_state& State )
{
    if( State.InputCursor > 0 )
    {
        if( State.InputCursor < State.InputBytes )
        {
            x_memmove( State.InputBuffer,
                       State.InputBuffer + State.InputCursor,
                       State.InputBytes - State.InputCursor );
            State.InputBytes -= State.InputCursor;
        }
        else
        {
            State.InputBytes = 0;
        }

        State.InputCursor = 0;
    }
}

//==============================================================================

void audio_mp3_mgr::mp3_state_refill( audio_stream* pStream, mp3_decoder_state& State )
{
    mp3_state_compact( State );

    while( (State.InputBytes < MP3_INPUT_BUFFER_SIZE) && !State.EndOfInput )
    {
        s32 Capacity = (s32)sizeof( State.InputBuffer ) - State.InputBytes;
        if( Capacity <= 0 )
            break;

        s32 Bytes = mp3_stream_read( pStream,
                                     State,
                                     State.InputBuffer + State.InputBytes,
                                     Capacity );
        if( Bytes <= 0 )
            break;

        State.InputBytes += Bytes;
    }
}

//==============================================================================

s32 audio_mp3_mgr::mp3_state_available_bytes( const mp3_decoder_state& State )
{
    if( State.InputBytes <= State.InputCursor )
        return 0;

    return State.InputBytes - State.InputCursor;
}

//==============================================================================

void audio_mp3_mgr::mp3_state_finish( audio_stream* pStream, mp3_decoder_state& State )
{
    ASSERT( pStream );

    if( State.DecodeComplete )
        return;

    State.DecodeComplete = TRUE;
    pStream->StreamDone  = TRUE;

    if( State.TotalSamplesDecoded > 0 )
    {
        pStream->Samples[LEFT_CHANNEL].Sample.nSamples = State.TotalSamplesDecoded;

        if( pStream->Type == STEREO_STREAM )
            pStream->Samples[RIGHT_CHANNEL].Sample.nSamples = State.TotalSamplesDecoded;
    }
}

//==============================================================================

s32 audio_mp3_mgr::mp3_state_decode_frame( audio_stream* pStream, mp3_decoder_state& State )
{
    State.SamplesAvailable = 0;
    State.SampleOffset     = 0;

    mp3_state_refill( pStream, State );

    while( mp3_state_available_bytes( State ) > 0 )
    {
        s32 BytesLeft;
        s32 SamplesDecoded;
        s32 FrameBytes;

        x_memset( &State.FrameInfo, 0, sizeof( State.FrameInfo ) );

        BytesLeft      = mp3_state_available_bytes( State );
        SamplesDecoded = mp3dec_decode_frame( &State.Decoder,
                                              State.InputBuffer + State.InputCursor,
                                              BytesLeft,
                                              State.SamplesBuffer,
                                              &State.FrameInfo );
        FrameBytes     = State.FrameInfo.frame_bytes;

        if( (SamplesDecoded > 0) &&
            (FrameBytes > 0) &&
            (State.FrameInfo.layer == 3) &&
            (State.FrameInfo.channels > 0) &&
            (State.FrameInfo.channels <= 2) )
        {
            State.InputCursor += x_min( FrameBytes, BytesLeft );
            State.Channels = State.FrameInfo.channels;

            State.TotalSamplesDecoded += SamplesDecoded;
            State.SamplesAvailable = SamplesDecoded;
            State.SampleOffset     = 0;

            return SamplesDecoded;
        }

        if( FrameBytes > 0 )
        {
            State.InputCursor += x_min( FrameBytes, BytesLeft );
        }
        else if( !State.EndOfInput )
        {
            s32 OldBytes = State.InputBytes;

            mp3_state_refill( pStream, State );

            if( State.InputBytes > OldBytes )
                continue;

            if( mp3_state_available_bytes( State ) >= MP3_INPUT_BUFFER_SIZE )
                State.InputCursor++;
        }
        else
        {
            State.InputCursor = State.InputBytes;
        }

        mp3_state_refill( pStream, State );
    }

    mp3_state_finish( pStream, State );
    return 0;
}

//==============================================================================

audio_mp3_mgr::audio_mp3_mgr( void )
{
}

//==============================================================================

audio_mp3_mgr::~audio_mp3_mgr( void )
{
    if( s_Initialized )
    {
        Kill();
    }
}

//==============================================================================

void audio_mp3_mgr::Init( void )
{
    ASSERT( s_Initialized == FALSE );
    s_Initialized = TRUE;
}

//==============================================================================

void audio_mp3_mgr::Kill( void )
{
    ASSERT( s_Initialized );

    for( s32 i = 0; i < MAX_AUDIO_STREAMS; i++ )
    {
        audio_stream* pStream = &g_AudioStreamMgr.m_AudioStreams[i];
        if( pStream->HandleMP3 )
        {
            Close( pStream );
        }
    }

    s_Initialized = FALSE;
}

//==============================================================================

void audio_mp3_mgr::Open( audio_stream* pStream )
{
    ASSERT( s_Initialized );
    ASSERT( IsValidStream( pStream ) );

    pStream->CursorMP3      = 0;
    pStream->WaveformCursor = 0;
    pStream->StreamDone     = FALSE;

    if( pStream->HandleMP3 )
    {
        mp3_decoder_state* pOldState = (mp3_decoder_state*)pStream->HandleMP3;
        x_free( pOldState );
        pStream->HandleMP3 = NULL;
    }

    if( (pStream->FileHandle == NULL) || (pStream->WaveformLength == 0) )
        return;

    mp3_decoder_state* pState = (mp3_decoder_state*)x_malloc( sizeof( mp3_decoder_state ) );
    ASSERT( pState );

    mp3_state_reset( *pState, pStream );

    pStream->HandleMP3 = pState;
}

//==============================================================================

void audio_mp3_mgr::Close( audio_stream* pStream )
{
    ASSERT( s_Initialized );
    ASSERT( IsValidStream( pStream ) );

    if( pStream->HandleMP3 )
    {
        mp3_decoder_state* pState = (mp3_decoder_state*)pStream->HandleMP3;
        x_free( pState );
        pStream->HandleMP3 = NULL;
    }
}

//==============================================================================

void audio_mp3_mgr::Seek( audio_stream* pStream )
{
    ASSERT( s_Initialized );
    ASSERT( IsValidStream( pStream ) );

    if( pStream->HandleMP3 == NULL )
        return;

    // Lock the audio hardware.
    g_AudioHardware.Lock();

    pStream->CursorMP3      = 0;
    pStream->WaveformCursor = 0;
    pStream->StreamDone     = FALSE;

    mp3_decoder_state* pState = (mp3_decoder_state*)pStream->HandleMP3;
    mp3_state_reset( *pState, pStream );

    // Unlock it now.
    g_AudioHardware.Unlock();
}

//==============================================================================

void audio_mp3_mgr::Decode( audio_stream* pStream, s16* pBufferL, s16* pBufferR, s32 nSamples )
{
    ASSERT( s_Initialized );
    ASSERT( IsValidStream( pStream ) );
    ASSERT( nSamples >= 0 );
    ASSERT( nSamples <= 512 );

    if( nSamples <= 0 )
        return;

    if( nSamples > 512 )
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

    mp3_decoder_state* pState = (mp3_decoder_state*)pStream->HandleMP3;

    // Lock the audio hardware.
    g_AudioHardware.Lock();

    xbool bIsStereo     = (pStream->Type == STEREO_STREAM);
    s16*  pOutL         = pBufferL;
    s16*  pOutR         = pBufferR;
    s32   SamplesNeeded = nSamples;

    while( SamplesNeeded > 0 )
    {
        if( pState->SamplesAvailable <= 0 )
        {
            s32 Decoded = mp3_state_decode_frame( pStream, *pState );
            if( Decoded <= 0 )
                break;
        }

        s32 CopyCount = x_min( pState->SamplesAvailable, SamplesNeeded );

        if( bIsStereo )
        {
            ASSERT( pOutL );
            ASSERT( pOutR );

            if( pState->Channels == 2 )
            {
                s16* pSrc = pState->SamplesBuffer + (pState->SampleOffset * 2);
                for( s32 i = 0; i < CopyCount; i++ )
                {
                    *pOutL++ = *pSrc++;
                    *pOutR++ = *pSrc++;
                }
            }
            else
            {
                s16* pSrc = pState->SamplesBuffer + pState->SampleOffset;
                for( s32 i = 0; i < CopyCount; i++ )
                {
                    s16 Sample = *pSrc++;
                    *pOutL++ = Sample;
                    *pOutR++ = Sample;
                }
            }
        }
        else
        {
            ASSERT( pOutL );

            if( pState->Channels == 2 )
            {
                s16* pSrc = pState->SamplesBuffer + (pState->SampleOffset * 2);
                for( s32 i = 0; i < CopyCount; i++ )
                {
                    *pOutL++ = pSrc[0];
                    pSrc += 2;
                }
            }
            else
            {
                s16* pSrc = pState->SamplesBuffer + pState->SampleOffset;
                x_memcpy( pOutL, pSrc, CopyCount * sizeof(s16) );
                pOutL += CopyCount;
            }
        }

        pState->SampleOffset     += CopyCount;
        pState->SamplesAvailable -= CopyCount;
        SamplesNeeded            -= CopyCount;
    }

    // Unlock it now.
    g_AudioHardware.Unlock();

    if( SamplesNeeded > 0 )
    {
        if( pOutL )
            x_memset( pOutL, 0, SamplesNeeded * sizeof(s16) );
        if( bIsStereo && pOutR )
            x_memset( pOutR, 0, SamplesNeeded * sizeof(s16) );
    }
}

//==============================================================================
