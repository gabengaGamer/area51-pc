//==============================================================================
//
//  MoviePlayer_WebM_Audio.cpp
//
//  Audio decoding for WebM playback using SDL3.
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
//  INCLUDES
//==============================================================================

#include "SDL3/SDL.h"

#include "x_files.hpp"
#include "x_threads.hpp"
#include "x_memory.hpp"
#include "x_math.hpp"

#include "MoviePlayer_WebM_Private.hpp"

using namespace movie_webm;

//==============================================================================
// CONSTANTS
//==============================================================================

namespace
{
    static const s32 MAX_OPUS_FRAME_MS       = 120;
    static const s32 AUDIO_SUBMIT_TIMEOUT_MS = 500;
    static const s32 AUDIO_MAX_QUEUED_MS     = 250;
}

//==============================================================================
// CONSTRUCTION / DESTRUCTION
//==============================================================================

audio_decoder::audio_decoder(void)
{
    m_Volume               = 1.0f;
    m_CodecType            = CODEC_NONE;
    m_Channels             = 0;
    m_SampleRate           = 0;
    m_BitsPerSample        = 16;
    m_isInitialized         = FALSE;
    m_SdlInitialized       = FALSE;
    m_pAudioStream         = NULL;
    m_MaxQueuedBytes       = 0;

    m_pOpusDecoder         = NULL;
    m_pOpusMSDecoder       = NULL;
    m_OpusStreams          = 0;
    m_OpusCoupled          = 0;
    m_OpusPreSkip          = 0;
    m_OpusPreSkipRemaining = 0;
    m_MaxFrameSamples      = 0;
    m_OpusGain             = 0;

    m_pVorbisInfo          = NULL;
    m_pVorbisComment       = NULL;
    m_pVorbisDsp           = NULL;
    m_pVorbisBlock         = NULL;
    m_VorbisInitialized    = FALSE;
    m_VorbisPacketIndex    = 0;

    for (s32 i = 0; i < 3; ++i)
    {
        m_VorbisHeaders[i].Clear();
    }
}

//==============================================================================

audio_decoder::~audio_decoder(void)
{
    Shutdown();
}

//==============================================================================
// INITIALIZATION / SHUTDOWN
//==============================================================================

xbool audio_decoder::Initialize(const player_config& Config)
{
    Shutdown();

    if (!Config.HasAudio)
        return TRUE;

    m_Volume        = 1.0f;
    m_CodecType     = CODEC_NONE;
    m_Channels      = (Config.AudioChannels > 0) ? Config.AudioChannels : 2;
    m_SampleRate    = (Config.AudioSampleRate > 0) ? Config.AudioSampleRate : 48000;
    m_BitsPerSample = 16; //m_BitsPerSample = (Config.AudioBitDepth > 0) ? Config.AudioBitDepth : 16;  

    m_MaxFrameSamples = (m_SampleRate * MAX_OPUS_FRAME_MS) / 1000;
    if (m_MaxFrameSamples <= 0)
        m_MaxFrameSamples = 5760;

    xstring CodecId = Config.AudioCodecId;
    CodecId.MakeUpper();

    if (CodecId.Find("OPUS") != -1)
    {
        m_CodecType = CODEC_OPUS;
    }
    else if (CodecId.Find("VORBIS") != -1)
    {
        m_CodecType = CODEC_VORBIS;
    }
    else
    {
        x_DebugMsg("MoviePlayer_WebM: Unsupported audio codec '%s'\n", (const char*)CodecId);
        return FALSE;
    }

    xbool codecInitialized = FALSE;
    if (m_CodecType == CODEC_OPUS)
    {
        codecInitialized = InitializeOpus(Config);
    }
    else if (m_CodecType == CODEC_VORBIS)
    {
        codecInitialized = InitializeVorbis(Config);
    }

    if (!codecInitialized)
    {
        Shutdown();
        return FALSE;
    }

    if (!InitializeSDL())
    {
        Shutdown();
        return FALSE;
    }

    m_isInitialized = TRUE;
    SetVolume(1.0f);
    return TRUE;
}

//==============================================================================

void audio_decoder::Shutdown(void)
{
    DestroyOpus();
    DestroyVorbis();
    ShutdownSDL();

    m_CompressedBuffer.Clear();
    m_PCMBuffer.Clear();

    m_CodecType      = CODEC_NONE;
    m_isInitialized   = FALSE;
    m_MaxFrameSamples= 0;
    m_MaxQueuedBytes = 0;
}

