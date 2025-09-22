//==============================================================================
//
//  MoviePlayer_FFmpeg.cpp
//
//  FFmpeg video codec implementation for PC (DirectX11) platform
//
//==============================================================================

//-----------------------------------------------------------------------------
//
// GLOBAL TODO: Media clock, XAudio2 management and video decoding shoud be separated. 
// All this modules should be moved to entripy "video" sub-module.
//
//-----------------------------------------------------------------------------

// FIXED?: TODO: Fix thread crush due to non found video. Thread issue.
// FIXED?: TODO: Fix possible race condition.
// TODO: Make "Language" system. Just update StateMgr "SelectBestClip" system, lol.
// FIXED?: TODO: Re-Check possible mem leaks.
// TODO: Implement PlayResident from Bink ???

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

#include "x_files.hpp"
#include "x_threads.hpp"
#include "Entropy.hpp"
#include "movieplayer.hpp"

//==============================================================================
// GLOBAL INSTANCES
//==============================================================================

movie_player Movie;

//==============================================================================
// MEDIA CLOCK IMPLEMENTATION
//==============================================================================

media_clock::media_clock(void)
{
    m_StartTime = 0;
    m_PauseTime = 0;
    m_PausedDuration = 0.0;
    m_bRunning = FALSE;
    m_bPaused = FALSE;
}

//==============================================================================

void media_clock::Start(void)
{
    m_StartTime = x_GetTime();
    m_PausedDuration = 0.0;
    m_bRunning = TRUE;
    m_bPaused = FALSE;
}

//==============================================================================

void media_clock::Stop(void)
{
    m_bRunning = FALSE;
    m_bPaused = FALSE;
    m_PausedDuration = 0.0;
}

//==============================================================================

void media_clock::Pause(void)
{
    if (m_bRunning && !m_bPaused)
    {
        m_PauseTime = x_GetTime();
        m_bPaused = TRUE;
    }
}

//==============================================================================

void media_clock::Resume(void)
{
    if (m_bRunning && m_bPaused)
    {
        xtick currentTime = x_GetTime();
        m_PausedDuration += x_TicksToSec(currentTime - m_PauseTime);
        m_bPaused = FALSE;
    }
}

//==============================================================================

f64 media_clock::GetTime(void)
{
    if (!m_bRunning)
        return 0.0;
        
    if (m_bPaused)
    {
        return x_TicksToSec(m_PauseTime - m_StartTime) - m_PausedDuration;
    }
    else
    {
        xtick currentTime = x_GetTime();
        return x_TicksToSec(currentTime - m_StartTime) - m_PausedDuration;
    }
}

//==============================================================================
// CONSTRUCTOR
//==============================================================================

movie_private::movie_private(void)
{
    // Core state
    m_Width                 = 0;
    m_Height                = 0;
    m_FrameCount            = 0;
    m_Volume                = 1.0f;

    // Rendering state
    m_bForceStretch         = TRUE;

    // DirectX11 resources
    m_pTexture              = NULL;
    m_TextureVRAMID         = 0;

    // Threading
    m_pMovieThread          = NULL;
    m_pCommandQueue         = NULL;
    m_pResponseQueue        = NULL;
    m_bMovieThreadRunning   = FALSE;
    m_bMovieThreadExit      = FALSE;
    m_bThreadIsRunning      = FALSE;
    m_bThreadIsFinished     = FALSE;
    m_bThreadIsPaused       = FALSE;
    m_ThreadCurrentFrame    = 0;

    // Media Clock
    m_VideoFPS              = 25.0;
    m_VideoFrameCount       = 0;

    // FFmpeg context
    m_pFormatContext        = NULL;
    m_pCodecContext         = NULL;
    m_pAudioCodecContext    = NULL;
    m_pCodec                = NULL;
    m_pAudioCodec           = NULL;
    m_pFrame                = NULL;
    m_pFrameRGB             = NULL;
    m_pAudioFrame           = NULL;
    m_pSwsContext           = NULL;
    m_pSwrContext           = NULL;
    m_VideoStreamIndex      = -1;
    m_AudioStreamIndex      = -1;
    m_pBuffer               = NULL;
    m_BufferSize            = 0;
    m_IsLooped              = FALSE;

    // Audio system
    m_pXAudio2              = NULL;
    m_pMasterVoice          = NULL;
    m_pSourceVoice          = NULL;
    m_pVoiceCallback        = NULL;
    m_AudioInitialized      = FALSE;
    m_ComInitialized        = FALSE;
    m_pAudioBuffer          = NULL;
    m_bVoiceStarted         = FALSE;
}

//==============================================================================

movie_private::~movie_private(void)
{
    Kill();
}

//==============================================================================
// INITIALIZATION
//==============================================================================

