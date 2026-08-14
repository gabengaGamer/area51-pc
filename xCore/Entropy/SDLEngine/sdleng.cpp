//==============================================================================
//
//  sdleng.cpp
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
//  GLOBAL BACKEND HANDLES
//==============================================================================

SDL_GPUDevice*  g_pSDLGPUDevice = NULL;

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static struct sdleng_locals
{
    sdleng_locals( void )
    {
        x_memset( this, 0, sizeof(sdleng_locals) );
        PresentPolicy     = SDLENG_PRESENT_VSYNC;
        ActivePresentMode = SDL_GPU_PRESENTMODE_VSYNC;
    }

    SDL_GPUCommandBuffer*       pCommandBuffer;
    SDL_GPURenderPass*          pRenderPass;
    SDL_GPUTexture*             pSwapchainTexture;
    SDL_GPUTextureFormat        SwapchainFormat;
    u32                         BackBufferWidth;
    u32                         BackBufferHeight;
    u32                         AcquiredWidth;
    u32                         AcquiredHeight;
    sdleng_present_policy       PresentPolicy;
    SDL_GPUPresentMode          ActivePresentMode;
    xtick                       RenderStartTime;
    f32                         FramePacingWaitMs;
    f32                         RenderSubmitMs;

    xbool                       bInitialized;
    xbool                       bWindowClaimed;
    xbool                       bWindowInitializedByDevice;
    xbool                       bSwapchainAcquired;
    xbool                       bBackBufferRendered;
    xbool                       bSwapchainAcquireLogged;
} s;

//==============================================================================
//  HELPERS
//==============================================================================

enum
{
    SDLENG_DEFAULT_BACKBUFFER_WIDTH  = 1024,
    SDLENG_DEFAULT_BACKBUFFER_HEIGHT = 768
};

static
void sdleng_LogSDLError( const char* pContext )
{
    const char* pError = SDL_GetError();
    if( !pError || !pError[0] )
        pError = "unknown SDL error";

    x_DebugMsg( "SDLEngine: %s failed: %s\n", pContext, pError );
}

//==============================================================================

static
u32 sdleng_NormalizeExtent( s32 Value, u32 Fallback )
{
    return (Value > 0) ? (u32)Value : Fallback;
}

//==============================================================================

static
void sdleng_ResetFrameState( void )
{
    s.pCommandBuffer     = NULL;
    s.pRenderPass        = NULL;
    s.pSwapchainTexture  = NULL;
    s.AcquiredWidth      = 0;
    s.AcquiredHeight     = 0;
    s.bSwapchainAcquired = FALSE;
    s.bBackBufferRendered = FALSE;
}

//==============================================================================

static
void sdleng_ResetDeviceState( void )
{
    sdleng_ResetFrameState();

    s.SwapchainFormat            = SDL_GPU_TEXTUREFORMAT_INVALID;
    s.ActivePresentMode          = SDL_GPU_PRESENTMODE_VSYNC;
    s.BackBufferWidth            = 0;
    s.BackBufferHeight           = 0;
    s.FramePacingWaitMs          = 0.0f;
    s.RenderSubmitMs             = 0.0f;
    s.bInitialized               = FALSE;
    s.bWindowClaimed             = FALSE;
    s.bWindowInitializedByDevice = FALSE;
    s.bSwapchainAcquireLogged    = FALSE;
}

//==============================================================================

static
void sdleng_UpdateBackBufferSizeFromWindow( u32 FallbackWidth, u32 FallbackHeight )
{
    if( sdleng_WindowGetSDLWindow() )
        sdleng_WindowRefreshClientSize();

    s32 ClientWidth  = 0;
    s32 ClientHeight = 0;
    sdleng_WindowGetClientSize( ClientWidth, ClientHeight );

    s.BackBufferWidth  = (ClientWidth  > 0) ? (u32)ClientWidth  : FallbackWidth;
    s.BackBufferHeight = (ClientHeight > 0) ? (u32)ClientHeight : FallbackHeight;
}

//==============================================================================

