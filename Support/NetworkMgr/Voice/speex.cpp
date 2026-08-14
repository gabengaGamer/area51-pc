//==============================================================================
//
//  Speex.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_memory.hpp"
#include "x_plus.hpp"
#include "x_debug.hpp"

#include "Speex.hpp"

//==============================================================================
//  VARIABLES
//==============================================================================

static SPEEX8* g_SPEEX = NULL;

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool SpeexInit( void )
{
    if( g_SPEEX != NULL )
    {
        return TRUE;
    }

    SPEEX8* pCodec = reinterpret_cast<SPEEX8*>( x_malloc( sizeof( SPEEX8 ) ) );
    if( pCodec == NULL )
    {
        return FALSE;
    }

    x_memset( pCodec, 0, sizeof( SPEEX8 ) );

    pCodec->encode_state = speex_encoder_init( &speex_nb_mode );
    pCodec->decode_state = speex_decoder_init( &speex_nb_mode );
    if( (pCodec->encode_state == NULL) || (pCodec->decode_state == NULL) )
    {
        if( pCodec->encode_state != NULL )
        {
            speex_encoder_destroy( pCodec->encode_state );
        }

        if( pCodec->decode_state != NULL )
        {
            speex_decoder_destroy( pCodec->decode_state );
        }

        x_free( pCodec );
        return FALSE;
    }

    speex_bits_init( &pCodec->encode_bits );
    speex_bits_init( &pCodec->decode_bits );

    s32 Enhancement = 1;
    s32 Vbr = 0;
    s32 Quality = SPEEX8_QUALITY;
    s32 Complexity = 4;

    speex_decoder_ctl( pCodec->decode_state, SPEEX_SET_ENH, &Enhancement );
    speex_encoder_ctl( pCodec->encode_state, SPEEX_SET_VBR, &Vbr );
    speex_encoder_ctl( pCodec->encode_state, SPEEX_SET_QUALITY, &Quality );
    speex_encoder_ctl( pCodec->encode_state, SPEEX_SET_COMPLEXITY, &Complexity );

    g_SPEEX = pCodec;
    return TRUE;
}

//==============================================================================

xbool SpeexKill( void )
{
    if( g_SPEEX == NULL )
    {
        return TRUE;
    }

    speex_bits_destroy( &g_SPEEX->encode_bits );
    speex_bits_destroy( &g_SPEEX->decode_bits );
    speex_encoder_destroy( g_SPEEX->encode_state );
    speex_decoder_destroy( g_SPEEX->decode_state );
    x_free( g_SPEEX );
    g_SPEEX = NULL;

    return TRUE;
}

//==============================================================================

xbool SpeexReset( void )
{
    if( g_SPEEX == NULL )
    {
        return FALSE;
    }

    g_SPEEX->encode_in_samples = 0;
    x_memset( g_SPEEX->encode_in, 0, sizeof( g_SPEEX->encode_in ) );
    speex_bits_reset( &g_SPEEX->encode_bits );

    return speex_encoder_ctl( g_SPEEX->encode_state, SPEEX_RESET_STATE, NULL ) == 0;
}

//==============================================================================

xbool SpeexEncode( const s16* pSrc, const u32 SrcSize, u8* pDest, s32* pDestSize )
{
    s32 DestinationCapacity;

    if( pDestSize == NULL )
    {
        return FALSE;
    }

    DestinationCapacity = *pDestSize;
    *pDestSize = 0;

    if( (g_SPEEX == NULL) ||
        (pSrc == NULL) ||
        (pDest == NULL) ||
        (SrcSize == 0) ||
        (SrcSize > static_cast<u32>( S32_MAX )) ||
        ((SrcSize % sizeof( s16 )) != 0) ||
        (DestinationCapacity <= 0) )
    {
        return FALSE;
    }

    ASSERT( (SrcSize % sizeof( s16 )) == 0 );

    const s32 InputSamples = static_cast<s32>( SrcSize / sizeof( s16 ) );
    const s32 AvailableFrames = (g_SPEEX->encode_in_samples + InputSamples) /
                                SPEEX8_SAMPLES_PER_FRAME;
    if( (AvailableFrames > (DestinationCapacity / SPEEX8_BYTES_PER_EFRAME)) )
    {
        return FALSE;
    }

    const s16* pNextSample = pSrc;
    s32 SamplesRemaining = InputSamples;
    s32 BytesEncoded = 0;

    x_memset( pDest, 0, static_cast<u32>( DestinationCapacity ) );

    while( SamplesRemaining > 0 )
    {
        const s32 SamplesToCopy = MIN( SPEEX8_SAMPLES_PER_FRAME - g_SPEEX->encode_in_samples,
                                       SamplesRemaining );
        x_memcpy( g_SPEEX->encode_in + g_SPEEX->encode_in_samples,
                  pNextSample,
                  static_cast<u32>( SamplesToCopy * sizeof( s16 ) ) );
        g_SPEEX->encode_in_samples += SamplesToCopy;
        pNextSample += SamplesToCopy;
        SamplesRemaining -= SamplesToCopy;

        if( g_SPEEX->encode_in_samples == SPEEX8_SAMPLES_PER_FRAME )
        {
            speex_bits_reset( &g_SPEEX->encode_bits );
            if( speex_encode_int( g_SPEEX->encode_state,
                                  g_SPEEX->encode_in,
                                  &g_SPEEX->encode_bits ) < 0 )
            {
                g_SPEEX->encode_in_samples = 0;
                x_memset( g_SPEEX->encode_in, 0, sizeof( g_SPEEX->encode_in ) );
                *pDestSize = 0;
                return FALSE;
            }

            const s32 EncodedSize = speex_bits_write( &g_SPEEX->encode_bits,
                                                      reinterpret_cast<char*>( pDest + BytesEncoded ),
                                                      SPEEX8_BYTES_PER_EFRAME );
            if( EncodedSize != SPEEX8_BYTES_PER_EFRAME )
            {
                g_SPEEX->encode_in_samples = 0;
                x_memset( g_SPEEX->encode_in, 0, sizeof( g_SPEEX->encode_in ) );
                *pDestSize = 0;
                return FALSE;
            }

            BytesEncoded += SPEEX8_BYTES_PER_EFRAME;
            g_SPEEX->encode_in_samples = 0;
            x_memset( g_SPEEX->encode_in, 0, sizeof( g_SPEEX->encode_in ) );
        }
    }

    *pDestSize = BytesEncoded;
    return TRUE;
}

