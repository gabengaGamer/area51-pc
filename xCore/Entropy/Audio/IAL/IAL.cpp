//==============================================================================
//
//  IAL.cpp
//
//  Interactive Audio Layer for XAudio 2.9
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include <windows.h>
#include <mmreg.h>
#include <xaudio2.h>

#include "x_files.hpp"
#include "x_debug.hpp"

#include "IAL.h"

//==============================================================================
//  INCLUDE ADDITIONAL LIBRARIES
//==============================================================================

#pragma comment( lib, "xaudio2.lib" )

//==============================================================================
//  DEFINES
//==============================================================================

//#define IAL_VERBOSE

#ifdef IAL_VERBOSE
#define LOGGING_ENABLED 1
#else
#define LOGGING_ENABLED 0
#endif

#define IAL_MAX_CHANNELS        128
#define IAL_SAMPLE_RATE         22050
#define IAL_FRAME_TIME_MS       20
#define IAL_SAMPLES_PER_FRAME   (IAL_SAMPLE_RATE/(1000/IAL_FRAME_TIME_MS)*4)

#if (IAL_SAMPLE_RATE/(1000/IAL_FRAME_TIME_MS)*(1000/IAL_FRAME_TIME_MS)) != IAL_SAMPLE_RATE
#error IAL_SAMPLES_PER_FRAME not integer
#endif

//==============================================================================
//  GLOBALS
//==============================================================================

xbool                   s_Initialized = FALSE;

IXAudio2*               s_pXAudio2;
IXAudio2MasteringVoice* s_pMasterVoice;
IXAudio2SourceVoice*    s_pSourceVoice;
f32                     s_SystemVolume = 1.0f;

HWND        IAL_hWnd        = NULL;

ial_channel IAL_Channels        [IAL_MAX_CHANNELS];
s32         IAL_MixL            [IAL_SAMPLES_PER_FRAME];
s32         IAL_MixR            [IAL_SAMPLES_PER_FRAME];
s16         IAL_Out             [IAL_SAMPLES_PER_FRAME*2];
s32         IAL_OutputAmplitude [2] = { 0, 0 };

CRITICAL_SECTION    IAL_CriticalSection;

void IAL_MixFrame( void );

struct IAL_VoiceCallback : public IXAudio2VoiceCallback
{
    STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) {}
    STDMETHOD_(void, OnVoiceProcessingPassEnd)() {}
    STDMETHOD_(void, OnStreamEnd)() {}
    STDMETHOD_(void, OnBufferStart)(void*) {}
    STDMETHOD_(void, OnBufferEnd)(void*) { IAL_MixFrame(); }
    STDMETHOD_(void, OnLoopEnd)(void*) {}
    STDMETHOD_(void, OnVoiceError)(void*, HRESULT) {}
} s_VoiceCallback;

//==============================================================================
// FUNCTIONS
//==============================================================================

void IAL_GetMutex( void )
{
    if (!s_Initialized)
        return;

    EnterCriticalSection( &IAL_CriticalSection );
}

//==============================================================================

void IAL_ReleaseMutex( void )
{
    if (!s_Initialized)
        return;

    LeaveCriticalSection( &IAL_CriticalSection );
}

//==============================================================================

