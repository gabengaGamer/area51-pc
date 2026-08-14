//==============================================================================
//
//  audio_mp3_stream_decoder_state.hpp
//
//  minimp3 decoder state.
//
//==============================================================================

//==============================================================================

#ifndef AUDIO_MP3_STREAM_DECODER_STATE_HPP
#define AUDIO_MP3_STREAM_DECODER_STATE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Audio/audio_package_format.hpp"
#include "Minimp3/minimp3.h"

//==============================================================================
//  STRUCTS
//==============================================================================

struct audio_stream;

//==============================================================================
//  AUDIO MP3 STREAM DECODER STATE CLASS
//==============================================================================

class audio_mp3_stream_decoder_state
{
public:
    enum
    {
        INPUT_BUFFER_SIZE = MP3_BUFFER_SIZE,
        DECODE_SAMPLES    = 512,
    };

                    audio_mp3_stream_decoder_state( void );

        void        Reset                       ( const audio_stream* pStream );
        void        Compact                     ( void );
        s32         AvailableBytes              ( void ) const;
        void        Finish                      ( void );

public:
        mp3dec_t    Decoder;
        s32         FileCursor;
        s32         InputBytes;
        s32         InputCursor;
        s32         SamplesAvailable;
        s32         SampleOffset;
        s32         Channels;
        xbool       EndOfInput;
        xbool       DecodeComplete;
        s16         SamplesBuffer[MINIMP3_MAX_SAMPLES_PER_FRAME];
        u8          InputBuffer[INPUT_BUFFER_SIZE];
};

//==============================================================================
#endif // AUDIO_MP3_STREAM_DECODER_STATE_HPP
//==============================================================================
