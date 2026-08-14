//==============================================================================
//
//  sdleng_shader.cpp
//
//==============================================================================

// TODO: Implement compute shaders handling

#include "x_target.hpp"

#if defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"
#include "e_ECS.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

#ifndef X_PLUS_HPP
#include "x_plus.hpp"
#endif

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static shader_backend* s_pShaderList = NULL;
static u32             s_ShaderCount = 0;
static u64             s_NextShaderSerial = 1;

struct sdlshader_sampler_cache_entry
{
    SDL_GPUTexture* pTexture;
    SDL_GPUSampler* pSampler;
    u64             Serial;

    sdlshader_sampler_cache_entry( void ) :
        pTexture( NULL ),
        pSampler( NULL ),
        Serial  ( 0 )
    {
    }
};

struct sdlshader_texture_cache_entry
{
    SDL_GPUTexture* pTexture;
    u64             Serial;

    sdlshader_texture_cache_entry( void ) :
        pTexture( NULL ),
        Serial  ( 0 )
    {
    }
};

struct sdlshader_buffer_cache_entry
{
    SDL_GPUBuffer* pBuffer;
    u64            Serial;

    sdlshader_buffer_cache_entry( void ) :
        pBuffer( NULL ),
        Serial ( 0 )
    {
    }
};

static xarray<sdlshader_sampler_cache_entry> s_SamplerBindings[2];
static xarray<sdlshader_texture_cache_entry> s_StorageTextureBindings[2];
static xarray<sdlshader_buffer_cache_entry>  s_StorageBufferBindings[2];
static u64                                   s_ShaderBindingSerial = 1;
#if !defined(X_RETAIL)
static const render_pipeline_backend* s_pDebugPipeline = NULL;
static u32 s_DebugVertexSamplerMask   = 0;
static u32 s_DebugFragmentSamplerMask = 0;
#endif

//==============================================================================
//  GRAPHICS BINDING VALIDATION
//==============================================================================

#if !defined(X_RETAIL)

void sdleng_ResetGraphicsBindingDebug( void )
{
    s_pDebugPipeline          = NULL;
    s_DebugVertexSamplerMask   = 0;
    s_DebugFragmentSamplerMask = 0;
}

//==============================================================================

void sdleng_SetGraphicsPipelineDebug( const render_pipeline_backend* pPipeline )
{
    s_pDebugPipeline = pPipeline;
}

//==============================================================================

void sdleng_ClearGraphicsPipelineDebug( const render_pipeline_backend* pPipeline )
{
    if( s_pDebugPipeline == pPipeline )
        s_pDebugPipeline = NULL;
}

//==============================================================================

void sdleng_RecordSamplerBindingDebug( shader_stage Stage, u32 Slot )
{
    if( Slot >= 32 )
        return;

    if( Stage == SHADER_STAGE_VERTEX )
        s_DebugVertexSamplerMask |= (1u << Slot);
    else if( Stage == SHADER_STAGE_PIXEL )
        s_DebugFragmentSamplerMask |= (1u << Slot);
}

//==============================================================================

xbool sdleng_ValidateGraphicsBindings( void )
{
    if( !s_pDebugPipeline )
    {
        x_DebugMsg( "SDLEngine: draw has no active graphics pipeline\n" );
        return FALSE;
    }

    const shader_resource_counts* pVertexResources = &s_pDebugPipeline->VertexResources;
    const shader_resource_counts* pPixelResources  = &s_pDebugPipeline->PixelResources;
    const char* pPipelineName = s_pDebugPipeline->pDebugName ? s_pDebugPipeline->pDebugName : "unnamed";

    const u32 VertexSamplerCount = pVertexResources ? pVertexResources->SamplerCount : 0;
    for( u32 Slot = 0; Slot < VertexSamplerCount; Slot++ )
    {
        if( !(s_DebugVertexSamplerMask & (1u << Slot)) )
        {
            x_DebugMsg( "SDLEngine: pipeline '%s' missing vertex sampler slot %u\n",
                        pPipelineName,
                        Slot );
            return FALSE;
        }
    }

    const u32 FragmentSamplerCount = pPixelResources ? pPixelResources->SamplerCount : 0;
    for( u32 Slot = 0; Slot < FragmentSamplerCount; Slot++ )
    {
        if( !(s_DebugFragmentSamplerMask & (1u << Slot)) )
        {
            x_DebugMsg( "SDLEngine: pipeline '%s' missing fragment sampler slot %u of %u (bound mask 0x%08X)\n",
                        pPipelineName,
                        Slot,
                        FragmentSamplerCount,
                        s_DebugFragmentSamplerMask );
            return FALSE;
        }
    }

    return TRUE;
}

#endif // !defined(X_RETAIL)

//==============================================================================
//  HELPERS
//==============================================================================

static
void sdlshader_LogSDLError( const char* pContext )
{
    const char* pError = SDL_GetError();
    if( !pError || !pError[0] )
        pError = "unknown SDL error";

    x_DebugMsg( "SDLShader: %s failed: %s\n", pContext, pError );
}

//==============================================================================