void IAL_MixChannel( ial_channel* pChannel, s32* pL, s32* pR, s32 nDstSamples )
{
    f32 VolumeL = pChannel->VolumeL;
    f32 VolumeR = pChannel->VolumeR;
    s32 NewVolL = (s32)(32767.0f * MINMAX(-1.0f, VolumeL, 1.0f));
    s32 NewVolR = (s32)(32767.0f * MINMAX(-1.0f, VolumeR, 1.0f));
    s32 VolL    = pChannel->MixedVolL;
    s32 VolR    = pChannel->MixedVolR;
    s32 dVolL   = (NewVolL - VolL) / nDstSamples;
    s32 dVolR   = (NewVolR - VolR) / nDstSamples;

    // Mix the samples
    s32 iDst = 0;
    while( (nDstSamples > 0) && (pChannel->State == IAL_PLAY) )
    {
        s32 SrcStop;
        s32 SrcLoopTo;

        // Get the stop point in the source samples
        if( pChannel->Looped )
        {
/*
                        if( pChannel->Cursor > pChannel->LoopEnd )
                        {
                                SrcStop   = pChannel->nSamples;
                                SrcLoopTo = 0;
                        }
                        else if( (pChannel->Cursor + nDstSamples) > pChannel->LoopEnd )
                        {
                                SrcStop   = pChannel->LoopEnd;
                                SrcLoopTo = pChannel->LoopEnd-1;
                                pChannel->Looped = FALSE;
                        }
                        else
*/
                        {
                                SrcStop   = pChannel->LoopEnd;
                                SrcLoopTo = pChannel->LoopStart;
                        }
        }
        else
        {
            SrcStop   = pChannel->nSamples;
            SrcLoopTo = pChannel->nSamples-1;
        }

        s32 Ratio  = (s32)(65536.0f * pChannel->SampleRate * pChannel->Pitch / IAL_SAMPLE_RATE);
        s32 RatioI = Ratio >> 16;
        s32 RatioF = Ratio & 65535;

        // Mix the samples into the mix buffers
        s32 iSrc     = pChannel->Cursor;
        s32 iSrcFrac = pChannel->Fraction;
        while( (nDstSamples > 0) && (iSrc < SrcStop) )
        {
//            ASSERT( iSrc >= 0 );
//            ASSERT( iSrc < pChannel->nSamples );
//            ASSERT( iDst < IAL_SAMPLES_PER_FRAME );
//            ASSERT( !pChannel->Looped || (iSrc < pChannel->LoopEnd) );

            s32 iSrc2 = iSrc+1;
            if( iSrc2 >= SrcStop )
                iSrc2 = SrcLoopTo;

            s32 s1 = (pChannel->pData[iSrc ] * (65535-iSrcFrac)) >> 16;
            s32 s2 = (pChannel->pData[iSrc2] * iSrcFrac) >> 16;
            s32 s  = s1 + s2;

            pL[iDst] += (s * VolL) >> 15;
            pR[iDst] += (s * VolR) >> 15;

            iSrc     += RatioI;
            iSrcFrac += RatioF;
            iSrc     += iSrcFrac >> 16;
            iSrcFrac &= 65535;

            iDst++;
            nDstSamples--;

            VolL += dVolL;
            VolR += dVolR;
        }

        // At the end of the sample or the end of the loop?
        bool AtEnd      = iSrc >= pChannel->nSamples;
        bool AtLoopEnd  = pChannel->Looped && (iSrc >= pChannel->LoopEnd);
        if( AtLoopEnd )
        {
            pChannel->Cursor    = iSrc - (pChannel->LoopEnd - pChannel->LoopStart);
            pChannel->Fraction  = iSrcFrac;
        }
        else if( AtEnd )
        {
            pChannel->Cursor   = pChannel->nSamples;
            pChannel->Fraction = 0;
            pChannel->State    = IAL_DONE;
        }
        else
        {
            pChannel->Cursor   = iSrc;
            pChannel->Fraction = iSrcFrac;
        }
    }

    pChannel->MixedVolL = NewVolL;
    pChannel->MixedVolR = NewVolR;
}

//==============================================================================

void IAL_MixFrame( void )
{
    s32 iChannel;
    s32 i;
    s32 nSamples;

    if( !s_Initialized || !s_pSourceVoice )
        return;

    IAL_GetMutex();

    nSamples = IAL_SAMPLES_PER_FRAME;

    ZeroMemory( IAL_MixL, nSamples*sizeof(s32) );
    ZeroMemory( IAL_MixR, nSamples*sizeof(s32) );

    for( iChannel=0 ; iChannel<IAL_MAX_CHANNELS ; iChannel++ )
    {
        if( IAL_Channels[iChannel].State == IAL_PLAY )
            IAL_MixChannel( &IAL_Channels[iChannel], IAL_MixL, IAL_MixR, nSamples );
    }

    IAL_OutputAmplitude[0] = 0;
    IAL_OutputAmplitude[1] = 0;
    for( i=0 ; i<nSamples ; i++ )
    {
        s32 s;

        s = IAL_MixL[i];
        if( s < -32768 ) s = -32768;
        if( s >  32767 ) s =  32767;
        IAL_Out[i*2] = (s16)s;
        s = x_abs( s );
        IAL_OutputAmplitude[0] = max( IAL_OutputAmplitude[0], s );

        s = IAL_MixR[i];
        if( s < -32768 ) s = -32768;
        if( s >  32767 ) s =  32767;
        IAL_Out[i*2+1] = (s16)s;
        s = x_abs( s );
        IAL_OutputAmplitude[1] = max( IAL_OutputAmplitude[1], s );
    }

    XAUDIO2_BUFFER Buffer = {0};
    Buffer.AudioBytes = nSamples * sizeof(s16) * 2;
    Buffer.pAudioData = (const BYTE*)IAL_Out;
    s_pSourceVoice->SubmitSourceBuffer( &Buffer );

    IAL_ReleaseMutex();
}

