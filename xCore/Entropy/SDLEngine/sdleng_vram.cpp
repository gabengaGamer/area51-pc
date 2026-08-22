//==============================================================================
//
//  sdleng_vram.cpp
//
//==============================================================================

#include "x_target.hpp"

#if defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static vram_texture_backend* s_pTextureList = NULL;
static u32                   s_TextureCount = 0;

//==============================================================================
//  HELPERS
//==============================================================================

static
u32 sdlvram_MaxU32( u32 A, u32 B )
{
    return (A > B) ? A : B;
}

//==============================================================================

static
u32 sdlvram_MipExtent( u32 Extent, u32 MipLevel )
{
    Extent >>= MipLevel;
    return Extent ? Extent : 1;
}

//==============================================================================

static
xbool sdlvram_ToSDLTextureType( vram_texture_type Type, SDL_GPUTextureType& SDLType )
{
    switch( Type )
    {
        case VRAM_TEXTURE_TYPE_2D:         SDLType = SDL_GPU_TEXTURETYPE_2D;         return TRUE;
        case VRAM_TEXTURE_TYPE_2D_ARRAY:   SDLType = SDL_GPU_TEXTURETYPE_2D_ARRAY;   return TRUE;
        case VRAM_TEXTURE_TYPE_3D:         SDLType = SDL_GPU_TEXTURETYPE_3D;         return TRUE;
        case VRAM_TEXTURE_TYPE_CUBE:       SDLType = SDL_GPU_TEXTURETYPE_CUBE;       return TRUE;
        case VRAM_TEXTURE_TYPE_CUBE_ARRAY: SDLType = SDL_GPU_TEXTURETYPE_CUBE_ARRAY; return TRUE;
        default: return FALSE;
    }
}

//==============================================================================

static
SDL_GPUTextureUsageFlags sdlvram_ToSDLUsageFlags( u32 UsageFlags )
{
    SDL_GPUTextureUsageFlags SDLFlags = 0;

    if( UsageFlags & VRAM_TEXTURE_USAGE_SAMPLED )
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_SAMPLER;

    if( UsageFlags & VRAM_TEXTURE_USAGE_COLOR_TARGET )
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    if( UsageFlags & VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET )
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

    if( UsageFlags & VRAM_TEXTURE_USAGE_GRAPHICS_STORAGE_READ )
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ;

    if( UsageFlags & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ )
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;

    if( UsageFlags & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE )
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;

    if( UsageFlags & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE )
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE;

    return SDLFlags;
}

//==============================================================================

static
xbool sdlvram_IsCompressedFormat( vram_texture_format Format )
{
    return (Format == VRAM_TEXTURE_FORMAT_BC1_RGBA) ||
           (Format == VRAM_TEXTURE_FORMAT_BC2_RGBA) ||
           (Format == VRAM_TEXTURE_FORMAT_BC3_RGBA);
}

//==============================================================================

static
xbool sdlvram_CanGenerateMipmaps( const vram_texture_desc& Desc )
{
    return ((Desc.UsageFlags & VRAM_TEXTURE_USAGE_COLOR_TARGET) != 0) &&
           !sdlvram_IsCompressedFormat( Desc.Format ) &&
           !sdleng_IsDepthFormat( Desc.Format );
}

//==============================================================================

static
u32 sdlvram_GetBlockSize( vram_texture_format Format )
{
    switch( Format )
    {
        case VRAM_TEXTURE_FORMAT_BC1_RGBA: return 8;
        case VRAM_TEXTURE_FORMAT_BC2_RGBA: return 16;
        case VRAM_TEXTURE_FORMAT_BC3_RGBA: return 16;
        default:                           return 0;
    }
}

//==============================================================================

static
u32 sdlvram_GetBytesPerPixel( vram_texture_format Format )
{
    switch( Format )
    {
        case VRAM_TEXTURE_FORMAT_R8:       return 1;
        case VRAM_TEXTURE_FORMAT_B5G6R5:   return 2;
        case VRAM_TEXTURE_FORMAT_B5G5R5A1: return 2;
        case VRAM_TEXTURE_FORMAT_B4G4R4A4: return 2;
        case VRAM_TEXTURE_FORMAT_RG16F:    return 4;
        case VRAM_TEXTURE_FORMAT_R32F:     return 4;
        case VRAM_TEXTURE_FORMAT_RGBA8:    return 4;
        case VRAM_TEXTURE_FORMAT_BGRA8:    return 4;
        case VRAM_TEXTURE_FORMAT_RGB10A2:  return 4;
        case VRAM_TEXTURE_FORMAT_RGBA16F:  return 8;
        case VRAM_TEXTURE_FORMAT_RGBA32F:  return 16;
        default:                           return 0;
    }
}

