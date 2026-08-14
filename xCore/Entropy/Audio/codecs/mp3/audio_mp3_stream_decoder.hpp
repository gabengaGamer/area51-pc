//==============================================================================
//
//  audio_mp3_stream_decoder.hpp
//
//  runtime minimp3 stream decoder.
//
//==============================================================================

#ifndef AUDIO_MP3_STREAM_DECODER_HPP
#define AUDIO_MP3_STREAM_DECODER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_stream_decoder.hpp"
#include "audio_mp3_stream_decoder_state.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct audio_stream;
enum  stream_type;
class io_fs;

//==============================================================================
//  AUDIO MP3 STREAM DECODER CLASS
//==============================================================================

class audio_mp3_stream_decoder : public audio_stream_decoder
{
public:
                            audio_mp3_stream_decoder( io_fs&         FileSystem,
                                                      audio_stream*  pStream );

    virtual audio_decode_result Decode                  ( audio_stream* pStream,
                                                          s16*          pBufferL,
                                                          s16*          pBufferR,
                                                          s32           nSamples );

    s32                     Read                    ( audio_stream* pStream,
                                                      void*         pBuffer,
                                                      s32           nBytes );
    s32                     Refill                  ( audio_stream* pStream );
    s32                     DecodeFrame             ( audio_stream* pStream );
    void                    CopySamples             ( stream_type   Type,
                                                      s16*&         pOutL,
                                                      s16*&         pOutR,
                                                      s32           nSamples );

private:
    io_fs&                          m_FileSystem;
    audio_mp3_stream_decoder_state  m_State;
};

//==============================================================================
#endif // AUDIO_MP3_STREAM_DECODER_HPP
//==============================================================================
