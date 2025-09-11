#include "x_target.hpp"

#if !defined(TARGET_PC)
#error This is for a PC target build. Please exclude from build rules.
#endif

#include "audio_channel_mgr.hpp"
#include "audio_hardware.hpp"
#include "audio_package.hpp"
#include "audio_inline.hpp"
#include "audio_stream_mgr.hpp"
#include "e_ScratchMem.hpp"
#include "audio_mp3_mgr.hpp"

#include "entropy.hpp"

//------------------------------------------------------------------------------

#include "audio_prologic2.hpp" // g_ProLogicIICoeff takes too much space

//------------------------------------------------------------------------------
// PC specific defines. 

#define MAX_HARDWARE_CHANNELS   (64)
#define MINIMUM_PRIORITY        (1)
#define MAXIMUM_PRIORITY        (31)
#define DSP_SAMPLE_RATE         (32000)     // DSP running at 32kHz

//------------------------------------------------------------------------------

static xbool        s_IsInitialized         = FALSE;        // Sentinel
static channel      s_Channels[ MAX_HARDWARE_CHANNELS ];    // Channel buffer

//------------------------------------------------------------------------------

audio_hardware g_AudioHardware;

//==============================================================================
// Hardware specific functions.
//==============================================================================

static void pc_UpdateStreamPCM( channel* pChannel )
{
    u32   CurrentPosition  = pChannel->CurrBufferPosition;
    u32   PreviousPosition = pChannel->StreamData.PreviousPosition; 
    s32   Transition;
    xbool bTransition;

    // Update previous.
    pChannel->StreamData.PreviousPosition = CurrentPosition;

    // Which buffer are we in?
    if( PreviousPosition <= pChannel->MidPoint )
    {
        // Did a buffer transition occur?
        bTransition = (CurrentPosition > pChannel->MidPoint);
        Transition  = 1;
    }
    else
    {
        // Did a buffer transition occur?
        bTransition = (CurrentPosition <= pChannel->MidPoint);
        Transition  = 2;
    }

    // Transition occur?
    if( bTransition )
    {
        // Update the base position if buffer wrap occured.
        if( Transition == 2 )
        {
            pChannel->Hardware.BasePosition += STREAM_BUFFER_SIZE;
        }

        // Stream done?
        if( pChannel->StreamData.pStream->StreamDone )
        {
        }
        // Need to read from the stream?
        else if( pChannel->StreamData.StreamControl )
        {
            // Fill the read buffer.
            g_AudioStreamMgr.ReadStream( pChannel->StreamData.pStream );
        }
    }

    if( pChannel->StreamData.pStream && pChannel->StreamData.pStream->StreamDone )
	{
		// Calculate absolute position
        pChannel->Hardware.CurrentPosition = pChannel->Hardware.BasePosition + pChannel->CurrBufferPosition;

        // Need to release it?
        if( pChannel->Hardware.CurrentPosition >= pChannel->Sample.pColdSample->nSamples )
        {
            // Nuke it.
            g_AudioHardware.ReleaseChannel( pChannel );
        }
	}
}

//------------------------------------------------------------------------------

static void pc_UpdateStreamMP3( channel* pChannel )
{
	hot_sample* pSample = pChannel->Sample.pHotSample;

	// Did a wrap occur?
	if( (pChannel->StreamData.PreviousPosition >= pChannel->MidPoint) && 
		(pChannel->CurrBufferPosition < pChannel->MidPoint) )
	{
		LOG_MESSAGE( "pc_UpdateStreamMP3", "Played a BUFFER!" );
		pChannel->Hardware.BasePosition += STREAM_BUFFER_SIZE;
	}

	// Update previous.
	pChannel->StreamData.PreviousPosition = pChannel->CurrBufferPosition;

	if( pChannel->StreamData.StreamControl )
	{
		// Get the current position within the sound, position is in WORDS.
		u32 CurrentPosition = pChannel->CurrBufferPosition*2;
loop:
		u32 Cursor = pChannel->StreamData.WriteCursor;
		
		if( CurrentPosition > Cursor )
		{
			while( (((STREAM_BUFFER_SIZE*2) - CurrentPosition) + Cursor) < (STREAM_BUFFER_SIZE) )
			{
				g_AudioHardware.UpdateMP3( pChannel->StreamData.pStream );
				goto loop;
			}
		}
		else
		{
			while( (Cursor-CurrentPosition) < (STREAM_BUFFER_SIZE) )
			{
				g_AudioHardware.UpdateMP3( pChannel->StreamData.pStream );
				goto loop;
			}
		}
	}

	// Calculate absolute position
	pChannel->Hardware.CurrentPosition = pChannel->Hardware.BasePosition + pChannel->CurrBufferPosition;

	// Need to release it?
	if( pChannel->Hardware.CurrentPosition >= pSample->nSamples )
	{
		// Nuke it.
		g_AudioHardware.ReleaseChannel( pChannel );
	}
}

