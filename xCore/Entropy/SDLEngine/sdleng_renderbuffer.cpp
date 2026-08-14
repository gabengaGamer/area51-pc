//==============================================================================
//
//  sdleng_renderbuffer.cpp
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

static rbuffer_backend* s_pBufferList = NULL;
static u32              s_BufferCount = 0;

struct sdlbuffer_vertex_binding_cache
{
    SDL_GPUBuffer* pBuffer;
    u32            Offset;
};

static xarray<sdlbuffer_vertex_binding_cache> s_VertexBindings;
static SDL_GPUBuffer*                         s_pIndexBuffer = NULL;
static u32                                    s_IndexOffset = 0;
static rbuffer_index_format                   s_IndexFormat = RBUFFER_INDEX_FORMAT_U16;

//==============================================================================
//  HELPERS
//==============================================================================

static
void sdlbuffer_LogSDLError( const char* pContext )
{
    const char* pError = SDL_GetError();
    if( !pError || !pError[0] )
        pError = "unknown SDL error";

    x_DebugMsg( "SDLBuffer: %s failed: %s\n", pContext, pError );
}

//==============================================================================

static
void sdlbuffer_InvalidateBindings( SDL_GPUBuffer* pBuffer )
{
    for( s32 i = 0; i < s_VertexBindings.GetCount(); ++i )
    {
        if( s_VertexBindings[i].pBuffer == pBuffer )
            s_VertexBindings[i].pBuffer = NULL;
    }

    if( s_pIndexBuffer == pBuffer )
        s_pIndexBuffer = NULL;
}

//==============================================================================

static
xbool sdlbuffer_HasAnyFlag( u32 Flags, u32 TestFlags )
{
    return (Flags & TestFlags) != 0;
}

//==============================================================================

static
u32 sdlbuffer_CountFlags( u32 Flags )
{
    u32 Count = 0;
    while( Flags )
    {
        Count += (Flags & 1);
        Flags >>= 1;
    }
    return Count;
}

//==============================================================================

static
xbool sdlbuffer_IsStorageUsage( u32 UsageFlags )
{
    const u32 StorageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ |
                             RBUFFER_USAGE_COMPUTE_STORAGE_READ  |
                             RBUFFER_USAGE_COMPUTE_STORAGE_WRITE;
    return sdlbuffer_HasAnyFlag( UsageFlags, StorageFlags );
}

//==============================================================================

static
xbool sdlbuffer_ValidateUsageFlags( u32 UsageFlags )
{
    const u32 KnownFlags = RBUFFER_USAGE_VERTEX                |
                           RBUFFER_USAGE_INDEX                 |
                           RBUFFER_USAGE_INDIRECT              |
                           RBUFFER_USAGE_GRAPHICS_STORAGE_READ |
                           RBUFFER_USAGE_COMPUTE_STORAGE_READ  |
                           RBUFFER_USAGE_COMPUTE_STORAGE_WRITE;

    if( (UsageFlags == 0) || (UsageFlags & ~KnownFlags) )
        return FALSE;

    const u32 DrawInputFlags = UsageFlags & (RBUFFER_USAGE_VERTEX |
                                             RBUFFER_USAGE_INDEX  |
                                             RBUFFER_USAGE_INDIRECT);
    if( sdlbuffer_CountFlags( DrawInputFlags ) > 1 )
        return FALSE;

    return TRUE;
}

//==============================================================================