//==============================================================================

static
u32 sdlvram_CalcRowPitch( vram_texture_format Format, u32 Width )
{
    if( sdlvram_IsCompressedFormat( Format ) )
    {
        const u32 BlocksWide = sdlvram_MaxU32( 1, (Width + 3) / 4 );
        return BlocksWide * sdlvram_GetBlockSize( Format );
    }

    return Width * sdlvram_GetBytesPerPixel( Format );
}

//==============================================================================

static
u32 sdlvram_CalcSlicePitch( vram_texture_format Format, u32 Width, u32 Height )
{
    const u32 RowPitch = sdlvram_CalcRowPitch( Format, Width );

    if( sdlvram_IsCompressedFormat( Format ) )
        return RowPitch * sdlvram_MaxU32( 1, (Height + 3) / 4 );

    return RowPitch * Height;
}

//==============================================================================

static
xbool sdlvram_ValidateTextureDesc( const vram_texture_desc& Desc,
                                   SDL_GPUTextureType&      TextureType,
                                   SDL_GPUTextureFormat&    Format,
                                   SDL_GPUSampleCount&      SampleCount,
                                   SDL_GPUTextureUsageFlags& UsageFlags )
{
    if( !g_pSDLGPUDevice || (Desc.Width == 0) || (Desc.Height == 0) ||
        (Desc.Depth == 0) || (Desc.LayerCount == 0) || (Desc.MipCount == 0) )
    {
        return FALSE;
    }

    if( !sdlvram_ToSDLTextureType( Desc.Type, TextureType ) ||
        !sdleng_ToSDLSampleCount( Desc.SampleCount, SampleCount ) )
    {
        return FALSE;
    }

    Format = sdleng_ToSDLTextureFormat( Desc.Format );
    if( Format == SDL_GPU_TEXTUREFORMAT_INVALID )
        return FALSE;

    const u32 KnownUsageFlags = VRAM_TEXTURE_USAGE_SAMPLED                    |
                                VRAM_TEXTURE_USAGE_COLOR_TARGET               |
                                VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET       |
                                VRAM_TEXTURE_USAGE_GRAPHICS_STORAGE_READ      |
                                VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ       |
                                VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE      |
                                VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE;
    if( (Desc.UsageFlags == 0) || (Desc.UsageFlags & ~KnownUsageFlags) )
        return FALSE;

    if( (Desc.UsageFlags & VRAM_TEXTURE_USAGE_SAMPLED) &&
        (Desc.UsageFlags & VRAM_TEXTURE_USAGE_GRAPHICS_STORAGE_READ) )
    {
        return FALSE;
    }

    if( sdleng_IsDepthFormat( Desc.Format ) )
    {
        const u32 AllowedDepthUsage = VRAM_TEXTURE_USAGE_SAMPLED |
                                      VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET;
        if( Desc.UsageFlags & ~AllowedDepthUsage )
            return FALSE;
    }

    switch( Desc.Type )
    {
        case VRAM_TEXTURE_TYPE_2D:
            if( (Desc.Depth != 1) || (Desc.LayerCount != 1) )
                return FALSE;
            break;

        case VRAM_TEXTURE_TYPE_2D_ARRAY:
            if( Desc.Depth != 1 )
                return FALSE;
            break;

        case VRAM_TEXTURE_TYPE_3D:
            if( (Desc.LayerCount != 1) ||
                (Desc.UsageFlags & VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET) )
            {
                return FALSE;
            }
            break;

        case VRAM_TEXTURE_TYPE_CUBE:
            if( (Desc.Width != Desc.Height) || (Desc.Depth != 1) || (Desc.LayerCount != 6) )
                return FALSE;
            break;

        case VRAM_TEXTURE_TYPE_CUBE_ARRAY:
            if( (Desc.Width != Desc.Height) || (Desc.Depth != 1) || ((Desc.LayerCount % 6) != 0) )
                return FALSE;
            break;

        default:
            return FALSE;
    }

    const u32 MaxMipCount = vram_CalcMipCount( Desc.Width,
                                                Desc.Height,
                                                (Desc.Type == VRAM_TEXTURE_TYPE_3D) ? Desc.Depth : 1 );
    if( Desc.MipCount > MaxMipCount )
        return FALSE;

    if( Desc.SampleCount > 1 )
    {
        const u32 InvalidMSAAUsage = VRAM_TEXTURE_USAGE_SAMPLED                    |
                                     VRAM_TEXTURE_USAGE_GRAPHICS_STORAGE_READ      |
                                     VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ       |
                                     VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE      |
                                     VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE;
        if( (Desc.Type != VRAM_TEXTURE_TYPE_2D) || (Desc.MipCount != 1) ||
            (Desc.UsageFlags & InvalidMSAAUsage) ||
            !(Desc.UsageFlags & (VRAM_TEXTURE_USAGE_COLOR_TARGET |
                                 VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET)) )
        {
            return FALSE;
        }
    }

    UsageFlags = sdlvram_ToSDLUsageFlags( Desc.UsageFlags );
    if( !UsageFlags ||
        !SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice, Format, TextureType, UsageFlags ) )
    {
        return FALSE;
    }

    if( !SDL_GPUTextureSupportsSampleCount( g_pSDLGPUDevice, Format, SampleCount ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
vram_texture_format sdlvram_FromBitmapFormat( xbitmap::format Format )
{
    switch( Format )
    {
        case xbitmap::FMT_32_ARGB_8888: return VRAM_TEXTURE_FORMAT_BGRA8;
        case xbitmap::FMT_32_URGB_8888: return VRAM_TEXTURE_FORMAT_BGRA8;
        case xbitmap::FMT_16_ARGB_4444: return VRAM_TEXTURE_FORMAT_B4G4R4A4;
        case xbitmap::FMT_16_ARGB_1555: return VRAM_TEXTURE_FORMAT_B5G5R5A1;
        case xbitmap::FMT_16_URGB_1555: return VRAM_TEXTURE_FORMAT_B5G5R5A1;
        case xbitmap::FMT_16_RGB_565:   return VRAM_TEXTURE_FORMAT_B5G6R5;
        case xbitmap::FMT_DXT1:         return VRAM_TEXTURE_FORMAT_BC1_RGBA;
        case xbitmap::FMT_DXT3:         return VRAM_TEXTURE_FORMAT_BC2_RGBA;
        case xbitmap::FMT_DXT5:         return VRAM_TEXTURE_FORMAT_BC3_RGBA;
        default:                        return VRAM_TEXTURE_FORMAT_COUNT;
    }
}

//==============================================================================

static
void sdlvram_ReleaseBackend( vram_texture_backend* pBackend )
{
    if( !pBackend )
        return;

    if( pBackend->pTexture && g_pSDLGPUDevice )
        SDL_ReleaseGPUTexture( g_pSDLGPUDevice, pBackend->pTexture );

    pBackend->pTexture                 = NULL;
    pBackend->Resource.pBackend        = NULL;
    pBackend->ResourceBackend.Kind     = SDLENG_SHADER_RESOURCE_NONE;
    pBackend->ResourceBackend.pTexture = NULL;
    pBackend->ResourceBackend.pBuffer  = NULL;
}

//==============================================================================

static
xbool sdlvram_FillRegionFromTexture( const vram_texture& Texture,
                                     vram_texture_region Region,
                                     vram_texture_region& OutRegion )
{
    if( !Texture.pBackend )
        return FALSE;

    if( Region.MipLevel >= Texture.Desc.MipCount )
        return FALSE;

    OutRegion = Region;

    if( OutRegion.Width == 0 )
        OutRegion.Width = sdlvram_MipExtent( Texture.Desc.Width, OutRegion.MipLevel );

    if( OutRegion.Height == 0 )
        OutRegion.Height = sdlvram_MipExtent( Texture.Desc.Height, OutRegion.MipLevel );

    if( OutRegion.Depth == 0 )
        OutRegion.Depth = (Texture.Desc.Type == VRAM_TEXTURE_TYPE_3D)
                        ? sdlvram_MipExtent( Texture.Desc.Depth, OutRegion.MipLevel )
                        : 1;

    const u32 MipWidth  = sdlvram_MipExtent( Texture.Desc.Width,  OutRegion.MipLevel );
    const u32 MipHeight = sdlvram_MipExtent( Texture.Desc.Height, OutRegion.MipLevel );
    const u32 MipDepth  = (Texture.Desc.Type == VRAM_TEXTURE_TYPE_3D)
                        ? sdlvram_MipExtent( Texture.Desc.Depth, OutRegion.MipLevel )
                        : 1;

    if( (OutRegion.Width == 0) || (OutRegion.Height == 0) || (OutRegion.Depth == 0) ||
        (OutRegion.X >= MipWidth) || (OutRegion.Y >= MipHeight) || (OutRegion.Z >= MipDepth) ||
        (OutRegion.Width > (MipWidth - OutRegion.X)) ||
        (OutRegion.Height > (MipHeight - OutRegion.Y)) ||
        (OutRegion.Depth > (MipDepth - OutRegion.Z)) )
    {
        return FALSE;
    }

    if( Texture.Desc.Type == VRAM_TEXTURE_TYPE_3D )
    {
        if( OutRegion.Layer != 0 )
            return FALSE;
    }
    else if( OutRegion.Layer >= Texture.Desc.LayerCount )
    {
        return FALSE;
    }

    if( sdlvram_IsCompressedFormat( Texture.Desc.Format ) )
    {
        const xbool bWidthReachesEdge  = (OutRegion.X + OutRegion.Width) == MipWidth;
        const xbool bHeightReachesEdge = (OutRegion.Y + OutRegion.Height) == MipHeight;
        if( (OutRegion.X & 3) || (OutRegion.Y & 3) ||
            (!bWidthReachesEdge && (OutRegion.Width & 3)) ||
            (!bHeightReachesEdge && (OutRegion.Height & 3)) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

static
xbool sdlvram_SubmitUpload( vram_texture& Texture, const vram_texture_upload_desc& Upload )
{
    vram_texture_region Region;
    if( !sdlvram_FillRegionFromTexture( Texture, Upload.Region, Region ) )
        return FALSE;

    if( !g_pSDLGPUDevice || !Texture.pBackend || !Texture.pBackend->pTexture || !Upload.pData )
        return FALSE;

    const xbool bCompressed = sdlvram_IsCompressedFormat( Texture.Desc.Format );
    if( bCompressed && (Upload.RowPitch || Upload.SlicePitch) )
    {
        x_DebugMsg( "SDLVRAM: custom row/slice pitch for compressed uploads is not supported yet\n" );
        return FALSE;
    }

    const xbool bGenerateMips = Upload.bGenerateMips && (Texture.Desc.MipCount > 1);
    if( bGenerateMips && !sdlvram_CanGenerateMipmaps( Texture.Desc ) )
    {
        x_DebugMsg( "SDLVRAM: texture was not created for GPU mip generation\n" );
        return FALSE;
    }

    const u32 RowPitch   = Upload.RowPitch   ? Upload.RowPitch   : sdlvram_CalcRowPitch  ( Texture.Desc.Format, Region.Width );
    const u32 SlicePitch = Upload.SlicePitch ? Upload.SlicePitch : sdlvram_CalcSlicePitch( Texture.Desc.Format, Region.Width, Region.Height );
    const u64 RequiredSize = (u64)SlicePitch * Region.Depth;
    if( RequiredSize > 0xffffffffULL )
        return FALSE;

    const u32 UploadSize = Upload.Size ? Upload.Size : (u32)RequiredSize;

    if( (RowPitch == 0) || (SlicePitch == 0) || (UploadSize < RequiredSize) )
        return FALSE;

    if( !bCompressed )
    {
        const u32 BytesPerPixel = sdlvram_GetBytesPerPixel( Texture.Desc.Format );
        const u64 MinimumRowPitch = (u64)Region.Width * BytesPerPixel;
        const u64 MinimumSlicePitch = (u64)RowPitch * Region.Height;
        if( !BytesPerPixel || (RowPitch < MinimumRowPitch) ||
            (RowPitch % BytesPerPixel) || (SlicePitch < MinimumSlicePitch) )
        {
            return FALSE;
        }
    }

    SDL_GPUTransferBufferCreateInfo TransferDesc;
    x_memset( &TransferDesc, 0, sizeof(TransferDesc) );
    TransferDesc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    TransferDesc.size  = UploadSize;

    SDL_GPUTransferBuffer* pTransferBuffer = SDL_CreateGPUTransferBuffer( g_pSDLGPUDevice, &TransferDesc );
    if( !pTransferBuffer )
    {
        sdleng_LogError( "SDLVRAM", "SDL_CreateGPUTransferBuffer" );
        return FALSE;
    }

    void* pMapped = SDL_MapGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer, Upload.bCycle ? true : false );
    if( !pMapped )
    {
        sdleng_LogError( "SDLVRAM", "SDL_MapGPUTransferBuffer" );
        SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );
        return FALSE;
    }

    x_memcpy( pMapped, Upload.pData, UploadSize );
    SDL_UnmapGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );

    if( sdleng_InRenderPass() )
    {
        x_DebugMsg( "SDLVRAM: texture upload cannot run inside a render pass\n" );
        SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );
        return FALSE;
    }

    SDL_GPUCommandBuffer* pCommandBuffer = NULL;
    xbool bOwnCommandBuffer = FALSE;
    if( !sdleng_AcquireTransientCommandBuffer( pCommandBuffer,
                                                bOwnCommandBuffer,
                                                "SDLVRAM" ) )
    {
        SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );
        return FALSE;
    }

    SDL_GPUCopyPass* pCopyPass = SDL_BeginGPUCopyPass( pCommandBuffer );
    if( !pCopyPass )
    {
        sdleng_LogError( "SDLVRAM", "SDL_BeginGPUCopyPass" );
        sdleng_CancelTransientCommandBuffer( pCommandBuffer, bOwnCommandBuffer );
        SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );
        return FALSE;
    }

    SDL_GPUTextureTransferInfo Source;
    x_memset( &Source, 0, sizeof(Source) );
    Source.transfer_buffer = pTransferBuffer;

    if( !bCompressed && Upload.RowPitch )
        Source.pixels_per_row = RowPitch / sdlvram_GetBytesPerPixel( Texture.Desc.Format );

    if( !bCompressed && Upload.SlicePitch && RowPitch )
        Source.rows_per_layer = SlicePitch / RowPitch;

    SDL_GPUTextureRegion Destination;
    x_memset( &Destination, 0, sizeof(Destination) );
    Destination.texture = Texture.pBackend->pTexture;
    Destination.mip_level = Region.MipLevel;
    Destination.layer = Region.Layer;
    Destination.x = Region.X;
    Destination.y = Region.Y;
    Destination.z = Region.Z;
    Destination.w = Region.Width;
    Destination.h = Region.Height;
    Destination.d = Region.Depth;

    SDL_UploadToGPUTexture( pCopyPass, &Source, &Destination, Upload.bCycle ? true : false );
    SDL_EndGPUCopyPass( pCopyPass );

    if( bGenerateMips )
        SDL_GenerateMipmapsForGPUTexture( pCommandBuffer, Texture.pBackend->pTexture );

    if( !sdleng_SubmitTransientCommandBuffer( pCommandBuffer,
                                               bOwnCommandBuffer,
                                               "SDLVRAM" ) )
    {
        SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );
        return FALSE;
    }

    SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );
    return TRUE;
}

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