//==============================================================================

void audio_decoder::Flush(void)
{
    if (m_pAudioStream)
    {
        SDL_ClearAudioStream(m_pAudioStream);
    }

    ResetDecoders();
}

//==============================================================================
// DECODING
//==============================================================================

xbool audio_decoder::DecodeSample(const sample& Sample, mkvparser::IMkvReader* pReader)
{
    ASSERT(m_isInitialized);
    ASSERT(Sample.pBlock);
    ASSERT(pReader);

    if (!m_isInitialized || !Sample.pBlock || !pReader)
        return FALSE;

    const mkvparser::Block* pBlock = Sample.pBlock;
    const s32 frameCount = (s32)pBlock->GetFrameCount();

    for (s32 frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        const mkvparser::Block::Frame& Frame = pBlock->GetFrame(frameIndex);
        const s32 frameSize = (s32)Frame.len;

        if (frameSize <= 0)
            continue;

        m_CompressedBuffer.SetCount(frameSize);
        if (Frame.Read(pReader, m_CompressedBuffer.GetPtr()) < 0)
        {
            x_DebugMsg("MoviePlayer_WebM: Failed to read audio frame.\n");
            continue;
        }

        if (m_CodecType == CODEC_OPUS)
        {
            if (!DecodeOpusFrame(m_CompressedBuffer.GetPtr(), frameSize))
                return FALSE;
        }
        else if (m_CodecType == CODEC_VORBIS)
        {
            if (!DecodeVorbisPacket(m_CompressedBuffer.GetPtr(), frameSize))
                return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

void audio_decoder::SetVolume(f32 Volume)
{
    if (Volume < 0.0f) Volume = 0.0f;
    if (Volume > 1.0f) Volume = 1.0f;

    m_Volume = Volume;

    if (m_pAudioStream && !SDL_SetAudioStreamGain(m_pAudioStream, m_Volume))
    {
        x_DebugMsg("MoviePlayer_WebM: SDL_SetAudioStreamGain failed: %s\n", SDL_GetError());
    }
}

//==============================================================================
// INTERNAL HELPERS
//==============================================================================

xbool audio_decoder::InitializeSDL(void)
{
    if (m_pAudioStream)
        return TRUE;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        x_DebugMsg("MoviePlayer_WebM: SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s\n", SDL_GetError());
        return FALSE;
    }

    m_SdlInitialized = TRUE;

    SDL_AudioSpec spec;
    x_memset(&spec, 0, sizeof(spec));
    spec.format   = SDL_AUDIO_S16;
    spec.channels = m_Channels;
    spec.freq     = m_SampleRate;

    m_pAudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!m_pAudioStream)
    {
        x_DebugMsg("MoviePlayer_WebM: SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        ShutdownSDL();
        return FALSE;
    }

    m_MaxQueuedBytes = ((m_SampleRate * AUDIO_MAX_QUEUED_MS) / 1000) * m_Channels * (s32)sizeof(s16);
    if (m_MaxQueuedBytes <= 0)
        m_MaxQueuedBytes = m_SampleRate * m_Channels * (s32)sizeof(s16);

    if (!SDL_SetAudioStreamGain(m_pAudioStream, m_Volume))
    {
        x_DebugMsg("MoviePlayer_WebM: SDL_SetAudioStreamGain failed: %s\n", SDL_GetError());
    }

    if (!SDL_ResumeAudioStreamDevice(m_pAudioStream))
    {
        x_DebugMsg("MoviePlayer_WebM: SDL_ResumeAudioStreamDevice failed: %s\n", SDL_GetError());
        ShutdownSDL();
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void audio_decoder::ShutdownSDL(void)
{
    if (m_pAudioStream)
    {
        SDL_PauseAudioStreamDevice(m_pAudioStream);
        SDL_DestroyAudioStream(m_pAudioStream);
        m_pAudioStream = NULL;
    }

    if (m_SdlInitialized)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_SdlInitialized = FALSE;
    }
}

//==============================================================================
// OPUS IMPLEMENTATION
//==============================================================================

xbool audio_decoder::InitializeOpus(const player_config& Config)
{
    DestroyOpus();

    const s32 privateSize = Config.AudioCodecPrivate.GetCount();
    const u8* pPrivate    = (privateSize > 0) ? Config.AudioCodecPrivate.GetPtr() : NULL;

    if (pPrivate && (privateSize >= 19) && (x_memcmp(pPrivate, "OpusHead", 8) == 0))
    {
        const u8 channelCount = pPrivate[9];
        if (channelCount > 0)
            m_Channels = channelCount;

        const s32 preSkip = (s32)(pPrivate[10] | (pPrivate[11] << 8));
        m_OpusPreSkip          = preSkip;
        m_OpusPreSkipRemaining = preSkip;

        const s32 inputRate = (s32)(pPrivate[12] | (pPrivate[13] << 8) | (pPrivate[14] << 16) | (pPrivate[15] << 24));
        if (inputRate > 0)
            m_SampleRate = inputRate;

        const s16 gain = (s16)(pPrivate[16] | (pPrivate[17] << 8));
        m_OpusGain = (s32)gain;

        const u8 mappingFamily = pPrivate[18];
        if (mappingFamily == 0)
        {
            s32 err = 0;
            m_pOpusDecoder = opus_decoder_create(m_SampleRate, m_Channels, &err);
            if ((err != OPUS_OK) || !m_pOpusDecoder)
            {
                x_DebugMsg("MoviePlayer_WebM: Failed to create Opus decoder (%d).\n", err);
                DestroyOpus();
                return FALSE;
            }

            if (gain != 0)
            {
                opus_decoder_ctl(m_pOpusDecoder, OPUS_SET_GAIN(gain));
            }

            m_OpusStreams = 1;
            m_OpusCoupled = (m_Channels > 1) ? 1 : 0;
        }
        else
        {
            if (privateSize < 21)
            {
                x_DebugMsg("MoviePlayer_WebM: Invalid Opus header (mapping).\n");
                return FALSE;
            }

            const u8 streamCount  = pPrivate[19];
            const u8 coupledCount = pPrivate[20];
            const s32 mappingSize = privateSize - 21;

            if (mappingSize < m_Channels)
            {
                x_DebugMsg("MoviePlayer_WebM: Invalid Opus mapping size.\n");
                return FALSE;
            }

            m_OpusMapping.SetCount(mappingSize);
            x_memcpy(m_OpusMapping.GetPtr(), pPrivate + 21, mappingSize);

            s32 err = 0;
            m_pOpusMSDecoder = opus_multistream_decoder_create(m_SampleRate, m_Channels, streamCount, coupledCount, m_OpusMapping.GetPtr(), &err);
            if ((err != OPUS_OK) || !m_pOpusMSDecoder)
            {
                x_DebugMsg("MoviePlayer_WebM: Failed to create Opus multistream decoder (%d).\n", err);
                DestroyOpus();
                return FALSE;
            }

            if (gain != 0)
            {
                opus_multistream_decoder_ctl(m_pOpusMSDecoder, OPUS_SET_GAIN(gain));
            }

            m_OpusStreams = streamCount;
            m_OpusCoupled = coupledCount;
        }
    }
    else
    {
        int err = 0;
        m_pOpusDecoder = opus_decoder_create(m_SampleRate, m_Channels, &err);
        if ((err != OPUS_OK) || !m_pOpusDecoder)
        {
            x_DebugMsg("MoviePlayer_WebM: Failed to create default Opus decoder (%d).\n", err);
            DestroyOpus();
            return FALSE;
        }

        m_OpusStreams = 1;
        m_OpusCoupled = (m_Channels > 1) ? 1 : 0;
        m_OpusPreSkip          = 0;
        m_OpusPreSkipRemaining = 0;
        m_OpusGain             = 0;
    }

    m_PCMBuffer.SetCount(m_MaxFrameSamples * m_Channels);
    return TRUE;
}

//==============================================================================

void audio_decoder::DestroyOpus(void)
{
    if (m_pOpusDecoder)
    {
        opus_decoder_destroy(m_pOpusDecoder);
        m_pOpusDecoder = NULL;
    }

    if (m_pOpusMSDecoder)
    {
        opus_multistream_decoder_destroy(m_pOpusMSDecoder);
        m_pOpusMSDecoder = NULL;
    }

    m_OpusMapping.Clear();
    m_OpusStreams          = 0;
    m_OpusCoupled          = 0;
    m_OpusPreSkip          = 0;
    m_OpusPreSkipRemaining = 0;
    m_OpusGain             = 0;
}

//==============================================================================

xbool audio_decoder::DecodeOpusFrame(const u8* pData, s32 DataSize)
{
    ASSERT(m_pOpusDecoder || m_pOpusMSDecoder);
    ASSERT(pData);
    ASSERT(DataSize > 0);    

    if ((!m_pOpusDecoder && !m_pOpusMSDecoder) || (DataSize <= 0))
        return TRUE;

    if (m_PCMBuffer.GetCount() < (m_MaxFrameSamples * m_Channels))
    {
        m_PCMBuffer.SetCount(m_MaxFrameSamples * m_Channels);
    }

    s16* pOutput = m_PCMBuffer.GetPtr();
    s32  samples = 0;

    if (m_pOpusMSDecoder)
    {
        samples = opus_multistream_decode(m_pOpusMSDecoder, pData, DataSize, pOutput, m_MaxFrameSamples, 0);
    }
    else
    {
        samples = opus_decode(m_pOpusDecoder, pData, DataSize, pOutput, m_MaxFrameSamples, 0);
    }

    if (samples < 0)
    {
        x_DebugMsg("MoviePlayer_WebM: Opus decode error (%d).\n", samples);
        return FALSE;
    }

    if (samples == 0)
        return TRUE;

    if (m_OpusPreSkipRemaining > 0)
    {
        const s32 skip = x_min(m_OpusPreSkipRemaining, samples);
        m_OpusPreSkipRemaining -= skip;
        
        const s32 remaining = samples - skip;
        if (remaining > 0)
        {
            x_memmove(pOutput, pOutput + (skip * m_Channels), remaining * m_Channels * (s32)sizeof(s16));
            samples = remaining;
        }
        else
        {
            return TRUE;
        }
    }

    return SubmitPCM(pOutput, samples);
}

//==============================================================================
// VORBIS IMPLEMENTATION
//==============================================================================

xbool audio_decoder::InitializeVorbis(const player_config& Config)
{
    DestroyVorbis();

    if (!ParseVorbisPrivate(Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to parse Vorbis private data.\n");
        DestroyVorbis();
        return FALSE;
    }

    if (!InitializeVorbisState())
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to initialize Vorbis decoder state.\n");
        DestroyVorbis();
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool audio_decoder::ParseVorbisPrivate(const player_config& Config)
{
    const s32 privateSize = Config.AudioCodecPrivate.GetCount();
    const u8* pPrivate    = (privateSize > 0) ? Config.AudioCodecPrivate.GetPtr() : NULL;

    if (!pPrivate || (privateSize <= 0))
        return FALSE;

    const s32 packetCount = (s32)pPrivate[0] + 1;
    if (packetCount != 3)
        return FALSE;

    const u8* pCurrent = pPrivate + 1;
    const u8* pEnd     = pPrivate + privateSize;

    s32 packetSizes[3] = {0, 0, 0};

    for (s32 i = 0; i < packetCount - 1; ++i)
    {
        s32 length = 0;
        while (pCurrent < pEnd)
        {
            const u8 value = *pCurrent++;
            length += value;
            if (value < 255)
                break;
        }
        packetSizes[i] = length;
    }

    for (s32 i = 0; i < packetCount; ++i)
    {
        s32 length = 0;
        if (i < packetCount - 1)
        {
            length = packetSizes[i];
        }
        else
        {
            length = (s32)(pEnd - pCurrent);
        }

        if ((length < 0) || ((pCurrent + length) > pEnd))
            return FALSE;

        m_VorbisHeaders[i].SetCount(length);
        if (length > 0)
        {
            x_memcpy(m_VorbisHeaders[i].GetPtr(), pCurrent, length);
        }
        pCurrent += length;
    }

    if (m_VorbisHeaders[0].GetCount() < 16)
        return FALSE;

    const u8* pHeader = m_VorbisHeaders[0].GetPtr();
    if ((pHeader[0] != 0x01) || (x_memcmp(pHeader + 1, "vorbis", 6) != 0))
        return FALSE;

    const u8 channelCount = pHeader[11];
    if (channelCount > 0)
        m_Channels = channelCount;

    const s32 sampleRate = (s32)(pHeader[12] | (pHeader[13] << 8) | (pHeader[14] << 16) | (pHeader[15] << 24));
    if (sampleRate > 0)
        m_SampleRate = sampleRate;

    m_VorbisPacketIndex = 0;
    return TRUE;
}

//==============================================================================

xbool audio_decoder::InitializeVorbisState(void)
{
    DestroyVorbisState(FALSE);

    if ((m_VorbisHeaders[0].GetCount() == 0) ||
        (m_VorbisHeaders[1].GetCount() == 0) ||
        (m_VorbisHeaders[2].GetCount() == 0))
    {
        return FALSE;
    }

    m_pVorbisInfo    = (vorbis_info*)x_malloc(sizeof(vorbis_info));
    m_pVorbisComment = (vorbis_comment*)x_malloc(sizeof(vorbis_comment));
    m_pVorbisDsp     = (vorbis_dsp_state*)x_malloc(sizeof(vorbis_dsp_state));
    m_pVorbisBlock   = (vorbis_block*)x_malloc(sizeof(vorbis_block));

    if (!m_pVorbisInfo || !m_pVorbisComment || !m_pVorbisDsp || !m_pVorbisBlock)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    x_memset(m_pVorbisInfo, 0, sizeof(vorbis_info));
    x_memset(m_pVorbisComment, 0, sizeof(vorbis_comment));
    x_memset(m_pVorbisDsp, 0, sizeof(vorbis_dsp_state));
    x_memset(m_pVorbisBlock, 0, sizeof(vorbis_block));

    vorbis_info_init(m_pVorbisInfo);
    vorbis_comment_init(m_pVorbisComment);

    ogg_packet packet;
    x_memset(&packet, 0, sizeof(packet));

    // Identification header
    packet.packet   = m_VorbisHeaders[0].GetPtr();
    packet.bytes    = m_VorbisHeaders[0].GetCount();
    packet.b_o_s    = 1;
    packet.packetno = 0;
    if (vorbis_synthesis_headerin(m_pVorbisInfo, m_pVorbisComment, &packet) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    // Comment header
    packet.packet   = m_VorbisHeaders[1].GetPtr();
    packet.bytes    = m_VorbisHeaders[1].GetCount();
    packet.b_o_s    = 0;
    packet.packetno = 1;
    if (vorbis_synthesis_headerin(m_pVorbisInfo, m_pVorbisComment, &packet) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    // Setup header
    packet.packet   = m_VorbisHeaders[2].GetPtr();
    packet.bytes    = m_VorbisHeaders[2].GetCount();
    packet.packetno = 2;
    if (vorbis_synthesis_headerin(m_pVorbisInfo, m_pVorbisComment, &packet) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    if (vorbis_synthesis_init(m_pVorbisDsp, m_pVorbisInfo) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    if (vorbis_block_init(m_pVorbisDsp, m_pVorbisBlock) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    m_VorbisInitialized = TRUE;
    m_VorbisPacketIndex = 0;
    return TRUE;
}

//==============================================================================

void audio_decoder::DestroyVorbis(void)
{
    DestroyVorbisState(TRUE);
}

//==============================================================================

void audio_decoder::DestroyVorbisState(xbool ClearHeaders)
{
    if (m_pVorbisBlock)
    {
        vorbis_block_clear(m_pVorbisBlock);
        x_free(m_pVorbisBlock);
        m_pVorbisBlock = NULL;
    }

    if (m_pVorbisDsp)
    {
        vorbis_dsp_clear(m_pVorbisDsp);
        x_free(m_pVorbisDsp);
        m_pVorbisDsp = NULL;
    }

    if (m_pVorbisComment)
    {
        vorbis_comment_clear(m_pVorbisComment);
        x_free(m_pVorbisComment);
        m_pVorbisComment = NULL;
    }

    if (m_pVorbisInfo)
    {
        vorbis_info_clear(m_pVorbisInfo);
        x_free(m_pVorbisInfo);
        m_pVorbisInfo = NULL;
    }

    if (ClearHeaders)
    {
        for (s32 i = 0; i < 3; ++i)
            m_VorbisHeaders[i].Clear();
    }

    m_VorbisInitialized = FALSE;
    m_VorbisPacketIndex = 0;
}

//==============================================================================

xbool audio_decoder::DecodeVorbisPacket(const u8* pData, s32 DataSize)
{
    ASSERT(m_VorbisInitialized);
    ASSERT(m_pVorbisBlock);
    ASSERT(m_pVorbisDsp);
    ASSERT(DataSize > 0);    
    ASSERT(pData);

    if (!m_VorbisInitialized || !m_pVorbisBlock || !m_pVorbisDsp || (DataSize <= 0))
        return TRUE;

    ogg_packet packet;
    x_memset(&packet, 0, sizeof(packet));

    packet.packet   = const_cast<unsigned char*>(pData);
    packet.bytes    = DataSize;
    packet.packetno = ++m_VorbisPacketIndex;

    const s32 result = (s32)vorbis_synthesis(m_pVorbisBlock, &packet);
    if (result != 0)
    {
        x_DebugMsg("MoviePlayer_WebM: Vorbis synthesis failed (%d).\n", result);
        return FALSE;
    }

    if (vorbis_synthesis_blockin(m_pVorbisDsp, m_pVorbisBlock) != 0)
    {
        x_DebugMsg("MoviePlayer_WebM: Vorbis blockin failed.\n");
        return FALSE;
    }

    while (TRUE)
    {
        f32** ppPcm = NULL;
        const s32 samples = (s32)vorbis_synthesis_pcmout(m_pVorbisDsp, &ppPcm);
        if (samples <= 0)
            break;

        const s32 sampleCount = samples;
        if (sampleCount > (INT_MAX / m_Channels))
        {
            x_DebugMsg("MoviePlayer_WebM: Sample count overflow detected.\n");
            vorbis_synthesis_read(m_pVorbisDsp, sampleCount);
            continue;
        }
        
        const s32 totalSamples = sampleCount * m_Channels;
        if (totalSamples <= 0)
        {
            vorbis_synthesis_read(m_pVorbisDsp, sampleCount);
            continue;
        }

        m_PCMBuffer.SetCount(totalSamples);
        s16* pOut = m_PCMBuffer.GetPtr();

        for (s32 i = 0; i < sampleCount; ++i)
        {
            for (s32 ch = 0; ch < m_Channels; ++ch)
            {
                f32 sample = (ppPcm[ch])[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;

                s32 value = (sample >= 0.0f) ? (s32)(sample * 32767.0f + 0.5f) : (s32)(sample * 32768.0f - 0.5f);
                if (value > 32767) value = 32767;
                if (value < -32768) value = -32768;

                *pOut++ = (s16)value;
            }
        }

        SubmitPCM(m_PCMBuffer.GetPtr(), sampleCount);
        vorbis_synthesis_read(m_pVorbisDsp, sampleCount);
    }

    return TRUE;
}

//==============================================================================

void audio_decoder::ResetDecoders(void)
{
    if (m_CodecType == CODEC_OPUS)
    {
        if (m_pOpusDecoder)
        {
            opus_decoder_ctl(m_pOpusDecoder, OPUS_RESET_STATE);
        }
        if (m_pOpusMSDecoder)
        {
            opus_multistream_decoder_ctl(m_pOpusMSDecoder, OPUS_RESET_STATE);
        }
        m_OpusPreSkipRemaining = m_OpusPreSkip;
    }
    else if (m_CodecType == CODEC_VORBIS)
    {
        if (!InitializeVorbisState())
        {
            x_DebugMsg("MoviePlayer_WebM: Failed to reset Vorbis decoder.\n");
        }
    }
}

//==============================================================================

xbool audio_decoder::SubmitPCM(const s16* pSamples, s32 SampleCount)
{
    ASSERT(m_pAudioStream);
    ASSERT(pSamples);
    ASSERT(SampleCount > 0);    
    
    if (!m_pAudioStream || !pSamples || (SampleCount <= 0))
        return TRUE;

    s32 waitMs = 0;
    while (SDL_GetAudioStreamQueued(m_pAudioStream) >= m_MaxQueuedBytes)
    {
        if (waitMs >= AUDIO_SUBMIT_TIMEOUT_MS)
        {
            x_DebugMsg("MoviePlayer_WebM: Audio submit timeout, dropping buffer.\n");
            return FALSE;
        }
        x_DelayThread(1);
        ++waitMs;
    }

    const s32 bytes = SampleCount * m_Channels * (s32)sizeof(s16);
    if (!SDL_PutAudioStreamData(m_pAudioStream, pSamples, bytes))
    {
        x_DebugMsg("MoviePlayer_WebM: SDL_PutAudioStreamData failed: %s\n", SDL_GetError());
        return FALSE;
    }

    return TRUE;
}