static
xbool sdlshader_IsValidStage( shader_stage Stage )
{
    return (Stage == SHADER_STAGE_VERTEX) ||
           (Stage == SHADER_STAGE_PIXEL)  ||
           (Stage == SHADER_STAGE_COMPUTE);
}

//==============================================================================

static
u32 sdlshader_HashBinding( shader_binding_kind Kind, const char* pName )
{
    u32 Hash = 2166136261u;
    while( *pName )
    {
        Hash ^= (u8)*pName++;
        Hash *= 16777619u;
    }

    Hash ^= (u32)Kind + 0x9E3779B9u + (Hash << 6) + (Hash >> 2);
    return Hash;
}

//==============================================================================

static
void sdlshader_BuildBindingLookup( shader_backend& Backend )
{
    Backend.BindingLookup.Clear();
    Backend.BindingLookupMask = 0;

    const u32 BindingCount = (u32)Backend.Bindings.GetCount();
    if( BindingCount == 0 )
        return;

    u32 TableSize = 1;
    while( TableSize < (BindingCount * 2) )
        TableSize <<= 1;

    Backend.BindingLookup.SetCount( TableSize );
    Backend.BindingLookupMask = TableSize - 1;
    for( u32 i = 0; i < TableSize; ++i )
        Backend.BindingLookup[i] = -1;

    for( u32 BindingIndex = 0; BindingIndex < BindingCount; ++BindingIndex )
    {
        const shader_binding_record& Binding = Backend.Bindings[BindingIndex];
        u32 LookupIndex = sdlshader_HashBinding( Binding.Kind, Binding.Name ) & Backend.BindingLookupMask;

        for( u32 Probe = 0; Probe < TableSize; ++Probe )
        {
            const s32 ExistingIndex = Backend.BindingLookup[LookupIndex];
            if( ExistingIndex < 0 )
            {
                Backend.BindingLookup[LookupIndex] = (s32)BindingIndex;
                break;
            }

            const shader_binding_record& Existing = Backend.Bindings[ExistingIndex];
            if( (Existing.Kind == Binding.Kind) && (x_strcmp( Existing.Name, Binding.Name ) == 0) )
                break;

            LookupIndex = (LookupIndex + 1) & Backend.BindingLookupMask;
        }
    }
}

//==============================================================================

static
u32 sdlshader_StageCacheIndex( shader_stage Stage )
{
    return (Stage == SHADER_STAGE_VERTEX) ? 0u : 1u;
}

//==============================================================================

static
xbool sdlshader_CacheSamplerBinding( shader_stage     Stage,
                                     u32              Slot,
                                     SDL_GPUTexture*  pTexture,
                                     SDL_GPUSampler*  pSampler )
{
    xarray<sdlshader_sampler_cache_entry>& Bindings = s_SamplerBindings[sdlshader_StageCacheIndex( Stage )];
    if( Slot >= (u32)Bindings.GetCount() )
        Bindings.SetCount( Slot + 1 );

    sdlshader_sampler_cache_entry& Entry = Bindings[Slot];
    if( (Entry.Serial == s_ShaderBindingSerial) &&
        (Entry.pTexture == pTexture) &&
        (Entry.pSampler == pSampler) )
    {
        return FALSE;
    }

    Entry.pTexture = pTexture;
    Entry.pSampler = pSampler;
    Entry.Serial   = s_ShaderBindingSerial;
    return TRUE;
}

//==============================================================================

static
xbool sdlshader_CacheStorageTextureBinding( shader_stage    Stage,
                                            u32             Slot,
                                            SDL_GPUTexture* pTexture )
{
    xarray<sdlshader_texture_cache_entry>& Bindings = s_StorageTextureBindings[sdlshader_StageCacheIndex( Stage )];
    if( Slot >= (u32)Bindings.GetCount() )
        Bindings.SetCount( Slot + 1 );

    sdlshader_texture_cache_entry& Entry = Bindings[Slot];
    if( (Entry.Serial == s_ShaderBindingSerial) && (Entry.pTexture == pTexture) )
        return FALSE;

    Entry.pTexture = pTexture;
    Entry.Serial   = s_ShaderBindingSerial;
    return TRUE;
}

//==============================================================================

static
xbool sdlshader_CacheStorageBufferBinding( shader_stage   Stage,
                                           u32            Slot,
                                           SDL_GPUBuffer* pBuffer )
{
    xarray<sdlshader_buffer_cache_entry>& Bindings = s_StorageBufferBindings[sdlshader_StageCacheIndex( Stage )];
    if( Slot >= (u32)Bindings.GetCount() )
        Bindings.SetCount( Slot + 1 );

    sdlshader_buffer_cache_entry& Entry = Bindings[Slot];
    if( (Entry.Serial == s_ShaderBindingSerial) && (Entry.pBuffer == pBuffer) )
        return FALSE;

    Entry.pBuffer = pBuffer;
    Entry.Serial  = s_ShaderBindingSerial;
    return TRUE;
}

//==============================================================================

void sdleng_ResetShaderBindings( void )
{
    ++s_ShaderBindingSerial;
}

//==============================================================================