void vram_Init( void )
{
    s_pTextureList = NULL;
    s_TextureCount = 0;
}

//==============================================================================

void vram_Kill( void )
{
    while( s_pTextureList )
    {
        vram_texture_backend* pBackend = s_pTextureList;
        sdleng_UnlinkBackend( s_pTextureList, pBackend, s_TextureCount );
        sdlvram_ReleaseBackend( pBackend );

        if( pBackend->pOwner )
        {
            pBackend->pOwner->Desc     = vram_texture_desc();
            pBackend->pOwner->pBackend = NULL;
            pBackend->pOwner           = NULL;
        }

        delete pBackend;
    }
}

//==============================================================================
//  TEXTURE OBJECTS
//==============================================================================

xbool vram_CreateTexture( vram_texture& Texture, const vram_texture_desc& Desc )
{
    vram_DestroyTexture( Texture );

    SDL_GPUTextureType       TextureType;
    SDL_GPUTextureFormat     Format;
    SDL_GPUSampleCount       SampleCount;
    SDL_GPUTextureUsageFlags UsageFlags;
    if( !sdlvram_ValidateTextureDesc( Desc, TextureType, Format, SampleCount, UsageFlags ) )
    {
        x_DebugMsg( "SDLVRAM: invalid or unsupported texture descriptor for '%s'\n",
                    Desc.pDebugName ? Desc.pDebugName : "unnamed" );
        return FALSE;
    }

    SDL_GPUTextureCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof(CreateInfo) );
    CreateInfo.type                 = TextureType;
    CreateInfo.format               = Format;
    CreateInfo.usage                = UsageFlags;
    CreateInfo.width                = Desc.Width;
    CreateInfo.height               = Desc.Height;
    CreateInfo.layer_count_or_depth = (Desc.Type == VRAM_TEXTURE_TYPE_3D) ? Desc.Depth : Desc.LayerCount;
    CreateInfo.num_levels           = Desc.MipCount;
    CreateInfo.sample_count         = SampleCount;

    SDL_GPUTexture* pTexture = SDL_CreateGPUTexture( g_pSDLGPUDevice, &CreateInfo );
    if( !pTexture )
    {
        sdleng_LogError( "SDLVRAM", "SDL_CreateGPUTexture" );
        return FALSE;
    }

    if( Desc.pDebugName )
        SDL_SetGPUTextureName( g_pSDLGPUDevice, pTexture, Desc.pDebugName );

    vram_texture_backend* pBackend = new vram_texture_backend;
    if( !pBackend )
    {
        SDL_ReleaseGPUTexture( g_pSDLGPUDevice, pTexture );
        return FALSE;
    }

    pBackend->pOwner                  = &Texture;
    pBackend->pTexture                = pTexture;
    pBackend->Resource.pBackend       = &pBackend->ResourceBackend;
    pBackend->ResourceBackend.Kind    = SDLENG_SHADER_RESOURCE_TEXTURE;
    pBackend->ResourceBackend.pTexture= pTexture;
    pBackend->ResourceBackend.pBuffer = NULL;

    Texture.Desc     = Desc;
    Texture.pBackend = pBackend;

    sdleng_LinkBackend( s_pTextureList, pBackend, s_TextureCount );
    return TRUE;
}