static
xbool sdleng_EnsureWindow( sdleng_native_window_handle hWindow, u32 Width, u32 Height )
{
    SDL_Window* pExistingWindow = sdleng_WindowGetSDLWindow();
    if( pExistingWindow )
    {
        if( hWindow && sdleng_WindowGetHandle() && (sdleng_WindowGetHandle() != hWindow) )
        {
            x_DebugMsg( "SDLEngine: existing SDL window does not match requested native window\n" );
            return FALSE;
        }

        g_pSDLWindow = pExistingWindow;
        sdleng_UpdateBackBufferSizeFromWindow( Width, Height );
        return TRUE;
    }

    sdleng_window_desc WindowDesc;
    x_memset( &WindowDesc, 0, sizeof(WindowDesc) );
    WindowDesc.hWindow     = hWindow;
    WindowDesc.Width       = Width;
    WindowDesc.Height      = Height;
    WindowDesc.pTitle      = "Dreamlnd 51";
    WindowDesc.DisplayMode = sdleng_WindowGetDisplayMode();

    if( !sdleng_WindowInit( WindowDesc ) )
        return FALSE;

    g_pSDLWindow                 = sdleng_WindowGetSDLWindow();
    s.bWindowInitializedByDevice = TRUE;

    sdleng_UpdateBackBufferSizeFromWindow( Width, Height );
    return g_pSDLWindow != NULL;
}

//==============================================================================

static
xbool sdleng_ToPresentMode( sdleng_present_policy Policy, SDL_GPUPresentMode& PresentMode )
{
    switch( Policy )
    {
        case SDLENG_PRESENT_IMMEDIATE: PresentMode = SDL_GPU_PRESENTMODE_IMMEDIATE; return TRUE;
        case SDLENG_PRESENT_MAILBOX:   PresentMode = SDL_GPU_PRESENTMODE_MAILBOX;   return TRUE;
        case SDLENG_PRESENT_VSYNC:     PresentMode = SDL_GPU_PRESENTMODE_VSYNC;     return TRUE;
        default:
        {
            x_DebugMsg( "SDLEngine: invalid present policy %d\n", (s32)Policy );
            return FALSE;
        }
    }
}

//==============================================================================

static
const char* sdleng_GetPresentModeName( SDL_GPUPresentMode Mode )
{
    switch( Mode )
    {
        case SDL_GPU_PRESENTMODE_IMMEDIATE: return "immediate";
        case SDL_GPU_PRESENTMODE_MAILBOX:   return "mailbox";
        case SDL_GPU_PRESENTMODE_VSYNC:     return "vsync";
        default:                            return "unknown";
    }
}

//==============================================================================

static
xbool sdleng_ConfigureSwapchain( void )
{
    if( !g_pSDLGPUDevice || !g_pSDLWindow )
        return FALSE;

    if( !SDL_WindowSupportsGPUSwapchainComposition( g_pSDLGPUDevice,
                                                    g_pSDLWindow,
                                                    SDLENG_GPU_SWAPCHAIN_COMPOSITION ) )
    {
        x_DebugMsg( "SDLEngine: requested swapchain composition is unsupported\n" );
        return FALSE;
    }

    SDL_GPUPresentMode PresentMode;
    if( !sdleng_ToPresentMode( s.PresentPolicy, PresentMode ) )
        return FALSE;

    if( !SDL_WindowSupportsGPUPresentMode( g_pSDLGPUDevice,
                                           g_pSDLWindow,
                                           PresentMode ) )
    {
        x_DebugMsg( "SDLEngine: requested present mode '%s' is unsupported\n",
                    sdleng_GetPresentModeName( PresentMode ) );
        return FALSE;
    }

    if( !SDL_SetGPUSwapchainParameters( g_pSDLGPUDevice,
                                        g_pSDLWindow,
                                        SDLENG_GPU_SWAPCHAIN_COMPOSITION,
                                        PresentMode ) )
    {
        sdleng_LogSDLError( "SDL_SetGPUSwapchainParameters" );
        return FALSE;
    }

    s.ActivePresentMode       = PresentMode;
    s.bSwapchainAcquireLogged = FALSE;

    s.SwapchainFormat = SDL_GetGPUSwapchainTextureFormat( g_pSDLGPUDevice, g_pSDLWindow );
    if( s.SwapchainFormat == SDL_GPU_TEXTUREFORMAT_INVALID )
    {
        x_DebugMsg( "SDLEngine: invalid swapchain texture format\n" );
        return FALSE;
    }

    sdleng_UpdateBackBufferSizeFromWindow( s.BackBufferWidth  ? s.BackBufferWidth  : SDLENG_DEFAULT_BACKBUFFER_WIDTH,
                                           s.BackBufferHeight ? s.BackBufferHeight : SDLENG_DEFAULT_BACKBUFFER_HEIGHT );
    x_DebugMsg( "SDLEngine: swapchain present=%s frames-in-flight=1 size=%ux%u\n",
                sdleng_GetPresentModeName( PresentMode ),
                s.BackBufferWidth,
                s.BackBufferHeight );

    return TRUE;
}

