//==============================================================================
//
//  audio_stream_runtime.hpp
//
//==============================================================================

#ifndef AUDIO_STREAM_RUNTIME_HPP
#define AUDIO_STREAM_RUNTIME_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"
#include "IOManager/io_request.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct audio_runtime;
struct audio_stream;
class  audio_mgr;
class  io_fs;
class  io_mgr;

//==============================================================================
//  AUDIO STREAM RUNTIME CLASS
//==============================================================================

class audio_stream_runtime
{
friend class audio_mgr;
friend struct audio_runtime;

public:
                            audio_stream_runtime   ( io_fs& FileSystem,
                                                     io_mgr& Io );
                           ~audio_stream_runtime   ( void );

            void            Init                   ( audio_runtime& Runtime );
            void            Kill                   ( void );
            void            RegisterStream         ( s32           Slot,
                                                     audio_stream* pStream );
            xbool           OpenFile               ( audio_stream* pStream );
            void            CloseFile              ( audio_stream* pStream );
            xbool           Warm                   ( audio_stream* pStream,
                                                     io_request::callback_fn* pCallback = NULL );
            xbool           Read                   ( audio_stream* pStream,
                                                     io_request::callback_fn* pCallback = NULL );
            void            UpdateDecoded          ( audio_stream* pStream );

private:
            audio_runtime&  Runtime                ( void );
            s32             FindSlot               ( audio_stream* pStream ) const;
            audio_stream*   GetSlotStream          ( s32 Slot );
            xbool           IsValidStream          ( audio_stream* pStream ) const;
            channel*        ControlChannel         ( audio_stream* pStream );
            xbool           NeedsDecodeRefill      ( audio_stream* pStream );
            xbool           WriteDecodedChunk      ( audio_stream* pStream );
            audio_stream*   FindRequestStream      ( io_request* pRequest ) const;
            s32             GetRequestReadBufferIndex
                                                    ( io_request* pRequest ) const;
            void            ReadCallback           ( io_request*   pRequest,
                                                     audio_stream* pStream,
                                                     s32           ReadBufferIndex );
            void            ReadComplete           ( io_request* pRequest );
            void            WarmComplete           ( io_request* pRequest );
            xbool           GetOpenFilename        ( audio_stream* pStream,
                                                     char*         pFilename,
                                                     s32           FilenameBytes );
            void            DetachFile             ( audio_stream* pStream,
                                                     io_open_file*& pFile,
                                                     audio_stream_decoder*& pDecoder );
            xbool           WarmStream             ( audio_stream* pStream,
                                                     io_request::callback_fn* pCallback );
            xbool           QueueRead              ( audio_stream* pStream,
                                                     io_request::callback_fn* pCallback );
            void            DecodeAvailable        ( audio_stream* pStream );
            void            SetRequest             ( audio_stream* pStream,
                                                     io_request::callback_fn* pCallback );

static      audio_stream_runtime*
                            FromRequest            ( io_request* pRequest );
static      void            ReadRequestCallback    ( io_request* pRequest );
static      void            WarmRequestCallback    ( io_request* pRequest );

            audio_runtime*  m_pRuntime;
            io_fs&          m_FileSystem;
            io_mgr&         m_Io;
            audio_stream*   m_Streams[MAX_AUDIO_STREAMS];
            uaddr           m_ReadBuffers[2];
            u32             m_ActiveReadBuffer;
            s16             m_DecodeLeftBuffer [ MAX_AUDIO_STREAMS ][ 512 ];
            s16             m_DecodeRightBuffer[ MAX_AUDIO_STREAMS ][ 512 ];
            s32             m_DecodeWhichBuffer;
};

//==============================================================================
#endif // AUDIO_STREAM_RUNTIME_HPP
//==============================================================================