//==============================================================================

xbool IAL_Init( HWND hWnd )
{
    ASSERT( !s_Initialized );

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_Init( 0x%08X )", hWnd );

    IAL_hWnd = hWnd;

    x_memset( &IAL_Channels, 0, sizeof(IAL_Channels) );

    if( FAILED( XAudio2Create( &s_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
        return FALSE;

    if( FAILED( s_pXAudio2->CreateMasteringVoice( &s_pMasterVoice ) ) )
        return FALSE;

    WAVEFORMATEX wfx;
    x_memset( &wfx, 0, sizeof(wfx) );
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = 2;
    wfx.nSamplesPerSec  = IAL_SAMPLE_RATE;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = 4;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if( FAILED( s_pXAudio2->CreateSourceVoice( &s_pSourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &s_VoiceCallback ) ) )
        return FALSE;

    InitializeCriticalSection( &IAL_CriticalSection );

    s_Initialized = TRUE;

    IAL_MixFrame();
    s_pSourceVoice->Start( 0 );

    return TRUE;
}

//==============================================================================

void IAL_Kill( void )
{
    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_Kill()" );

    if( s_pSourceVoice )
    {
        s_pSourceVoice->DestroyVoice();
        s_pSourceVoice = NULL;
    }

    if( s_pMasterVoice )
    {
        s_pMasterVoice->DestroyVoice();
        s_pMasterVoice = NULL;
    }

    if( s_pXAudio2 )
    {
        s_pXAudio2->Release();
        s_pXAudio2 = NULL;
    }

    DeleteCriticalSection( &IAL_CriticalSection );

    s_Initialized = FALSE;
}

//==============================================================================

void IAL_SetSystemVolume( f32 Volume )
{
    s_SystemVolume = Volume;
    if( s_pMasterVoice )
        s_pMasterVoice->SetVolume( Volume );
}

//==============================================================================

ial_hchannel IAL_allocate_channel( void )
{
    IAL_GetMutex();

    for( s32 i=0 ; i<IAL_MAX_CHANNELS ; i++ )
    {
        if( !IAL_Channels[i].Allocated )
        {
            IAL_Channels[i].Allocated = TRUE;
            IAL_Channels[i].Sequence++;
            IAL_ReleaseMutex();
            CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_allocate_channel() = %d", i + 1 );
            return( (IAL_Channels[i].Sequence << 16) + i + 1 );
        }
    }

    IAL_ReleaseMutex();

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_allocate_channel() = %d", 0 );

    return 0;
}

//==============================================================================

s32 IAL_hChannelToIndex( ial_hchannel hChannel )
{
    s32 Index       = (hChannel & 65535) - 1;
    s32 Sequence    = hChannel >> 16;

    if( Index < 0 )
        Index = IAL_MAX_CHANNELS;
    else if( Index >= IAL_MAX_CHANNELS )
        Index = IAL_MAX_CHANNELS;
    else if( IAL_Channels[Index].Sequence != Sequence )
        Index = IAL_MAX_CHANNELS;

    return Index;
}

//==============================================================================

void IAL_release_channel ( ial_hchannel hChannel )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return;

    IAL_GetMutex();

    IAL_Channels[Index].State     = IAL_DONE;
    IAL_Channels[Index].Allocated = FALSE;

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_release_channel() = %d", Index + 1 );

    IAL_ReleaseMutex();
}

//==============================================================================

void IAL_init_channel( ial_hchannel hChannel, void* pData, s32 nSamples, s32 LoopCount, s32 LoopStart, s32 LoopEnd,
                       ial_format Format, s32 SampleRate, f32 VolumeL, f32 VolumeR, f32 Pitch )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return;

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_init_channel( %d, 0x%08x, %d, %d, %d, %d, %d, %d, %f, %f, %f )", Index, (u32)pData, nSamples, LoopCount, LoopStart, LoopEnd, Format, SampleRate, VolumeL, VolumeR, Pitch );

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    Channel.pData       = (s16*)pData;
    Channel.Cursor      = 0;
    Channel.Fraction    = 0;
    Channel.Looped      = (LoopCount != 0);
    Channel.LoopStart   = LoopStart;
    Channel.LoopEnd     = LoopEnd;
    Channel.SampleRate  = SampleRate;
    Channel.nSamples    = nSamples;
    Channel.State       = IAL_STOP;
    Channel.VolumeL     = VolumeL;
    Channel.VolumeR     = VolumeR;
    Channel.Pitch       = Pitch;

    s32 NewVolL = (s32)(32767.0f * MINMAX(-1.0f, VolumeL, 1.0f));
    s32 NewVolR = (s32)(32767.0f * MINMAX(-1.0f, VolumeR, 1.0f));

    Channel.MixedVolL   = NewVolL;
    Channel.MixedVolR   = NewVolR;

    IAL_ReleaseMutex();
}

//==============================================================================

void IAL_start_channel( ial_hchannel hChannel )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return;

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_start_channel( %d )", Index );

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    Channel.State = IAL_PLAY;

    IAL_ReleaseMutex();
}