//==============================================================================

xbool sdleng_SetPresentPolicy( sdleng_present_policy Policy )
{
    if( (Policy < SDLENG_PRESENT_VSYNC) || (Policy > SDLENG_PRESENT_IMMEDIATE) )
        return FALSE;

    if( s.PresentPolicy == Policy )
        return TRUE;

    if( s.pCommandBuffer || s.pRenderPass || s.bSwapchainAcquired )
    {
        x_DebugMsg( "SDLEngine: present mode cannot change during an active frame\n" );
        return FALSE;
    }

    const sdleng_present_policy PreviousPolicy = s.PresentPolicy;
    s.PresentPolicy = Policy;

    if( s.bWindowClaimed && !sdleng_ConfigureSwapchain() )
    {
        s.PresentPolicy = PreviousPolicy;
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool sdleng_IsPresentPolicySupported( sdleng_present_policy Policy )
{
    if( !g_pSDLGPUDevice || !g_pSDLWindow || !s.bWindowClaimed )
    {
        return FALSE;
    }

    SDL_GPUPresentMode PresentMode;
    if( !sdleng_ToPresentMode( Policy, PresentMode ) )
    {
        return FALSE;
    }

    return SDL_WindowSupportsGPUPresentMode( g_pSDLGPUDevice,
                                              g_pSDLWindow,
                                              PresentMode );
}

//==============================================================================

sdleng_present_policy sdleng_GetPresentPolicy( void )
{
    if( s.bWindowClaimed )
    {
        switch( s.ActivePresentMode )
        {
            case SDL_GPU_PRESENTMODE_IMMEDIATE: return SDLENG_PRESENT_IMMEDIATE;
            case SDL_GPU_PRESENTMODE_MAILBOX:   return SDLENG_PRESENT_MAILBOX;
            case SDL_GPU_PRESENTMODE_VSYNC:
            default:                            return SDLENG_PRESENT_VSYNC;
        }
    }

    return s.PresentPolicy;
}

//==============================================================================

static
xbool sdleng_SubmitCurrentCommandBuffer( void )
{
    if( !s.pCommandBuffer )
        return TRUE;

    if( s.pRenderPass )
    {
        x_DebugMsg( "SDLEngine: cannot submit with an active render pass\n" );
        return FALSE;
    }

    if( !s.bSwapchainAcquired || !s.bBackBufferRendered )
    {
        x_DebugMsg( "SDLEngine: frame submit requires an acquired and rendered backbuffer\n" );
        return FALSE;
    }

    SDL_GPUCommandBuffer* pCommandBuffer = s.pCommandBuffer;
    const xtick RenderStartTime = s.RenderStartTime;
    sdleng_ResetFrameState();

    xbool bSubmitted;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Submit" );
        bSubmitted = SDL_SubmitGPUCommandBuffer( pCommandBuffer );
    }

    if( !bSubmitted )
    {
        sdleng_LogSDLError( "SDL_SubmitGPUCommandBuffer" );
        return FALSE;
    }

    s.RenderSubmitMs = RenderStartTime
                       ? x_TicksToMs( x_GetTime() - RenderStartTime )
                       : 0.0f;
    return TRUE;
}

//==============================================================================
//  BACKEND LIFETIME
//==============================================================================

xbool sdleng_CreateDeviceForWindow( sdleng_native_window_handle hWindow, s32 Width, s32 Height )
{
    if( s.bInitialized )
    {
        if( !hWindow || (sdleng_WindowGetHandle() == hWindow) )
            return TRUE;

        x_DebugMsg( "SDLEngine: device already initialized for another window\n" );
        return FALSE;
    }

    const u32 RequestedWidth  = sdleng_NormalizeExtent( Width,  SDLENG_DEFAULT_BACKBUFFER_WIDTH  );
    const u32 RequestedHeight = sdleng_NormalizeExtent( Height, SDLENG_DEFAULT_BACKBUFFER_HEIGHT );

    if( !sdleng_EnsureWindow( hWindow, RequestedWidth, RequestedHeight ) )
    {
        sdleng_DestroyDevice();
        return FALSE;
    }

#if defined(CONFIG_DEBUG) || defined(X_DEBUG)
    const bool DebugMode = true;
#else
    const bool DebugMode = false;
#endif

    g_pSDLGPUDevice = SDL_CreateGPUDevice( SDLENG_GPU_SHADER_FORMATS,
                                           DebugMode,
                                           SDLENG_GPU_DRIVER_NAME );
    if( !g_pSDLGPUDevice )
    {
        sdleng_LogSDLError( "SDL_CreateGPUDevice" );
        sdleng_DestroyDevice();
        return FALSE;
    }

    if( !SDL_SetGPUAllowedFramesInFlight( g_pSDLGPUDevice, 1 ) )
    {
        sdleng_LogSDLError( "SDL_SetGPUAllowedFramesInFlight(1)" );
        sdleng_DestroyDevice();
        return FALSE;
    }

    if( !SDL_ClaimWindowForGPUDevice( g_pSDLGPUDevice, g_pSDLWindow ) )
    {
        sdleng_LogSDLError( "SDL_ClaimWindowForGPUDevice" );
        sdleng_DestroyDevice();
        return FALSE;
    }
    s.bWindowClaimed = TRUE;

    if( !sdleng_ConfigureSwapchain() )
    {
        sdleng_DestroyDevice();
        return FALSE;
    }

    s.bInitialized = TRUE;

    x_GetProfiler().SetProperty( "Renderer.Driver", SDL_GetGPUDeviceDriver( g_pSDLGPUDevice ) );
    x_DebugMsg( "SDLEngine: GPU device initialized using driver '%s' shader formats 0x%08X\n",
                SDL_GetGPUDeviceDriver( g_pSDLGPUDevice ),
                SDL_GetGPUShaderFormats( g_pSDLGPUDevice ) );

    return TRUE;
}

//==============================================================================

void sdleng_DestroyDevice( void )
{
    const xbool bDestroyWindow = s.bWindowInitializedByDevice;

    if( s.pRenderPass )
    {
        x_DebugMsg( "SDLEngine: closing render pass during device teardown\n" );
        sdleng_EndRenderPass();
    }
    sdleng_CancelFrame();
    sdleng_WaitForIdle();

    if( s.bWindowClaimed && g_pSDLGPUDevice && g_pSDLWindow )
    {
        SDL_ReleaseWindowFromGPUDevice( g_pSDLGPUDevice, g_pSDLWindow );
        s.bWindowClaimed = FALSE;
    }

    if( g_pSDLGPUDevice )
    {
        SDL_DestroyGPUDevice( g_pSDLGPUDevice );
        g_pSDLGPUDevice = NULL;
    }

    if( bDestroyWindow )
        sdleng_WindowKill();

    g_pSDLWindow = sdleng_WindowGetSDLWindow();
    sdleng_ResetDeviceState();
}

//==============================================================================

xbool sdleng_WaitForIdle( void )
{
    if( !g_pSDLGPUDevice )
        return TRUE;

    if( !SDL_WaitForGPUIdle( g_pSDLGPUDevice ) )
    {
        sdleng_LogSDLError( "SDL_WaitForGPUIdle" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  FRAME AND PASS LIFETIME
//==============================================================================

xbool sdleng_AcquireCommandBuffer( void )
{
    if( !s.bInitialized || !g_pSDLGPUDevice )
        return FALSE;

    if( s.pCommandBuffer )
        return TRUE;

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "AcquireCommandBuffer" );
        s.pCommandBuffer = SDL_AcquireGPUCommandBuffer( g_pSDLGPUDevice );
    }
    if( !s.pCommandBuffer )
    {
        sdleng_LogSDLError( "SDL_AcquireGPUCommandBuffer" );
        return FALSE;
    }

    s.RenderStartTime = 0;
    return TRUE;
}

xbool sdleng_AcquireSwapchainTexture( void )
{
    if( !s.bInitialized || !g_pSDLGPUDevice || !g_pSDLWindow )
        return FALSE;

    if( s.pRenderPass )
        return FALSE;

    if( s.bSwapchainAcquired )
        return TRUE;

    sdleng_UpdateBackBufferSizeFromWindow( s.BackBufferWidth  ? s.BackBufferWidth  : SDLENG_DEFAULT_BACKBUFFER_WIDTH,
                                           s.BackBufferHeight ? s.BackBufferHeight : SDLENG_DEFAULT_BACKBUFFER_HEIGHT );

    if( !s.pCommandBuffer )
    {
        x_DebugMsg( "SDLEngine: swapchain acquire requires an active command buffer\n" );
        return FALSE;
    }

    const xtick AcquireStart = x_GetTime();
    xbool bAcquired;
    {
        X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.WaitForSwapchain" );
        bAcquired = SDL_WaitAndAcquireGPUSwapchainTexture( s.pCommandBuffer,
                                                           g_pSDLWindow,
                                                           &s.pSwapchainTexture,
                                                           &s.AcquiredWidth,
                                                           &s.AcquiredHeight );
    }
    s.FramePacingWaitMs = x_TicksToMs( x_GetTime() - AcquireStart );

    if( !bAcquired )
    {
        sdleng_LogSDLError( "SDL_WaitAndAcquireGPUSwapchainTexture" );
        return FALSE;
    }

    if( !s.pSwapchainTexture )
    {
        return TRUE;
    }

    s.bSwapchainAcquired = TRUE;

    if( s.AcquiredWidth && s.AcquiredHeight )
    {
        s.BackBufferWidth  = s.AcquiredWidth;
        s.BackBufferHeight = s.AcquiredHeight;
    }

    if( !s.bSwapchainAcquireLogged )
    {
        x_DebugMsg( "SDLEngine: swapchain acquire=%1.3fms size=%ux%u\n",
                    s.FramePacingWaitMs,
                    s.BackBufferWidth,
                    s.BackBufferHeight );
        s.bSwapchainAcquireLogged = TRUE;
    }

    return TRUE;
}

//==============================================================================

xbool sdleng_BeginRenderPass( const SDL_GPUColorTargetInfo*        pColorTargets,
                              u32                                  ColorTargetCount,
                              const SDL_GPUDepthStencilTargetInfo* pDepthStencilTarget )
{
    if( !s.pCommandBuffer )
    {
        x_DebugMsg( "SDLEngine: render pass requires an active frame lifecycle\n" );
        return FALSE;
    }

    if( s.pRenderPass )
        return FALSE;

    if( ColorTargetCount && !pColorTargets )
        return FALSE;

    if( !ColorTargetCount && !pDepthStencilTarget )
        return FALSE;

    static xprofile_counter RenderPassCountMetric =
        x_GetProfiler().RegisterCounter( "RenderPassCalls", "Renderer" );
    RenderPassCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick RenderStart = x_GetTime();
    const xtick BeginStart = bFineTiming ? RenderStart : 0;
    s.pRenderPass = SDL_BeginGPURenderPass( s.pCommandBuffer,
                                            pColorTargets,
                                            ColorTargetCount,
                                            pDepthStencilTarget );
    const xtick BeginEnd = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone RenderPassMetric =
            x_GetProfiler().RegisterZone( "RenderPassAPI", "RendererAPI" );
        RenderPassMetric.Record( BeginEnd - BeginStart );
    }
    if( !s.pRenderPass )
    {
        sdleng_LogSDLError( "SDL_BeginGPURenderPass" );
        return FALSE;
    }

    if( s.RenderStartTime == 0 )
    {
        s.RenderStartTime = RenderStart;
    }

    for( u32 iTarget = 0; iTarget < ColorTargetCount; iTarget++ )
    {
        if( pColorTargets[iTarget].texture == s.pSwapchainTexture )
        {
            s.bBackBufferRendered = TRUE;
            break;
        }
    }

    sdleng_ResetPipelineBinding();
    sdleng_ResetBufferBindings();
    sdleng_ResetShaderBindings();
    sdleng_ResetGraphicsBindingDebug();
    return TRUE;
}

//==============================================================================

void sdleng_EndRenderPass( void )
{
    if( !s.pRenderPass )
        return;

    static xprofile_counter RenderPassCountMetric =
        x_GetProfiler().RegisterCounter( "RenderPassCalls", "Renderer" );
    RenderPassCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick EndStart = bFineTiming ? x_GetTime() : 0;
    SDL_EndGPURenderPass( s.pRenderPass );
    const xtick EndEnd = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone RenderPassMetric =
            x_GetProfiler().RegisterZone( "RenderPassAPI", "RendererAPI" );
        RenderPassMetric.Record( EndEnd - EndStart );
    }
    s.pRenderPass = NULL;
    sdleng_ResetPipelineBinding();
    sdleng_ResetBufferBindings();
    sdleng_ResetShaderBindings();
    sdleng_ResetGraphicsBindingDebug();
}

