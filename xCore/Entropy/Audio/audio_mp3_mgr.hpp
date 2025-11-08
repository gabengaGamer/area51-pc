//==============================================================================
//
//  audio_mp3_mgr.hpp
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

#ifndef AUDIO_MP3_MGR_HPP
#define AUDIO_MP3_MGR_HPP

//==============================================================================
//  STRUCTURES
//==============================================================================

struct audio_stream;
enum  stream_type;

class audio_mp3_mgr
{

public:
                    audio_mp3_mgr               ( void );
                   ~audio_mp3_mgr               ( void );

        void        Init                        ( void );
        void        Kill                        ( void );

        void        Open                        ( audio_stream* pStream );
        void        Close                       ( audio_stream* pStream );
        void        Decode                      ( audio_stream* pStream,
                                                  s16*          pBufferL,
                                                  s16*          pBufferR,
                                                  s32           nSamples );

private:
        struct      mp3_decoder_state;
        static s32  mp3_fetch_data              ( audio_stream*       pStream,
                                                  void*               pBuffer,
                                                  s32                 nBytes,
                                                  s32                 Offset );
        static void mp3_state_reset             ( mp3_decoder_state&  State,
                                                  stream_type         Type );
        static void mp3_state_compact           ( mp3_decoder_state&  State );
        static void mp3_state_refill            ( audio_stream*       pStream,
                                                  mp3_decoder_state&  State );
        static s32  mp3_state_available_bytes   ( const mp3_decoder_state& State );
        static s32  mp3_state_decode_frame      ( audio_stream*       pStream,
                                                  mp3_decoder_state&  State );
        static xbool IsValidStream              ( const audio_stream* pStream );
        static xbool s_Initialized;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern audio_mp3_mgr g_AudioMP3Mgr;

//==============================================================================
#endif // AUDIO_MP3_MGR_HPP
//==============================================================================