//------------------------------------------------------------------------------

s32 GetAudioLevel( void )
{
    s32 Left  = IAL_get_output_amplitude( 0 );
    s32 Right = IAL_get_output_amplitude( 1 );
    return max( Left, Right );
}

//------------------------------------------------------------------------------

void audio_hardware::UpdateStream( channel* pChannel )
{
    // Error check.
    ASSERT( pChannel );
    ASSERT( pChannel->StreamData.pStream );

    // Cold, active, running channel?
    if( pChannel && (pChannel->Type == COLD_SAMPLE) && (pChannel->State == STATE_RUNNING) )
    { 
        // What kind of compression?
        switch( pChannel->Sample.pHotSample->CompressionType )
        {
            // PCM?
            case PCM:
                pc_UpdateStreamPCM( pChannel );
                break;

			case MP3:
				pc_UpdateStreamMP3( pChannel );
				break;

            default:
                ASSERT( 0 );
                break;
        }
    }
}
//------------------------------------------------------------------------------

audio_hardware::audio_hardware( void )
{
    m_FirstChannel   = s_Channels;
    m_LastChannel    = s_Channels + (MAX_HARDWARE_CHANNELS - 1);
    m_InterruptLevel = 0;
    m_InterruptState = FALSE;
}

//------------------------------------------------------------------------------

audio_hardware::~audio_hardware( void )
{
}

//------------------------------------------------------------------------------
void* audio_hardware::AllocAudioRam( s32 nBytes )
{
    return x_malloc(nBytes);
}

//------------------------------------------------------------------------------

void audio_hardware::FreeAudioRam( void* Address )
{
    x_free(Address);
}

//------------------------------------------------------------------------------

s32 audio_hardware::GetAudioRamFree     ( void )
{
    return 0;
}

//------------------------------------------------------------------------------

void audio_hardware::Init( s32 MemSize )
{
    ASSERTS( !s_IsInitialized, "Already initialized" );

    IAL_Init();

    (void)MemSize;

    for( s32 i=0 ; i<MAX_HARDWARE_CHANNELS; i++ )
    {
        channel& Channel = s_Channels[ i ];
        Channel.Hardware.hChannel        = IAL_allocate_channel();
        Channel.Hardware.CurrentPosition = 0;
        Channel.Hardware.BasePosition    = 0;
        Channel.Hardware.InUse           = FALSE;
    }

	// Start up the mp3 decoder
	g_AudioMP3Mgr.Init();

    // ok its done.
    s_IsInitialized = TRUE;
}

//------------------------------------------------------------------------------

void audio_hardware::Kill( void )
{
    ASSERT( s_IsInitialized );

    // Kill IAL
    IAL_Kill();

    s_IsInitialized = FALSE;
}

//------------------------------------------------------------------------------
// Doesn't do anything on PC
//
void audio_hardware::ResizeMemory( s32 )
{
}

//------------------------------------------------------------------------------

s32 audio_hardware::NumChannels( void )
{
    return MAX_HARDWARE_CHANNELS;
}

//------------------------------------------------------------------------------

channel* audio_hardware::GetChannelBuffer( void )
{
    return s_Channels;
}

//------------------------------------------------------------------------------

// TODO: Inline this function.
xbool audio_hardware::AcquireChannel( channel* pChannel )
{
    ASSERT( !pChannel->Hardware.InUse );
    pChannel->Hardware.InUse = TRUE;

    // Tell the world!
    return( pChannel->Hardware.hChannel != NULL );
}