void movie_private::Init(void)
{
    // Initialize message queues
    m_pCommandQueue = new xmesgq(8);
    m_pResponseQueue = new xmesgq(8);
    
    // Start movie thread with proper parameter passing
    m_bMovieThreadExit = FALSE;
    m_bMovieThreadRunning = FALSE;
    
    // Create argument array with this pointer
    char* pThisArg = reinterpret_cast<char*>(this);
    char* pArgs[2] = { pThisArg, NULL };
    
    m_pMovieThread = new xthread(MovieThreadEntry, "Movie Player", 128*1024, 2, 1, pArgs);
    
    // Wait for thread to start
    while (!m_bMovieThreadRunning)
    {
        x_DelayThread(1);
    }
}

//==============================================================================
// MOVIE THREAD ENTRY POINT
//==============================================================================

void movie_private::MovieThreadEntry(s32 argc, char** argv)
{
    movie_private* pThis = nullptr;
    
    if (argc > 0 && argv && argv[0])
    {
        pThis = reinterpret_cast<movie_private*>(argv[0]);
    }
    
    ASSERT(pThis);
    if (pThis)
    {
        pThis->MovieThreadMain();
    }
}

//==============================================================================
// MOVIE THREAD MAIN LOOP
//==============================================================================

void movie_private::MovieThreadMain(void)
{
    m_bMovieThreadRunning = TRUE;
    
    InitializeFFmpeg();
    InitializeAudio();
    
    while (!m_bMovieThreadExit)
    {
        if (!m_pCommandQueue)
            break;
        
        // Process commands from main thread
        movie_command* pCmd = (movie_command*)m_pCommandQueue->Recv(MQ_NOBLOCK);
        if (pCmd)
        {
            ProcessCommand(pCmd);
            delete pCmd;
        }
        
        // Process video decoding (audio handled by XAudio2 callback)
        if (m_bThreadIsRunning && !m_bThreadIsFinished && !m_bThreadIsPaused)
        {
            ProcessVideoDecoding();
        }
        
        x_DelayThread(1);
    }
    
    CleanupFFmpeg();
    CleanupAudio();
    m_bMovieThreadRunning = FALSE;
}

//==============================================================================
// VIDEO DECODING
//==============================================================================

void movie_private::ProcessVideoDecoding(void)
{
    if (!m_MediaClock.IsRunning())
        return;

    if (m_VideoFPS <= 0.0f)
        return;

    const s32 MAX_FRAMES_PER_PASS = 4;
    s32 framesProcessed = 0;

    f64 currentTime = m_MediaClock.GetTime();
    f64 nextVideoTime = (f64)m_VideoFrameCount / m_VideoFPS;

    // Decode video frames that should be displayed by current time
    while (currentTime >= nextVideoTime && m_bThreadIsRunning && !m_bThreadIsFinished)
    {
        if (DecodeNextVideoFrame())
        {
            m_VideoFrameCount++;
            UpdateRenderData();
            framesProcessed++;

            currentTime = m_MediaClock.GetTime();
            nextVideoTime = (f64)m_VideoFrameCount / m_VideoFPS;

            if (framesProcessed >= MAX_FRAMES_PER_PASS)
            {
                if (currentTime >= nextVideoTime)
                {
                    s32 targetFrame = (s32)(currentTime * m_VideoFPS);
                    if (m_FrameCount > 0 && targetFrame > m_FrameCount)
                    {
                        targetFrame = m_FrameCount;
                    }
                    if (targetFrame > m_VideoFrameCount)
                    {
                        s32 framesToSkip = targetFrame - m_VideoFrameCount;
                        s32 skipped = 0;
                        while (skipped < framesToSkip && m_bThreadIsRunning && !m_bThreadIsFinished)
                        {
                            if (!DecodeNextVideoFrame())
                            {
                                break;
                            }

                            m_VideoFrameCount++;
                            skipped++;
                        }

                        nextVideoTime = (f64)m_VideoFrameCount / m_VideoFPS;
                    }
                }

                break;
            }
        }
        else
        {
            break;
        }
    }
}

//==============================================================================
// COMMAND PROCESSING
//==============================================================================

