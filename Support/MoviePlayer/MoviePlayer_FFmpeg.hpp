//==============================================================================
//
//  MoviePlayer_FFmpeg.hpp
//
//  FFmpeg video codec implementation for PC (DirectX11) platform
//
//==============================================================================

#ifndef MOVIEPLAYER_FFMPEG_HPP
#define MOVIEPLAYER_FFMPEG_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be included for PC platform. Please check your exclusions on your project spec.
#endif

#include "x_files.hpp"
#include "x_threads.hpp"
#include "Entropy.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/mem.h>
    #include <libavutil/opt.h>
}

//TODO: GS: Add static link support.
//#pragma comment(lib, "avformat.lib")
//#pragma comment(lib, "avcodec.lib")
//#pragma comment(lib, "swscale.lib")
//#pragma comment(lib, "swresample.lib")
//#pragma comment(lib, "avutil.lib")

#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")

//==============================================================================
// FORWARD DECLARATIONS
//==============================================================================

class movie_private;

//==============================================================================
// COMMAND SYSTEM
//==============================================================================

enum movie_command_type
{
    MOVIE_CMD_NONE = 0,
    MOVIE_CMD_OPEN,
    MOVIE_CMD_CLOSE,
    MOVIE_CMD_PAUSE,
    MOVIE_CMD_RESUME,
    MOVIE_CMD_SET_VOLUME,
    MOVIE_CMD_SHUTDOWN
};

struct movie_command
{
    movie_command_type  Type;
    union
    {
        struct { 
            char        Filename[256];
            xbool       PlayResident;
            xbool       IsLooped;
        } Open;
        struct {
            f32         Volume;
        } SetVolume;
    } Data;
};

//==============================================================================
// MEDIA CLOCK
//==============================================================================

class media_clock
{
public:
                    media_clock     (void);
    void            Start           (void);
    void            Stop            (void);
    void            Pause           (void);
    void            Resume          (void);
    f64             GetTime         (void);
    xbool           IsRunning       (void) const { return m_bRunning; }
    xbool           IsPaused        (void) const { return m_bPaused; }

private:
    xtick           m_StartTime;
    xtick           m_PauseTime;
    f64             m_PausedDuration;   // In seconds
    xbool           m_bRunning;
    xbool           m_bPaused;
};

//==============================================================================
// RENDER DATA
//==============================================================================

struct movie_render_data
{
    u8*             pFrameData;
    s32             Width;
    s32             Height;
    s32             Pitch;
    xbool           bHasNewFrame;
    xbool           bIsValid;
    xmutex          DataMutex;
    
    movie_render_data(void) : pFrameData(NULL), Width(0), Height(0), Pitch(0), 
                             bHasNewFrame(FALSE), bIsValid(FALSE) {}
};

//==============================================================================
// XAUDIO2 VOICE CALLBACK
//==============================================================================

class XAudioVoiceCallback : public IXAudio2VoiceCallback
{
private:
    movie_private*  m_pMovie;
    
public:
                    XAudioVoiceCallback (movie_private* pMovie) : m_pMovie(pMovie) {}
                   ~XAudioVoiceCallback (void) {}

    void __stdcall  OnVoiceProcessingPassStart (UINT32 SamplesRequired) {}
    void __stdcall  OnVoiceProcessingPassEnd   (void) {}
    void __stdcall  OnStreamEnd                (void) {}
    void __stdcall  OnBufferStart              (void* pBufferContext) {}
    void __stdcall  OnBufferEnd                (void* pBufferContext);
    void __stdcall  OnLoopEnd                  (void* pBufferContext) {}
    void __stdcall  OnVoiceError               (void* pBufferContext, HRESULT Error) {}
};

//==============================================================================
// MOVIE PRIVATE CLASS
//==============================================================================

class movie_private
{
public:
                            movie_private       (void);
                           ~movie_private       (void);							
    void                    Init                (void);
    xbool                   Open                (const char* pFilename, xbool PlayResident, xbool IsLooped);
    void                    Close               (void);
    void                    Kill                (void);
    
    // Status queries
    s32                     GetWidth            (void)      { return m_Width; }
    s32                     GetHeight           (void)      { return m_Height; }
    xbool                   IsRunning           (void)      { return m_bThreadIsRunning; }
    xbool                   IsFinished          (void)      { return m_bThreadIsFinished; }
    s32                     GetCurrentFrame     (void)      { return m_ThreadCurrentFrame; }
    s32                     GetFrameCount       (void)      { return m_FrameCount; }
    
