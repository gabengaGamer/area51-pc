//==============================================================================
//
//  MoviePlayer_WebM.cpp
//
//  Public entry points for the WebM movie backend.
//
//==============================================================================

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

#include "x_files.hpp"
#include "x_threads.hpp"
#include "x_memory.hpp"
#include "Entropy.hpp"
#include "../MoviePlayer.hpp"

#include "MoviePlayer_WebM_Private.hpp"
#include "UI/ui_renderer.hpp"

//==============================================================================
// CONSTANTS
//==============================================================================

namespace
{
    static const f64 VIDEO_PRESENT_LEAD = 0.005;
    static const f64 AUDIO_BUFFER_LEAD  = 0.25;
}

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

movie_player Movie;

//==============================================================================
// MOVIE PRIVATE IMPLEMENTATION
//==============================================================================

movie_private::movie_private(void)
{
    m_Width             = 0;
    m_Height            = 0;
    m_Volume            = 1.0f;
    m_IsLooped          = FALSE;
    m_WorkerService     = x_worker_service(HNULL);
    m_bThreadExit       = FALSE;
    m_bThreadRunning    = FALSE;
    m_bThreadFinished   = TRUE;
    m_bPaused           = FALSE;
    m_bPlaybackActive   = FALSE;
    m_bVideoEOF         = FALSE;
    m_bThreadBusy       = FALSE;

    m_LastVideoTime     = 0.0f;
    m_bHasPendingVideo  = FALSE;
    m_PendingVideoSample = movie_webm::sample();
}

//==============================================================================

movie_private::~movie_private(void)
{
    Kill();
}

//==============================================================================

void movie_private::Init(void)
{
    if (m_WorkerService.IsNonNull())
        return;

    m_bThreadExit     = FALSE;
    m_bThreadRunning  = FALSE;
    m_bThreadFinished = TRUE;

    ASSERTS(x_WorkersIsInit(), "MoviePlayer_WebM requires x_workers to be initialized");
    if (!x_WorkersIsInit())
        return;

    if (!x_WorkerServiceStart(WorkerEntry, this, "MoviePlayerWebM", m_WorkerService))
    {
        ASSERTS(FALSE, "MoviePlayer_WebM failed to start worker service");
        return;
    }

    #if X_WORKERS_DEBUG && X_WORKERS_DEBUG_LOG
    x_DebugMsg("MoviePlayer_WebM: using x_worker service %08x\n", m_WorkerService.Handle);
    #endif

    while (!m_bThreadRunning)
    {
        x_DelayThread(1);
    }
}

//==============================================================================