//------------------------------------------------------------------------------

// TODO: Inline this function.
void audio_hardware::ReleaseChannel( channel* pChannel )
{

    // Look for segue.
    if( pChannel->pElement && pChannel->pElement->pVoice )
    {
        voice* pVoice = pChannel->pElement->pVoice;
        voice* pNext  = pVoice->pSegueVoiceNext;
        voice* pPrev  = pVoice->pSegueVoicePrev;

        // Take it out of list
        if( pPrev )
        {
            pPrev->pSegueVoiceNext = NULL;
        }

        if( pNext && (pNext->StartQ == 0) )
        {
            // Mark the queued voice to start.
            pNext->StartQ          = 1;
            pNext->pSegueVoicePrev = NULL;
        }
    }

    // If the hardware channel is active, stop it!
    //if( pChannel->Hardware.hChannel )
	if( pChannel->Hardware.InUse )
    {
        IAL_end_channel( pChannel->Hardware.hChannel );

        // Does this channel have a stream?
        if( pChannel->StreamData.pStream && pChannel->StreamData.StreamControl )
        {
            // If so, nuke the stream
            g_AudioStreamMgr.ReleaseStream( pChannel->StreamData.pStream );
            pChannel->StreamData.pStream = NULL;
        }

        // No longer in use.
        pChannel->Hardware.InUse = FALSE;
    }
}

//------------------------------------------------------------------------------

// TODO: Inline this function.
void audio_hardware::ClearChannel( channel* pChannel )
{
}

//------------------------------------------------------------------------------

// TODO: Inline this function.
void audio_hardware::DuplicatePriority( channel* pDest, channel* pSrc )
{
    // Just until the callback runs.
    pDest->Hardware.Priority = pSrc->Hardware.Priority;
}

//------------------------------------------------------------------------------

// TODO: Inline this function.
xbool audio_hardware::IsChannelActive( channel* pChannel )
{
    return pChannel->Hardware.InUse;
}

//------------------------------------------------------------------------------

void audio_hardware::StartChannel( channel* pChannel )
{
    // If the channel has hardware then start it!
    if( pChannel->Hardware.hChannel )
        IAL_start_channel( pChannel->Hardware.hChannel );
}

//------------------------------------------------------------------------------

void audio_hardware::StopChannel( channel* pChannel )
{
    // If the hardware channel is active, stop it!
    if( pChannel->Hardware.hChannel )
        IAL_stop_channel( pChannel->Hardware.hChannel );
}

//------------------------------------------------------------------------------

void audio_hardware::PauseChannel( channel* pChannel )
{
    // If the channel has hardware then start it!
    if( pChannel->Hardware.hChannel )
        IAL_pause_channel( pChannel->Hardware.hChannel );
}

//------------------------------------------------------------------------------

void audio_hardware::ResumeChannel( channel* pChannel )
{
    // If the hardware channel is active, stop it!
    if( pChannel->Hardware.hChannel )
        IAL_resume_channel( pChannel->Hardware.hChannel );
}

//------------------------------------------------------------------------------

void audio_hardware::InitChannel( channel* pChannel )
{
    hot_sample*      pHotSample = pChannel->Sample.pHotSample;
    xbool            IsLooped   = (pHotSample->LoopEnd > 0);
    u32              AudioRam   = pHotSample->AudioRam;
    ial_hchannel     hChannel   = pChannel->Hardware.hChannel;

    pChannel->Hardware.CurrentPosition = 0;
    pChannel->CurrBufferPosition       = 0;
    pChannel->PrevBufferPosition       = 0;

    switch( pHotSample->CompressionType )
    {
    case PCM:
        // Init the sample
        IAL_init_channel( hChannel, (void*)pHotSample->AudioRam, pHotSample->nSamples, IsLooped ? 1 : 0, pHotSample->LoopStart, pHotSample->LoopEnd, pHotSample->SampleRate, pChannel->Volume*pChannel->Pan3d.GetX(), pChannel->Volume*pChannel->Pan3d.GetY(), 1.0f );
        break;
    case ADPCM:
        // Init the sample
        ASSERT( 0 );
        break;
    case MP3:
        // Init the sample
        ASSERT( 0 );
        break;
    }
}

