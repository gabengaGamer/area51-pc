//==============================================================================
//
//  MoviePlayer_WebM_Private.hpp
//
//  Internal declarations for the WebM movie player backend.
//
//==============================================================================

#ifndef MOVIEPLAYER_WEBM_PRIVATE_HPP
#define MOVIEPLAYER_WEBM_PRIVATE_HPP

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

// Let it be only for PC, for now...

#include "x_target.hpp"

#if !defined( TARGET_DESKTOP )
#error This file should only be compiled for a desktop platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include <climits>
#include "x_files.hpp"
#include "Entropy.hpp"

#include "mkvparser/mkvparser.h"
#include "mkvparser/mkvreader.h"
#include "webm/reader.h"
#include "vpx/vpx_image.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
#include "opus.h"
#include "opus_multistream.h"
#include "vorbis/codec.h"
#include "ogg/ogg.h"

struct SDL_AudioStream;
struct OpusDecoder;
struct OpusMSDecoder;
struct vorbis_info;
struct vorbis_comment;
struct vorbis_dsp_state;
struct vorbis_block;

namespace movie_webm
{
//==============================================================================
// CONSTANTS
//==============================================================================

static const f64 WEBM_NANOSEC = 1000000000.0;

//==============================================================================
// ENUMS AND STRUCTURES
//==============================================================================

enum stream_Type
{
    STREAm_Type_UNKNOWN = 0,
    STREAm_Type_VIDEO,
    STREAm_Type_AUDIO
};

//------------------------------------------------------------------------------

struct player_config
{
    player_config(void)
    {
        VideoTrack      = -1;
        AudioTrack      = -1;
        Width           = 0;
        Height          = 0;
        FrameRate       = 0.0f;
        Duration        = 0.0;
        TimecodeScale   = WEBM_NANOSEC;
        HasAudio        = FALSE;
        IsLooped        = FALSE;
        AudioChannels   = 0;
        AudioSampleRate = 0;
        AudioBitDepth   = 16;
        AudioLanguage   = XL_LANG_ENGLISH;
        AudioCodecId.Clear();
        VideoCodecId.Clear();
        AudioCodecPrivate.Clear();
    }

    s32         VideoTrack;
    s32         AudioTrack;
    s32         Width;
    s32         Height;
    f32         FrameRate;
    f64         Duration;
    f64         TimecodeScale;
    xbool       HasAudio;
    xbool       IsLooped;
    xstring     VideoCodecId;
    xstring     AudioCodecId;
    s32         AudioChannels;
    s32         AudioSampleRate;
    s32         AudioBitDepth;
    x_language  AudioLanguage;
    xarray<u8>  AudioCodecPrivate;
};

//------------------------------------------------------------------------------

struct sample
{
    sample(void)
    {
        Type        = STREAm_Type_UNKNOWN;
        pEntry      = NULL;
        pBlock      = NULL;
        pCluster    = NULL;
        TimeSeconds = 0.0;
        IsKeyFrame  = FALSE;
    }

    stream_Type                 Type;
    const mkvparser::BlockEntry* pEntry;
    const mkvparser::Block*     pBlock;
    const mkvparser::Cluster*   pCluster;
    f64                         TimeSeconds;
    xbool                       IsKeyFrame;
};

//------------------------------------------------------------------------------

struct render_data
{
    render_data(void)
    {
        pFrameData  = NULL;
        Width       = 0;
        Height      = 0;
        Pitch       = 0;
        FrameTime   = 0.0;
        bHasNewFrame= FALSE;
        bIsValid    = FALSE;
    }

    u8*         pFrameData;
    s32         Width;
    s32         Height;
    s32         Pitch;
    f64         FrameTime;
    xbool       bHasNewFrame;
    xbool       bIsValid;
    xmutex      DataMutex;
};

//==============================================================================
// FILE READER
//==============================================================================

class file_reader : public mkvparser::IMkvReader
{
public:
                    file_reader     (void);
                   ~file_reader     (void);

    xbool           Open            (const char* pFilename);
    void            Close           (void);

    virtual int     Read            (long long position, long length, unsigned char* buffer);
    virtual int     Length          (long long* total, long long* available);

private:
    X_FILE*         m_pFile;
    long long       m_Length;
};

//==============================================================================
// CONTAINER
//==============================================================================

class container
{
public:
                    container       (void);
                   ~container       (void);

