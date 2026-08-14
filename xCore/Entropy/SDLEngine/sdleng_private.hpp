//==============================================================================
//
//  sdleng_private.hpp
//
//==============================================================================

#ifndef SDLENG_PRIVATE_HPP
#define SDLENG_PRIVATE_HPP

#include "x_target.hpp"

#if defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "sdleng_window.hpp"
#include "e_Shader.hpp"
#include "e_RenderBuffer.hpp"
#include "e_RenderDraw.hpp"
#include "e_RenderTarget.hpp"
#include "e_RenderState.hpp"
#include "e_VRAM.hpp"

#include "SDL3/SDL.h"

//==============================================================================
//  CONFIGURATION
//==============================================================================

#ifndef SDLENG_GPU_DRIVER_NAME
#define SDLENG_GPU_DRIVER_NAME "vulkan"
#endif

#ifndef SDLENG_GPU_SHADER_FORMATS
#define SDLENG_GPU_SHADER_FORMATS (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL)
#endif

#ifndef SDLENG_GPU_SWAPCHAIN_COMPOSITION
#define SDLENG_GPU_SWAPCHAIN_COMPOSITION SDL_GPU_SWAPCHAINCOMPOSITION_SDR
#endif

//==============================================================================
//  GLOBAL BACKEND HANDLES
//==============================================================================

extern SDL_GPUDevice*          g_pSDLGPUDevice;
extern SDL_Window*             g_pSDLWindow;

enum sdleng_present_policy
{
    SDLENG_PRESENT_VSYNC = 0,
    SDLENG_PRESENT_MAILBOX,
    SDLENG_PRESENT_IMMEDIATE
};

//==============================================================================
//  BACKEND RESOURCE HANDLES
//==============================================================================

enum sdleng_shader_resource_kind
{
    SDLENG_SHADER_RESOURCE_NONE = 0,
    SDLENG_SHADER_RESOURCE_TEXTURE,
    SDLENG_SHADER_RESOURCE_BUFFER
};

//------------------------------------------------------------------------------

struct shader_resource_backend
{
    sdleng_shader_resource_kind Kind;
    SDL_GPUTexture*             pTexture;
    SDL_GPUBuffer*              pBuffer;