static
xbool sdlbuffer_ToSDLUsageFlags( u32 UsageFlags, SDL_GPUBufferUsageFlags& SDLFlags )
{
    if( !sdlbuffer_ValidateUsageFlags( UsageFlags ) )
        return FALSE;

    SDLFlags = 0;

    if( UsageFlags & RBUFFER_USAGE_VERTEX )
        SDLFlags |= SDL_GPU_BUFFERUSAGE_VERTEX;

    if( UsageFlags & RBUFFER_USAGE_INDEX )
        SDLFlags |= SDL_GPU_BUFFERUSAGE_INDEX;

    if( UsageFlags & RBUFFER_USAGE_INDIRECT )
        SDLFlags |= SDL_GPU_BUFFERUSAGE_INDIRECT;

    if( UsageFlags & RBUFFER_USAGE_GRAPHICS_STORAGE_READ )
        SDLFlags |= SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;

    if( UsageFlags & RBUFFER_USAGE_COMPUTE_STORAGE_READ )
        SDLFlags |= SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;

    if( UsageFlags & RBUFFER_USAGE_COMPUTE_STORAGE_WRITE )
        SDLFlags |= SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;

    return SDLFlags != 0;
}

//==============================================================================

static
SDL_GPUIndexElementSize sdlbuffer_ToSDLIndexFormat( rbuffer_index_format Format )
{
    switch( Format )
    {
        case RBUFFER_INDEX_FORMAT_U16: return SDL_GPU_INDEXELEMENTSIZE_16BIT;
        case RBUFFER_INDEX_FORMAT_U32: return SDL_GPU_INDEXELEMENTSIZE_32BIT;
        default:                       return SDL_GPU_INDEXELEMENTSIZE_16BIT;
    }
}

//==============================================================================

static
void sdlbuffer_LinkBuffer( rbuffer_backend* pBackend )
{
    pBackend->pPrev = NULL;
    pBackend->pNext = s_pBufferList;

    if( s_pBufferList )
        s_pBufferList->pPrev = pBackend;

    s_pBufferList = pBackend;
    s_BufferCount++;
}

//==============================================================================

static
void sdlbuffer_UnlinkBuffer( rbuffer_backend* pBackend )
{
    if( pBackend->pPrev )
        pBackend->pPrev->pNext = pBackend->pNext;
    else if( s_pBufferList == pBackend )
        s_pBufferList = pBackend->pNext;

    if( pBackend->pNext )
        pBackend->pNext->pPrev = pBackend->pPrev;

    pBackend->pPrev = NULL;
    pBackend->pNext = NULL;

    if( s_BufferCount )
        s_BufferCount--;
}

//==============================================================================

static
void sdlbuffer_ReleaseBackend( rbuffer_backend* pBackend )
{
    if( !pBackend )
        return;

    if( pBackend->pUploadBuffer && g_pSDLGPUDevice )
        SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, pBackend->pUploadBuffer );

    if( pBackend->pBuffer && g_pSDLGPUDevice )
        SDL_ReleaseGPUBuffer( g_pSDLGPUDevice, pBackend->pBuffer );

    pBackend->pUploadBuffer            = NULL;
    pBackend->UploadBufferSize         = 0;
    pBackend->pBuffer                  = NULL;
    pBackend->Resource.pBackend        = NULL;
    pBackend->ResourceBackend.Kind     = SDLENG_SHADER_RESOURCE_NONE;
    pBackend->ResourceBackend.pTexture = NULL;
    pBackend->ResourceBackend.pBuffer  = NULL;
    pBackend->bShaderResource          = FALSE;
}

//==============================================================================