//------------------------------------------------------------------------------

void audio_hardware::InitChannelStreamed( channel* pChannel )
{
    hot_sample*     pHotSample  = pChannel->Sample.pHotSample;
    xbool           IsLooped    = (pChannel->Sample.pHotSample->LoopEnd != 0);
    u32             AudioRam    = pHotSample->AudioRam;
    ial_hchannel    hChannel    = pChannel->Hardware.hChannel;

    s32 nSampleBytes = pChannel->Sample.pHotSample->nSamples * 2;
    s32 nBufferBytes = STREAM_BUFFER_SIZE*2;

    // Set the current position
    pChannel->CurrBufferPosition = 0;
	pChannel->PrevBufferPosition = 0;
	pChannel->StreamData.PreviousPosition = 0;
	pChannel->StreamData.WriteCursor      = 0;

	// Nuke the number of samples played.
	pChannel->StreamData.nSamplesPlayed = 0;

    // setup the buffer pointer
    hot_sample* pSample  = pChannel->Sample.pHotSample;

    // MUST be looped!
    ASSERT( IsLooped );

    // Set the mid point, clear loop stop.
    pChannel->MidPoint                 = STREAM_BUFFER_SIZE / 2;
    pChannel->Hardware.CurrentPosition = 0;
    pChannel->Hardware.BasePosition    = 0;
    pChannel->PrevBufferPosition       = 0;

    // Init.
    pChannel->nSamplesAdjust  =
    pChannel->nSamplesBase    =
    pChannel->PlayPosition    =
    pChannel->ReleasePosition = 0;

    s32 LoopStart = 0;
    s32 LoopEnd   = STREAM_BUFFER_SIZE;

    // Will sample fit in one buffer?
    if( nSampleBytes < nBufferBytes )
    {
        IsLooped = FALSE;
        LoopEnd  = nSampleBytes / 2;
    }

    IAL_init_channel( hChannel,
                      (void*)pHotSample->AudioRam,
                      LoopEnd,
                      IsLooped ? 1 : 0,
                      LoopStart, LoopEnd,
                      pHotSample->SampleRate,
                      pChannel->Volume*pChannel->Pan3d.GetX(),
                      pChannel->Volume*pChannel->Pan3d.GetY(),
                      1.0f );
}

//------------------------------------------------------------------------------

void audio_hardware::Lock( void )
{
    m_LockMutex.Acquire();
    m_InterruptLevel++;
}

//------------------------------------------------------------------------------

void audio_hardware::Unlock( void )
{
    m_InterruptLevel--;
    m_LockMutex.Release();
}

//------------------------------------------------------------------------------

u32 audio_hardware::GetSamplesPlayed( channel* pChannel )
{
    u32 SamplesPlayed = 0;
    if( pChannel )
    {
        // What kind of compression?
        switch( pChannel->Sample.pHotSample->CompressionType )
        {
            // ADPCM?
            case ADPCM:
            {
                ASSERT( 0 );
                break;
            }

            case PCM:
                pChannel->nSamplesAdjust = pChannel->CurrBufferPosition;
                break;

			case MP3:
				pChannel->nSamplesAdjust = pChannel->CurrBufferPosition;
				break;

            default:
                ASSERT( 0 );
                break;
        }
    

        // Tell the world. (base is always 0 on pc until streaming is implemented).
        SamplesPlayed = pChannel->nSamplesBase + pChannel->nSamplesAdjust;
    }
    return SamplesPlayed;
}

//------------------------------------------------------------------------------

