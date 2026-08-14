//==============================================================================
//
//  audio_runtime.hpp
//
//==============================================================================

#ifndef AUDIO_RUNTIME_HPP
#define AUDIO_RUNTIME_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_command_queue.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_channel_mgr.hpp"
#include "Audio/audio_voice_mgr.hpp"
#include "Audio/audio_stream_mgr.hpp"
#include "Audio/audio_stream_decoder_factory.hpp"
#include "Audio/audio_stream_runtime.hpp"
#include "Audio/audio_package_registry.hpp"
#include "Audio/audio_descriptor_runtime.hpp"
#include "Audio/audio_spatial_mgr.hpp"
#include "x_workers.hpp"

//==============================================================================
//  TYPES
//==============================================================================

class audio_mgr;

//==============================================================================
//  AUDIO RUNTIME
//==============================================================================

struct audio_runtime
{
                    audio_runtime               ( audio_mgr& Audio );
                   ~audio_runtime               ( void );

    xbool           SubmitCommand               ( audio_command& Command,
                                                  audio_command_result* pResult,
                                                  xbool WaitForCompletion );
    xbool           SubmitCommandAsync          ( audio_command& Command );
    xbool           SubmitCommandSync           ( audio_command& Command,
                                                  audio_command_result* pResult = NULL );
    xbool           SubmitSimpleCommandAsync    ( audio_command_type Type );
    xbool           SubmitSimpleCommandSync     ( audio_command_type Type,
                                                  audio_command_result& Result );
    xbool           SubmitFloatCommandAsync     ( audio_command_type Type,
                                                  f32 Value );
    xbool           SubmitS32CommandAsync       ( audio_command_type Type,
                                                  s32 Value );
    xbool           SubmitS32CommandSync        ( audio_command_type Type,
                                                  s32 Value,
                                                  audio_command_result& Result );
    xbool           SubmitVoiceCommandAsync     ( audio_command_type Type,
                                                  voice_id VoiceID );
    xbool           SubmitVoiceCommandSync      ( audio_command_type Type,
                                                  voice_id VoiceID,
                                                  audio_command_result& Result );
    xbool           SubmitVoiceFloatCommandAsync( audio_command_type Type,
                                                  voice_id VoiceID,
                                                  f32 Value );
    xbool           SubmitVoiceBoolCommandAsync ( audio_command_type Type,
                                                  voice_id VoiceID,
                                                  xbool Value );
    xbool           SubmitTextCommandSync       ( audio_command_type Type,
                                                  const char* pText,
                                                  audio_command_result& Result );
    voice_id        SubmitPlayCommandSync       ( const char* pIdentifier,
                                                  xbool AutoStart,
                                                  const vector3& Position,
                                                  s32 ZoneID,
                                                  xbool IsPositional,
                                                  xbool bVolumeClip,
                                                  xbool UseReservedStream );
    xbool           SubmitSegueCommandSync      ( voice_id VoiceID,
                                                  voice_id VoiceToQ,
                                                  audio_command_result& Result );
    xbool           SubmitGetPositionCommandSync( voice_id VoiceID,
                                                  vector3& Position,
                                                  s32& ZoneID );
    xbool           SubmitSetPositionCommandAsync( voice_id VoiceID,
                                                   const vector3& Position,
                                                   s32 ZoneID );
    xbool           SubmitVoiceFalloffCommandAsync( voice_id VoiceID,
                                                    f32 Near,
                                                    f32 Far );
    xbool           SubmitVoiceEarCommandAsync  ( voice_id VoiceID,
                                                  ear_id EarID );
    xbool           SubmitClipCommandAsync      ( f32 NearClip,
                                                  f32 FarClip );
    xbool           SubmitEarCommandAsync       ( audio_command_type Type,
                                                  ear_id EarID );
    xbool           SubmitSetEarCommandAsync    ( ear_id EarID,
                                                  const matrix4& W2V,
                                                  const vector3& Position,
                                                  s32 ZoneID,
                                                  f32 Volume );
    xbool           SubmitEarZoneVolumeCommandAsync( ear_id EarID,
                                                     s32 ZoneID,
                                                     f32 Volume );
    xbool           SubmitEarZoneVolumesCommandSync( ear_id EarID,
                                                    f32* pVolumes );
    xbool           SubmitIoRequestCommandAsync ( audio_command_type Type,
                                                  io_request* pRequest );
    void            CompleteCommand             ( audio_command& Command,
                                                  const audio_command_result& Result );
    void            ExecuteCommand              ( audio_command& Command );
    xbool           ExecuteServiceCommand       ( audio_command& Command,
                                                  audio_command_result& Result );
    xbool           ExecuteStreamCommand        ( audio_command& Command,
                                                  audio_command_result& Result );
    xbool           ExecutePackageCommand       ( audio_command& Command,
                                                  audio_command_result& Result );
    xbool           ExecuteVoiceCommand         ( audio_command& Command,
                                                  audio_command_result& Result );
    xbool           ExecuteSpatialCommand       ( audio_command& Command,
                                                  audio_command_result& Result );

    audio_mgr&                  Audio;
    audio_backend              Backend;
    audio_channel_mgr           Channels;
    audio_voice_mgr             Voices;
    audio_stream_mgr            Streams;
    audio_stream_decoder_factory Decoders;
    audio_stream_runtime        StreamRuntime;
    audio_package_registry      Packages;
    audio_descriptor_runtime    Descriptors;
    audio_spatial_mgr           Spatial;
    audio_command_queue         Commands;
    x_worker_service            Service;
    volatile xbool              ServiceRunning;
    volatile s32                ServiceThreadId;
    s32                         AudioDuckLevel;
    f32                         Time;
};

//==============================================================================
#endif // AUDIO_RUNTIME_HPP
//==============================================================================