void movie_private::ProcessCommand(movie_command* pCmd)
{
    if (!pCmd)
        return;
        
    switch (pCmd->Type)
    {
        case MOVIE_CMD_OPEN:
            {
                xbool result = OpenInternal(pCmd->Data.Open.Filename,
                                          pCmd->Data.Open.PlayResident,
                                          pCmd->Data.Open.IsLooped);
                if (m_pResponseQueue)
                {
                    m_pResponseQueue->Send(reinterpret_cast<void*>((uaddr)result + 1), MQ_NOBLOCK);
                }
            }
            break;

        case MOVIE_CMD_CLOSE:
            CloseInternal();
            if (m_pResponseQueue)
            {
                m_pResponseQueue->Send(reinterpret_cast<void*>((uaddr)TRUE + 1), MQ_NOBLOCK);
            }
            break;

        case MOVIE_CMD_PAUSE:
            m_bThreadIsPaused = TRUE;
            m_MediaClock.Pause();
            if (m_AudioInitialized && m_pSourceVoice)
            {
                m_pSourceVoice->Stop(0);
            }
            if (m_pResponseQueue)
            {
                m_pResponseQueue->Send(reinterpret_cast<void*>((uaddr)TRUE + 1), MQ_NOBLOCK);
            }
            break;

        case MOVIE_CMD_RESUME:
            m_bThreadIsPaused = FALSE;
            m_MediaClock.Resume();
            if (m_AudioInitialized && m_pSourceVoice && m_bVoiceStarted)
            {
                HRESULT hr = m_pSourceVoice->Start(0);
                if (FAILED(hr))
                {
                    x_DebugMsg("Failed to restart source voice: 0x%08X\n", hr);
                }
            }
            if (m_pResponseQueue)
            {
                m_pResponseQueue->Send(reinterpret_cast<void*>((uaddr)TRUE + 1), MQ_NOBLOCK);
            }
            break;

        case MOVIE_CMD_SET_VOLUME:
            SetVolumeInternal(pCmd->Data.SetVolume.Volume);
            if (m_pResponseQueue)
            {
                m_pResponseQueue->Send(reinterpret_cast<void*>((uaddr)TRUE + 1), MQ_NOBLOCK);
            }
            break;

        case MOVIE_CMD_SHUTDOWN:
            m_bMovieThreadExit = TRUE;
            if (m_pResponseQueue)
            {
                m_pResponseQueue->Send(reinterpret_cast<void*>((uaddr)TRUE + 1), MQ_NOBLOCK);
            }
            break;
            
        default:
            x_DebugMsg("Unknown movie command: %d\n", pCmd->Type);
            break;
    }
}

//==============================================================================
// PUBLIC INTERFACE (MAIN THREAD)
//==============================================================================

xbool movie_private::Open(const char* pFilename, xbool PlayResident, xbool IsLooped)
{
    movie_command cmd;
    cmd.Type = MOVIE_CMD_OPEN;
    x_strcpy(cmd.Data.Open.Filename, pFilename);
    cmd.Data.Open.PlayResident = PlayResident;
    cmd.Data.Open.IsLooped = IsLooped;
    
    if (!SendCommand(MOVIE_CMD_OPEN, &cmd))
    {
        return FALSE;
    }
    
    return WaitForResponse();
}

//==============================================================================

void movie_private::Close(void)
{
    if (SendCommand(MOVIE_CMD_CLOSE, NULL))
    {
        WaitForResponse();
    }
}

//==============================================================================

void movie_private::Pause(void)
{
    if (SendCommand(MOVIE_CMD_PAUSE, NULL))
    {
        WaitForResponse();
    }
}

//==============================================================================

void movie_private::Resume(void)
{
    if (SendCommand(MOVIE_CMD_RESUME, NULL))
    {
        WaitForResponse();
    }
}

//==============================================================================

void movie_private::SetVolume(f32 Volume)
{
    movie_command cmd;
    cmd.Type = MOVIE_CMD_SET_VOLUME;
    cmd.Data.SetVolume.Volume = Volume;
    
    if (SendCommand(MOVIE_CMD_SET_VOLUME, &cmd))
    {
        WaitForResponse();
    }
}

//==============================================================================

void movie_private::Kill(void)
{
    // Send shutdown command
    movie_command* pCmd = new movie_command;
    pCmd->Type = MOVIE_CMD_SHUTDOWN;
    
    if (m_pCommandQueue && m_pCommandQueue->Send(pCmd, MQ_NOBLOCK))
    {
        WaitForResponse(5000);
    }
    else
    {
        delete pCmd;
    }
    
    // Wait for thread to finish
    if (m_pMovieThread)
    {
        s32 timeout = 0;
        while (m_bMovieThreadRunning && timeout < 3000)
        {
            x_DelayThread(1);
            timeout++;
        }
        
        delete m_pMovieThread;
        m_pMovieThread = NULL;
    }
    
    // Clean up render data
    m_RenderData.DataMutex.Enter();
    if (m_RenderData.pFrameData)
    {
        av_free(m_RenderData.pFrameData);
        m_RenderData.pFrameData = NULL;
    }
    m_RenderData.DataMutex.Exit();
    
    // Clean up queues
    if (m_pCommandQueue)
    {
        delete m_pCommandQueue;
        m_pCommandQueue = NULL;
    }
    
    if (m_pResponseQueue)
    {
        delete m_pResponseQueue;
        m_pResponseQueue = NULL;
    }
    
    DestroyVideoTexture();
}