xbool movie_private::Open(const char* pFilename, xbool PlayResident, xbool IsLooped, x_language Language)
{
    (void)PlayResident;

    if (!pFilename || !pFilename[0])
        return FALSE;

    Init();
    if (m_WorkerService.IsNull())
        return FALSE;

    Close();

    m_IsLooped = IsLooped;
    m_Config   = movie_webm::player_config();
    m_Config.IsLooped = IsLooped;
    m_Config.AudioLanguage = Language;

    if (!m_Container.Open(pFilename, m_Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to open movie '%s'\n", pFilename);
        return FALSE;
    }

    if (!m_VideoDecoder.Initialize(m_Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to initialize video decoder for '%s'\n", pFilename);
        m_Container.Close();
        return FALSE;
    }

    if (m_Config.HasAudio)
    {
        if (!m_AudioDecoder.Initialize(m_Config))
        {
            x_DebugMsg("MoviePlayer_WebM: Audio initialization failed for '%s'\n", pFilename);
            m_Config.HasAudio = FALSE;
        }
        else
        {
            m_AudioDecoder.SetVolume(m_Volume);
        }
    }

    m_Width         = m_Config.Width;
    m_Height        = m_Config.Height;
    m_LastVideoTime = 0.0;

    m_renderData.DataMutex.Enter();
    m_renderData.pFrameData   = NULL;
    m_renderData.Width        = 0;
    m_renderData.Height       = 0;
    m_renderData.Pitch        = 0;
    m_renderData.FrameTime    = 0.0;
    m_renderData.bHasNewFrame = FALSE;
    m_renderData.bIsValid     = FALSE;
    m_renderData.DataMutex.Exit();

    m_RenderBuffer.Clear();

    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();

    m_Clock.Reset();

    m_bPaused           = FALSE;
    m_bVideoEOF         = FALSE;
    m_bThreadFinished   = FALSE;
    m_bPlaybackActive   = FALSE;
    m_bThreadBusy       = FALSE;

    PrimePlayback();

    m_Clock.Start();

    m_bPlaybackActive = TRUE;

    return TRUE;
}

//==============================================================================

void movie_private::Close(void)
{
    m_bPlaybackActive = FALSE;
    m_bVideoEOF       = FALSE;

    while (m_bThreadBusy)
    {
        x_DelayThread(1);
    }

    m_Clock.Stop();

    m_renderData.DataMutex.Enter();
    m_renderData.pFrameData   = NULL;
    m_renderData.Width        = 0;
    m_renderData.Height       = 0;
    m_renderData.Pitch        = 0;
    m_renderData.FrameTime    = 0.0;
    m_renderData.bHasNewFrame = FALSE;
    m_renderData.bIsValid     = FALSE;
    m_renderData.DataMutex.Exit();

    m_RenderBuffer.Clear();

    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();

    m_AudioDecoder.Shutdown();
    m_VideoDecoder.Shutdown();
    m_Container.Close();

    DestroyVideoTexture();

    m_bThreadFinished = TRUE;
}

//==============================================================================

void movie_private::Kill(void)
{
    Close();
    Shutdown();
}

//==============================================================================

void movie_private::Shutdown(void)
{
    if (m_WorkerService.IsNull())
        return;

    m_bThreadExit = TRUE;

    while (m_bThreadRunning)
    {
        x_DelayThread(1);
    }

    if (m_WorkerService.IsNonNull())
    {
        x_WorkerServiceWait(m_WorkerService);
        x_WorkerServiceRelease(m_WorkerService);
        m_WorkerService = x_worker_service(HNULL);
    }
}

//==============================================================================

void movie_private::SetVolume(f32 Volume)
{
    m_Volume = Volume;
    m_AudioDecoder.SetVolume(Volume);
}

//==============================================================================

void movie_private::Pause(void)
{
    if (m_bPaused)
        return;

    m_bPaused = TRUE;
    m_Clock.Pause();
}

//==============================================================================

void movie_private::Resume(void)
{
    if (!m_bPaused)
        return;

    m_bPaused = FALSE;
    m_Clock.Resume();
}

//==============================================================================

void movie_private::Render(void)
{
    m_renderData.DataMutex.Enter();

    const xbool hasNewFrame = m_renderData.bHasNewFrame;
    const xbool isValid     = m_renderData.bIsValid;

    if (hasNewFrame && isValid)
    {
        if (!vram_IsValid(m_VideoTexture))
        {
            if (!CreateVideoTexture())
            {
                m_renderData.DataMutex.Exit();
                return;
            }
        }

        if (!UpdateVideoTexture())
        {
            m_renderData.DataMutex.Exit();
            return;
        }

        m_renderData.bHasNewFrame = FALSE;
    }

    m_renderData.DataMutex.Exit();

    if (isValid)
    {
        RenderVideoFrame();
    }
}

//==============================================================================

void movie_private::WorkerEntry(void* pData)
{
    movie_private* pThis = reinterpret_cast<movie_private*>(pData);

    ASSERT(pThis);

    if (pThis)
    {
        pThis->ThreadMain();
    }
}

//==============================================================================

void movie_private::ThreadMain(void)
{
    m_bThreadRunning  = TRUE;

    while (!m_bThreadExit)
    {
        ThreadLoop();

        if (!m_bPlaybackActive || m_bPaused)
        {
            x_DelayThread(4);
        }
    }

    m_bThreadRunning  = FALSE;
    m_bThreadFinished = TRUE;
}

//==============================================================================

void movie_private::ThreadLoop(void)
{
    if (!m_bPlaybackActive || m_bPaused)
        return;

    struct busy_scope
    {
        volatile xbool& m_bBusy;
        busy_scope(volatile xbool& b) : m_bBusy(b) { m_bBusy = TRUE; }
        ~busy_scope() { m_bBusy = FALSE; }
    } guard(m_bThreadBusy);

    movie_webm::sample Sample;

    if (m_bHasPendingVideo)
    {
        Sample = m_PendingVideoSample;
    }
    else
    {
        if (!m_Container.PeekSample(Sample))
        {
            HandleEndOfStream();
            return;
        }
    }

    const f64 playbackTime  = m_Clock.GetTime();
    const f64 frameDuration = (m_Config.FrameRate > 0.0f)
                              ? (1.0 / (f64)m_Config.FrameRate) : (1.0 / 30.0);

    if (Sample.Type == movie_webm::STREAm_Type_AUDIO)
    {
        if (m_Config.HasAudio && (Sample.TimeSeconds > playbackTime + AUDIO_BUFFER_LEAD))
        {
            SleepMilliseconds(Sample.TimeSeconds - (playbackTime + AUDIO_BUFFER_LEAD));
            return;
        }

        if (!m_Container.ReadSample(Sample))
        {
            HandleEndOfStream();
            return;
        }

        if (m_Config.HasAudio)
            m_AudioDecoder.DecodeSample(Sample, m_Container.GetReader());

        return;
    }

    if (Sample.Type != movie_webm::STREAm_Type_VIDEO)
    {
        m_Container.ReadSample(Sample);
        return;
    }

    if (m_bHasPendingVideo)
    {
        m_bHasPendingVideo = FALSE;
    }
    else
    {
        if (!m_Container.ReadSample(Sample))
        {
            HandleEndOfStream();
            return;
        }
    }

    const f64 delta = Sample.TimeSeconds - playbackTime;

    if (delta > VIDEO_PRESENT_LEAD)
    {
        m_PendingVideoSample = Sample;
        m_bHasPendingVideo   = TRUE;
        PumpAudio(Sample.TimeSeconds + AUDIO_BUFFER_LEAD);
        SleepMilliseconds(delta - VIDEO_PRESENT_LEAD);
        return;
    }

    if (delta < -frameDuration)
    {
        m_LastVideoTime = Sample.TimeSeconds;
        PumpAudio(Sample.TimeSeconds + AUDIO_BUFFER_LEAD);
        return;
    }

    if (!ProcessVideoSample(Sample))
        return;

    m_LastVideoTime = Sample.TimeSeconds;

    if (!m_IsLooped && m_Config.Duration > 0.0)
    {
        const f64 endThreshold = x_max(0.0, m_Config.Duration - frameDuration);
        if (Sample.TimeSeconds >= endThreshold)
        {
            HandleEndOfStream();
            return;
        }
    }

    PumpAudio(Sample.TimeSeconds + AUDIO_BUFFER_LEAD);
}

//==============================================================================

xbool movie_private::PrimePlayback(void)
{
    movie_webm::sample Sample;
    xbool bDecodedFrame = FALSE;

    while (m_Container.PeekSample(Sample))
    {
        if (Sample.Type == movie_webm::STREAm_Type_AUDIO)
        {
            if (!m_Container.ReadSample(Sample))
                break;

            if (m_Config.HasAudio)
            {
                m_AudioDecoder.DecodeSample(Sample, m_Container.GetReader());
            }

            continue;
        }

        if (Sample.Type == movie_webm::STREAm_Type_VIDEO)
        {
            if (!m_Container.ReadSample(Sample))
                break;

            if (!ProcessVideoSample(Sample))
                break;

            m_LastVideoTime = Sample.TimeSeconds;
            PreloadAudio(Sample.TimeSeconds + AUDIO_BUFFER_LEAD);
            bDecodedFrame = TRUE;
            break;
        }

        if (!m_Container.ReadSample(Sample))
            break;
    }

    if (!bDecodedFrame)
    {
        m_Container.Rewind();

        if (m_Config.HasAudio)
        {
            m_AudioDecoder.Flush();
        }

        m_LastVideoTime = 0.0;
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void movie_private::PreloadAudio(f64 TargetTime)
{
    if (!m_Config.HasAudio)
        return;

    while (TRUE)
    {
        movie_webm::sample Sample;

        if (!m_Container.PeekSample(Sample))
            return;

        if (Sample.Type != movie_webm::STREAm_Type_AUDIO)
            return;

        if (Sample.TimeSeconds > TargetTime)
            return;

        if (!m_Container.ReadSample(Sample))
            return;

        m_AudioDecoder.DecodeSample(Sample, m_Container.GetReader());
    }
}

//==============================================================================

xbool movie_private::ProcessVideoSample(const movie_webm::sample& Sample)
{
    if (!m_VideoDecoder.DecodeSample(Sample, m_Container.GetReader()))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to decode video sample at %f sec\n", Sample.TimeSeconds);
        return FALSE;
    }

    UpdateRenderBuffer(Sample);
    return TRUE;
}

//==============================================================================

void movie_private::UpdateRenderBuffer(const movie_webm::sample& Sample)
{
    const u8* pFrameData = m_VideoDecoder.GetFrameData();
    if (!pFrameData)
        return;

    const s32 pitch  = m_VideoDecoder.GetFramePitch();
    const s32 width  = m_VideoDecoder.GetWidth();
    const s32 height = m_VideoDecoder.GetHeight();

    const s32 frameSize = pitch * height;
    if (frameSize <= 0)
        return;

    m_RenderBuffer.SetCount(frameSize);
    x_memcpy(m_RenderBuffer.GetPtr(), pFrameData, frameSize);

    m_renderData.DataMutex.Enter();
    m_renderData.pFrameData   = m_RenderBuffer.GetPtr();
    m_renderData.Width        = width;
    m_renderData.Height       = height;
    m_renderData.Pitch        = pitch;
    m_renderData.FrameTime    = Sample.TimeSeconds;
    m_renderData.bHasNewFrame = TRUE;
    m_renderData.bIsValid     = TRUE;
    m_renderData.DataMutex.Exit();
}

//==============================================================================

void movie_private::PumpAudio(f64 TargetTime)
{
    if (!m_Config.HasAudio)
        return;

    while (m_bPlaybackActive)
    {
        movie_webm::sample Sample;

        if (!m_Container.PeekSample(Sample))
        {
            if (!m_bHasPendingVideo)
                HandleEndOfStream();
            return;
        }

        if (Sample.Type != movie_webm::STREAm_Type_AUDIO)
            return;

        if (Sample.TimeSeconds > TargetTime)
            return;

        if (!m_Container.ReadSample(Sample))
        {
            if (!m_bHasPendingVideo)
                HandleEndOfStream();
            return;
        }

        m_AudioDecoder.DecodeSample(Sample, m_Container.GetReader());
    }
}

//==============================================================================

void movie_private::HandleEndOfStream(void)
{
    if (!m_bPlaybackActive)
        return;

    if (m_IsLooped)
    {
        ResetPlayback();
        return;
    }

    m_bPlaybackActive = FALSE;
    m_bVideoEOF       = TRUE;
    m_bThreadFinished = TRUE;
    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();

    if (m_Config.HasAudio)
    {
        m_AudioDecoder.Flush();
    }

    m_Clock.Stop();
}

//==============================================================================

void movie_private::SleepMilliseconds(f64 Seconds)
{
    if (Seconds <= 0.0)
        return;

    const s32 sleepMS = (s32)(Seconds * 1000.0 + 0.5);
    if (sleepMS > 0)
    {
        x_DelayThread(sleepMS);
    }
}

//==============================================================================

void movie_private::ResetPlayback(void)
{
    m_bPlaybackActive = FALSE;

    m_Container.Rewind();
    m_VideoDecoder.Shutdown();
    if (!m_VideoDecoder.Initialize(m_Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to reinitialize video decoder during loop.\n");
        m_bPlaybackActive = FALSE;
        m_bVideoEOF = TRUE;
        m_bThreadFinished = TRUE;
        return;
    }
    if (m_Config.HasAudio)
    {
        m_AudioDecoder.Flush();
    }
    m_LastVideoTime = 0.0;

    m_renderData.DataMutex.Enter();
    m_renderData.bHasNewFrame = FALSE;
    m_renderData.DataMutex.Exit();

    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();

    m_Clock.Reset();

    if (!PrimePlayback())
    {
        m_renderData.DataMutex.Enter();
        m_renderData.pFrameData   = NULL;
        m_renderData.Width        = 0;
        m_renderData.Height       = 0;
        m_renderData.Pitch        = 0;
        m_renderData.FrameTime    = 0.0;
        m_renderData.bHasNewFrame = FALSE;
        m_renderData.bIsValid     = FALSE;
        m_renderData.DataMutex.Exit();

        m_bPlaybackActive = FALSE;
        m_bVideoEOF       = TRUE;
        m_bThreadFinished = TRUE;
        return;
    }

    m_Clock.Start();

    m_bVideoEOF       = FALSE;
    m_bThreadFinished = FALSE;
    m_bPlaybackActive = TRUE;
}

//==============================================================================

xbool movie_private::CreateVideoTexture(void)
{
    if ((m_renderData.Width <= 0) || (m_renderData.Height <= 0))
        return FALSE;

    DestroyVideoTexture();

    vram_texture_desc Desc;
    Desc.Width      = (u32)m_renderData.Width;
    Desc.Height     = (u32)m_renderData.Height;
    Desc.Format     = VRAM_TEXTURE_FORMAT_BGRA8;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.pDebugName = "MovieVideoTexture";
    return vram_CreateTexture(m_VideoTexture, Desc);
}

//==============================================================================

void movie_private::DestroyVideoTexture(void)
{
    vram_DestroyTexture(m_VideoTexture);
}

//==============================================================================

xbool movie_private::UpdateVideoTexture(void)
{
    if (!vram_IsValid(m_VideoTexture) ||
        !m_renderData.pFrameData ||
        (m_renderData.Width <= 0) ||
        (m_renderData.Height <= 0) ||
        (m_renderData.Pitch <= 0))
    {
        return FALSE;
    }

    if ((m_VideoTexture.Desc.Width != (u32)m_renderData.Width) ||
        (m_VideoTexture.Desc.Height != (u32)m_renderData.Height))
    {
        if (!CreateVideoTexture())
            return FALSE;
    }

    vram_texture_upload_desc Upload;
    Upload.Region.Width  = (u32)m_renderData.Width;
    Upload.Region.Height = (u32)m_renderData.Height;
    Upload.Region.Depth  = 1;
    Upload.pData         = m_renderData.pFrameData;
    Upload.Size          = (u32)(m_renderData.Pitch * m_renderData.Height);
    Upload.RowPitch      = (u32)m_renderData.Pitch;
    Upload.SlicePitch    = Upload.Size;
    Upload.bCycle        = TRUE;
    return vram_UploadTexture(m_VideoTexture, Upload);
}

//==============================================================================

void movie_private::RenderVideoFrame(void)
{
    if (!vram_IsValid(m_VideoTexture))
        return;

    ui_viewport const& Viewport = g_UIRenderer.GetViewport();
    rect const& Bounds = Viewport.GetLogicalBounds();
    vector2 Position = Bounds.Min;
    vector2 Size     = Bounds.GetSize();
    vector2 UV0(0.0f, 0.0f);
    vector2 UV1(1.0f, 1.0f);

    const shader_resource* pVideoResource = vram_GetShaderResource(m_VideoTexture);
    if (pVideoResource)
    {
        g_UIRenderer.DrawImage(*pVideoResource,
                               Position,
                               Size,
                               UV0,
                               UV1,
                               XCOLOR_WHITE,
                               0.0f,
                               UI_BLEND_ALPHA,
                               UI_SAMPLER_LINEAR_CLAMP);
    }
}

//==============================================================================