static
SDL_GPUShaderFormat sdlshader_ToSDLFormat( shader_format Format )
{
    switch( Format )
    {
        case SHADER_FORMAT_SPIRV:    return SDL_GPU_SHADERFORMAT_SPIRV;
        case SHADER_FORMAT_DXIL:     return SDL_GPU_SHADERFORMAT_DXIL;
        case SHADER_FORMAT_MSL:      return SDL_GPU_SHADERFORMAT_MSL;
        case SHADER_FORMAT_METALLIB: return SDL_GPU_SHADERFORMAT_METALLIB;
        case SHADER_FORMAT_PRIVATE:  return SDL_GPU_SHADERFORMAT_PRIVATE;
        default:                     return SDL_GPU_SHADERFORMAT_INVALID;
    }
}

//==============================================================================

static
void sdlshader_AddFormatCandidate( shader_format* pFormats,
                                   u32&           Count,
                                   shader_format  Format )
{
    for( u32 i = 0; i < Count; i++ )
    {
        if( pFormats[i] == Format )
            return;
    }

    if( Count < 5 )
        pFormats[Count++] = Format;
}

//==============================================================================

static
const ecs_shader_code* sdlshader_SelectShaderCode( const ecs_shader_container& Container )
{
    if( !g_pSDLGPUDevice )
        return NULL;

    const SDL_GPUShaderFormat SupportedFormats = SDL_GetGPUShaderFormats( g_pSDLGPUDevice );
    const char* pDriver = SDL_GetGPUDeviceDriver( g_pSDLGPUDevice );
    shader_format Candidates[5];
    u32 CandidateCount = 0;

    if( pDriver && x_stristr( pDriver, "vulkan" ) )
        sdlshader_AddFormatCandidate( Candidates, CandidateCount, SHADER_FORMAT_SPIRV );
    else if( pDriver && (x_stristr( pDriver, "d3d12" ) || x_stristr( pDriver, "direct3d" )) )
        sdlshader_AddFormatCandidate( Candidates, CandidateCount, SHADER_FORMAT_DXIL );
    else if( pDriver && x_stristr( pDriver, "metal" ) )
        sdlshader_AddFormatCandidate( Candidates, CandidateCount, SHADER_FORMAT_METALLIB );

    sdlshader_AddFormatCandidate( Candidates, CandidateCount, SHADER_FORMAT_SPIRV );
    sdlshader_AddFormatCandidate( Candidates, CandidateCount, SHADER_FORMAT_DXIL );
    sdlshader_AddFormatCandidate( Candidates, CandidateCount, SHADER_FORMAT_METALLIB );
    sdlshader_AddFormatCandidate( Candidates, CandidateCount, SHADER_FORMAT_MSL );
    sdlshader_AddFormatCandidate( Candidates, CandidateCount, SHADER_FORMAT_PRIVATE );

    for( u32 i = 0; i < CandidateCount; i++ )
    {
        const SDL_GPUShaderFormat SDLFormat = sdlshader_ToSDLFormat( Candidates[i] );
        if( (SDLFormat == SDL_GPU_SHADERFORMAT_INVALID) ||
            ((SupportedFormats & SDLFormat) == 0) )
        {
            continue;
        }

        const ecs_shader_code* pCode = ecs_FindShaderCode( Container, Candidates[i] );
        if( pCode )
            return pCode;
    }

    return NULL;
}

//==============================================================================

static
xbool sdlshader_ToSDLGraphicsStage( shader_stage Stage, SDL_GPUShaderStage& SDLStage )
{
    switch( Stage )
    {
        case SHADER_STAGE_VERTEX:
            SDLStage = SDL_GPU_SHADERSTAGE_VERTEX;
            return TRUE;

        case SHADER_STAGE_PIXEL:
            SDLStage = SDL_GPU_SHADERSTAGE_FRAGMENT;
            return TRUE;

        default:
            return FALSE;
    }
}

//==============================================================================

static
void sdlshader_LinkShader( shader_backend* pBackend )
{
    pBackend->pPrev = NULL;
    pBackend->pNext = s_pShaderList;

    if( s_pShaderList )
        s_pShaderList->pPrev = pBackend;

    s_pShaderList = pBackend;
    s_ShaderCount++;
}

//==============================================================================

static
void sdlshader_UnlinkShader( shader_backend* pBackend )
{
    if( pBackend->pPrev )
        pBackend->pPrev->pNext = pBackend->pNext;
    else if( s_pShaderList == pBackend )
        s_pShaderList = pBackend->pNext;

    if( pBackend->pNext )
        pBackend->pNext->pPrev = pBackend->pPrev;

    pBackend->pPrev = NULL;
    pBackend->pNext = NULL;

    if( s_ShaderCount )
        s_ShaderCount--;
}

//==============================================================================

static
void sdlshader_ReleaseBackend( shader_backend* pBackend )
{
    if( !pBackend )
        return;

    if( pBackend->pShader && g_pSDLGPUDevice )
        SDL_ReleaseGPUShader( g_pSDLGPUDevice, pBackend->pShader );

    pBackend->pShader = NULL;

    if( pBackend->pCode )
    {
        x_free( pBackend->pCode );
        pBackend->pCode = NULL;
    }

    if( pBackend->pEntryPoint )
    {
        x_free( pBackend->pEntryPoint );
        pBackend->pEntryPoint = NULL;
    }

    pBackend->CodeSize = 0;
}

