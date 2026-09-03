//==============================================================================
//
//  sdleng_utils.cpp
//
//  Shared SDL GPU format policy and small backend helpers.
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

#include "sdleng_private.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

static SDL_GPUTextureFormat s_DepthStencilFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

//==============================================================================

void sdleng_LogError( const char* pSubsystem, const char* pContext )
{
    const char* pError = SDL_GetError();
    if( !pError || !pError[0] )
        pError = "unknown SDL error";

    x_DebugMsg( "%s: %s failed: %s\n",
                pSubsystem ? pSubsystem : "SDLEngine",
                pContext ? pContext : "SDL operation",
                pError );
}

//==============================================================================

static
SDL_GPUTextureFormat sdleng_SelectDepthStencilFormat( xbool bSupportsD24S8,
                                                       xbool bSupportsD32FS8 )
{
    if( bSupportsD24S8 )
        return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    if( bSupportsD32FS8 )
        return SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}

//==============================================================================

xbool sdleng_InitializeFormatPolicy( void )
{
    s_DepthStencilFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    if( !g_pSDLGPUDevice )
        return FALSE;

    const SDL_GPUTextureUsageFlags Usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    const xbool bSupportsD24S8 = SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice,
                                                               SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
                                                               SDL_GPU_TEXTURETYPE_2D,
                                                               Usage );
    const xbool bSupportsD32FS8 = SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice,
                                                                SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
                                                                SDL_GPU_TEXTURETYPE_2D,
                                                                Usage );
    s_DepthStencilFormat = sdleng_SelectDepthStencilFormat( bSupportsD24S8,
                                                            bSupportsD32FS8 );

    ASSERT( sdleng_SelectDepthStencilFormat( TRUE, TRUE ) == SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT );
    ASSERT( sdleng_SelectDepthStencilFormat( FALSE, TRUE ) == SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT );
    ASSERT( sdleng_SelectDepthStencilFormat( FALSE, FALSE ) == SDL_GPU_TEXTUREFORMAT_INVALID );

    if( s_DepthStencilFormat == SDL_GPU_TEXTUREFORMAT_INVALID )
    {
        x_DebugMsg( "SDLEngine: GPU supports neither D24S8 nor D32FS8 depth/stencil targets\n" );
        return FALSE;
    }

    x_DebugMsg( "SDLEngine: selected depth/stencil texture format %s\n",
                (s_DepthStencilFormat == SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT) ? "D24_UNORM_S8_UINT" : "D32_FLOAT_S8_UINT" );
    return TRUE;
}

//==============================================================================

void sdleng_ResetFormatPolicy( void )
{
    s_DepthStencilFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
}

//==============================================================================

xbool sdleng_ToSDLSampleCount( u32 SampleCount, SDL_GPUSampleCount& SDLSampleCount )
{
    switch( SampleCount )
    {
        case 1: SDLSampleCount = SDL_GPU_SAMPLECOUNT_1; return TRUE;
        case 2: SDLSampleCount = SDL_GPU_SAMPLECOUNT_2; return TRUE;
        case 4: SDLSampleCount = SDL_GPU_SAMPLECOUNT_4; return TRUE;
        case 8: SDLSampleCount = SDL_GPU_SAMPLECOUNT_8; return TRUE;
        default: return FALSE;
    }
}

//==============================================================================