static
xbool sdlbuffer_GetUploadBuffer(rbuffer& Buffer,
    const rbuffer_upload_desc& Upload,
    SDL_GPUTransferBuffer*& pTransferBuffer,
    xbool& bOwnTransferBuffer)
{
    static xprofile_counter UploadCreateMetric =
        x_GetProfiler().RegisterCounter("UploadBufferCreates", "Renderer");

    pTransferBuffer = NULL;
    bOwnTransferBuffer = FALSE;

    SDL_GPUTransferBufferCreateInfo TransferDesc;
    x_memset(&TransferDesc, 0, sizeof(TransferDesc));
    TransferDesc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    TransferDesc.size = Upload.bCycle ? Buffer.Desc.Size : Upload.Size;

    if (!Upload.bCycle)
    {
        pTransferBuffer = SDL_CreateGPUTransferBuffer(g_pSDLGPUDevice, &TransferDesc);
        if (pTransferBuffer)
            UploadCreateMetric.Add();
        bOwnTransferBuffer = TRUE;
        return pTransferBuffer != NULL;
    }

    rbuffer_backend* pBackend = Buffer.pBackend;
    if (pBackend->pUploadBuffer && (pBackend->UploadBufferSize >= Upload.Size))
    {
        pTransferBuffer = pBackend->pUploadBuffer;
        return TRUE;
    }

    SDL_GPUTransferBuffer* pNewBuffer = SDL_CreateGPUTransferBuffer(g_pSDLGPUDevice, &TransferDesc);
    if (!pNewBuffer)
        return FALSE;

    UploadCreateMetric.Add();

    if (pBackend->pUploadBuffer)
        SDL_ReleaseGPUTransferBuffer(g_pSDLGPUDevice, pBackend->pUploadBuffer);

    pBackend->pUploadBuffer = pNewBuffer;
    pBackend->UploadBufferSize = TransferDesc.size;
    pTransferBuffer = pNewBuffer;
    return TRUE;
}

//==============================================================================

static
void sdlbuffer_ReleaseTransientUploadBuffer( SDL_GPUTransferBuffer* pTransferBuffer,
                                             xbool                  bOwnTransferBuffer )
{
    if( bOwnTransferBuffer && pTransferBuffer )
        SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );
}

//==============================================================================

static
xbool sdlbuffer_GetCopyCommandBuffer( SDL_GPUCommandBuffer*& pCommandBuffer, xbool& bOwnCommandBuffer )
{
    if( sdleng_InRenderPass() )
    {
        x_DebugMsg( "SDLBuffer: buffer upload cannot run inside a render pass\n" );
        return FALSE;
    }

    pCommandBuffer    = sdleng_GetCommandBuffer();
    bOwnCommandBuffer = FALSE;

    if( pCommandBuffer )
        return TRUE;

    if( !g_pSDLGPUDevice )
        return FALSE;

    pCommandBuffer = SDL_AcquireGPUCommandBuffer( g_pSDLGPUDevice );
    if( !pCommandBuffer )
    {
        sdlbuffer_LogSDLError( "SDL_AcquireGPUCommandBuffer" );
        return FALSE;
    }

    bOwnCommandBuffer = TRUE;
    return TRUE;
}

//==============================================================================

