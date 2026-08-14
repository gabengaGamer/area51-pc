//==============================================================================
//
//  audio_render_command.hpp
//
//==============================================================================

#ifndef AUDIO_RENDER_COMMAND_HPP
#define AUDIO_RENDER_COMMAND_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum audio_render_command_type
{
    AUDIO_RENDER_COMMAND_NONE,
    AUDIO_RENDER_COMMAND_START_CHANNEL,
    AUDIO_RENDER_COMMAND_STOP_CHANNEL,
    AUDIO_RENDER_COMMAND_PAUSE_CHANNEL,
    AUDIO_RENDER_COMMAND_RESUME_CHANNEL,
    AUDIO_RENDER_COMMAND_END_CHANNEL,
    AUDIO_RENDER_COMMAND_SET_VOLUME,
    AUDIO_RENDER_COMMAND_SET_PITCH,
    AUDIO_RENDER_COMMAND_STOP_LOOP,
};

//------------------------------------------------------------------------------

struct audio_render_start_channel
{
    u32     hChannel;
    u32     StartSerial;
    void*   pData;
    s32     nSamples;
    s32     LoopCount;
    s32     LoopStart;
    s32     LoopEnd;
    s32     SampleRate;
    f32     VolumeL;
    f32     VolumeR;
    f32     Pitch;
};

//------------------------------------------------------------------------------

struct audio_render_channel_command
{
    u32     hChannel;
};

//------------------------------------------------------------------------------

struct audio_render_volume_command
{
    u32     hChannel;
    f32     VolumeL;
    f32     VolumeR;
};

//------------------------------------------------------------------------------

struct audio_render_pitch_command
{
    u32     hChannel;
    f32     Pitch;
};

//------------------------------------------------------------------------------

struct audio_render_stop_loop_command
{
    u32     hChannel;
    s32     nSamples;
};

//------------------------------------------------------------------------------

struct audio_render_command
{
    audio_render_command_type Type;
    union
    {
        audio_render_start_channel     Start;
        audio_render_channel_command   Channel;
        audio_render_volume_command    Volume;
        audio_render_pitch_command     Pitch;
        audio_render_stop_loop_command StopLoop;
    };
};

//==============================================================================
#endif // AUDIO_RENDER_COMMAND_HPP
//==============================================================================