//==============================================================================

xbool SpeexDecode( const u8* pSrc, const u32 SrcSize, s16* pDest, s32* pDestSize )
{
    s32 DestinationCapacity;
    s32 DestinationFrames;
    s32 AvailableFrames;
    s32 FramesToDecode;
    s32 BytesConsumed;
    s16* pNextSample;

    if( pDestSize == NULL )
    {
        return FALSE;
    }

    DestinationCapacity = *pDestSize;
    *pDestSize = 0;

    if( (g_SPEEX == NULL) ||
        (pDest == NULL) ||
        (DestinationCapacity <= 0) ||
        ((DestinationCapacity % sizeof( s16 )) != 0) ||
        ((SrcSize > 0) && (pSrc == NULL)) )
    {
        return FALSE;
    }

    if( SrcSize > static_cast<u32>( sizeof( g_SPEEX->decode_in ) - g_SPEEX->decode_in_bytes ) )
    {
        g_SPEEX->decode_in_bytes = 0;
        return FALSE;
    }

    if( SrcSize > 0 )
    {
        x_memcpy( g_SPEEX->decode_in + g_SPEEX->decode_in_bytes,
                  pSrc,
                  static_cast<s32>( SrcSize ) );
        g_SPEEX->decode_in_bytes += static_cast<s32>( SrcSize );
    }

    DestinationFrames = DestinationCapacity /
                        (SPEEX8_SAMPLES_PER_FRAME * sizeof( s16 ));
    AvailableFrames = g_SPEEX->decode_in_bytes / SPEEX8_BYTES_PER_EFRAME;
    FramesToDecode = MIN( DestinationFrames, AvailableFrames );
    if( FramesToDecode <= 0 )
    {
        return TRUE;
    }

    x_memset( pDest, 0, static_cast<u32>( DestinationCapacity ) );
    pNextSample = pDest;

    for( s32 Frame = 0; Frame < FramesToDecode; Frame++ )
    {
        speex_bits_reset( &g_SPEEX->decode_bits );
        speex_bits_read_from( &g_SPEEX->decode_bits,
                              reinterpret_cast<const char*>( g_SPEEX->decode_in +
                                                             (Frame * SPEEX8_BYTES_PER_EFRAME) ),
                              SPEEX8_BYTES_PER_EFRAME );
        if( speex_decode_int( g_SPEEX->decode_state,
                              &g_SPEEX->decode_bits,
                              reinterpret_cast<spx_int16_t*>( pNextSample ) ) < 0 )
        {
            x_memset( pNextSample,
                      0,
                      SPEEX8_SAMPLES_PER_FRAME * sizeof( s16 ) );
            *pDestSize = 0;
            g_SPEEX->decode_in_bytes = 0;
            return FALSE;
        }

        pNextSample += SPEEX8_SAMPLES_PER_FRAME;
        *pDestSize += SPEEX8_SAMPLES_PER_FRAME * sizeof( s16 );
    }

    BytesConsumed = FramesToDecode * SPEEX8_BYTES_PER_EFRAME;
    if( BytesConsumed < g_SPEEX->decode_in_bytes )
    {
        const s32 BytesToKeep = g_SPEEX->decode_in_bytes - BytesConsumed;
        x_memmove( g_SPEEX->decode_in,
                   g_SPEEX->decode_in + BytesConsumed,
                   static_cast<u32>( BytesToKeep ) );
        g_SPEEX->decode_in_bytes = BytesToKeep;
    }
    else
    {
        g_SPEEX->decode_in_bytes = 0;
    }

    return TRUE;
}
