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
    MP3_FINAL_FRAME_PAD_BYTES = 2304,
};

//------------------------------------------------------------------------------

static xbool mp3_probe_stream_header( const audio_stream* pStream, s32 ExpectedHz, s32 ExpectedChannels )
{
    ASSERT( pStream );

    s32 ProbeBytes = x_min( (s32)pStream->WaveformLength, MP3_BUFFER_SIZE );
    if( ProbeBytes <= 0 )
        return FALSE;

    const u8* pProbeData = (const u8*)pStream->MainRAM[0];
    s32       ProbeCursor = 0;
    s32       SkipCount   = 0;
    const s32 MAX_SKIP_ATTEMPTS = 100;

    while( (ProbeCursor < ProbeBytes) && (SkipCount <= MAX_SKIP_ATTEMPTS) )
    {
        mp3dec_t            ProbeDecoder;
        mp3dec_frame_info_t ProbeInfo;
        s32                 SamplesDecoded;
        s32                 BytesLeft;
        s32                 Channels;

        mp3dec_init( &ProbeDecoder );
        x_memset( &ProbeInfo, 0, sizeof( ProbeInfo ) );

        BytesLeft = ProbeBytes - ProbeCursor;
        SamplesDecoded = mp3dec_decode_frame( &ProbeDecoder,
                                              pProbeData + ProbeCursor,
                                              BytesLeft,
                                              NULL,
                                              &ProbeInfo );

        Channels = ProbeInfo.channels ? ProbeInfo.channels : ExpectedChannels;
        if( (SamplesDecoded > 0) &&
            (ProbeInfo.frame_bytes > 0) &&
            (ProbeInfo.layer == 3) &&
            (ProbeInfo.hz > 0) &&
            ((ExpectedHz <= 0) || (ProbeInfo.hz == ExpectedHz)) &&
            (Channels == ExpectedChannels) )
        {
            return TRUE;
        }

        s32 SkipBytes = 1;
        if( (SamplesDecoded > 0) && (ProbeInfo.frame_bytes > 0) )
        {
            SkipBytes = ProbeInfo.frame_offset + 1;
        }
        else if( ProbeInfo.frame_bytes > 0 )
        {
            SkipBytes = ProbeInfo.frame_bytes;
        }

        if( SkipBytes <= 0 )
            SkipBytes = 1;

        ProbeCursor += x_min( SkipBytes, BytesLeft );
        SkipCount++;
    }

    return FALSE;
}