    xbool           Open            (const char* pFilename, player_config& Config);
    void            Close           (void);
    void            Rewind          (void);

    xbool           PeekSample      (sample& OutSample);
    xbool           ReadSample      (sample& OutSample);

    mkvparser::IMkvReader* GetReader(void) { return &m_FileReader; }

private:
    xbool           AcquireSample   (sample& OutSample);
    xbool           InitializeSegment   (player_config& Config);
    xbool           SelectTracks        (player_config& Config);
    xbool           AdvanceCluster      (void);
    xbool           AdvanceEntry        (void);

private:
    file_reader                 m_FileReader;
    mkvparser::Segment*         m_pSegment;
    const mkvparser::Tracks*    m_pTracks;
    const mkvparser::Cluster*   m_pCluster;
    const mkvparser::BlockEntry* m_pEntry;
    f64                         m_TimeScale;
    f64                         m_Duration;
    s32                         m_VideoTrack;
    s32                         m_AudioTrack;
    xbool                       m_bEOF;
    xbool                       m_bHasPendingSample;
    sample                      m_PendingSample;
};

//==============================================================================
// VIDEO DECODER
//==============================================================================

class video_decoder
{
public:
                    video_decoder   (void);
                   ~video_decoder   (void);

    xbool           Initialize      (const player_config& Config);
    void            Shutdown        (void);

    xbool           DecodeSample    (const sample& Sample, mkvparser::IMkvReader* pReader);

    const u8*       GetFrameData    (void) const { return (m_FrameBuffer.GetCount() > 0) ? m_FrameBuffer.GetPtr() : NULL; }
    s32             GetFramePitch   (void) const { return m_FramePitch; }
    s32             GetWidth        (void) const { return m_Width; }
    s32             GetHeight       (void) const { return m_Height; }

private:
    xbool           InitializeCodec (const player_config& Config);
    void            DestroyCodec    (void);
    xbool           ConvertFrame    (const vpx_image_t& Image);

private:
    vpx_codec_ctx_t             m_CodecCtx;
    const vpx_codec_iface_t*    m_pCodecIface;
    xbool                       m_bCodecInitialized;

    xarray<u8>                  m_CompressedBuffer;
    xarray<u8>                  m_FrameBuffer;
    s32                         m_FramePitch;
    s32                         m_Width;
    s32                         m_Height;
};

//==============================================================================
// AUDIO DECODER
//==============================================================================

class audio_decoder
{
public:
                    audio_decoder   (void);
                   ~audio_decoder   (void);

    xbool           Initialize      (const player_config& Config);
    void            Shutdown        (void);
    void            Flush           (void);
    xbool           DecodeSample    (const sample& Sample, mkvparser::IMkvReader* pReader);

    void            SetVolume       (f32 Volume);

private:
    enum codec_type
    {
        CODEC_NONE = 0,
        CODEC_OPUS,
        CODEC_VORBIS
    };

    xbool           InitializeSDL       (void);
    void            ShutdownSDL         (void);
    xbool           InitializeOpus      (const player_config& Config);
    void            DestroyOpus         (void);
    xbool           InitializeVorbis    (const player_config& Config);
    xbool           InitializeVorbisState(void);
    xbool           ParseVorbisPrivate  (const player_config& Config);
    void            DestroyVorbis       (void);
    void            DestroyVorbisState  (xbool ClearHeaders);
    void            ResetDecoders       (void);
    xbool           SubmitPCM           (const s16* pSamples, s32 SampleCount);
    xbool           DecodeOpusFrame     (const u8* pData, s32 DataSize);
    xbool           DecodeVorbisPacket  (const u8* pData, s32 DataSize);

private:
    f32                         m_Volume;
    codec_type                  m_CodecType;
    s32                         m_Channels;
    s32                         m_SampleRate;
    s32                         m_BitsPerSample;
    xbool                       m_isInitialized;
    xbool                       m_SdlInitialized;
    SDL_AudioStream*            m_pAudioStream;
    s32                         m_MaxQueuedBytes;

    xarray<u8>                  m_CompressedBuffer;
    xarray<s16>                 m_PCMBuffer;

