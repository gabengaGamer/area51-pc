//==============================================================================
//
//  audio_output_mixer.hpp
//
//  Internal software mixer used by audio output backends.
//
//==============================================================================

#ifndef AUDIO_OUTPUT_MIXER_HPP
#define AUDIO_OUTPUT_MIXER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Audio/backend/audio_render_command.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define AUDIO_OUTPUT_SAMPLE_RATE         22050
#define AUDIO_OUTPUT_FRAME_TIME_MS       20
#define AUDIO_OUTPUT_SAMPLES_PER_FRAME   (AUDIO_OUTPUT_SAMPLE_RATE/(1000/AUDIO_OUTPUT_FRAME_TIME_MS)*4)

#if (AUDIO_OUTPUT_SAMPLE_RATE/(1000/AUDIO_OUTPUT_FRAME_TIME_MS)*(1000/AUDIO_OUTPUT_FRAME_TIME_MS)) != AUDIO_OUTPUT_SAMPLE_RATE
#error AUDIO_OUTPUT_SAMPLES_PER_FRAME not integer
#endif

//#define AUDIO_OUTPUT_VERBOSE

#ifdef AUDIO_OUTPUT_VERBOSE
#define AUDIO_OUTPUT_LOGGING_ENABLED 1
#else
#define AUDIO_OUTPUT_LOGGING_ENABLED 0
#endif

//==============================================================================
//  TYPES
//==============================================================================

enum audio_output_state
{
    AUDIO_OUTPUT_DONE,
    AUDIO_OUTPUT_PLAY,
    AUDIO_OUTPUT_PAUSED,
    AUDIO_OUTPUT_STOP,
};

typedef u32 audio_output_hchannel;

//==============================================================================
//  CLASSES
//==============================================================================

class audio_output_mixer
{
public:
                    audio_output_mixer      ( void );
                   ~audio_output_mixer      ( void );

    void            Init                    ( void );
    void            ApplyRenderCommand      ( const audio_render_command& Command );
    s32             MixFrame                ( s16* pOutput,
                                              s32  nSamples );
    s32             GetOutputLevel          ( void );

    audio_output_hchannel AllocateChannel   ( void );
    u32             ChannelStartSerial      ( audio_output_hchannel hChannel );
    audio_output_state ChannelStatus        ( audio_output_hchannel hChannel );
    s32             ChannelPosition         ( audio_output_hchannel hChannel );

private:

    enum
    {
        MAX_CHANNELS = 128
    };

    struct output_channel
    {
        xbool       Allocated;
        s32         Sequence;
        u32         StartSerial;

        s16*        pData;
        s32         SampleRate;
        s32         nSamples;

        s32         Cursor;
        s32         Fraction;
        bool        Looped;
        s32         LoopStart;
        s32         LoopEnd;
        s32         LoopBase;
        xbool       StopLoop;
        s32         StopSample;

        audio_output_state   State;

        f32         VolumeL;
        f32         VolumeR;
        f32         Pitch;

        s32         MixedVolL;
        s32         MixedVolR;
    };

    struct output_channel_snapshot
    {
        x_atomic_u32 hChannel;
        x_atomic_u32 StartSerial;
        x_atomic_s32 Position;
        x_atomic_s32 State;
    };

    void            MixChannel              ( output_channel* pChannel,
                                              s32*            pL,
                                              s32*            pR,
                                              s32             nDstSamples );
    s32             HandleToIndex           ( audio_output_hchannel hChannel );
    s32             HandleToSnapshotIndex   ( audio_output_hchannel hChannel ) const;
    void            PublishChannelSnapshot  ( s32 Index );

    output_channel          m_OutputChannels[MAX_CHANNELS];
    output_channel_snapshot m_ChannelSnapshots[MAX_CHANNELS];
    s32                     m_MixL[AUDIO_OUTPUT_SAMPLES_PER_FRAME];
    s32                     m_MixR[AUDIO_OUTPUT_SAMPLES_PER_FRAME];
    x_atomic_s32            m_OutputLevel;
};

//==============================================================================
#endif // AUDIO_OUTPUT_MIXER_HPP
//==============================================================================