    shader_resource_backend( void ) :
        Kind    ( SDLENG_SHADER_RESOURCE_NONE ),
        pTexture( NULL ),
        pBuffer ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_binding_record
{
    shader_binding_kind Kind;
    xstring             Name;
    u32                 Slot;
    u32                 Count;

    shader_binding_record( void ) :
        Kind ( SHADER_BINDING_SAMPLED_TEXTURE ),
        Name (),
        Slot ( 0 ),
        Count( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_backend
{
    shader*                pOwner;
    SDL_GPUShader*         pShader;
    u64                    Serial;
    void*                  pCode;
    u32                    CodeSize;
    char*                  pEntryPoint;
    shader_stage           Stage;
    shader_format          Format;
    SDL_GPUShaderFormat    SDLFormat;
    shader_resource_counts Resources;
    xarray<shader_binding_record>
                           Bindings;
    xarray<s32>            BindingLookup;
    u32                    BindingLookupMask;
    shader_backend*        pPrev;
    shader_backend*        pNext;

    shader_backend( void ) :
        pOwner     ( NULL ),
        pShader    ( NULL ),
        Serial     ( 0 ),
        pCode      ( NULL ),
        CodeSize   ( 0 ),
        pEntryPoint( NULL ),
        Stage      ( SHADER_STAGE_VERTEX ),
        Format     ( SHADER_FORMAT_UNKNOWN ),
        SDLFormat  ( SDL_GPU_SHADERFORMAT_INVALID ),
        Resources  (),
        Bindings   (),
        BindingLookup(),
        BindingLookupMask( 0 ),
        pPrev      ( NULL ),
        pNext      ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct render_pipeline_cache_entry;

//------------------------------------------------------------------------------

struct rstate_sampler_backend
{
    rstate_sampler* pOwner;
    SDL_GPUSampler* pSampler;
    rstate_sampler_backend* pPrev;
    rstate_sampler_backend* pNext;

    rstate_sampler_backend( void ) :
        pOwner  ( NULL ),
        pSampler( NULL ),
        pPrev   ( NULL ),
        pNext   ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct render_pipeline_backend
{
    render_pipeline*              pOwner;
    render_pipeline_cache_entry*  pEntry;
    shader_resource_counts        VertexResources;
    shader_resource_counts        PixelResources;
    char*                         pDebugName;
    render_pipeline_backend*      pPrev;
    render_pipeline_backend*      pNext;

    render_pipeline_backend( void ) :
        pOwner         ( NULL ),
        pEntry         ( NULL ),
        VertexResources(),
        PixelResources (),
        pDebugName     ( NULL ),
        pPrev          ( NULL ),
        pNext          ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct rtarget_backend
{
    rtarget*                pOwner;
    SDL_GPUTexture*         pTexture;
    shader_resource         Resource;
    shader_resource_backend ResourceBackend;
    xbool                   bOwnTexture;
    xbool                   bBackBuffer;
    xbool                   bShaderResource;
    rtarget_backend*        pPrev;
    rtarget_backend*        pNext;

    rtarget_backend( void ) :
        pOwner         ( NULL ),
        pTexture       ( NULL ),
        Resource       (),
        ResourceBackend(),
        bOwnTexture    ( FALSE ),
        bBackBuffer    ( FALSE ),
        bShaderResource( FALSE ),
        pPrev          ( NULL ),
        pNext          ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct rbuffer_backend
{
    rbuffer*                pOwner;
    SDL_GPUBuffer*          pBuffer;
    SDL_GPUTransferBuffer*  pUploadBuffer;
    u32                     UploadBufferSize;
    shader_resource         Resource;
    shader_resource_backend ResourceBackend;
    xbool                   bShaderResource;
    rbuffer_backend*        pPrev;
    rbuffer_backend*        pNext;

    rbuffer_backend( void ) :
        pOwner          ( NULL ),
        pBuffer         ( NULL ),
        pUploadBuffer   ( NULL ),
        UploadBufferSize( 0 ),
        Resource        (),
        ResourceBackend (),
        bShaderResource ( FALSE ),
        pPrev           ( NULL ),
        pNext           ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct vram_texture_backend
{
    vram_texture*           pOwner;
    SDL_GPUTexture*         pTexture;
    shader_resource         Resource;
    shader_resource_backend ResourceBackend;
    vram_texture_backend*   pPrev;
    vram_texture_backend*   pNext;

    vram_texture_backend( void ) :
        pOwner         ( NULL ),
        pTexture       ( NULL ),
        Resource       (),
        ResourceBackend(),
        pPrev          ( NULL ),
        pNext          ( NULL )
    {
    }
};

//==============================================================================
//  BACKEND LIFETIME
//==============================================================================

xbool                           sdleng_CreateDeviceForWindow      ( sdleng_native_window_handle hWindow,
                                                                    s32  Width,
                                                                    s32  Height );
void                            sdleng_DestroyDevice              ( void );
xbool                           sdleng_WaitForIdle                ( void );

//==============================================================================
//  FRAME AND PASS LIFETIME
//==============================================================================

xbool                           sdleng_AcquireCommandBuffer       ( void );
xbool                           sdleng_AcquireSwapchainTexture    ( void );
xbool                           sdleng_IsPresentPolicySupported   ( sdleng_present_policy Policy );
xbool                           sdleng_SetPresentPolicy           ( sdleng_present_policy Policy );
sdleng_present_policy          sdleng_GetPresentPolicy           ( void );
xbool                           sdleng_BeginRenderPass            ( const SDL_GPUColorTargetInfo*        pColorTargets,
                                                                    u32                                  ColorTargetCount,
                                                                    const SDL_GPUDepthStencilTargetInfo* pDepthStencilTarget );
void                            sdleng_EndRenderPass              ( void );
xbool                           sdleng_EndFrame                   ( void );
void                            sdleng_CancelFrame                ( void );
void                            sdleng_SetBackBufferViewport      ( void );
xbool                           sdleng_WasBackBufferRendered      ( void );

xbool                           sdleng_InFrame                    ( void );
xbool                           sdleng_InRenderPass               ( void );

//==============================================================================
//  ACCESSORS
//==============================================================================

SDL_GPUDevice*                  sdleng_GetDevice                  ( void );
SDL_Window*                     sdleng_GetWindow                  ( void );
SDL_GPUCommandBuffer*           sdleng_GetCommandBuffer           ( void );
SDL_GPURenderPass*              sdleng_GetRenderPass              ( void );
SDL_GPUTexture*                 sdleng_GetSwapchainTexture        ( void );
SDL_GPUTextureFormat            sdleng_GetSwapchainFormat         ( void );
f32                             sdleng_GetFramePacingWaitMs       ( void );
f32                             sdleng_GetRenderSubmitMs          ( void );
void                            sdleng_GetBackBufferSize          ( s32& Width,
                                                                    s32& Height );

SDL_GPUTexture*                 sdleng_GetGPUTexture              ( const shader_resource* pResource );
SDL_GPUBuffer*                  sdleng_GetGPUBuffer               ( const shader_resource* pResource );
SDL_GPUSampler*                 sdleng_GetGPUSampler              ( const rstate_sampler* pSampler );
SDL_GPUShader*                  sdleng_GetGPUShader               ( const shader* pShader );
const shader_resource_counts*   sdleng_GetShaderResourceCounts    ( const shader* pShader );

void                            sdleng_ResetPipelineBinding       ( void );
void                            sdleng_ResetBufferBindings        ( void );
void                            sdleng_ResetShaderBindings        ( void );
u32                             sdleng_GetPipelineCount           ( void );
u32                             sdleng_GetPipelineHandleCount     ( void );

#if defined(X_RETAIL)
inline void sdleng_ResetGraphicsBindingDebug( void ) {}
inline void sdleng_SetGraphicsPipelineDebug( const render_pipeline_backend* ) {}
inline void sdleng_ClearGraphicsPipelineDebug( const render_pipeline_backend* ) {}
inline void sdleng_RecordSamplerBindingDebug( shader_stage, u32 ) {}
inline xbool sdleng_ValidateGraphicsBindings( void ) { return TRUE; }
#else
void                            sdleng_ResetGraphicsBindingDebug  ( void );
void                            sdleng_SetGraphicsPipelineDebug   ( const render_pipeline_backend* pPipeline );
void                            sdleng_ClearGraphicsPipelineDebug ( const render_pipeline_backend* pPipeline );
void                            sdleng_RecordSamplerBindingDebug  ( shader_stage Stage,
                                                                    u32          Slot );
xbool                           sdleng_ValidateGraphicsBindings    ( void );
#endif

#endif // defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
#endif // SDLENG_PRIVATE_HPP
//==============================================================================