    // Playback control
    void                    SetVolume           (f32 Volume);
    void                    Pause               (void);
    void                    Resume              (void);
    
    // Rendering control
    void                    SetForceStretch     (xbool bForceStretch)    { m_bForceStretch = bForceStretch; }
    xbool                   IsForceStretch      (void)                   { return m_bForceStretch; }
    
    // Decoding
    xbitmap*                Decode              (void);

private:
    
    // Core state
    s32                     m_Width;
    s32                     m_Height;
    s32                     m_FrameCount;
    f32                     m_Volume;
    s32                     m_Language;

    // Rendering state
    xbool                   m_bForceStretch;

    // DirectX11 resources
    ID3D11Texture2D*        m_pTexture;
    s32                     m_TextureVRAMID;

    // Threading
    xthread*                m_pMovieThread;
    xmesgq*                 m_pCommandQueue;
    xmesgq*                 m_pResponseQueue;
    movie_render_data       m_RenderData;
    
    volatile xbool          m_bMovieThreadRunning;
    volatile xbool          m_bMovieThreadExit;
    volatile xbool          m_bThreadIsRunning;
    volatile xbool          m_bThreadIsFinished;
    volatile xbool          m_bThreadIsPaused;
    volatile s32            m_ThreadCurrentFrame;

    // Media Clock
    media_clock             m_MediaClock;
    f64                     m_VideoFPS;
    s32                     m_VideoFrameCount;

    // FFmpeg context
    AVFormatContext*        m_pFormatContext;
    AVCodecContext*         m_pCodecContext;
    AVCodecContext*         m_pAudioCodecContext;
    const AVCodec*          m_pCodec;
    const AVCodec*          m_pAudioCodec;
    AVFrame*                m_pFrame;
    AVFrame*                m_pFrameRGB;
    AVFrame*                m_pAudioFrame;
    SwsContext*             m_pSwsContext;
    SwrContext*             m_pSwrContext;
    s32                     m_VideoStreamIndex;
    s32                     m_AudioStreamIndex;
    u8*                     m_pBuffer;
    s32                     m_BufferSize;
    xbool                   m_IsLooped;

    // Audio system
    IXAudio2*               m_pXAudio2;
    IXAudio2MasteringVoice* m_pMasterVoice;
    IXAudio2SourceVoice*    m_pSourceVoice;
    XAudioVoiceCallback*    m_pVoiceCallback;
    xbool                   m_AudioInitialized;
    u8*                     m_pAudioBuffer;
    xbool                   m_bVoiceStarted;

    // Thread methods
    static void             MovieThreadEntry       (s32 argc, char** argv);
    void                    MovieThreadMain         (void);
    void                    ProcessCommand          (movie_command* pCmd);
    void                    UpdateRenderData        (void);
    
    // Media processing methods
    void                    ProcessVideoDecoding    (void);
    
    // Command helpers
    xbool                   SendCommand             (movie_command_type type, void* pData = NULL);
    xbool                   WaitForResponse         (s32 timeoutMs = 1000);
    
    // Movie thread operations
    xbool                   OpenInternal            (const char* pFilename, xbool PlayResident, xbool IsLooped);
    void                    CloseInternal           (void);
    void                    SetVolumeInternal       (f32 Volume);
    xbool                   DecodeNextVideoFrame    (void);
    
    // DirectX11 methods (main thread only)
    xbool                   CreateVideoTexture      (void);
    void                    DestroyVideoTexture     (void);
    xbool                   UpdateVideoTextureFromRenderData (void);
    void                    RenderVideoFrame        (void);
    
    // FFmpeg methods (movie thread only)
    xbool                   InitializeFFmpeg        (void);
    void                    CleanupFFmpeg           (void);
    xbool                   SetupAudioStream        (void);
    void                    WriteAudioData          (u8* pData, s32 size);
    
    // Audio methods
    xbool                   InitializeAudio         (void);
    void                    CleanupAudio            (void);
    void                    UpdateAudioVolume       (void);
};

//==============================================================================
// INLINE IMPLEMENTATION FOR XAUDIO2 CALLBACK
//==============================================================================

inline void __stdcall XAudioVoiceCallback::OnBufferEnd(void* pBufferContext)
{
    if (pBufferContext)
    {
        x_free(pBufferContext);
    }
}

#endif // MOVIEPLAYER_FFMPEG_HPP