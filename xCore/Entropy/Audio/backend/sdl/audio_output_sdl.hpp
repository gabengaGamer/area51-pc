//==============================================================================
//
//  audio_output_sdl.hpp
//
//  SDL3 audio output backend
//
//==============================================================================

#ifndef AUDIO_OUTPUT_SDL_HPP
#define AUDIO_OUTPUT_SDL_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"

//==============================================================================
//  TYPES
//==============================================================================

struct SDL_AudioStream;

typedef s32 sdl_audio_render_fn( void* pContext, s16* pOutput, s32 nSamples );

//==============================================================================
//  CLASSES
//==============================================================================

class sdl_audio_device
{
public:
                    sdl_audio_device           ( void );
                   ~sdl_audio_device           ( void );

        xbool       Init                       ( s32                  SampleRate,
                                                 s32                  FrameSamples,
                                                 sdl_audio_render_fn* pRender,
                                                 void*                pRenderContext );
        void        Kill                       ( void );
        xbool       Lock                       ( void );
        void        Unlock                     ( void );

private:

static  void        AudioOutputStreamCallback  ( void*            pUserData,
                                                 SDL_AudioStream* pStream,
                                                 int              AdditionalAmount,
                                                 int              TotalAmount );

        s32         MixFrame                   ( s32 nSamples );

        xbool                m_Initialized;
        xbool                m_SdlInitialized;
        SDL_AudioStream*     m_pAudioStream;
        sdl_audio_render_fn* m_pRender;
        void*                m_pRenderContext;
        s16*                 m_pOutput;
        s32                  m_FrameSamples;
};

//==============================================================================
#endif // AUDIO_OUTPUT_SDL_HPP
//==============================================================================
