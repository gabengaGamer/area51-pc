//==============================================================================
//
//  audio_stream_voice_binder.hpp
//
//==============================================================================

#ifndef AUDIO_STREAM_VOICE_BINDER_HPP
#define AUDIO_STREAM_VOICE_BINDER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"

//==============================================================================
//  TYPES
//==============================================================================

class audio_voice_mgr;
struct audio_runtime;

//==============================================================================
//  AUDIO STREAM VOICE BINDER CLASS
//==============================================================================

class audio_stream_voice_binder
{
public:

                            audio_stream_voice_binder  ( void );
                           ~audio_stream_voice_binder  ( void );

            void            Init                       ( audio_runtime&    Runtime );
            void            Kill                       ( void );

            voice*          UpdateCheckStreams         ( audio_voice_mgr&  Voices,
                                                         voice*            pVoice );

private:

inline      audio_runtime&  Runtime                    ( void ) { ASSERT( m_pRuntime ); return *m_pRuntime; }

            void            InstantiateStreamSample    ( audio_stream*     pStream,
                                                         s32               WhichChannel );

private:

            audio_runtime*  m_pRuntime;
};

//==============================================================================
#endif // AUDIO_STREAM_VOICE_BINDER_HPP
//==============================================================================