static xbool UpdatePosition( channel* pChannel )
{
    // Only need to do special stuff for cold samples
    if( pChannel->Type == COLD_SAMPLE )
    {
        // Played an entire buffers worth of samples?
        if( (pChannel->PrevBufferPosition >= pChannel->MidPoint) &&  
            (pChannel->CurrBufferPosition  <  pChannel->MidPoint) )
        {
            // What kind of compression?
            switch( pChannel->Sample.pHotSample->CompressionType )
            {
                // PCM?
                case PCM:
                    // This is one messed up equation...
                    pChannel->nSamplesBase += STREAM_BUFFER_SIZE;
                    break;

					// MP3?
				case MP3:
					// Update the number of samples played (PCM is word 
					// based so DONT multiply STREAM_BUFFER_SIZE by 2).
					pChannel->nSamplesBase += STREAM_BUFFER_SIZE;
					break;

                default:
                    ASSERT( 0 );
                    break;
            }
        }
    
        // Update previous.
        pChannel->PrevBufferPosition = pChannel->CurrBufferPosition;
    }

    // Release position specified?
    if( pChannel->ReleasePosition )
    {
        // Calculate number of samples played.
        pChannel->PlayPosition = g_AudioHardware.GetSamplesPlayed( pChannel );

        // Past the release position?
        if( pChannel->PlayPosition >= pChannel->ReleasePosition )
        {
            // All bad...
            return FALSE;
        }
    }

    // All good!
    return TRUE;
}

//------------------------------------------------------------------------------

static s16 s_LeftBuffer[MAX_AUDIO_STREAMS][512];
static s16 s_RightBuffer[MAX_AUDIO_STREAMS][512];
static int s_WhichBuffer = 0;

#include "stdio.h"

FILE* fL = NULL;
FILE* fR = NULL;

void audio_hardware::UpdateMP3( audio_stream* pStream )
{
	if( !pStream || !pStream->pChannel[0] )
        return;
        
    if( pStream->Type == STEREO_STREAM && !pStream->pChannel[1] )
        return;
	
	u32 ARAM;
	u32 Cursor;

	// Decode 512 Samples
	g_AudioMP3Mgr.Decode( pStream, s_LeftBuffer[ s_WhichBuffer ], s_RightBuffer[ s_WhichBuffer ], 512 );

	// Copy the left channel
	Cursor = pStream->pChannel[0]->StreamData.WriteCursor;
	ARAM   = pStream->Samples[0].Sample.AudioRam + Cursor;
	x_memcpy( (void*)ARAM, &s_LeftBuffer[ s_WhichBuffer ], 512 * sizeof(s16) );
	Cursor += 512*sizeof(s16);
	if( Cursor >= STREAM_BUFFER_SIZE*2 )
		Cursor = 0;
	pStream->pChannel[0]->StreamData.WriteCursor = Cursor;

	// Stereo?
	if( pStream->Type == STEREO_STREAM )
	{
		// Copy the right channel
		Cursor = pStream->pChannel[1]->StreamData.WriteCursor;
		ARAM   = pStream->Samples[1].Sample.AudioRam + Cursor;
		x_memcpy( (void*)ARAM, &s_RightBuffer[ s_WhichBuffer ], 512 * sizeof(s16) );
		Cursor += 512*sizeof(s16);
		if( Cursor >= STREAM_BUFFER_SIZE*2 )
			Cursor = 0;
		pStream->pChannel[1]->StreamData.WriteCursor = Cursor;
	}

	// Switch buffers.
	if( ++s_WhichBuffer >= MAX_AUDIO_STREAMS )
		s_WhichBuffer = 0;
}

//------------------------------------------------------------------------------