//==============================================================================

xbool sdleng_EndFrame( void )
{
    return sdleng_SubmitCurrentCommandBuffer();
}

//==============================================================================

void sdleng_CancelFrame( void )
{
    if( !s.pCommandBuffer )
    {
        return;
    }

    if( s.pRenderPass )
    {
        x_DebugMsg( "SDLEngine: cannot cancel a command buffer with an active render pass\n" );
        ASSERT( FALSE );
        return;
    }

    SDL_GPUCommandBuffer* pCommandBuffer = s.pCommandBuffer;
    const xbool bSwapchainAcquired = s.bSwapchainAcquired;
    sdleng_ResetFrameState();

    if( bSwapchainAcquired )
    {
        if( !SDL_SubmitGPUCommandBuffer( pCommandBuffer ) )
            sdleng_LogSDLError( "SDL_SubmitGPUCommandBuffer" );
    }
    else
    {
        if( !SDL_CancelGPUCommandBuffer( pCommandBuffer ) )
            sdleng_LogSDLError( "SDL_CancelGPUCommandBuffer" );
    }

}

//==============================================================================

void sdleng_SetBackBufferViewport( void )
{
    if( !s.pRenderPass )
        return;

    SDL_GPUViewport Viewport;
    Viewport.x         = 0.0f;
    Viewport.y         = 0.0f;
    Viewport.w         = (f32)(s.AcquiredWidth  ? s.AcquiredWidth  : s.BackBufferWidth);
    Viewport.h         = (f32)(s.AcquiredHeight ? s.AcquiredHeight : s.BackBufferHeight);
    Viewport.min_depth = 0.0f;
    Viewport.max_depth = 1.0f;

    static xprofile_counter DynamicStateCountMetric =
        x_GetProfiler().RegisterCounter( "DynamicStateCalls", "Renderer" );
    DynamicStateCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start = bFineTiming ? x_GetTime() : 0;
    SDL_SetGPUViewport( s.pRenderPass, &Viewport );
    const xtick End = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone DynamicStateMetric =
            x_GetProfiler().RegisterZone( "DynamicStateAPI", "RendererAPI" );
        DynamicStateMetric.Record( End - Start );
    }
}