//==============================================================================
// COMMAND HELPERS
//==============================================================================

xbool movie_private::SendCommand(movie_command_type type, void* pData)
{
    if (!m_pCommandQueue)
        return FALSE;
        
    movie_command* pCmd = new movie_command;
    pCmd->Type = type;

    switch (type)
    {
        case MOVIE_CMD_OPEN:
            if (pData)
            {
                movie_command* pSrcCmd = (movie_command*)pData;
                pCmd->Data.Open = pSrcCmd->Data.Open;
            }
            break;
            
        case MOVIE_CMD_SET_VOLUME:
            if (pData)
            {
                movie_command* pSrcCmd = (movie_command*)pData;
                pCmd->Data.SetVolume = pSrcCmd->Data.SetVolume;
            }
            break;
            
        case MOVIE_CMD_CLOSE:
        case MOVIE_CMD_PAUSE:
        case MOVIE_CMD_RESUME:
        case MOVIE_CMD_SHUTDOWN:
            break;
            
        default:
            delete pCmd;
            return FALSE;
    }
    
    if (!m_pCommandQueue->Send(pCmd, MQ_NOBLOCK))
    {
        delete pCmd;
        return FALSE;
    }
    
    return TRUE;
}

//==============================================================================

xbool movie_private::WaitForResponse(s32 timeoutMs)
{
    if (!m_pResponseQueue)
        return FALSE;
        
    s32 waited = 0;
    while (waited < timeoutMs)
    {
        void* pResponse = m_pResponseQueue->Recv(MQ_NOBLOCK);
        if (pResponse)
        {
            uaddr value = (uaddr)pResponse;
            if (value > 0)
            {
                value--;
            }
            return value != 0;
        }
        x_DelayThread(1);
        waited++;
    }
    return FALSE;
}

//==============================================================================
// MOVIE THREAD OPERATIONS
//==============================================================================

xbool movie_private::OpenInternal(const char* pFilename, xbool PlayResident, xbool IsLooped)
{
    m_IsLooped = IsLooped;
    
    char filepath[512];
    const char* extensions[] = { ".webm", ".mp4", ".avi", NULL }; //You can add supported FFmpeg formats here.
    
    for (s32 i = 0; extensions[i] != NULL; i++)
    {
        x_sprintf(filepath, "C:\\GameData\\A51\\Release\\PC\\%s%s", pFilename, extensions[i]);
        
        if (avformat_open_input(&m_pFormatContext, filepath, NULL, NULL) == 0)
        {
            break;
        }
    }
    
    if (!m_pFormatContext)
    {
        m_bThreadIsFinished = TRUE;
        return FALSE;
    }
    
    if (avformat_find_stream_info(m_pFormatContext, NULL) < 0)
    {
        CleanupFFmpeg();
        m_bThreadIsFinished = TRUE;
        return FALSE;
    }
    
    // Find video and audio streams
    m_VideoStreamIndex = -1;
    m_AudioStreamIndex = -1;
    
    for (u32 i = 0; i < m_pFormatContext->nb_streams; i++)
    {
        if (m_pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && m_VideoStreamIndex == -1)
        {
            m_VideoStreamIndex = (s32)i;
        }
        else if (m_pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_AudioStreamIndex == -1)
        {
            m_AudioStreamIndex = (s32)i;
        }
    }
    
    if (m_VideoStreamIndex == -1)
    {
        CleanupFFmpeg();
        m_bThreadIsFinished = TRUE;
        return FALSE;
    }
    
    // Setup video codec
    m_pCodecContext = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(m_pCodecContext, m_pFormatContext->streams[m_VideoStreamIndex]->codecpar);
    m_pCodec = avcodec_find_decoder(m_pCodecContext->codec_id);
    
    if (!m_pCodec || avcodec_open2(m_pCodecContext, m_pCodec, NULL) < 0)
    {
        CleanupFFmpeg();
        m_bThreadIsFinished = TRUE;
        return FALSE;
    }
    
    // Setup audio if available
    if (m_AudioStreamIndex != -1)
    {
        SetupAudioStream();
    }
    
    // Allocate frames
    m_pFrame = av_frame_alloc();
    m_pFrameRGB = av_frame_alloc();
    m_pAudioFrame = av_frame_alloc();
    
    if (!m_pFrame || !m_pFrameRGB || !m_pAudioFrame)
    {
        CleanupFFmpeg();
        m_bThreadIsFinished = TRUE;
        return FALSE;
    }
    
    // Get video dimensions
    m_Width = m_pCodecContext->width;
    m_Height = m_pCodecContext->height;
    
    // Calculate video FPS
    AVStream* pVideoStream = m_pFormatContext->streams[m_VideoStreamIndex];
    if (pVideoStream->r_frame_rate.num && pVideoStream->r_frame_rate.den)
    {
        m_VideoFPS = av_q2d(pVideoStream->r_frame_rate);
    }
    else if (pVideoStream->avg_frame_rate.num && pVideoStream->avg_frame_rate.den)
    {
        m_VideoFPS = av_q2d(pVideoStream->avg_frame_rate);
    }
    else
    {
        m_VideoFPS = 25.0;
    }
    
    // Clamp FPS to reasonable values
    if (m_VideoFPS < 1.0 || m_VideoFPS > 120.0)
    {
        m_VideoFPS = 25.0;
    }
    
    // Calculate frame count
    if (pVideoStream->duration != AV_NOPTS_VALUE)
    {
        f64 duration = (f64)pVideoStream->duration * av_q2d(pVideoStream->time_base);
        m_FrameCount = (s32)(duration * m_VideoFPS);
    }
    else
    {
        m_FrameCount = 0;
    }
    
    // Setup RGB conversion
    s32 numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, m_Width, m_Height, 32);
    m_pBuffer = (u8*)av_malloc(numBytes * sizeof(u8));
    m_BufferSize = numBytes;
    
    av_image_fill_arrays(m_pFrameRGB->data, m_pFrameRGB->linesize, m_pBuffer, 
                        AV_PIX_FMT_BGRA, m_Width, m_Height, 32);
    
    // Create scaling context
    m_pSwsContext = sws_getContext(m_Width, m_Height, m_pCodecContext->pix_fmt,
                                  m_Width, m_Height, AV_PIX_FMT_BGRA,
                                  SWS_BILINEAR, NULL, NULL, NULL);
    
    if (!m_pSwsContext)
    {
        CleanupFFmpeg();
        m_bThreadIsFinished = TRUE;
        return FALSE;
    }
    
    // Initialize media clock and counters
    m_VideoFrameCount = 0;
    m_MediaClock.Start();
    
    m_ThreadCurrentFrame = 0;
    m_bThreadIsFinished = FALSE;
    m_bThreadIsRunning = TRUE;
    
    return TRUE;
}