struct audio_mp3_mgr::mp3_decoder_state
{
    mp3dec_t                Decoder;
    mp3dec_frame_info_t     FrameInfo;
    s32                     InputBytes;
    s32                     InputCursor;
    s32                     BytesConsumed;
    s32                     SamplesAvailable;
    s32                     SampleOffset;
    s32                     Channels;
    s32                     FrameHz;
    s32                     FrameChannels;
    s32                     ExpectedHz;
    s32                     ExpectedChannels;
    xbool                   EndOfStream;
    xbool                   HeaderValid;
    s16                     SamplesBuffer[MINIMP3_MAX_SAMPLES_PER_FRAME];
    u8                      InputBuffer[(MP3_BUFFER_SIZE * 2) + MP3_FINAL_FRAME_PAD_BYTES];
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

s32 audio_mp3_mgr::mp3_fetch_data( audio_stream* pStream, void* pBuffer, s32 nBytes, s32 Offset )
{
    (void)Offset;

    ASSERT( pStream );
    ASSERT( IsValidStream( pStream ) );
    ASSERT( (Offset == 0) || (Offset == -1) );

    if( Offset == 0 )
    {
        ASSERT( pStream->CursorMP3 < MP3_BUFFER_SIZE );
        pStream->CursorMP3 = 0;
    }

    s32 Previous    = pStream->CursorMP3;
    s32 Current     = Previous + nBytes;
    xbool bTransition = FALSE;

    if( Current > (MP3_BUFFER_SIZE * 2) )
    {
        ASSERT( Previous <= (MP3_BUFFER_SIZE * 2) );

        s32 Length = (MP3_BUFFER_SIZE * 2) - Previous;
        if( Length )
        {
            x_memcpy( pBuffer, (void*)(pStream->MainRAM[0] + Previous), Length );
            x_memset( (void*)(pStream->MainRAM[0] + Previous), 0, Length );
            pBuffer = (void*)((uaddr)pBuffer + Length);
        }

        Current -= (MP3_BUFFER_SIZE * 2);
        x_memcpy( pBuffer, (void*)pStream->MainRAM[0], Current );
        x_memset( (void*)pStream->MainRAM[0], 0, Current );
    }
    else
    {
        x_memcpy( pBuffer, (void*)(pStream->MainRAM[0] + Previous), nBytes );
        x_memset( (void*)(pStream->MainRAM[0] + Previous), 0, nBytes );
    }

    pStream->CursorMP3 = Current;

    if( Previous <= MP3_BUFFER_SIZE )
    {
        bTransition = (Current > MP3_BUFFER_SIZE);
    }
    else
    {
        bTransition = (Current <= MP3_BUFFER_SIZE);
    }

    if( bTransition && !pStream->StreamDone )
    {
        // Fill the read buffer.
        g_AudioStreamMgr.ReadStream( pStream );
    }

    return nBytes;
}

//==============================================================================

void audio_mp3_mgr::mp3_state_reset( mp3_decoder_state& State, const audio_stream* pStream )
{
    ASSERT( pStream );

    mp3dec_init( &State.Decoder );
    x_memset( &State.FrameInfo, 0, sizeof( State.FrameInfo ) );
    State.InputBytes        = 0;
    State.InputCursor       = 0;
    State.BytesConsumed     = 0;
    State.SamplesAvailable  = 0;
    State.SampleOffset      = 0;
    State.Channels          = (pStream->Type == STEREO_STREAM) ? 2 : 1;
    State.FrameHz           = 0;
    State.FrameChannels     = 0;
    State.ExpectedHz        = (pStream->Samples[0].Sample.SampleRate > 0) ? pStream->Samples[0].Sample.SampleRate : 0;
    State.ExpectedChannels  = (pStream->Type == STEREO_STREAM) ? 2 : 1;
    State.EndOfStream       = FALSE;
    State.HeaderValid       = FALSE;
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

    while( (State.InputBytes < MP3_BUFFER_SIZE) && !State.EndOfStream )
    {
        s32 Remaining = (s32)pStream->WaveformLength - State.BytesConsumed;
        if( Remaining <= 0 )
        {
            State.EndOfStream = TRUE;
            break;
        }

        s32 Capacity = (s32)sizeof( State.InputBuffer ) - State.InputBytes;
        if( Capacity <= 0 )
            break;

        s32 ToFetch = x_min( Remaining, MP3_BUFFER_SIZE );
        ToFetch     = x_min( ToFetch, Capacity );

        if( ToFetch <= 0 )
            break;

        s32 OffsetValue = (State.BytesConsumed == 0) ? 0 : -1;
        s32 Bytes = mp3_fetch_data( pStream, State.InputBuffer + State.InputBytes, ToFetch, OffsetValue );
        if( Bytes <= 0 )
        {
            State.EndOfStream = TRUE;
            break;
        }

        State.InputBytes    += Bytes;
        State.BytesConsumed += Bytes;

        if( Bytes < ToFetch )
        {
            State.EndOfStream = TRUE;
            break;
        }
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

xbool audio_mp3_mgr::mp3_state_validate_frame( const mp3_decoder_state& State )
{
    const mp3dec_frame_info_t& FrameInfo = State.FrameInfo;

    if( (FrameInfo.frame_bytes <= 0) ||
        (FrameInfo.layer != 3) ||
        (FrameInfo.hz <= 0) )
    {
        return FALSE;
    }

    s32 Channels = FrameInfo.channels ? FrameInfo.channels : State.ExpectedChannels;
    if( (Channels <= 0) || (Channels > 2) )
        return FALSE;

    if( (State.ExpectedHz > 0) && (FrameInfo.hz != State.ExpectedHz) )
        return FALSE;

    if( (State.ExpectedChannels > 0) && (Channels != State.ExpectedChannels) )
        return FALSE;

    if( State.HeaderValid )
    {
        if( (State.FrameHz != FrameInfo.hz) ||
            (State.FrameChannels != Channels) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

s32 audio_mp3_mgr::mp3_state_decode_frame( audio_stream* pStream, mp3_decoder_state& State )
{
    State.SamplesAvailable = 0;
    State.SampleOffset     = 0;

    mp3_state_refill( pStream, State );

    s32 SkipCount = 0;
    const s32 MAX_SKIP_ATTEMPTS = 100;

    while( mp3_state_available_bytes( State ) > 0 )
    {
        if( SkipCount > MAX_SKIP_ATTEMPTS )
        {
            State.EndOfStream = TRUE;
            break;
        }

        s32 BytesLeft      = mp3_state_available_bytes( State );
        mp3dec_t SavedDecoder = State.Decoder;
        s32 SamplesDecoded = mp3dec_decode_frame( &State.Decoder,
                                                  State.InputBuffer + State.InputCursor,
                                                  BytesLeft,
                                                  State.SamplesBuffer,
                                                  &State.FrameInfo );
        s32 FrameBytes     = State.FrameInfo.frame_bytes;
        s32 FrameOffset    = State.FrameInfo.frame_offset;

        if( FrameBytes <= 0 )
        {
            if( State.EndOfStream ||
                (State.InputCursor == 0 &&
                 State.InputBytes >= (s32)sizeof( State.InputBuffer )) )
            {
                FrameBytes = (BytesLeft > 0) ? 1 : 0;
                SkipCount++;
            }
        }
        else
        {
            SkipCount = 0;
        }

        if( SamplesDecoded > 0 )
        {
            if( !mp3_state_validate_frame( State ) )
            {
                // Miles kept searching until headers matched the original stream.
                State.Decoder = SavedDecoder;

                s32 SkipBytes = FrameOffset + 1;
                if( SkipBytes <= 0 )
                    SkipBytes = 1;

                State.InputCursor += x_min( SkipBytes, BytesLeft );
                if( State.InputCursor > State.InputBytes )
                    State.InputCursor = State.InputBytes;

                SkipCount++;
                mp3_state_refill( pStream, State );
                continue;
            }

            State.InputCursor += FrameBytes;
            if( State.InputCursor > State.InputBytes )
                State.InputCursor = State.InputBytes;

            State.Channels      = State.FrameInfo.channels ? State.FrameInfo.channels : State.ExpectedChannels;
            State.FrameHz       = State.FrameInfo.hz;
            State.FrameChannels = State.Channels;
            State.HeaderValid   = TRUE;

            State.SamplesAvailable = SamplesDecoded;
            State.SampleOffset     = 0;
            return SamplesDecoded;
        }

        if( (SamplesDecoded <= 0) && State.EndOfStream && (BytesLeft > 0) )
        {
            s32 PadBytes = x_min( (s32)MP3_FINAL_FRAME_PAD_BYTES,
                                  (s32)sizeof( State.InputBuffer ) - (State.InputCursor + BytesLeft) );

            if( PadBytes > 0 )
            {
                mp3dec_t            PaddedDecoder = SavedDecoder;
                mp3dec_frame_info_t OriginalInfo  = State.FrameInfo;
                mp3dec_frame_info_t PaddedInfo;
                s32                 PaddedSamples;

                x_memset( State.InputBuffer + State.InputCursor + BytesLeft, 0, PadBytes );
                x_memset( &PaddedInfo, 0, sizeof( PaddedInfo ) );

                PaddedSamples = mp3dec_decode_frame( &PaddedDecoder,
                                                     State.InputBuffer + State.InputCursor,
                                                     BytesLeft + PadBytes,
                                                     State.SamplesBuffer,
                                                     &PaddedInfo );

                if( PaddedSamples > 0 )
                {
                    State.FrameInfo = PaddedInfo;

                    if( mp3_state_validate_frame( State ) )
                    {
                        State.Decoder          = PaddedDecoder;
                        State.InputCursor      = State.InputBytes;
                        State.Channels         = State.FrameInfo.channels ? State.FrameInfo.channels : State.ExpectedChannels;
                        State.FrameHz          = State.FrameInfo.hz;
                        State.FrameChannels    = State.Channels;
                        State.HeaderValid      = TRUE;
                        State.SamplesAvailable = PaddedSamples;
                        State.SampleOffset     = 0;
                        return PaddedSamples;
                    }
                }

                State.FrameInfo = OriginalInfo;
            }
        }

        State.InputCursor += FrameBytes;
        if( State.InputCursor > State.InputBytes )
            State.InputCursor = State.InputBytes;

        if( State.EndOfStream && (mp3_state_available_bytes( State ) <= 0) )
            break;

        mp3_state_refill( pStream, State );
    }

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

    pStream->CursorMP3 = 0;

    if( pStream->HandleMP3 )
    {
        mp3_decoder_state* pOldState = (mp3_decoder_state*)pStream->HandleMP3;
        x_free( pOldState );
        pStream->HandleMP3 = NULL;
    }

    mp3_decoder_state* pState = (mp3_decoder_state*)x_malloc( sizeof( mp3_decoder_state ) );
    ASSERT( pState );

    mp3_state_reset( *pState, pStream );

    if( !mp3_probe_stream_header( pStream, pState->ExpectedHz, pState->ExpectedChannels ) )
    {
        x_free( pState );
        return;
    }

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

    pStream->CursorMP3 = 0;

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

    mp3_decoder_state* pState = (mp3_decoder_state*)pStream->HandleMP3;

    // Lock the audio hardware.
    g_AudioHardware.Lock();

    xbool bIsStereo = (pStream->Type == STEREO_STREAM);
    s16* pOutL      = pBufferL;
    s16* pOutR      = pBufferR;
    s32  SamplesNeeded = nSamples;

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

        pState->SampleOffset    += CopyCount;
        pState->SamplesAvailable -= CopyCount;
        SamplesNeeded           -= CopyCount;
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