void audio_hardware::Update( void )
{
    u32         Dirty;
    channel*    pChannel;
    channel*    pHead;
    xbool       SetPriority;
    s32         Priority;
    xbool       bCanStart;
    xbool       bQueueStart = FALSE;

    // Ok to do the hardwre update?
    bCanStart = g_AudioHardware.GetDoHardwareUpdate();

    // Clear it.
    g_AudioHardware.ClearDoHardwareUpdate();

    // Need to update the priorities?
    if( (SetPriority = g_AudioHardware.GetDirtyBit( CALLBACK_DB_PRIORITY )) != 0 )
    {
        // Clear the dirty bit.
        g_AudioHardware.ClearDirtyBit( CALLBACK_DB_PRIORITY );

        // Start at lowest priority.
        Priority = MINIMUM_PRIORITY;
    }

    // Loop through active channels (starting with lowest priority).
    pHead    = g_AudioChannelMgr.UsedList();
    pChannel = pHead->Link.pPrev;

    while( pChannel != pHead )
    {
        channel* pPrevChannel = pChannel->Link.pPrev;

        // Hardware voice to update?
        if( pChannel->Hardware.hChannel )
        {
            // Check if anything needs updating for the voice.
            Dirty = pChannel->Dirty;
            if( Dirty )
            {
                // Volume Dirty?
                if( Dirty & (CHANNEL_DB_VOLUME | CHANNEL_DB_PAN) )
                {
                    if( pChannel->pElement && 
                        pChannel->pElement->pVoice && 
                        pChannel->pElement->pVoice->IsPositional &&
                        pChannel->pElement->pVoice->DegreesToSound != -1 )
                    {
                        s32 j = pChannel->pElement->pVoice->DegreesToSound;
                        f32 vLeft  = g_ProLogicIICoeff[j][0] * pChannel->Volume;
                        f32 vRight = g_ProLogicIICoeff[j][1] * pChannel->Volume;
                        IAL_set_channel_volume( pChannel->Hardware.hChannel, vLeft, vRight );
                    }
                    else
                    {
                        IAL_set_channel_volume( pChannel->Hardware.hChannel, pChannel->Pan3d.GetX() * pChannel->Volume, pChannel->Pan3d.GetY() * pChannel->Volume );
                    }
                    Dirty &= ~(CHANNEL_DB_VOLUME | CHANNEL_DB_PAN);
                }

                // Pitch Dirty?
                if( Dirty & CHANNEL_DB_PITCH )
                {
                    // Set pitch into hardware.
                    IAL_set_channel_pitch( pChannel->Hardware.hChannel, pChannel->Pitch );

                    // Clear dirty bit.
                    Dirty &= ~CHANNEL_DB_PITCH;
                }

                if( Dirty & CHANNEL_DB_EFFECTSEND )
                {
                    // TODO: Put Effect send in.

                    // Clear dirty bit.
                    Dirty &= ~CHANNEL_DB_EFFECTSEND;
                }
            }

            // Update the dirty bits.
            pChannel->Dirty = Dirty;

            // Update state machine.
            switch( pChannel->State )
            {
                case STATE_NOT_STARTED:
                    break;
                case STATE_STARTING:
                {
                    xbool bStart = bCanStart;

                    if( pChannel->pElement && pChannel->pElement->pVoice && (pChannel->pElement->pVoice->StartQ==2) )
                    {
                        bStart      = TRUE;
                        bQueueStart = TRUE;
                    }

                    if( bStart )

                    {
                        StartChannel( pChannel );
                        pChannel->State = STATE_RUNNING;
                    }
                    break;
                }
                case STATE_RESUMING:
                    if( bCanStart )
                    {
                        g_AudioHardware.ResumeChannel( pChannel );
                        pChannel->State = STATE_RUNNING;
                    }
                    break;
                case STATE_RUNNING:

                    pChannel->CurrBufferPosition = IAL_channel_position( pChannel->Hardware.hChannel );

                    if( !UpdatePosition( pChannel ) || (IAL_channel_status( pChannel->Hardware.hChannel ) == IAL_DONE) )
                    {
                        // Release the channel.
                        g_AudioHardware.ReleaseChannel( pChannel );
                        pChannel->State = STATE_STOPPED;
                    }
                    break;
                case STATE_PAUSING:
                    if( bCanStart )
                    {
                        g_AudioHardware.PauseChannel( pChannel );
                        pChannel->State = STATE_PAUSED;
                    }
                    break;
                case STATE_PAUSED:
                case STATE_STOPPED:
                    break;
            }
        }

        // Previous channel...
        pChannel = pPrevChannel;
    }

    // Special stuff when we start a queued sound.
    if( bQueueStart )
    {
        // Loop through active channels (starting with lowest priority).
        pHead    = g_AudioChannelMgr.UsedList();
        pChannel = pHead->Link.pPrev;

        while( pChannel != pHead )
        {    
            if( pChannel->pElement && pChannel->pElement->pVoice && (pChannel->pElement->pVoice->StartQ==2) )
                pChannel->pElement->pVoice->StartQ = 0;
            
            // Previous channel...
            pChannel = pChannel->Link.pPrev;
        }
    }
}

//------------------------------------------------------------------------------