static
xbool sdlbuffer_SubmitUpload( rbuffer& Buffer, const rbuffer_upload_desc& Upload )
{
    if( !Buffer.pBackend || !Buffer.pBackend->pBuffer || !Upload.pData || (Upload.Size == 0) )
        return FALSE;

    if( !g_pSDLGPUDevice || sdleng_InRenderPass() )
        return FALSE;

    if( Upload.Offset > Buffer.Desc.Size )
        return FALSE;

    if( Upload.Size > (Buffer.Desc.Size - Upload.Offset) )
        return FALSE;

    SDL_GPUTransferBuffer* pTransferBuffer = NULL;
    xbool bOwnTransferBuffer = FALSE;
    if( !sdlbuffer_GetUploadBuffer( Buffer, Upload, pTransferBuffer, bOwnTransferBuffer ) )
    {
        sdlbuffer_LogSDLError( "SDL_CreateGPUTransferBuffer" );
        return FALSE;
    }

    void* pMapped = SDL_MapGPUTransferBuffer( g_pSDLGPUDevice,
                                              pTransferBuffer,
                                              bOwnTransferBuffer ? false : true );
    if( !pMapped )
    {
        sdlbuffer_LogSDLError( "SDL_MapGPUTransferBuffer" );
        sdlbuffer_ReleaseTransientUploadBuffer( pTransferBuffer, bOwnTransferBuffer );
        return FALSE;
    }

    x_memcpy( pMapped, Upload.pData, Upload.Size );
    SDL_UnmapGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );

    SDL_GPUCommandBuffer* pCommandBuffer = NULL;
    xbool bOwnCommandBuffer = FALSE;
    if( !sdlbuffer_GetCopyCommandBuffer( pCommandBuffer, bOwnCommandBuffer ) )
    {
        sdlbuffer_ReleaseTransientUploadBuffer( pTransferBuffer, bOwnTransferBuffer );
        return FALSE;
    }

    SDL_GPUCopyPass* pCopyPass = SDL_BeginGPUCopyPass( pCommandBuffer );
    if( !pCopyPass )
    {
        sdlbuffer_LogSDLError( "SDL_BeginGPUCopyPass" );
        if( bOwnCommandBuffer )
            SDL_CancelGPUCommandBuffer( pCommandBuffer );
        sdlbuffer_ReleaseTransientUploadBuffer( pTransferBuffer, bOwnTransferBuffer );
        return FALSE;
    }

    SDL_GPUTransferBufferLocation Source;
    x_memset( &Source, 0, sizeof(Source) );
    Source.transfer_buffer = pTransferBuffer;
    Source.offset          = 0;

    SDL_GPUBufferRegion Destination;
    x_memset( &Destination, 0, sizeof(Destination) );
    Destination.buffer = Buffer.pBackend->pBuffer;
    Destination.offset = Upload.Offset;
    Destination.size   = Upload.Size;

    SDL_UploadToGPUBuffer( pCopyPass,
                           &Source,
                           &Destination,
                           Upload.bCycle ? true : false );
    SDL_EndGPUCopyPass( pCopyPass );

    if( bOwnCommandBuffer && !SDL_SubmitGPUCommandBuffer( pCommandBuffer ) )
    {
        sdlbuffer_LogSDLError( "SDL_SubmitGPUCommandBuffer" );
        sdlbuffer_ReleaseTransientUploadBuffer( pTransferBuffer, bOwnTransferBuffer );
        return FALSE;
    }

    sdlbuffer_ReleaseTransientUploadBuffer( pTransferBuffer, bOwnTransferBuffer );
    return TRUE;
}

//==============================================================================
//  RENDER BUFFER FUNCTIONS
//==============================================================================