    // Opus
    OpusDecoder*                m_pOpusDecoder;
    OpusMSDecoder*              m_pOpusMSDecoder;
    xarray<u8>                  m_OpusMapping;
    s32                         m_OpusStreams;
    s32                         m_OpusCoupled;
    s32                         m_OpusPreSkip;
    s32                         m_OpusPreSkipRemaining;
    s32                         m_MaxFrameSamples;
    s32                         m_OpusGain;

    // Vorbis
    vorbis_info*                m_pVorbisInfo;
    vorbis_comment*             m_pVorbisComment;
    vorbis_dsp_state*           m_pVorbisDsp;
    vorbis_block*               m_pVorbisBlock;
    xbool                       m_VorbisInitialized;
    long long                   m_VorbisPacketIndex;
    xarray<u8>                  m_VorbisHeaders[3];
};

//==============================================================================
// SYNC CLOCK
//==============================================================================

class sync_clock
{
public:
                    sync_clock      (void);

    void            Start           (void);
    void            Stop            (void);
    void            Pause           (void);
    void            Resume          (void);
    void            Reset           (void);

    f64             GetTime         (void) const;
    xbool           IsRunning       (void) const { return m_bRunning; }
    xbool           IsPaused        (void) const { return m_bPaused; }

private:
    xtick           m_StartTime;
    xtick           m_PauseTime;
    f64             m_PausedDuration;
    xbool           m_bRunning;
    xbool           m_bPaused;
};

} // namespace movie_webm

//==============================================================================
// MOVIE PRESENTATION
//==============================================================================

//==============================================================================
// MOVIE PRIVATE CLASS
//==============================================================================

class movie_private
{
public:
                    movie_private       (void);
                   ~movie_private       (void);

    void            Init                (void);
    xbool           Open                (const char* pFilename, xbool PlayResident, xbool IsLooped, x_language Language);
    void            Close               (void);
    void            Kill                (void);

    s32             GetWidth            (void)      { return m_Width; }
    s32             GetHeight           (void)      { return m_Height; }
    xbool           IsRunning           (void)      { return m_bThreadRunning; }
    xbool           IsFinished          (void)      { return m_bThreadFinished; }

    void            SetVolume           (f32 Volume);
    void            Pause               (void);
    void            Resume              (void);

    void            Render              (void);

private:
    void            Shutdown            (void);

    static void     WorkerEntry         (void* pData);
    void            ThreadMain          (void);
    void            ThreadLoop          (void);

    xbool           ProcessVideoSample  (const movie_webm::sample& Sample);
    void            UpdateRenderBuffer  (const movie_webm::sample& Sample);
    void            ResetPlayback       (void);
    xbool           PrimePlayback       (void);
    void            PreloadAudio        (f64 TargetTime);    
    void            PumpAudio           (f64 TargetTime);
    void            HandleEndOfStream   (void);
    void            SleepMilliseconds   (f64 Seconds);

    xbool           CreateVideoTexture  (void);
    void            DestroyVideoTexture (void);
    xbool           UpdateVideoTexture  (void);
    void            RenderVideoFrame    (void);

private:
    // Core state
    s32                         m_Width;
    s32                         m_Height;
    f32                         m_Volume;
    xbool                       m_IsLooped;

    // Threading
    x_worker_service            m_WorkerService;
    volatile xbool              m_bThreadExit;
    volatile xbool              m_bThreadRunning;
    volatile xbool              m_bThreadFinished;
    volatile xbool              m_bPaused;
    volatile xbool              m_bPlaybackActive;
    volatile xbool              m_bVideoEOF;
    volatile xbool              m_bThreadBusy;

    // Rendering
    vram_texture                m_VideoTexture;
    movie_webm::render_data     m_renderData;
    xarray<u8>                  m_RenderBuffer;

    // Playback helpers
    movie_webm::container       m_Container;
    movie_webm::video_decoder   m_VideoDecoder;
    movie_webm::audio_decoder   m_AudioDecoder;
    movie_webm::sync_clock      m_Clock;
    movie_webm::player_config   m_Config;
    f64                         m_LastVideoTime;
    movie_webm::sample          m_PendingVideoSample;
    xbool                       m_bHasPendingVideo;
};

#endif // MOVIEPLAYER_WEBM_PRIVATE_HPP