//==============================================================================

void movie_private::CloseInternal(void)
{
    m_MediaClock.Stop();
    m_bThreadIsFinished = TRUE;
    m_bThreadIsPaused = FALSE;
    m_bThreadIsRunning = FALSE;
    m_ThreadCurrentFrame = 0;
    m_VideoFrameCount = 0;
    m_FrameCount = 0;
    CleanupFFmpeg();
}

//==============================================================================

void movie_private::SetVolumeInternal(f32 Volume)
{
    m_Volume = Volume;
    UpdateAudioVolume();
}

//==============================================================================
// DECODE NEXT VIDEO FRAME
//==============================================================================

xbool movie_private::DecodeNextVideoFrame(void)
{
    if (!m_pFormatContext)
        return FALSE;
        
    AVPacket packet;
    s32 readResult;
    
    while ((readResult = av_read_frame(m_pFormatContext, &packet)) >= 0)
    {
        if (packet.stream_index == m_VideoStreamIndex)
        {
            s32 ret = avcodec_send_packet(m_pCodecContext, &packet);
            if (ret >= 0)
            {
                ret = avcodec_receive_frame(m_pCodecContext, m_pFrame);
                if (ret >= 0)
                {
                    sws_scale(m_pSwsContext, (const u8* const*)m_pFrame->data, m_pFrame->linesize,
                             0, m_Height, m_pFrameRGB->data, m_pFrameRGB->linesize);   
                    
                    m_ThreadCurrentFrame++;
                    
                    av_packet_unref(&packet);
                    return TRUE;
                }
            }
        }
        else if (packet.stream_index == m_AudioStreamIndex && m_pAudioCodecContext)
        {
            s32 ret = avcodec_send_packet(m_pAudioCodecContext, &packet);
            if (ret >= 0)
            {
                ret = avcodec_receive_frame(m_pAudioCodecContext, m_pAudioFrame);
                if (ret >= 0)
                {
                    s32 out_samples = swr_convert(m_pSwrContext,
                        &m_pAudioBuffer, 48000,
                        (const u8**)m_pAudioFrame->data, m_pAudioFrame->nb_samples);
                        
                    if (out_samples > 0)
                    {
                        s32 out_size = out_samples * 2 * 2; // 2 channels * 2 bytes per sample
                        WriteAudioData(m_pAudioBuffer, out_size);
                    }
                }
            }
        }
        
        av_packet_unref(&packet);
    }

    if (readResult == AVERROR_EOF)
    {
        if (m_IsLooped)
        {
            s32 seekResult = av_seek_frame(m_pFormatContext, m_VideoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
            if (seekResult >= 0)
            {
                if (m_pCodecContext)
                {
                    avcodec_flush_buffers(m_pCodecContext);
                }
                if (m_pAudioCodecContext)
                {
                    avcodec_flush_buffers(m_pAudioCodecContext);
                }

                m_VideoFrameCount = 0;
                m_ThreadCurrentFrame = 0;
                m_MediaClock.Start();

                return DecodeNextVideoFrame();
            }
        }
    }

    m_bThreadIsFinished = TRUE;
    return FALSE;
}

//==============================================================================
// RENDER DATA MANAGEMENT
//==============================================================================

void movie_private::UpdateRenderData(void)
{
    if (!m_pFrameRGB || !m_pFrameRGB->data[0])
        return;
        
    m_RenderData.DataMutex.Enter();
    
    s32 requiredSize = m_Width * m_Height * 4;
    if (!m_RenderData.pFrameData || m_RenderData.Width != m_Width || m_RenderData.Height != m_Height)
    {
        if (m_RenderData.pFrameData)
        {
            av_free(m_RenderData.pFrameData);
        }
        
        m_RenderData.pFrameData = (u8*)av_malloc(requiredSize);
        m_RenderData.Width = m_Width;
        m_RenderData.Height = m_Height;
        m_RenderData.Pitch = m_Width * 4;
    }
    
    if (m_RenderData.pFrameData)
    {
        u8* pDest = m_RenderData.pFrameData;
        u8* pSrc = m_pFrameRGB->data[0];
        
        for (s32 y = 0; y < m_Height; y++)
        {
            x_memcpy(pDest, pSrc, m_Width * 4);
            pDest += m_Width * 4;
            pSrc += m_pFrameRGB->linesize[0];
        }
        
        m_RenderData.bHasNewFrame = TRUE;
        m_RenderData.bIsValid = TRUE;
    }
    
    m_RenderData.DataMutex.Exit();
}

//==============================================================================
// RENDER (MAIN THREAD)
//==============================================================================

xbitmap* movie_private::Decode(void)
{
    // Check if device is available
    if (!g_pd3dDevice)
    {
        return NULL;
    }
    
    // Get render data from movie thread
    m_RenderData.DataMutex.Enter();
    
    xbool hasNewFrame = m_RenderData.bHasNewFrame;
    xbool isValid = m_RenderData.bIsValid;
    
    if (hasNewFrame && isValid)
    {
        // Recreate texture if needed
        if (!m_pTexture)
        {
            if (!CreateVideoTexture())
            {
                m_RenderData.DataMutex.Exit();
                return NULL;
            }
        }
        
        // Update texture with new frame
        if (UpdateVideoTextureFromRenderData())
        {
            m_RenderData.bHasNewFrame = FALSE;
        }
    }
    
    m_RenderData.DataMutex.Exit();
    
    // Render current frame if valid
    if (isValid)
    {
        RenderVideoFrame();
    }
    
    return NULL;
}

//==============================================================================
// DIRECTX11 METHODS (MAIN THREAD)
//==============================================================================

xbool movie_private::CreateVideoTexture(void)
{
    if (!g_pd3dDevice || m_Width <= 0 || m_Height <= 0)
        return FALSE;
        
    D3D11_TEXTURE2D_DESC desc;
    x_memset(&desc, 0, sizeof(desc));
    desc.Width = m_Width;
    desc.Height = m_Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
    
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&desc, NULL, &m_pTexture);
    if (FAILED(hr))
        return FALSE;
    
    m_TextureVRAMID = vram_Register(m_pTexture);
    
    return TRUE;
}