xbool rbuffer_Create( rbuffer& Buffer, const rbuffer_desc& Desc, const void* pInitialData )
{
    rbuffer_Destroy( Buffer );

    if( !g_pSDLGPUDevice )
        return FALSE;

    if( (Desc.Size == 0) || !sdlbuffer_ValidateUsageFlags( Desc.UsageFlags ) )
        return FALSE;

    if( (Desc.Stride != 0) && (Desc.Size % Desc.Stride) )
        return FALSE;

    SDL_GPUBufferUsageFlags UsageFlags;
    if( !sdlbuffer_ToSDLUsageFlags( Desc.UsageFlags, UsageFlags ) )
        return FALSE;

    SDL_PropertiesID Props = 0;
    if( Desc.pDebugName && Desc.pDebugName[0] )
    {
        Props = SDL_CreateProperties();
        if( !Props )
        {
            sdlbuffer_LogSDLError( "SDL_CreateProperties" );
            return FALSE;
        }

        if( !SDL_SetStringProperty( Props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, Desc.pDebugName ) )
        {
            sdlbuffer_LogSDLError( "SDL_SetStringProperty" );
            SDL_DestroyProperties( Props );
            return FALSE;
        }
    }

    SDL_GPUBufferCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof(CreateInfo) );
    CreateInfo.usage = UsageFlags;
    CreateInfo.size  = Desc.Size;
    CreateInfo.props = Props;

    SDL_GPUBuffer* pBuffer = SDL_CreateGPUBuffer( g_pSDLGPUDevice, &CreateInfo );

    if( Props )
        SDL_DestroyProperties( Props );

    if( !pBuffer )
    {
        sdlbuffer_LogSDLError( "SDL_CreateGPUBuffer" );
        return FALSE;
    }

    rbuffer_backend* pBackend = new rbuffer_backend;
    if( !pBackend )
    {
        SDL_ReleaseGPUBuffer( g_pSDLGPUDevice, pBuffer );
        return FALSE;
    }

    pBackend->pOwner  = &Buffer;
    pBackend->pBuffer = pBuffer;

    if( sdlbuffer_IsStorageUsage( Desc.UsageFlags ) )
    {
        pBackend->Resource.pBackend        = &pBackend->ResourceBackend;
        pBackend->ResourceBackend.Kind     = SDLENG_SHADER_RESOURCE_BUFFER;
        pBackend->ResourceBackend.pTexture = NULL;
        pBackend->ResourceBackend.pBuffer  = pBuffer;
        pBackend->bShaderResource          = TRUE;
    }

    Buffer.Desc     = Desc;
    Buffer.pBackend = pBackend;
    sdlbuffer_LinkBuffer( pBackend );

    if( pInitialData )
    {
        if( !rbuffer_Upload( Buffer, pInitialData, Desc.Size ) )
        {
            rbuffer_Destroy( Buffer );
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

void rbuffer_Destroy( rbuffer& Buffer )
{
    rbuffer_backend* pBackend = Buffer.pBackend;
    if( !pBackend )
        return;

    sdlbuffer_InvalidateBindings( pBackend->pBuffer );
    sdlbuffer_UnlinkBuffer( pBackend );
    sdlbuffer_ReleaseBackend( pBackend );
    delete pBackend;

    Buffer.Desc     = rbuffer_desc();
    Buffer.pBackend = NULL;
}

//==============================================================================

xbool rbuffer_Upload( rbuffer& Buffer, const rbuffer_upload_desc& Upload )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "BufferUploads" );
    return sdlbuffer_SubmitUpload( Buffer, Upload );
}

//==============================================================================

xbool rbuffer_Upload( rbuffer& Buffer, const void* pData, u32 Size, u32 Offset, xbool bCycle )
{
    rbuffer_upload_desc Upload;
    Upload.pData  = pData;
    Upload.Size   = Size;
    Upload.Offset = Offset;
    Upload.bCycle = bCycle;
    return rbuffer_Upload( Buffer, Upload );
}

//==============================================================================

xbool rbuffer_BindVertex( const rbuffer& Buffer, u32 Slot, u32 Offset )
{
    if( !Buffer.pBackend || !Buffer.pBackend->pBuffer )
        return FALSE;

    if( !(Buffer.Desc.UsageFlags & RBUFFER_USAGE_VERTEX) )
        return FALSE;

    if( Offset >= Buffer.Desc.Size )
        return FALSE;

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();
    if( !pRenderPass )
        return FALSE;

    if( Slot >= (u32)s_VertexBindings.GetCount() )
    {
        const s32 OldCount = s_VertexBindings.GetCount();
        s_VertexBindings.SetCount( Slot + 1 );
        for( s32 i = OldCount; i < s_VertexBindings.GetCount(); ++i )
        {
            s_VertexBindings[i].pBuffer = NULL;
            s_VertexBindings[i].Offset  = 0;
        }
    }

    sdlbuffer_vertex_binding_cache& Cached = s_VertexBindings[Slot];
    const xbool bIssued = (Cached.pBuffer != Buffer.pBackend->pBuffer) ||
                          (Cached.Offset  != Offset);
    static xprofile_counter BufferBindRequestMetric =
        x_GetProfiler().RegisterCounter( "BufferBindRequests", "Renderer" );
    static xprofile_counter BufferBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "BufferBindIssued", "Renderer" );
    BufferBindRequestMetric.Add();
    if( bIssued )
        BufferBindIssuedMetric.Add();
    if( !bIssued )
        return TRUE;

    SDL_GPUBufferBinding Binding;
    Binding.buffer = Buffer.pBackend->pBuffer;
    Binding.offset = Offset;

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start = bFineTiming ? x_GetTime() : 0;
    SDL_BindGPUVertexBuffers( pRenderPass, Slot, &Binding, 1 );
    const xtick End = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone BufferBindMetric =
            x_GetProfiler().RegisterZone( "BufferBindAPI", "RendererAPI" );
        BufferBindMetric.Record( End - Start );
    }
    Cached.pBuffer = Binding.buffer;
    Cached.Offset  = Binding.offset;
    return TRUE;
}

