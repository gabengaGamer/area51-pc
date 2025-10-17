//==============================================================================
//
//  audio_mp3_mgr.hpp
//
//  dr_lib based mp3 decoder.
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

#ifndef AUDIO_MP3_MGR_HPP
#define AUDIO_MP3_MGR_HPP

//==============================================================================
//  STRUCTURES
//==============================================================================

struct audio_stream;

class audio_mp3_mgr
{

public:

                    audio_mp3_mgr       ( void );
                   ~audio_mp3_mgr       ( void );
void                Init                ( void );
void                Kill                ( void );
void                Open                ( audio_stream* pStream );
void                Close               ( audio_stream* pStream );
void                Decode              ( audio_stream* pStream,
                                          s16*          pBufferL,
                                          s16*          pBufferR,
                                          s32           nSamples );
void                NotifyStreamData    ( audio_stream* pStream,
                                          s32           nBytes );
void                ResetStreamBuffer   ( audio_stream* pStream );
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern audio_mp3_mgr g_AudioMP3Mgr;

//==============================================================================
#endif // AUDIO_MP3_MGR_HPP
//==============================================================================