//==============================================================================

void movie_private::DestroyVideoTexture(void)
{
    if (m_TextureVRAMID != 0)
    {
        vram_Unregister(m_TextureVRAMID);
        m_TextureVRAMID = 0;
    }
    
    if (m_pTexture)
    {
        m_pTexture->Release();
        m_pTexture = NULL;
    }
}

//==============================================================================

xbool movie_private::UpdateVideoTextureFromRenderData(void)
{
    if (!g_pd3dContext || !m_pTexture || !m_RenderData.pFrameData)
        return FALSE;
        
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = g_pd3dContext->Map(m_pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        return FALSE;
    }
    
    u8* pDest = (u8*)mappedResource.pData;
    u8* pSrc = m_RenderData.pFrameData;
    
    for (s32 y = 0; y < m_RenderData.Height; y++)
    {
        x_memcpy(pDest, pSrc, m_RenderData.Width * 4);
        pDest += mappedResource.RowPitch;
        pSrc += m_RenderData.Pitch;
    }
    
    g_pd3dContext->Unmap(m_pTexture, 0);
    return TRUE;
}

//==============================================================================

void movie_private::RenderVideoFrame(void)
{
    if (!g_pd3dContext || m_TextureVRAMID == 0)
        return;
        
    s32 WindowWidth, WindowHeight;
    eng_GetRes(WindowWidth, WindowHeight);
    
    f32 destX, destY, destW, destH;
    
    if (m_bForceStretch) 
    {
        destX = 0.0f;
        destY = 0.0f;
        destW = (f32)WindowWidth;
        destH = (f32)WindowHeight;
    } 
    else 
    {
        f32 aspectRatio = (f32)m_Width / (f32)m_Height;
        f32 screenRatio = (f32)WindowWidth / (f32)WindowHeight;
        
        if (screenRatio > aspectRatio) 
        {
            destH = (f32)WindowHeight;
            destW = destH * aspectRatio;
            destX = (WindowWidth - destW) / 2.0f;
            destY = 0.0f;
        } 
        else 
        {
            destW = (f32)WindowWidth;
            destH = destW / aspectRatio;
            destX = 0.0f;
            destY = (WindowHeight - destH) / 2.0f;
        }
    }
    
    draw_Begin(DRAW_QUADS, DRAW_2D | DRAW_UI_RTARGET | DRAW_TEXTURED | DRAW_USE_ALPHA | DRAW_CULL_NONE  );
    
    if (!m_bForceStretch)
    {
        draw_SetTexture();
        draw_Color(XCOLOR_BLACK);
        draw_Vertex(0.0f, 0.0f, 0.5f);
        draw_Vertex(0.0f, (f32)WindowHeight, 0.5f);
        draw_Vertex((f32)WindowWidth, (f32)WindowHeight, 0.5f);
        draw_Vertex((f32)WindowWidth, 0.0f, 0.5f);
    }
    
    vram_Activate(m_TextureVRAMID);
    draw_Color(XCOLOR_WHITE);
    draw_UV(0.0f, 0.0f); draw_Vertex(destX, destY, 0.5f);
    draw_UV(0.0f, 1.0f); draw_Vertex(destX, destY + destH, 0.5f);
    draw_UV(1.0f, 1.0f); draw_Vertex(destX + destW, destY + destH, 0.5f);
    draw_UV(1.0f, 0.0f); draw_Vertex(destX + destW, destY, 0.5f);
    
    draw_End();
}

