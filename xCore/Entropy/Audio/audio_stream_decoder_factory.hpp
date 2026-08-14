//==============================================================================
//
//  audio_stream_decoder_factory.hpp
//
//==============================================================================

#ifndef AUDIO_STREAM_DECODER_FACTORY_HPP
#define AUDIO_STREAM_DECODER_FACTORY_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Audio/audio_package_format.hpp"
#include "Audio/audio_stream_decoder.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct audio_stream;
struct audio_runtime;
class  io_fs;

//==============================================================================
//  AUDIO STREAM DECODER FACTORY CLASS
//==============================================================================

class audio_stream_decoder_factory
{
public:
                        audio_stream_decoder_factory( io_fs& FileSystem );
                       ~audio_stream_decoder_factory( void );

        void            Init                    ( audio_runtime& Runtime );
        void            Kill                    ( void );
        xbool           Open                    ( audio_stream* pStream );
        void            Close                   ( audio_stream_decoder* pDecoder );
        xbool           IsOpen                  ( const audio_stream* pStream ) const;
        xbool           UsesRuntimeDecode       ( compression_types CompressionType ) const;
        xbool           UsesRuntimeDecode       ( const audio_stream* pStream ) const;

private:
        io_fs&          m_FileSystem;
};

//==============================================================================
#endif // AUDIO_STREAM_DECODER_FACTORY_HPP
//==============================================================================