//==============================================================================

static
xbool sdlshader_LoadFileBlob( shader_blob& Blob, const char* pFileName )
{
    Blob = shader_blob();

    if( !pFileName )
        return FALSE;

    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        x_DebugMsg( "SDLShader: failed to open shader file '%s'\n", pFileName );
        return FALSE;
    }

    const s32 FileLength = x_flength( pFile );
    if( FileLength <= 0 )
    {
        x_DebugMsg( "SDLShader: shader file '%s' is empty\n", pFileName );
        x_fclose( pFile );
        return FALSE;
    }

    void* pData = x_malloc( FileLength );
    if( !pData )
    {
        x_fclose( pFile );
        return FALSE;
    }

    const s32 BytesRead = x_fread( pData, 1, FileLength, pFile );
    x_fclose( pFile );

    if( BytesRead != FileLength )
    {
        x_DebugMsg( "SDLShader: failed to read shader file '%s' (%d/%d bytes)\n",
                    pFileName,
                    BytesRead,
                    FileLength );
        x_free( pData );
        return FALSE;
    }

    Blob.pData = pData;
    Blob.Size  = (u32)FileLength;
    return TRUE;
}

//==============================================================================

static
void sdlshader_FreeFileBlob( shader_blob& Blob )
{
    if( Blob.pData )
        x_free( (void*)Blob.pData );

    Blob = shader_blob();
}

//==============================================================================