SDL_GPUTextureFormat sdleng_ToSDLTextureFormat( rtarget_format Format )
{
    switch( Format )
    {
        case RTARGET_FORMAT_RGBA8:          return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        case RTARGET_FORMAT_BGRA8:          return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        case RTARGET_FORMAT_RGBA16F:        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        case RTARGET_FORMAT_RGBA32F:        return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        case RTARGET_FORMAT_RGB10A2:        return SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM;
        case RTARGET_FORMAT_R8:             return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        case RTARGET_FORMAT_RG16F:          return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
        case RTARGET_FORMAT_R32F:           return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        case RTARGET_FORMAT_DEPTH_STENCIL:  return s_DepthStencilFormat;
        case RTARGET_FORMAT_DEPTH32F:       return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        default:                            return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}

//==============================================================================

SDL_GPUTextureFormat sdleng_ToSDLTextureFormat( vram_texture_format Format )
{
    switch( Format )
    {
        case VRAM_TEXTURE_FORMAT_RGBA8:           return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        case VRAM_TEXTURE_FORMAT_BGRA8:           return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        case VRAM_TEXTURE_FORMAT_RGB10A2:         return SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM;
        case VRAM_TEXTURE_FORMAT_RGBA16F:         return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        case VRAM_TEXTURE_FORMAT_RGBA32F:         return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        case VRAM_TEXTURE_FORMAT_R8:              return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        case VRAM_TEXTURE_FORMAT_RG16F:           return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
        case VRAM_TEXTURE_FORMAT_R32F:            return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        case VRAM_TEXTURE_FORMAT_B5G6R5:          return SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM;
        case VRAM_TEXTURE_FORMAT_B5G5R5A1:        return SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM;
        case VRAM_TEXTURE_FORMAT_B4G4R4A4:        return SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM;
        case VRAM_TEXTURE_FORMAT_BC1_RGBA:        return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
        case VRAM_TEXTURE_FORMAT_BC2_RGBA:        return SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM;
        case VRAM_TEXTURE_FORMAT_BC3_RGBA:        return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
        case VRAM_TEXTURE_FORMAT_DEPTH_STENCIL:   return s_DepthStencilFormat;
        case VRAM_TEXTURE_FORMAT_DEPTH32F:        return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        default:                                  return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}

//==============================================================================

xbool sdleng_IsDepthFormat( rtarget_format Format )
{
    return (Format == RTARGET_FORMAT_DEPTH_STENCIL) || (Format == RTARGET_FORMAT_DEPTH32F);
}

//==============================================================================

xbool sdleng_IsDepthFormat( vram_texture_format Format )
{
    return (Format == VRAM_TEXTURE_FORMAT_DEPTH_STENCIL) || (Format == VRAM_TEXTURE_FORMAT_DEPTH32F);
}

//==============================================================================

xbool sdleng_HasStencil( rtarget_format Format )
{
    return Format == RTARGET_FORMAT_DEPTH_STENCIL;
}

//==============================================================================

xbool sdleng_HasAlpha( rtarget_format Format )
{
    return (Format == RTARGET_FORMAT_RGBA8)   ||
           (Format == RTARGET_FORMAT_BGRA8)   ||
           (Format == RTARGET_FORMAT_RGBA16F) ||
           (Format == RTARGET_FORMAT_RGBA32F) ||
           (Format == RTARGET_FORMAT_RGB10A2);
}

//==============================================================================

xbool sdleng_AcquireTransientCommandBuffer( SDL_GPUCommandBuffer*& pCommandBuffer,
                                             xbool&                 bOwned,
                                             const char*            pSubsystem )
{
    pCommandBuffer = NULL;
    bOwned = FALSE;

    if( !g_pSDLGPUDevice || sdleng_InRenderPass() )
        return FALSE;

    pCommandBuffer = sdleng_GetCommandBuffer();
    if( pCommandBuffer )
        return TRUE;

    pCommandBuffer = SDL_AcquireGPUCommandBuffer( g_pSDLGPUDevice );
    if( !pCommandBuffer )
    {
        sdleng_LogError( pSubsystem, "SDL_AcquireGPUCommandBuffer" );
        return FALSE;
    }

    bOwned = TRUE;
    return TRUE;
}

//==============================================================================

xbool sdleng_SubmitTransientCommandBuffer( SDL_GPUCommandBuffer* pCommandBuffer,
                                            xbool                 bOwned,
                                            const char*           pSubsystem )
{
    if( !bOwned )
        return TRUE;

    if( !pCommandBuffer || !SDL_SubmitGPUCommandBuffer( pCommandBuffer ) )
    {
        sdleng_LogError( pSubsystem, "SDL_SubmitGPUCommandBuffer" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void sdleng_CancelTransientCommandBuffer( SDL_GPUCommandBuffer* pCommandBuffer, xbool bOwned )
{
    if( bOwned && pCommandBuffer )
        SDL_CancelGPUCommandBuffer( pCommandBuffer );
}

#endif