//==============================================================================

xbool sdleng_InFrame( void )
{
    return s.pCommandBuffer != NULL;
}

//==============================================================================

xbool sdleng_InRenderPass( void )
{
    return s.pRenderPass != NULL;
}

//==============================================================================

xbool sdleng_WasBackBufferRendered( void )
{
    return s.bBackBufferRendered;
}

//==============================================================================
//  ACCESSORS
//==============================================================================

SDL_GPUDevice* sdleng_GetDevice( void )
{
    return g_pSDLGPUDevice;
}

//==============================================================================

SDL_Window* sdleng_GetWindow( void )
{
    return g_pSDLWindow;
}

//==============================================================================

SDL_GPUCommandBuffer* sdleng_GetCommandBuffer( void )
{
    return s.pCommandBuffer;
}

//==============================================================================

SDL_GPURenderPass* sdleng_GetRenderPass( void )
{
    return s.pRenderPass;
}

//==============================================================================

SDL_GPUTexture* sdleng_GetSwapchainTexture( void )
{
    return s.pSwapchainTexture;
}

//==============================================================================

SDL_GPUTextureFormat sdleng_GetSwapchainFormat( void )
{
    return s.SwapchainFormat;
}

//==============================================================================

f32 sdleng_GetFramePacingWaitMs( void )
{
    return s.FramePacingWaitMs;
}

//==============================================================================

f32 sdleng_GetRenderSubmitMs( void )
{
    return s.RenderSubmitMs;
}

//==============================================================================

void sdleng_GetBackBufferSize( s32& Width, s32& Height )
{
    if( !s.bSwapchainAcquired )
    {
        sdleng_UpdateBackBufferSizeFromWindow( s.BackBufferWidth  ? s.BackBufferWidth  : SDLENG_DEFAULT_BACKBUFFER_WIDTH,
                                               s.BackBufferHeight ? s.BackBufferHeight : SDLENG_DEFAULT_BACKBUFFER_HEIGHT );
    }

    Width  = (s32)s.BackBufferWidth;
    Height = (s32)s.BackBufferHeight;
}

//==============================================================================
#endif // defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