static
xbool sdlshader_GetCommandBuffer( SDL_GPUCommandBuffer*& pCommandBuffer )
{
    pCommandBuffer = sdleng_GetCommandBuffer();
    if( !pCommandBuffer )
    {
        x_DebugMsg( "SDLShader: no active command buffer for shader binding\n" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool sdlshader_GetRenderPass( SDL_GPURenderPass*& pRenderPass )
{
    pRenderPass = sdleng_GetRenderPass();
    if( !pRenderPass )
    {
        x_DebugMsg( "SDLShader: no active render pass for graphics resource binding\n" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool sdlshader_ReportComputePassMissing( void )
{
    x_DebugMsg( "SDLShader: compute resource binding requires a compute pass backend\n" );
    return FALSE;
}

//==============================================================================
//  SHADER LIFETIME
//==============================================================================

void shader_Init( void )
{
    s_pShaderList = NULL;
    s_ShaderCount = 0;
}

//==============================================================================

void shader_Kill( void )
{
    while( s_pShaderList )
    {
        shader_backend* pBackend = s_pShaderList;
        sdlshader_UnlinkShader( pBackend );
        sdlshader_ReleaseBackend( pBackend );

        if( pBackend->pOwner )
        {
            pBackend->pOwner->Stage    = SHADER_STAGE_VERTEX;
            pBackend->pOwner->Format   = SHADER_FORMAT_UNKNOWN;
            pBackend->pOwner->pBackend = NULL;
            pBackend->pOwner           = NULL;
        }

        delete pBackend;
    }
}

//==============================================================================

xbool shader_Create( shader& Shader, const shader_desc& Desc )
{
    if( !g_pSDLGPUDevice )
        return FALSE;

    if( !sdlshader_IsValidStage( Desc.Stage ) || !Desc.Blob.pData || (Desc.Blob.Size == 0) )
        return FALSE;

    if( Desc.BindingCount && !Desc.pBindings )
        return FALSE;

    if( Desc.Stage == SHADER_STAGE_COMPUTE )
    {
        x_DebugMsg( "SDLShader: compute shaders are created through compute pipelines in SDL GPU\n" );
        return FALSE;
    }

    SDL_GPUShaderStage SDLStage;
    if( !sdlshader_ToSDLGraphicsStage( Desc.Stage, SDLStage ) )
        return FALSE;

    const SDL_GPUShaderFormat SDLFormat = sdlshader_ToSDLFormat( Desc.Format );
    if( SDLFormat == SDL_GPU_SHADERFORMAT_INVALID )
        return FALSE;

    const SDL_GPUShaderFormat SupportedFormats = SDL_GetGPUShaderFormats( g_pSDLGPUDevice );
    if( (SupportedFormats & SDLFormat) == 0 )
    {
        x_DebugMsg( "SDLShader: shader format 0x%08X is not supported by the active SDL GPU device\n",
                    SDLFormat );
        return FALSE;
    }

    shader_backend* pBackend = new shader_backend;
    if( !pBackend )
        return FALSE;

    pBackend->pCode = x_malloc( Desc.Blob.Size );
    if( !pBackend->pCode )
    {
        delete pBackend;
        return FALSE;
    }

    x_memcpy( pBackend->pCode, Desc.Blob.pData, Desc.Blob.Size );
    pBackend->CodeSize    = Desc.Blob.Size;
    pBackend->pEntryPoint = x_strdup( Desc.pEntryPoint ? Desc.pEntryPoint : "main" );
    if( !pBackend->pEntryPoint )
    {
        sdlshader_ReleaseBackend( pBackend );
        delete pBackend;
        return FALSE;
    }

    SDL_PropertiesID Props = 0;
    if( Desc.pDebugName && Desc.pDebugName[0] )
    {
        Props = SDL_CreateProperties();
        if( !Props )
        {
            sdlshader_LogSDLError( "SDL_CreateProperties" );
            sdlshader_ReleaseBackend( pBackend );
            delete pBackend;
            return FALSE;
        }

        if( !SDL_SetStringProperty( Props, SDL_PROP_GPU_SHADER_CREATE_NAME_STRING, Desc.pDebugName ) )
        {
            sdlshader_LogSDLError( "SDL_SetStringProperty" );
            SDL_DestroyProperties( Props );
            sdlshader_ReleaseBackend( pBackend );
            delete pBackend;
            return FALSE;
        }
    }

    SDL_GPUShaderCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof(CreateInfo) );
    CreateInfo.code_size            = pBackend->CodeSize;
    CreateInfo.code                 = (const Uint8*)pBackend->pCode;
    CreateInfo.entrypoint           = pBackend->pEntryPoint;
    CreateInfo.format               = SDLFormat;
    CreateInfo.stage                = SDLStage;
    CreateInfo.num_samplers         = Desc.Resources.SamplerCount;
    CreateInfo.num_storage_textures = Desc.Resources.StorageTextureCount;
    CreateInfo.num_storage_buffers  = Desc.Resources.StorageBufferCount;
    CreateInfo.num_uniform_buffers  = Desc.Resources.UniformBufferCount;
    CreateInfo.props                = Props;

    SDL_GPUShader* pShader = SDL_CreateGPUShader( g_pSDLGPUDevice, &CreateInfo );

    if( Props )
        SDL_DestroyProperties( Props );

    if( !pShader )
    {
        sdlshader_LogSDLError( "SDL_CreateGPUShader" );
        sdlshader_ReleaseBackend( pBackend );
        delete pBackend;
        return FALSE;
    }

    shader_Destroy( Shader );

    pBackend->pOwner     = &Shader;
    pBackend->pShader    = pShader;
    pBackend->Serial     = s_NextShaderSerial++;
    pBackend->Stage      = Desc.Stage;
    pBackend->Format     = Desc.Format;
    pBackend->SDLFormat  = SDLFormat;
    pBackend->Resources  = Desc.Resources;
    pBackend->Bindings.Clear();
    for( u32 i=0; i<Desc.BindingCount; i++ )
    {
        const shader_binding_desc& DescBinding = Desc.pBindings[i];
        if( !DescBinding.pName || !DescBinding.pName[0] || (DescBinding.Count == 0) )
            continue;

        shader_binding_record& Binding = pBackend->Bindings.Append();
        Binding.Kind  = DescBinding.Kind;
        Binding.Name  = DescBinding.pName;
        Binding.Slot  = DescBinding.Slot;
        Binding.Count = DescBinding.Count;
    }
    sdlshader_BuildBindingLookup( *pBackend );

    Shader.Stage    = Desc.Stage;
    Shader.Format   = Desc.Format;
    Shader.pBackend = pBackend;

    sdlshader_LinkShader( pBackend );
    return TRUE;
}

//==============================================================================

xbool shader_LoadFromFile( shader& Shader, const shader_file_desc& Desc )
{
    shader_blob Blob;
    if( !sdlshader_LoadFileBlob( Blob, Desc.pFileName ) )
        return FALSE;

    shader_desc ShaderDesc;
    ShaderDesc.Stage       = Desc.Stage;
    ShaderDesc.Format      = Desc.Format;
    ShaderDesc.Blob        = Blob;
    ShaderDesc.pEntryPoint = Desc.pEntryPoint;
    ShaderDesc.Resources   = Desc.Resources;
    ShaderDesc.pBindings   = Desc.pBindings;
    ShaderDesc.BindingCount = Desc.BindingCount;
    ShaderDesc.pDebugName  = Desc.pDebugName ? Desc.pDebugName : Desc.pFileName;

    const xbool Result = shader_Create( Shader, ShaderDesc );
    sdlshader_FreeFileBlob( Blob );
    return Result;
}

//==============================================================================

xbool shader_LoadFromEcs( shader& Shader, const shader_ecs_desc& Desc )
{
    ecs_shader_container Container;
    if( !ecs_LoadShaderContainer( Desc.pFileName, Container ) )
        return FALSE;

    xarray<shader_binding_desc> BindingDescs;
    BindingDescs.SetCount( Container.Bindings.GetCount() );
    for( s32 i = 0; i < Container.Bindings.GetCount(); i++ )
    {
        const ecs_shader_binding& ContainerBinding = Container.Bindings[i];
        shader_binding_desc& BindingDesc = BindingDescs[i];
        BindingDesc.Kind  = ContainerBinding.Kind;
        BindingDesc.pName = ContainerBinding.Name;
        BindingDesc.Slot  = ContainerBinding.Slot;
        BindingDesc.Count = ContainerBinding.Count;
    }

    const ecs_shader_code* pCode = sdlshader_SelectShaderCode( Container );
    if( !pCode )
    {
        x_DebugMsg( "SDLShader: no supported bytecode in ECS file '%s'\n", Desc.pFileName );
        return FALSE;
    }

    const char* pEntryPoint = pCode->Entry.IsEmpty() ?
                              (const char*)Container.Entry :
                              (const char*)pCode->Entry;
    shader_desc ShaderDesc;
    ShaderDesc.Stage        = Container.Stage;
    ShaderDesc.Format       = pCode->Format;
    ShaderDesc.Blob         = shader_blob( pCode->Code.GetPtr(), (u32)pCode->Code.GetCount() );
    ShaderDesc.pEntryPoint  = pEntryPoint;
    ShaderDesc.Resources    = Container.Resources;
    ShaderDesc.pBindings    = BindingDescs.GetCount() ? &BindingDescs[0] : NULL;
    ShaderDesc.BindingCount = (u32)BindingDescs.GetCount();
    ShaderDesc.pDebugName   = Desc.pDebugName ? Desc.pDebugName :
                               (Container.Name.IsEmpty() ? Desc.pFileName :
                                                            (const char*)Container.Name);
    return shader_Create( Shader, ShaderDesc );
}

//==============================================================================

xbool shader_LoadFromEcs( shader& Shader, const char* pEcsFileName )
{
    shader_ecs_desc Desc;
    Desc.pFileName = pEcsFileName;
    return shader_LoadFromEcs( Shader, Desc );
}

//==============================================================================

void shader_Destroy( shader& Shader )
{
    shader_backend* pBackend = Shader.pBackend;
    if( !pBackend )
        return;

    sdlshader_UnlinkShader( pBackend );
    sdlshader_ReleaseBackend( pBackend );
    delete pBackend;

    Shader.Stage    = SHADER_STAGE_VERTEX;
    Shader.Format   = SHADER_FORMAT_UNKNOWN;
    Shader.pBackend = NULL;
}

//==============================================================================
//  UNIFORM DATA
//==============================================================================

xbool shader_PushUniformData( shader_stage Stage, u32 Slot, const void* pData, u32 Size )
{
    if( !sdlshader_IsValidStage( Stage ) || !pData || (Size == 0) )
        return FALSE;

    SDL_GPUCommandBuffer* pCommandBuffer = NULL;
    if( !sdlshader_GetCommandBuffer( pCommandBuffer ) )
        return FALSE;

    static xprofile_counter UniformPushCountMetric =
        x_GetProfiler().RegisterCounter( "UniformPushCalls", "Renderer" );
    UniformPushCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start = bFineTiming ? x_GetTime() : 0;
    switch( Stage )
    {
        case SHADER_STAGE_VERTEX:
            SDL_PushGPUVertexUniformData( pCommandBuffer, Slot, pData, Size );
            break;

        case SHADER_STAGE_PIXEL:
            SDL_PushGPUFragmentUniformData( pCommandBuffer, Slot, pData, Size );
            break;

        case SHADER_STAGE_COMPUTE:
            SDL_PushGPUComputeUniformData( pCommandBuffer, Slot, pData, Size );
            break;

        default:
            return FALSE;
    }

    const xtick End = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone UniformPushMetric =
            x_GetProfiler().RegisterZone( "UniformPushAPI", "RendererAPI" );
        UniformPushMetric.Record( End - Start );
    }
    return TRUE;
}

//==============================================================================

xbool shader_PushUniformData( const shader& Shader, shader_stage Stage, const char* pName, const void* pData, u32 Size )
{
    u32 Slot = 0;
    if( !shader_FindUniformSlot( Shader, pName, Slot ) )
        return FALSE;

    return shader_PushUniformData( Stage, Slot, pData, Size );
}

//==============================================================================

xbool shader_PushUniformData( const shader_uniform_binding& Binding )
{
    return shader_PushUniformData( Binding.Stage,
                                   Binding.Slot,
                                   Binding.pData,
                                   Binding.Size );
}

//==============================================================================
//  SAMPLER BINDING
//==============================================================================

xbool shader_BindSampler( const shader_sampler_binding& Binding )
{
    if( !sdlshader_IsValidStage( Binding.Stage ) )
        return FALSE;

    SDL_GPUTexture* pTexture = sdleng_GetGPUTexture( Binding.pTexture );
    SDL_GPUSampler* pSampler = sdleng_GetGPUSampler( Binding.pSampler );
    if( !pTexture || !pSampler )
        return FALSE;

    if( Binding.Stage == SHADER_STAGE_COMPUTE )
        return sdlshader_ReportComputePassMissing();

    SDL_GPURenderPass* pRenderPass = NULL;
    if( !sdlshader_GetRenderPass( pRenderPass ) )
        return FALSE;

    const xbool bIssued = sdlshader_CacheSamplerBinding( Binding.Stage,
                                                        Binding.Slot,
                                                        pTexture,
                                                        pSampler );
    static xprofile_counter ResourceBindRequestMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindRequests", "Renderer" );
    static xprofile_counter ResourceBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindIssued", "Renderer" );
    ResourceBindRequestMetric.Add();
    if( bIssued )
        ResourceBindIssuedMetric.Add();
    sdleng_RecordSamplerBindingDebug( Binding.Stage, Binding.Slot );
    if( !bIssued )
        return TRUE;

    SDL_GPUTextureSamplerBinding SDLBinding;
    x_memset( &SDLBinding, 0, sizeof(SDLBinding) );
    SDLBinding.texture = pTexture;
    SDLBinding.sampler = pSampler;

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start = bFineTiming ? x_GetTime() : 0;
    if( Binding.Stage == SHADER_STAGE_VERTEX )
        SDL_BindGPUVertexSamplers( pRenderPass, Binding.Slot, &SDLBinding, 1 );
    else
        SDL_BindGPUFragmentSamplers( pRenderPass, Binding.Slot, &SDLBinding, 1 );
    const xtick End = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone ResourceBindMetric =
            x_GetProfiler().RegisterZone( "ResourceBindAPI", "RendererAPI" );
        ResourceBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool shader_BindSampler( const shader&          Shader,
                          shader_stage           Stage,
                          const char*            pName,
                          const shader_resource* pTexture,
                          const rstate_sampler*  pSampler )
{
    u32 Slot = 0;
    if( !shader_FindSampledTextureSlot( Shader, pName, Slot ) )
        return FALSE;

    shader_sampler_binding Binding( Stage, Slot, pTexture, pSampler );
    return shader_BindSampler( Binding );
}

//==============================================================================

xbool shader_BindSamplers( const shader_sampler_binding* pBindings, u32 Count )
{
    if( Count == 0 )
        return TRUE;

    if( !pBindings )
        return FALSE;

    for( u32 i = 0; i < Count; i++ )
    {
        if( !shader_BindSampler( pBindings[i] ) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  STORAGE TEXTURE BINDING
//==============================================================================

xbool shader_BindStorageTexture( const shader_storage_texture_binding& Binding )
{
    if( !sdlshader_IsValidStage( Binding.Stage ) )
        return FALSE;

    SDL_GPUTexture* pTexture = sdleng_GetGPUTexture( Binding.pTexture );
    if( !pTexture )
        return FALSE;

    if( Binding.Stage == SHADER_STAGE_COMPUTE )
        return sdlshader_ReportComputePassMissing();

    SDL_GPURenderPass* pRenderPass = NULL;
    if( !sdlshader_GetRenderPass( pRenderPass ) )
        return FALSE;

    const xbool bIssued = sdlshader_CacheStorageTextureBinding( Binding.Stage,
                                                               Binding.Slot,
                                                               pTexture );
    static xprofile_counter ResourceBindRequestMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindRequests", "Renderer" );
    static xprofile_counter ResourceBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindIssued", "Renderer" );
    ResourceBindRequestMetric.Add();
    if( bIssued )
        ResourceBindIssuedMetric.Add();
    if( !bIssued )
        return TRUE;

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start = bFineTiming ? x_GetTime() : 0;
    if( Binding.Stage == SHADER_STAGE_VERTEX )
        SDL_BindGPUVertexStorageTextures( pRenderPass, Binding.Slot, &pTexture, 1 );
    else
        SDL_BindGPUFragmentStorageTextures( pRenderPass, Binding.Slot, &pTexture, 1 );
    const xtick End = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone ResourceBindMetric =
            x_GetProfiler().RegisterZone( "ResourceBindAPI", "RendererAPI" );
        ResourceBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool shader_BindStorageTexture( const shader&          Shader,
                                 shader_stage           Stage,
                                 const char*            pName,
                                 const shader_resource* pTexture )
{
    u32 Slot = 0;
    if( !shader_FindStorageTextureSlot( Shader, pName, Slot ) )
        return FALSE;

    shader_storage_texture_binding Binding( Stage, Slot, pTexture );
    return shader_BindStorageTexture( Binding );
}

//==============================================================================

xbool shader_BindStorageTextures( const shader_storage_texture_binding* pBindings, u32 Count )
{
    if( Count == 0 )
        return TRUE;

    if( !pBindings )
        return FALSE;

    for( u32 i = 0; i < Count; i++ )
    {
        if( !shader_BindStorageTexture( pBindings[i] ) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  STORAGE BUFFER BINDING
//==============================================================================

xbool shader_BindStorageBuffer( const shader_storage_buffer_binding& Binding )
{
    if( !sdlshader_IsValidStage( Binding.Stage ) )
        return FALSE;

    SDL_GPUBuffer* pBuffer = sdleng_GetGPUBuffer( Binding.pBuffer );
    if( !pBuffer )
        return FALSE;

    if( Binding.Stage == SHADER_STAGE_COMPUTE )
        return sdlshader_ReportComputePassMissing();

    SDL_GPURenderPass* pRenderPass = NULL;
    if( !sdlshader_GetRenderPass( pRenderPass ) )
        return FALSE;

    const xbool bIssued = sdlshader_CacheStorageBufferBinding( Binding.Stage,
                                                              Binding.Slot,
                                                              pBuffer );
    static xprofile_counter ResourceBindRequestMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindRequests", "Renderer" );
    static xprofile_counter ResourceBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindIssued", "Renderer" );
    ResourceBindRequestMetric.Add();
    if( bIssued )
        ResourceBindIssuedMetric.Add();
    if( !bIssued )
        return TRUE;

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start = bFineTiming ? x_GetTime() : 0;
    if( Binding.Stage == SHADER_STAGE_VERTEX )
        SDL_BindGPUVertexStorageBuffers( pRenderPass, Binding.Slot, &pBuffer, 1 );
    else
        SDL_BindGPUFragmentStorageBuffers( pRenderPass, Binding.Slot, &pBuffer, 1 );
    const xtick End = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone ResourceBindMetric =
            x_GetProfiler().RegisterZone( "ResourceBindAPI", "RendererAPI" );
        ResourceBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool shader_BindStorageBuffer( const shader&          Shader,
                                shader_stage           Stage,
                                const char*            pName,
                                const shader_resource* pBuffer )
{
    u32 Slot = 0;
    if( !shader_FindStorageBufferSlot( Shader, pName, Slot ) )
        return FALSE;

    shader_storage_buffer_binding Binding( Stage, Slot, pBuffer );
    return shader_BindStorageBuffer( Binding );
}

//==============================================================================

xbool shader_BindStorageBuffers( const shader_storage_buffer_binding* pBindings, u32 Count )
{
    if( Count == 0 )
        return TRUE;

    if( !pBindings )
        return FALSE;

    for( u32 i = 0; i < Count; i++ )
    {
        if( !shader_BindStorageBuffer( pBindings[i] ) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  BINDING SET
//==============================================================================

xbool shader_BindSet( const shader_binding_set_desc& Desc )
{
    for( u32 i = 0; i < Desc.UniformCount; i++ )
    {
        if( !Desc.pUniforms || !shader_PushUniformData( Desc.pUniforms[i] ) )
            return FALSE;
    }

    if( !shader_BindSamplers( Desc.pSamplers, Desc.SamplerCount ) )
        return FALSE;

    if( !shader_BindStorageTextures( Desc.pStorageTextures, Desc.StorageTextureCount ) )
        return FALSE;

    if( !shader_BindStorageBuffers( Desc.pStorageBuffers, Desc.StorageBufferCount ) )
        return FALSE;

    return TRUE;
}

//==============================================================================
//  BINDING REFLECTION LOOKUP
//==============================================================================

xbool shader_FindBindingSlot( const shader&       Shader,
                              shader_binding_kind Kind,
                              const char*         pName,
                              u32&                Slot )
{
    Slot = 0;

    if( !Shader.pBackend || !pName || !pName[0] )
        return FALSE;

    static xprofile_counter BindingLookupMetric =
        x_GetProfiler().RegisterCounter( "ShaderBindingLookups", "Renderer" );
    BindingLookupMetric.Add();

    const shader_backend& Backend = *Shader.pBackend;
    const u32 LookupCount = (u32)Backend.BindingLookup.GetCount();
    if( LookupCount == 0 )
        return FALSE;

    u32 LookupIndex = sdlshader_HashBinding( Kind, pName ) & Backend.BindingLookupMask;
    for( u32 Probe = 0; Probe < LookupCount; ++Probe )
    {
        const s32 BindingIndex = Backend.BindingLookup[LookupIndex];
        if( BindingIndex < 0 )
            return FALSE;

        const shader_binding_record& Binding = Backend.Bindings[BindingIndex];
        if( (Binding.Kind == Kind) && (x_strcmp( Binding.Name, pName ) == 0) )
        {
            Slot = Binding.Slot;
            return TRUE;
        }

        LookupIndex = (LookupIndex + 1) & Backend.BindingLookupMask;
    }

    return FALSE;
}

//==============================================================================

xbool shader_FindSampledTextureSlot( const shader& Shader, const char* pName, u32& Slot )
{
    return shader_FindBindingSlot( Shader, SHADER_BINDING_SAMPLED_TEXTURE, pName, Slot );
}

//==============================================================================

xbool shader_FindUniformSlot( const shader& Shader, const char* pName, u32& Slot )
{
    return shader_FindBindingSlot( Shader, SHADER_BINDING_UNIFORM_BUFFER, pName, Slot );
}

//==============================================================================

xbool shader_FindStorageTextureSlot( const shader& Shader, const char* pName, u32& Slot )
{
    return shader_FindBindingSlot( Shader, SHADER_BINDING_STORAGE_TEXTURE, pName, Slot );
}

//==============================================================================

xbool shader_FindStorageBufferSlot( const shader& Shader, const char* pName, u32& Slot )
{
    return shader_FindBindingSlot( Shader, SHADER_BINDING_STORAGE_BUFFER, pName, Slot );
}

//==============================================================================
//  SDL BACKEND ACCESSORS
//==============================================================================

SDL_GPUSampler* sdleng_GetGPUSampler( const rstate_sampler* pSampler )
{
    if( !pSampler || !pSampler->pBackend )
        return NULL;

    return pSampler->pBackend->pSampler;
}

//==============================================================================

SDL_GPUShader* sdleng_GetGPUShader( const shader* pShader )
{
    if( !pShader || !pShader->pBackend )
        return NULL;

    return pShader->pBackend->pShader;
}

//==============================================================================

const shader_resource_counts* sdleng_GetShaderResourceCounts( const shader* pShader )
{
    if( !pShader || !pShader->pBackend )
        return NULL;

    return &pShader->pBackend->Resources;
}

//==============================================================================
#endif // defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