//==============================================================================
// FFMPEG METHODS (MOVIE THREAD)
//==============================================================================

xbool movie_private::InitializeFFmpeg(void)
{
    // GS: Initially, this function was used as an initializer for the old version of FFmpeg. 
    // Now it doesn't do anything, but I'll keep it for symmetry (initializer/destructor).
    return TRUE;
}

//==============================================================================

void movie_private::CleanupFFmpeg(void)
{
    if (m_pSwrContext)
    {
        swr_free(&m_pSwrContext);
        m_pSwrContext = NULL;
    }
    
    if (m_pSwsContext)
    {
        sws_freeContext(m_pSwsContext);
        m_pSwsContext = NULL;
    }
    
    if (m_pBuffer)
    {
        av_free(m_pBuffer);
        m_pBuffer = NULL;
    }
    
    if (m_pAudioBuffer)
    {
        av_free(m_pAudioBuffer);
        m_pAudioBuffer = NULL;
    }
    
    if (m_pAudioFrame)
    {
        av_frame_free(&m_pAudioFrame);
    }
    
    if (m_pFrameRGB)
    {
        av_frame_free(&m_pFrameRGB);
    }
    
    if (m_pFrame)
    {
        av_frame_free(&m_pFrame);
    }
    
    if (m_pAudioCodecContext)
    {
        avcodec_free_context(&m_pAudioCodecContext);
    }
    
    if (m_pCodecContext)
    {
        avcodec_free_context(&m_pCodecContext);
    }
    
    if (m_pFormatContext)
    {
        avformat_close_input(&m_pFormatContext);
    }
    
    m_VideoStreamIndex = -1;
    m_AudioStreamIndex = -1;
}

//==============================================================================
// AUDIO METHODS (MOVIE THREAD)
//==============================================================================

xbool movie_private::InitializeAudio(void)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        m_ComInitialized = TRUE;
    }
    else
    {
        m_ComInitialized = FALSE;
        return FALSE;
    }

    hr = XAudio2Create(&m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        if (m_ComInitialized)
        {
            CoUninitialize();
            m_ComInitialized = FALSE;
        }
        return FALSE;
    }

    hr = m_pXAudio2->CreateMasteringVoice(&m_pMasterVoice);
    if (FAILED(hr))
    {
        CleanupAudio();
        return FALSE;
    }

    m_AudioInitialized = TRUE;
    return TRUE;
}

//==============================================================================