//==============================================================================

xbool vram_CreateTexture( vram_texture& Texture, const xbitmap& Bitmap, xbool bGenerateMips, const char* pDebugName )
{
    vram_texture_format Format = sdlvram_FromBitmapFormat( Bitmap.GetFormat() );
    if( Format == VRAM_TEXTURE_FORMAT_COUNT )
        return FALSE;

    if( bGenerateMips && sdlvram_IsCompressedFormat( Format ) )
    {
        x_DebugMsg( "SDLVRAM: compressed textures use uploaded mip data, not GPU-generated mipmaps\n" );
        bGenerateMips = FALSE;
    }

    vram_texture_desc Desc;
    Desc.Type       = VRAM_TEXTURE_TYPE_2D;
    Desc.Width      = Bitmap.GetWidth();
    Desc.Height     = Bitmap.GetHeight();
    Desc.Depth      = 1;
    Desc.LayerCount = 1;
    Desc.MipCount   = bGenerateMips ? vram_CalcMipCount( Desc.Width, Desc.Height ) : 1;
    Desc.Format     = Format;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.SampleCount= 1;
    Desc.pDebugName = pDebugName;

    if( bGenerateMips )
        Desc.UsageFlags |= VRAM_TEXTURE_USAGE_COLOR_TARGET;

    if( !vram_CreateTexture( Texture, Desc ) )
        return FALSE;

    if( !vram_UploadTexture( Texture, Bitmap, bGenerateMips ) )
    {
        vram_DestroyTexture( Texture );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool vram_CreateTextureCube( vram_texture& Texture, const xbitmap* pFaces, s32 nFaces, xbool bGenerateMips, const char* pDebugName )
{
    if( !pFaces || (nFaces != 6) )
        return FALSE;

    vram_texture_format Format = sdlvram_FromBitmapFormat( pFaces[0].GetFormat() );
    if( Format == VRAM_TEXTURE_FORMAT_COUNT )
        return FALSE;

    if( bGenerateMips && sdlvram_IsCompressedFormat( Format ) )
    {
        x_DebugMsg( "SDLVRAM: compressed cube textures use uploaded mip data, not GPU-generated mipmaps\n" );
        bGenerateMips = FALSE;
    }

    for( s32 i = 1; i < nFaces; i++ )
    {
        if( (pFaces[i].GetWidth()  != pFaces[0].GetWidth()) ||
            (pFaces[i].GetHeight() != pFaces[0].GetHeight()) ||
            (pFaces[i].GetFormat() != pFaces[0].GetFormat()) )
        {
            return FALSE;
        }
    }

    vram_texture_desc Desc;
    Desc.Type       = VRAM_TEXTURE_TYPE_CUBE;
    Desc.Width      = pFaces[0].GetWidth();
    Desc.Height     = pFaces[0].GetHeight();
    Desc.Depth      = 1;
    Desc.LayerCount = 6;
    Desc.MipCount   = bGenerateMips ? vram_CalcMipCount( Desc.Width, Desc.Height ) : 1;
    Desc.Format     = Format;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.SampleCount= 1;
    Desc.pDebugName = pDebugName;

    if( bGenerateMips )
        Desc.UsageFlags |= VRAM_TEXTURE_USAGE_COLOR_TARGET;

    if( !vram_CreateTexture( Texture, Desc ) )
        return FALSE;

    if( !vram_UploadTextureCube( Texture, pFaces, nFaces, bGenerateMips ) )
    {
        vram_DestroyTexture( Texture );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void vram_DestroyTexture( vram_texture& Texture )
{
    vram_texture_backend* pBackend = Texture.pBackend;
    if( !pBackend )
        return;

    sdleng_UnlinkBackend( s_pTextureList, pBackend, s_TextureCount );
    sdlvram_ReleaseBackend( pBackend );
    delete pBackend;

    Texture.Desc     = vram_texture_desc();
    Texture.pBackend = NULL;
}

//==============================================================================
//  UPLOADS
//==============================================================================

xbool vram_UploadTexture( vram_texture& Texture, const vram_texture_upload_desc& Upload )
{
    return sdlvram_SubmitUpload( Texture, Upload );
}

//==============================================================================

xbool vram_UploadTexture( vram_texture& Texture, const xbitmap& Bitmap, xbool bGenerateMips )
{
    if( !Texture.pBackend )
        return FALSE;

    vram_texture_upload_desc Upload;
    Upload.Region.Width  = Bitmap.GetWidth();
    Upload.Region.Height = Bitmap.GetHeight();
    Upload.Region.Depth  = 1;
    Upload.pData         = Bitmap.GetPixelData();
    Upload.bGenerateMips = bGenerateMips;

    if( sdlvram_IsCompressedFormat( Texture.Desc.Format ) )
    {
        Upload.Size = sdlvram_CalcSlicePitch( Texture.Desc.Format, Bitmap.GetWidth(), Bitmap.GetHeight() );
    }
    else
    {
        Upload.RowPitch = (Bitmap.GetPWidth() * Bitmap.GetFormatInfo().BPP) / 8;
        Upload.Size     = Upload.RowPitch * Bitmap.GetHeight();
    }

    return vram_UploadTexture( Texture, Upload );
}

//==============================================================================

xbool vram_UploadTextureCube( vram_texture& Texture, const xbitmap* pFaces, s32 nFaces, xbool bGenerateMips )
{
    if( !Texture.pBackend || !pFaces || (nFaces != 6) )
        return FALSE;

    for( s32 i = 0; i < nFaces; i++ )
    {
        vram_texture_upload_desc Upload;
        Upload.Region.Layer  = i;
        Upload.Region.Width  = pFaces[i].GetWidth();
        Upload.Region.Height = pFaces[i].GetHeight();
        Upload.Region.Depth  = 1;
        Upload.pData         = pFaces[i].GetPixelData();
        Upload.bGenerateMips = FALSE;

        if( sdlvram_IsCompressedFormat( Texture.Desc.Format ) )
        {
            Upload.Size = sdlvram_CalcSlicePitch( Texture.Desc.Format, pFaces[i].GetWidth(), pFaces[i].GetHeight() );
        }
        else
        {
            Upload.RowPitch = (pFaces[i].GetPWidth() * pFaces[i].GetFormatInfo().BPP) / 8;
            Upload.Size     = Upload.RowPitch * pFaces[i].GetHeight();
        }

        if( !vram_UploadTexture( Texture, Upload ) )
            return FALSE;
    }

    if( bGenerateMips )
        return vram_GenerateMipmaps( Texture );

    return TRUE;
}

//==============================================================================

xbool vram_GenerateMipmaps( vram_texture& Texture )
{
    if( !g_pSDLGPUDevice || !Texture.pBackend || !Texture.pBackend->pTexture )
        return FALSE;

    if( Texture.Desc.MipCount <= 1 )
        return TRUE;

    if( !sdlvram_CanGenerateMipmaps( Texture.Desc ) )
    {
        x_DebugMsg( "SDLVRAM: texture was not created for GPU mip generation\n" );
        return FALSE;
    }

    if( sdleng_InRenderPass() )
    {
        x_DebugMsg( "SDLVRAM: mip generation cannot run inside a render pass\n" );
        return FALSE;
    }

    SDL_GPUCommandBuffer* pCommandBuffer = NULL;
    xbool bOwnCommandBuffer = FALSE;
    if( !sdleng_AcquireTransientCommandBuffer( pCommandBuffer,
                                                bOwnCommandBuffer,
                                                "SDLVRAM" ) )
        return FALSE;

    SDL_GenerateMipmapsForGPUTexture( pCommandBuffer, Texture.pBackend->pTexture );

    if( !sdleng_SubmitTransientCommandBuffer( pCommandBuffer,
                                               bOwnCommandBuffer,
                                               "SDLVRAM" ) )
        return FALSE;

    return TRUE;
}

//==============================================================================
//  RESOURCE ACCESS
//==============================================================================

xbool vram_IsValid( const vram_texture& Texture )
{
    return (Texture.pBackend != NULL) && (Texture.pBackend->pTexture != NULL);
}

//==============================================================================

const vram_texture_desc* vram_GetDesc( const vram_texture& Texture )
{
    return vram_IsValid( Texture ) ? &Texture.Desc : NULL;
}

//==============================================================================

const shader_resource* vram_GetShaderResource( const vram_texture& Texture )
{
    if( !vram_IsValid( Texture ) )
        return NULL;

    return &Texture.pBackend->Resource;
}

//==============================================================================
//  UTILITY
//==============================================================================

u32 vram_CalcMipCount( u32 Width, u32 Height, u32 Depth )
{
    u32 MipCount = 1;
    u32 Extent   = sdlvram_MaxU32( Width, sdlvram_MaxU32( Height, Depth ) );

    while( Extent > 1 )
    {
        Extent >>= 1;
        MipCount++;
    }

    return MipCount;
}

//==============================================================================

u32 vram_GetNTextures( void )
{
    return s_TextureCount;
}

//==============================================================================

void vram_PrintStats( void )
{
    x_printfxy( 0, 7, "NTextures:   %d", s_TextureCount );
}

//==============================================================================

void vram_SanityCheck( void )
{
}

//==============================================================================
//  SDL BACKEND ACCESSORS
//==============================================================================

SDL_GPUTexture* sdleng_GetGPUTexture( const shader_resource* pResource )
{
    if( !pResource || !pResource->pBackend )
        return NULL;

    if( pResource->pBackend->Kind != SDLENG_SHADER_RESOURCE_TEXTURE )
        return NULL;

    return pResource->pBackend->pTexture;
}

//==============================================================================

SDL_GPUBuffer* sdleng_GetGPUBuffer( const shader_resource* pResource )
{
    if( !pResource || !pResource->pBackend )
        return NULL;

    if( pResource->pBackend->Kind != SDLENG_SHADER_RESOURCE_BUFFER )
        return NULL;

    return pResource->pBackend->pBuffer;
}

//==============================================================================
#endif // defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