//==============================================================================

void IAL_stop_channel( ial_hchannel hChannel )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return;

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_stop_channel( %d )", Index );

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    //if( Channel.State == IAL_PLAY )
        Channel.State = IAL_STOP;

    IAL_ReleaseMutex();
}

//==============================================================================

void IAL_pause_channel( ial_hchannel hChannel )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return;

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_pause_channel( %d )", Index );

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    if( Channel.State == IAL_PLAY )
        Channel.State = IAL_PAUSED;

    IAL_ReleaseMutex();
}

//==============================================================================

void IAL_resume_channel( ial_hchannel hChannel )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return;

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_resume_channel( %d )", Index );

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    if( Channel.State == IAL_PAUSED )
        Channel.State = IAL_PLAY;

    IAL_ReleaseMutex();
}

//==============================================================================

void IAL_end_channel( ial_hchannel hChannel )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return;

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_end_channel( %d )", Index );

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    Channel.State = IAL_DONE;

    IAL_ReleaseMutex();
}

//==============================================================================

ial_state IAL_channel_status( ial_hchannel hChannel )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return IAL_DONE;

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    ial_state State = Channel.State;

    IAL_ReleaseMutex();

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_channel_status( %d ) = %d", Index, State );

    return State;
}

//==============================================================================

s32 IAL_channel_position( ial_hchannel hChannel )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return 0;

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    s32 Position = Channel.Cursor;

    IAL_ReleaseMutex();

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_channel_position( %d )", Index, Position );

    return Position;
}

//==============================================================================

void IAL_set_channel_volume( ial_hchannel hChannel, f32 VolumeL, f32 VolumeR )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return;

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_set_channel_volume( %d, %f, %f )", Index, VolumeL, VolumeR );

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    Channel.VolumeL = VolumeL;
    Channel.VolumeR = VolumeR;

    IAL_ReleaseMutex();
}

//==============================================================================

void IAL_set_channel_pitch( ial_hchannel hChannel, f32 Pitch )
{
    s32 Index = IAL_hChannelToIndex( hChannel );
    if( Index >= IAL_MAX_CHANNELS )
        return;

    CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_set_channel_pitch( %d, %f )", Index, Pitch );

    ial_channel& Channel = IAL_Channels[Index];

    IAL_GetMutex();

    Channel.Pitch = Pitch;

    IAL_ReleaseMutex();
}

//==============================================================================

void IAL_set_channel_looped( ial_hchannel hChannel, bool Looped )
{
        s32 Index = IAL_hChannelToIndex( hChannel );
        if( Index >= IAL_MAX_CHANNELS )
                return;

        CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_set_channel_looped( %s )", Looped ? "TRUE" : "FALSE" );

        ial_channel& Channel = IAL_Channels[Index];

        IAL_GetMutex();

        Channel.Looped = Looped;

        IAL_ReleaseMutex();
}

//==============================================================================

s32 IAL_get_output_amplitude( s32 Channel )
{
    ASSERT( Channel >= 0 );
    ASSERT( Channel <= 1 );

    return IAL_OutputAmplitude[Channel];
}

//==============================================================================

void IAL_stop_loop( ial_hchannel hChannel, s32 nSamples )
{
        s32 Index = IAL_hChannelToIndex( hChannel );
        if( Index >= IAL_MAX_CHANNELS )
                return;

        CLOG_MESSAGE( LOGGING_ENABLED, "IAL", "IAL_stop_loop" );

        ial_channel& Channel = IAL_Channels[Index];

        IAL_GetMutex();

        if( Channel.Looped )
        {
                Channel.LoopEnd = nSamples;
        }

        IAL_ReleaseMutex();
}
