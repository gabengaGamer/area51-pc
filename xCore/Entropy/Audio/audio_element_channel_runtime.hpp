//==============================================================================
//
//  audio_element_channel_runtime.hpp
//
//==============================================================================

#ifndef AUDIO_ELEMENT_CHANNEL_RUNTIME_HPP
#define AUDIO_ELEMENT_CHANNEL_RUNTIME_HPP

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
//  AUDIO ELEMENT CHANNEL RUNTIME
//==============================================================================

class audio_element_channel_runtime
{
public:

                            audio_element_channel_runtime ( void );
                           ~audio_element_channel_runtime ( void );

            void            Init                          ( audio_runtime&    Runtime );
            void            Kill                          ( void );

            xbool           ReleaseChannel                ( audio_voice_mgr&  Voices,
                                                            element*          pElement );
            void            StartElement                  ( audio_voice_mgr&  Voices,
                                                            element*          pElement );
            void            PauseElement                  ( audio_voice_mgr&  Voices,
                                                            element*          pElement );
            void            ResumeElement                 ( audio_voice_mgr&  Voices,
                                                            element*          pElement );
            void            ApplyElementVolume            ( audio_voice_mgr&  Voices,
                                                            element*          pElement );
            void            ApplyElementPan               ( audio_voice_mgr&  Voices,
                                                            element*          pElement );
            void            ApplyElementPitch             ( audio_voice_mgr&  Voices,
                                                            element*          pElement );
            void            ApplyElementEffectSend        ( audio_voice_mgr&  Voices,
                                                            element*          pElement );

private:

inline      audio_runtime&  Runtime                       ( void ) { ASSERT( m_pRuntime ); return *m_pRuntime; }

private:

            audio_runtime*  m_pRuntime;
};

//==============================================================================
#endif // AUDIO_ELEMENT_CHANNEL_RUNTIME_HPP
//==============================================================================