//==============================================================================

xbool rbuffer_BindVertex( const rbuffer_vertex_binding* pBindings, u32 Count )
{
    if( Count == 0 )
        return TRUE;

    if( !pBindings )
        return FALSE;

    for( u32 i = 0; i < Count; i++ )
    {
        if( !pBindings[i].pBuffer ||
            !rbuffer_BindVertex( *pBindings[i].pBuffer, pBindings[i].Slot, pBindings[i].Offset ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

xbool rbuffer_BindIndex( const rbuffer& Buffer, rbuffer_index_format Format, u32 Offset )
{
    if( !Buffer.pBackend || !Buffer.pBackend->pBuffer )
        return FALSE;

    if( !(Buffer.Desc.UsageFlags & RBUFFER_USAGE_INDEX) )
        return FALSE;

    if( Offset >= Buffer.Desc.Size )
        return FALSE;

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();
    if( !pRenderPass )
        return FALSE;

    const xbool bIssued = (s_pIndexBuffer != Buffer.pBackend->pBuffer) ||
                          (s_IndexOffset   != Offset)                   ||
                          (s_IndexFormat   != Format);
    static xprofile_counter BufferBindRequestMetric =
        x_GetProfiler().RegisterCounter( "BufferBindRequests", "Renderer" );
    static xprofile_counter BufferBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "BufferBindIssued", "Renderer" );
    BufferBindRequestMetric.Add();
    if( bIssued )
        BufferBindIssuedMetric.Add();
    if( !bIssued )
        return TRUE;

    SDL_GPUBufferBinding Binding;
    Binding.buffer = Buffer.pBackend->pBuffer;
    Binding.offset = Offset;

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start = bFineTiming ? x_GetTime() : 0;
    SDL_BindGPUIndexBuffer( pRenderPass, &Binding, sdlbuffer_ToSDLIndexFormat( Format ) );
    const xtick End = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone BufferBindMetric =
            x_GetProfiler().RegisterZone( "BufferBindAPI", "RendererAPI" );
        BufferBindMetric.Record( End - Start );
    }
    s_pIndexBuffer = Binding.buffer;
    s_IndexOffset  = Binding.offset;
    s_IndexFormat  = Format;
    return TRUE;
}

//==============================================================================

void sdleng_ResetBufferBindings( void )
{
    for( s32 i = 0; i < s_VertexBindings.GetCount(); ++i )
    {
        s_VertexBindings[i].pBuffer = NULL;
        s_VertexBindings[i].Offset  = 0;
    }

    s_pIndexBuffer = NULL;
    s_IndexOffset  = 0;
    s_IndexFormat  = RBUFFER_INDEX_FORMAT_U16;
}

//==============================================================================

xbool rbuffer_IsValid( const rbuffer& Buffer )
{
    return (Buffer.pBackend != NULL) && (Buffer.pBackend->pBuffer != NULL);
}

//==============================================================================

const rbuffer_desc* rbuffer_GetDesc( const rbuffer& Buffer )
{
    return rbuffer_IsValid( Buffer ) ? &Buffer.Desc : NULL;
}

//==============================================================================

const shader_resource* rbuffer_GetResource( const rbuffer& Buffer )
{
    if( !rbuffer_IsValid( Buffer ) ||
        !Buffer.pBackend->bShaderResource ||
        !Buffer.pBackend->Resource.pBackend )
    {
        return NULL;
    }

    return &Buffer.pBackend->Resource;
}

//==============================================================================
#endif // defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