void movie_private::CleanupAudio(void)
{
    if (m_pSourceVoice)
    {
        m_pSourceVoice->Stop(0);
        m_pSourceVoice->FlushSourceBuffers();

        XAUDIO2_VOICE_STATE state;
        do {
            x_DelayThread(1);
            m_pSourceVoice->GetState(&state);
        } while (state.BuffersQueued > 0);
        
        m_pSourceVoice->DestroyVoice();
        m_pSourceVoice = NULL;
    }
    
    if (m_pVoiceCallback)
    {
        delete m_pVoiceCallback;
        m_pVoiceCallback = NULL;
    }
    
    if (m_pMasterVoice)
    {
        m_pMasterVoice->DestroyVoice();
        m_pMasterVoice = NULL;
    }
    
    if (m_pXAudio2)
    {
        m_pXAudio2->Release();
        m_pXAudio2 = NULL;
    }
    
    if (m_ComInitialized)
    {
        CoUninitialize();
        m_ComInitialized = FALSE;
    }
    
    m_AudioInitialized = FALSE;
}

//==============================================================================

xbool movie_private::SetupAudioStream(void)
{
    if (!m_AudioInitialized || m_AudioStreamIndex == -1)
        return FALSE;
        
    m_pAudioCodecContext = avcodec_alloc_context3(NULL);
    if (!m_pAudioCodecContext)
        return FALSE;
        
    if (avcodec_parameters_to_context(m_pAudioCodecContext, m_pFormatContext->streams[m_AudioStreamIndex]->codecpar) < 0)
        return FALSE;
        
    m_pAudioCodec = avcodec_find_decoder(m_pAudioCodecContext->codec_id);
    
    if (!m_pAudioCodec || avcodec_open2(m_pAudioCodecContext, m_pAudioCodec, NULL) < 0)
    {
        return FALSE;
    }
    
    m_pSwrContext = swr_alloc();
    if (!m_pSwrContext)
        return FALSE;
        
    AVChannelLayout stereo_layout = AV_CHANNEL_LAYOUT_STEREO;
    av_opt_set_chlayout(m_pSwrContext, "in_chlayout", &m_pAudioCodecContext->ch_layout, 0);
    av_opt_set_int(m_pSwrContext, "in_sample_rate", m_pAudioCodecContext->sample_rate, 0);
    av_opt_set_sample_fmt(m_pSwrContext, "in_sample_fmt", m_pAudioCodecContext->sample_fmt, 0);
    
    av_opt_set_chlayout(m_pSwrContext, "out_chlayout", &stereo_layout, 0);
    av_opt_set_int(m_pSwrContext, "out_sample_rate", 44100, 0);
    av_opt_set_sample_fmt(m_pSwrContext, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
        
    if (swr_init(m_pSwrContext) < 0)
    {
        swr_free(&m_pSwrContext);
        return FALSE;
    }
    
    m_pAudioBuffer = (u8*)av_malloc(192000);
    
    WAVEFORMATEX wfx;
    x_memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    
    m_pVoiceCallback = new XAudioVoiceCallback(this);

    HRESULT hr = m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, m_pVoiceCallback);
    if (FAILED(hr))
    {
        CleanupAudio();
        return FALSE;
    }

    m_bVoiceStarted = FALSE;
    UpdateAudioVolume();

    return TRUE;
}

//==============================================================================

void movie_private::WriteAudioData(u8* pData, s32 size)
{
    if (!m_AudioInitialized || !m_pSourceVoice || size <= 0)
        return;

    XAUDIO2_VOICE_STATE state;
    m_pSourceVoice->GetState(&state);

    if (state.BuffersQueued >= 8)
    {
        return;
    }
        
    // Allocate buffer for audio data
    u8* pAudioBuffer = (u8*)x_malloc(size);
    if (!pAudioBuffer)
        return;
        
    x_memcpy(pAudioBuffer, pData, size);
    
    // Submit buffer to XAudio2
    XAUDIO2_BUFFER buffer;
    x_memset(&buffer, 0, sizeof(buffer));
    buffer.AudioBytes = size;
    buffer.pAudioData = pAudioBuffer;
    buffer.pContext = pAudioBuffer; // Store pointer for cleanup in callback
    
    HRESULT hr = m_pSourceVoice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr))
    {
        x_free(pAudioBuffer);
        x_DebugMsg("Failed to submit audio buffer: 0x%08X\n", hr);
    }
    else if (!m_bVoiceStarted)
    {
        hr = m_pSourceVoice->Start(0);
        if (FAILED(hr))
        {
            x_DebugMsg("Failed to start source voice: 0x%08X\n", hr);
        }
        else
        {
            m_bVoiceStarted = TRUE;
        }
    }
}

//==============================================================================

void movie_private::UpdateAudioVolume(void)
{
    if (!m_AudioInitialized || !m_pMasterVoice)
        return;
        
    f32 volume = m_Volume;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    
    m_pMasterVoice->SetVolume(volume);
}