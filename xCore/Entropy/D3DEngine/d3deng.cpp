//==============================================================================
//
//  d3deng.cpp
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//=========================================================================
// INCLUDES
//=========================================================================

#include "x_stdio.hpp"
#include "e_Engine.hpp"
#include "x_threads.hpp"

#include "d3deng_font.hpp"
#include "d3deng_shader.hpp"
#include "d3deng_state.hpp"
#include "d3deng_rtarget.hpp"
#include "d3deng_composite.hpp"

//=========================================================================
// INCLUDE ADITIONAL LIBRARIES
//=========================================================================

#pragma comment( lib, "winmm" )

// Auto include DirectX libs
#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )

//=========================================================================
// DEFINES
//=========================================================================

#define SCRACH_MEM_SIZE     (2*1024*1024)

//=========================================================================
// EXTERNAL DEPENDENCIES
//=========================================================================

xbool d3deng_InitInput  ( HWND Window );

void  vram_Init         ( void );
void  vram_Kill         ( void );

void  smem_Init         ( s32 NBytes );
void  smem_Kill         ( void );

void  draw_Init         ( void );
void  draw_Kill         ( void );

//=========================================================================
// GLOBAL VARIABLES
//=========================================================================

ID3D11Device*           g_pd3dDevice = NULL;
ID3D11DeviceContext*    g_pd3dContext = NULL;
IDXGISwapChain*         g_pSwapChain = NULL;
DXGI_SWAP_CHAIN_DESC    g_SwapChainDesc;

//=========================================================================
// LOCAL VARIABLES
//=========================================================================

static struct eng_locals
{
    eng_locals( void ) { memset( this, 0, sizeof(eng_locals) ); }

    //
    // general variables
    //
    HINSTANCE       hInst;
    HWND            Wnd;
    HWND            WndParent;
    HWND            WndDisplay;
    u32             Mode;
    xcolor          BackColor;
    xbool           inRenderPass;
    xbool           FrameCleared;

    //
    // View variables
    //
    view            View;

    //
    // Window manechment related varaibles
    //
    xbool               Active;
    xbool               Ready;
    xbool               bInModeChange;
    RECT                rcWindowRect;
    d3deng_display_mode DisplayMode;

    //
    // FPS variables
    //
    xtick           FPSFrameTime[8];
    xtick           FPSLastTime;
    s32             FPSIndex;
    xtimer          CPUTIMER;
    f32             CPUMS;
    f32             IMS;


} s;

static xarray<const eng_frame_stage*>   s_FrameStages;

//=========================================================================
// FUNCTIONS
//=========================================================================

static void  d3deng_ReleaseSwapChain( void );
static void  d3deng_ReleaseDevice( void );
static xbool d3deng_RecreateSwapChain( HWND hWnd, u32 Width, u32 Height );

//=========================================================================

static
const char* d3deng_GetDisplayModeName( d3deng_display_mode Mode )
{
    switch( Mode )
    {
        case ENG_DISPLAY_WINDOWED:     return "Windowed";
        case ENG_DISPLAY_BORDERLESS:   return "Borderless";
        default:                       return "Unknown";
    }
}

//=========================================================================

static
void ConvertComandLine( s32* pargc, char* argv[], LPSTR lpCmdLine )
{
    s32 argc = 1;

    if( *lpCmdLine )
    {
        argv[1] = lpCmdLine;
        argc = 2;

        do
        {
            if( *lpCmdLine == ' ' )
            {
                do
                {
                    *lpCmdLine = 0;
                    lpCmdLine++;

                } while( *lpCmdLine == ' ' );

                if( *lpCmdLine == 0 ) break;
                argv[argc++] = lpCmdLine;
            }

            lpCmdLine++;

        } while( *lpCmdLine );
    }

    *pargc = argc;
}

//=========================================================================

void d3deng_EntryPoint( s32& argc, char**& argv, HINSTANCE h1, HINSTANCE h2, LPSTR str1, INT i1 )
{
    static char* ArgvBuff[256] = { NULL };
    argv = ArgvBuff;
    ConvertComandLine( &argc, argv, str1 );
    s.hInst = h1;

    x_memset( s.FPSFrameTime, 0, sizeof(s.FPSFrameTime) );
    s.FPSLastTime = x_GetTime();

    x_Init(argc,argv);
}

//=========================================================================

void eng_Kill( void ) //Deprecated, but i still prefer to maintenance this func
{
    x_DebugMsg( "=== ENGINE SHUTDOWN START ===\n" );

    // Free sub modules (in reverse order of initialization)
    
    draw_Kill();
    x_DebugMsg( "Engine: Draw system shutdown\n" );
    
    smem_Kill();
    x_DebugMsg( "Engine: Scratch memory shutdown\n" );
    
    vram_Kill();
    x_DebugMsg( "Engine: VRAM system shutdown\n" );
    
    font_Kill();
    x_DebugMsg( "Engine: Font system shutdown\n" );
    
    text_Kill();
    x_DebugMsg( "Engine: Text system shutdown\n" );
    
    shader_Kill();
    x_DebugMsg( "Engine: Shader system shutdown\n" );
    
    state_Kill();
    x_DebugMsg( "Engine: Render state system shutdown\n" );   

    composite_Kill();
    x_DebugMsg( "Engine: Composite system shutdown\n" );

    rtarget_Kill();
    x_DebugMsg( "Engine: Render target system shutdown\n" );

    // Stop the d3d engine
    d3deng_ReleaseSwapChain();
    d3deng_ReleaseDevice();

    UnregisterClass( "Render Window", s.hInst );
    x_DebugMsg( "Engine: Window class unregistered\n" );

    s_FrameStages.Clear();

    x_DebugMsg( "=== ENGINE SHUTDOWN COMPLETE ===\n" );
}


//=========================================================================

s32 d3deng_ExitPoint( void )
{
    //eng_Kill();
    SetThreadExecutionState( ES_CONTINUOUS );
    x_Kill();
    return 0;
}

//=========================================================================

void d3deng_SetPresets( u32 Mode )
{
    s.Mode = Mode;
}

//=============================================================================

u32 d3deng_GetMode( void )
{
    return s.Mode ;
}

//=============================================================================

static
void d3deng_CalculateWindowLayout( s32 ReqW, s32 ReqH, d3deng_display_mode Mode,
                                    s32& OutW, s32& OutH, DWORD& OutStyle )
{
    const s32 screenW = GetSystemMetrics( SM_CXSCREEN );
    const s32 screenH = GetSystemMetrics( SM_CYSCREEN );
    const s32 targetW = MIN( ReqW, screenW );
    const s32 targetH = MIN( ReqH, screenH );

    switch( Mode )
    {
        case ENG_DISPLAY_WINDOWED:
        {
            RECT rect = { 0, 0, targetW, targetH };
            AdjustWindowRect( &rect, WS_OVERLAPPEDWINDOW, FALSE );

            if( (rect.right - rect.left) >= screenW || (rect.bottom - rect.top) >= screenH )
            {
                x_DebugMsg( "Engine: Requested windowed %dx%d, auto-switching to borderless %dx%d\n",
                            targetW, targetH, screenW, screenH );
                OutW     = screenW;
                OutH     = screenH;
                OutStyle = WS_POPUP | WS_VISIBLE;
            }
            else
            {
                OutW     = targetW;
                OutH     = targetH;
                OutStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
            }
            break;
        }

        case ENG_DISPLAY_BORDERLESS:
            OutW     = screenW;
            OutH     = screenH;
            OutStyle = WS_POPUP | WS_VISIBLE;
            break;

    }
}

//=============================================================================

void d3deng_SetDisplayMode( d3deng_display_mode Mode )
{
    if( Mode < ENG_DISPLAY_WINDOWED || Mode > ENG_DISPLAY_BORDERLESS )
        s.DisplayMode = ENG_DISPLAY_WINDOWED;
    else
        s.DisplayMode = Mode;
}

//=============================================================================

d3deng_display_mode d3deng_GetDisplayMode( void )
{
    return s.DisplayMode;
}

//=============================================================================

static
void d3deng_UpdateFPS( void )
{
    xtick CurrentTime;

    CurrentTime                     = x_GetTime();
    s.FPSFrameTime[ s.FPSIndex ]    = CurrentTime - s.FPSLastTime;
    s.FPSLastTime                   = CurrentTime;
    s.FPSIndex                     += 1;
    s.FPSIndex                     &= 0x07;
}

//=============================================================================

f32 eng_GetFPS( void )
{
    xtick Sum = 0;

    for (int i = 0; i < 8; ++i)
        Sum += s.FPSFrameTime[i];

    return( (f32)(s32)(((8.0f / x_TicksToMs( Sum )) * 1000.0f) + 0.5f) );
}

//=========================================================================

static
void d3deng_SetupPresentationParams( s32 XRes, s32 YRes )
{
    // Clear presentation parameters	
    ZeroMemory( &g_SwapChainDesc, sizeof(g_SwapChainDesc) );

    x_DebugMsg( "Engine: Setting up presentation params %dx%d (%s)\n",
                XRes, YRes, d3deng_GetDisplayModeName( s.DisplayMode ) );

    // Setup common parameters
    g_SwapChainDesc.Windowed              = TRUE;
    g_SwapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    g_SwapChainDesc.BufferDesc.Format     = DXGI_FORMAT_R8G8B8A8_UNORM;
    g_SwapChainDesc.BufferDesc.Width      = XRes;
    g_SwapChainDesc.BufferDesc.Height     = YRes;
    g_SwapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    g_SwapChainDesc.BufferCount           = 2;
    g_SwapChainDesc.SampleDesc.Count      = 1;
    g_SwapChainDesc.SampleDesc.Quality    = 0;
    g_SwapChainDesc.Flags                 = 0;

    x_DebugMsg( "Engine: Presentation params setup complete\n" );
}

//=========================================================================

static
void d3deng_ReleaseSwapChain( void )
{
    if( g_pSwapChain != NULL )
    {
        g_pSwapChain->Release();
        g_pSwapChain = NULL;
        x_DebugMsg( "Engine: Swap chain released\n" );
    }
}

//=========================================================================

static
void d3deng_ReleaseDevice( void )
{
    if( g_pd3dContext != NULL )
    {
        g_pd3dContext->Release();
        g_pd3dContext = NULL;
    }

    if( g_pd3dDevice != NULL )
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }
}

//=========================================================================

static
xbool d3deng_CreateD3DDevice( void )
{
    HRESULT Error;
    UINT    dwFlags = 0;

#ifdef X_DEBUG
    //dwFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Set DX11
    D3D_FEATURE_LEVEL requestedLevel = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL createdLevel;

    // Determine driver type
    D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
    if( s.Mode & ENG_ACT_SOFTWARE )
    {
        driverType = D3D_DRIVER_TYPE_WARP;
        x_DebugMsg( "Engine: Using WARP software renderer\n" );
    }
    else
    {
        x_DebugMsg( "Engine: Using hardware renderer\n" );
    }

    // Create the D3D device and context
    Error = D3D11CreateDevice(
        NULL,                       // Use default adapter
        driverType,                 // Hardware or WARP for software
        NULL,                       // No software device
        dwFlags,                    // Device creation flags
        &requestedLevel,            // Featured level
        1,                          // Number of feature levels
        D3D11_SDK_VERSION,          // SDK version
        &g_pd3dDevice,              // Output device
        &createdLevel,              // Output feature level
        &g_pd3dContext              // Output device context
    );

    // Handle creation errors
    if( Error != S_OK )
    {
        x_DebugMsg( "Engine: ERROR - Failed to create device (HRESULT=0x%08X)\n", Error );
        d3deng_ReleaseDevice();
        MessageBox( s.Wnd,
                   xfs("Error creating device: %d", Error),
                   "Device Error", MB_OK );
        return FALSE;
    }

    x_DebugMsg( "Engine: D3D11 device created successfully\n" );
    return TRUE;
}

//=========================================================================

static
xbool d3deng_CreateSwapChain( HWND hWnd )
{
    HRESULT Error;
    IDXGIDevice* pDXGIDevice = NULL;
    IDXGIAdapter* pAdapter = NULL;
    IDXGIFactory* pFactory = NULL;

    if( !g_pd3dDevice )
        return FALSE;

    g_SwapChainDesc.OutputWindow = hWnd;

    Error = g_pd3dDevice->QueryInterface( __uuidof(IDXGIDevice), (void**)&pDXGIDevice );
    if( Error == S_OK )
        Error = pDXGIDevice->GetAdapter( &pAdapter );
    if( Error == S_OK )
        Error = pAdapter->GetParent( __uuidof(IDXGIFactory), (void**)&pFactory );
    if( Error == S_OK )
        Error = pFactory->CreateSwapChain( g_pd3dDevice, &g_SwapChainDesc, &g_pSwapChain );

    if( pFactory )   pFactory->Release();
    if( pAdapter )   pAdapter->Release();
    if( pDXGIDevice) pDXGIDevice->Release();

    // Handle creation errors
    if( Error != S_OK )
    {
        x_DebugMsg( "Engine: ERROR - Failed to create swap chain (HRESULT=0x%08X)\n", Error );
        d3deng_ReleaseSwapChain();
        MessageBox( s.Wnd,
                   xfs("Error creating swap chain: %d", Error),
                   "Swap Chain Error", MB_OK );
        return FALSE;
    }

    return TRUE;
}

//=============================================================================

void d3deng_RegisterFrameStage( const eng_frame_stage& Stage )
{
    const eng_frame_stage* pStage = &Stage;

    for( s32 iStage = 0; iStage < s_FrameStages.GetCount(); ++iStage )
    {
        if( s_FrameStages[iStage] == pStage )
            return;
    }

    s_FrameStages.Append() = pStage;
}

//=========================================================================

void d3deng_UnregisterFrameStage( const eng_frame_stage& Stage )
{
    const eng_frame_stage* pStage = &Stage;

    for( s32 iStage = 0; iStage < s_FrameStages.GetCount(); ++iStage )
    {
        if( s_FrameStages[iStage] == pStage )
        {
            s_FrameStages.Delete( iStage );
            break;
        }
    }
}

//=========================================================================

typedef void (*stage_fn_t)( void );

static
void d3deng_RunFrameStages( stage_fn_t eng_frame_stage::* fn )
{
    for( s32 iStage = 0; iStage < s_FrameStages.GetCount(); )
    {
        const eng_frame_stage* pStage = s_FrameStages[iStage];
        if( pStage && (pStage->*fn) )
            (pStage->*fn)();

        if( (iStage < s_FrameStages.GetCount()) && (s_FrameStages[iStage] == pStage) )
            iStage++;
    }
}

//=========================================================================

static
void d3deng_UpdateSwapChain( HWND hWnd )
{
    // Read the new client area size from the window
    RECT clientRect;
    GetClientRect( hWnd, &clientRect );
    u32 width  = (u32)(clientRect.right  - clientRect.left);
    u32 height = (u32)(clientRect.bottom - clientRect.top);

    // Unbind all render targets before touching the swap chain buffers
    if( g_pd3dContext )
        g_pd3dContext->OMSetRenderTargets( 0, NULL, NULL );

    if( g_pSwapChain && width > 0 && height > 0 )
    {
        x_DebugMsg( "Engine: ResizeBuffers %dx%d\n", width, height );
        rtarget_ReleaseBackBufferTargets();
        g_SwapChainDesc.BufferDesc.Width  = width;
        g_SwapChainDesc.BufferDesc.Height = height;
        HRESULT hr = g_pSwapChain->ResizeBuffers( g_SwapChainDesc.BufferCount, width, height, DXGI_FORMAT_UNKNOWN, 0 );
        if( FAILED(hr) )
        {
            x_DebugMsg( "Engine: ResizeBuffers failed 0x%08X\n", hr );
            if( !d3deng_RecreateSwapChain( hWnd, width, height ) )
                x_DebugMsg( "Engine: ERROR - Failed to recreate swap chain after resize\n" );
        }
        else
        {
            // Re-bind the dependents
            rtarget_SetBackBuffer();
            rtarget_NotifyResolutionChanged();
        }
    }

    d3deng_UpdateDisplayWindow( hWnd );
}

//==============================================================================

static
xbool d3deng_RecreateSwapChain( HWND hWnd, u32 Width, u32 Height )
{
    x_DebugMsg( "Engine: Recreating swap chain\n" );

    rtarget_ReleaseBackBufferTargets();
    d3deng_ReleaseSwapChain();

    g_SwapChainDesc.BufferDesc.Width  = Width;
    g_SwapChainDesc.BufferDesc.Height = Height;

    if( !d3deng_CreateSwapChain( hWnd ) )
        return FALSE;

    rtarget_SetBackBuffer();
    rtarget_NotifyResolutionChanged();

    return TRUE;
}

//==============================================================================

xbool d3deng_ChangeDisplayMode( s32 Width, s32 Height, DXGI_FORMAT Format, d3deng_display_mode DisplayMode )
{
    if( Width <= 0 || Height <= 0 || !g_pSwapChain )
        return FALSE;

    // Resolve the final client dimensions and window style for the requested mode
    s32   clientW, clientH;
    DWORD style;
    d3deng_CalculateWindowLayout( Width, Height, DisplayMode, clientW, clientH, style );

    s.DisplayMode = DisplayMode;

    g_SwapChainDesc.BufferDesc.Format = Format;
    g_SwapChainDesc.BufferDesc.Width  = clientW;
    g_SwapChainDesc.BufferDesc.Height = clientH;

    // Set the flag so that WM_SIZE messages generated by the resize calls
    // below do not trigger a redundant swap chain update
    s.bInModeChange = TRUE;

    if( s.Wnd )
    {
        // Apply the new style and dimensions to the window.
        RECT wr = { 0, 0, clientW, clientH };
        AdjustWindowRect( &wr, style, FALSE );

        SetWindowLong( s.Wnd, GWL_STYLE, style );
        MoveWindow( s.Wnd, 0, 0, wr.right - wr.left, wr.bottom - wr.top, TRUE );
        ShowWindow( s.Wnd, SW_SHOW );
        UpdateWindow( s.Wnd );
    }

    // Release the back buffer before resizing
    rtarget_ReleaseBackBufferTargets();

    HRESULT hr = g_pSwapChain->ResizeBuffers(
        g_SwapChainDesc.BufferCount,
        clientW, clientH,
        Format,
        0
    );

    if( FAILED(hr) )
    {
        // Attempt a full swap chain recreation as a last resot
        x_DebugMsg( "Engine: ResizeBuffers failed 0x%08X\n", hr );
        if( !d3deng_RecreateSwapChain( s.Wnd, clientW, clientH ) )
        {
            x_DebugMsg( "Engine: ERROR - Failed to recreate swap chain\n" );
            s.bInModeChange = FALSE;
            return FALSE;
        }
    }

    // Re-bind the dependents
    rtarget_SetBackBuffer();
    d3deng_UpdateDisplayWindow( s.Wnd );
    rtarget_NotifyResolutionChanged();

    s.bInModeChange = FALSE;
    return TRUE;
}

//==============================================================================

void d3deng_UpdateDisplayWindow( HWND hWindow )
{
    // Store the display window and refresh our cached client rectangle
    s.WndDisplay = hWindow;

    GetClientRect( s.WndDisplay, &s.rcWindowRect );

    DWORD dwRenderWidth  = s.rcWindowRect.right  - s.rcWindowRect.left;
    DWORD dwRenderHeight = s.rcWindowRect.bottom - s.rcWindowRect.top;

    // Set the D3D11 viewport to cover the full client area of the window
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = (f32)dwRenderWidth;
    vp.Height   = (f32)dwRenderHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    if( g_pd3dContext )
        g_pd3dContext->RSSetViewports( 1, &vp );
}

//==============================================================================

static
void InitializeD3D( HWND hWnd, s32 XRes, s32 YRes )
{
    x_DebugMsg( "Engine: Initializing DirectX 11 (%dx%d)\n", XRes, YRes );

    // Setup presentation parameters
    d3deng_SetupPresentationParams( XRes, YRes );

    // Create D3D device
    if( !d3deng_CreateD3DDevice() )
    {
        x_DebugMsg( "Engine: ERROR - Failed to create D3D device\n" );
        return;
    }

    // Create swap chain
    if( !d3deng_CreateSwapChain( hWnd ) )
    {
        x_DebugMsg( "Engine: ERROR - Failed to create swap chain\n" );
        d3deng_ReleaseDevice();
        return;
    }

    // Initialize render target system
    rtarget_Init();
    rtarget_SetBackBuffer();

    ASSERT( g_pSwapChain && "Failed to create render targets" );

    d3deng_UpdateDisplayWindow( hWnd );

    x_DebugMsg( "Engine: DirectX 11 initialized\n" );
}

//=============================================================================

LRESULT CALLBACK eng_D3DWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch( uMsg )
    {
        case WM_ACTIVATE:
            s.Active = LOWORD(wParam) != WA_INACTIVE;
            break;

        case WM_ACTIVATEAPP:
            s.Active = (BOOL)wParam;
            if( !s.Active )
                SetThreadExecutionState( ES_CONTINUOUS );
            break;

        case WM_SIZE:
            // Check to see if we are losing or gaining our window
            if( SIZE_MAXHIDE==wParam || SIZE_MINIMIZED==wParam )
                s.Active = FALSE;
            else
                s.Active = TRUE;

            if( s.Active && s.Ready && !s.bInModeChange )
            {
                d3deng_UpdateSwapChain( hWnd );
                s.Ready = TRUE;
            }
            break;

        case WM_CLOSE:
            DestroyWindow( hWnd );
            return 0;

        case WM_DESTROY:
            SetThreadExecutionState( ES_CONTINUOUS );
            PostQuitMessage(0);
            return 0L;
    }
    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

//=========================================================================

static
HWND CreateWin( s32 Width, s32 Height )
{
    WNDCLASS wndClass = { CS_HREDRAW | CS_VREDRAW, eng_D3DWndProc, 0, 0, s.hInst,
                          NULL, LoadCursor(NULL, IDC_ARROW),
                          (HBRUSH)GetStockObject(WHITE_BRUSH), NULL,
                          TEXT("Render Window") };
    RegisterClass( &wndClass );

    s32   clientW, clientH;
    DWORD style;
    d3deng_CalculateWindowLayout( Width, Height, s.DisplayMode, clientW, clientH, style );

    RECT wr = { 0, 0, clientW, clientH };
    AdjustWindowRect( &wr, style, FALSE );

    return CreateWindow( TEXT("Render Window"), TEXT("Dreamlnd 51"),
                         style,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         wr.right - wr.left, wr.bottom - wr.top,
                         0L, 0L, s.hInst, 0L );
}

//=========================================================================

void d3deng_SetWindowHandle( HWND hWindow )
{
    s.Wnd = hWindow;
}

//=========================================================================

void d3deng_SetParentHandle( HWND hWindow )
{
    s.WndParent = hWindow;
}

//=========================================================================

static
void d3deng_GetClearColor( f32 out[4] )
{
    out[0] = s.BackColor.R / 255.0f;
    out[1] = s.BackColor.G / 255.0f;
    out[2] = s.BackColor.B / 255.0f;
    out[3] = s.BackColor.A / 255.0f;
}

//=========================================================================

void d3deng_SetDefaultStates( void )
{
    if( g_pd3dContext )
    {
        // Use rtarget system for clearing
        f32 clearColor[4] = { s.BackColor.R/255.0f, s.BackColor.G/255.0f, 
                              s.BackColor.B/255.0f, s.BackColor.A/255.0f };
        
        rtarget_Clear( RTARGET_CLEAR_ALL, clearColor, 1.0f, 0 );

        // Set default blend state
        state_SetBlend( STATE_BLEND_ALPHA );

        // Set default sampler state
        state_SetSampler( STATE_SAMPLER_ANISOTROPIC_WRAP, 0, STATE_SAMPLER_STAGE_PS );
    }
}

//=========================================================================

void eng_Init( void )
{
    char buffer[256];
    
    x_DebugMsg( "=== ENGINE INITIALIZATION START ===\n" );

    //TODO: GS: Ofc, made settings and .inl loader, but for now this code is good.
    s32 initXRes, initYRes;
    #ifdef X_RETAIL
        // Get ready to the retail.
        initXRes = GetSystemMetrics(SM_CXSCREEN);
        initYRes = GetSystemMetrics(SM_CYSCREEN);
    #else
        // Use desktop resolution as default
        initXRes = 1024;
        initYRes = 768;
        x_DebugMsg( "Engine: Using default resolution 1024x768\n" );
    #endif
    x_sprintf( buffer, "Engine: Using resolution %dx%d\n", initXRes, initYRes );
    x_DebugMsg( buffer );

    // Initialize the engine
    if( s.Wnd == 0 )
    {
        s.Wnd = CreateWin( initXRes, initYRes );
        x_DebugMsg( "Engine: Created main window\n" );
    }
    else
    {
        x_DebugMsg( "Engine: Using external window\n" );
    }

    // The default window to display will be the current window
    s.WndDisplay = s.Wnd;

    // Init D3D and render target system
    InitializeD3D( s.Wnd, initXRes, initYRes );

    // Show the window
    ShowWindow( s.Wnd, SW_SHOWDEFAULT );
    UpdateWindow( s.Wnd );
    x_DebugMsg( "Engine: Window shown\n" );

    composite_Init();
    x_DebugMsg( "Engine: Composite system initialized\n" );

    state_Init();
    x_DebugMsg( "Engine: Render state system initialized\n" );

    shader_Init();
    x_DebugMsg( "Engine: Shader system initialized\n" );

    text_Init();
    text_SetParams( initXRes, initYRes, 0, 0,
                    ENG_FONT_SIZEX, ENG_FONT_SIZEY,
                    8 );
    text_ClearBuffers();
    x_DebugMsg( "Engine: Text system initialized\n" );

    font_Init();
    x_DebugMsg( "Engine: Font system initialized\n" );

    // Initialize dinput
    if( s.WndParent )
    {
        VERIFY( d3deng_InitInput( s.WndParent ) == TRUE );
    }
    else
    {
        VERIFY( d3deng_InitInput( s.Wnd ) == TRUE );
    }
    x_DebugMsg( "Engine: Input system initialized\n" );

    // Initialize vram
    vram_Init();
    x_DebugMsg( "Engine: VRAM system initialized\n" );

    // Set the scrach memory
    smem_Init(SCRACH_MEM_SIZE);
    x_sprintf( buffer, "Engine: Scratch memory initialized (%d bytes)\n", SCRACH_MEM_SIZE );
    x_DebugMsg( buffer );

    draw_Init();
    x_DebugMsg( "Engine: Draw system initialized\n" );
    
    // Indicate the engine is ready
    s.Ready = TRUE;

    // Set default modes
    d3deng_SetDefaultStates();

    x_DebugMsg( "=== ENGINE INITIALIZATION COMPLETE ===\n" );
}

//=============================================================================

static const char* pPrevName = NULL;

xbool eng_Begin( const char* pTaskName )
{
    ASSERT( s.inRenderPass == FALSE );
    pPrevName = pTaskName;

    if( !g_pd3dDevice )
        return FALSE;

    if( s.FrameCleared == FALSE )
    {
        // Use rtarget system for clearing and setting targets		
        f32 clearColor[4];
        d3deng_GetClearColor( clearColor );

        rtarget_SetBackBuffer();
        rtarget_Clear( RTARGET_CLEAR_ALL, clearColor, 1.0f, 0 );

        d3deng_RunFrameStages( &eng_frame_stage::OnBeginFrame );

        s.FrameCleared = TRUE;
    }

    s.inRenderPass = TRUE;
    draw_ClearL2W();
    return TRUE;
}

//=============================================================================

void eng_End( void )
{
    s.inRenderPass = FALSE;
}

//=============================================================================

xbool eng_InBeginEnd( void )
{
    return s.inRenderPass;
}

//=============================================================================

void eng_PrintStats( void )
{
    x_DebugMsg("CPU:%1.1f  Pageflip:%1.1f  FPS:%1.1f\n",s.CPUMS,s.IMS,1000.0f/(s.CPUMS+s.IMS));
}

//=========================================================================

static
xbool d3deng_PresentFrame( void )
{
    static s32 frameCount = 0;
    static s32 errorCount = 0;

    if( !g_pSwapChain )
    {
        x_DebugMsg( "Engine: WARNING - No swap chain available for present\n" );
        return TRUE;
    }

    // Ensure rendering to back buffer before present
    rtarget_SetBackBuffer();

    HRESULT Error = g_pSwapChain->Present( 1, 0 );  // 0 = immediate present, VSYNC
    frameCount++;

    if( Error != S_OK )
    {
        errorCount++;
        x_DebugMsg( "Engine: ERROR - Present failed (HRESULT=0x%08X) [Frame %d, Errors %d]\n",
                    Error, frameCount, errorCount );
        return FALSE;
    }

    //if( (frameCount % 1024) == 0 )
    //    x_DebugMsg( "Engine: Frame %d presented successfully (Errors: %d)\n", frameCount, errorCount );

    return TRUE;
}

//=========================================================================

static 
void d3deng_UpdateTimers( void )
{
    // Toggle scratch memory
    smem_Toggle();

    // Update FPS
    d3deng_UpdateFPS();

    // Update internal timing
    s.CPUTIMER.Stop();
    s.CPUMS = s.CPUTIMER.ReadMs();
    s.CPUTIMER.Reset();
    s.CPUTIMER.Start();
}

//=========================================================================

void eng_PageFlip( void )
{
    CONTEXT( "eng_PageFlip" );

    xtimer InternalTime;
    InternalTime.Start();

    // If the user hasn't called eng_Begin then force a call
    if( s.inRenderPass == FALSE )
    {
        if( eng_Begin( NULL ) )
            eng_End();
    }

    // Check if we have a valid device before rendering text
    if( g_pd3dDevice )
        text_Render();

    text_ClearBuffers();

    d3deng_RunFrameStages( &eng_frame_stage::OnBeforePresent );

    d3deng_PresentFrame();
    s.FrameCleared = FALSE;

    InternalTime.Stop();
    s.IMS = InternalTime.ReadMs();
    d3deng_UpdateTimers();
}

//=============================================================================

void eng_ResetAfterException( void )
{
    // Clear the in scene flag	
    s.inRenderPass = FALSE;
    smem_ResetAfterException();
}

//=============================================================================

void eng_GetRes( s32& XRes, s32& YRes )
{
    XRes = s.rcWindowRect.right  - s.rcWindowRect.left;
    YRes = s.rcWindowRect.bottom - s.rcWindowRect.top;
}

//=============================================================================

void eng_MaximizeViewport( view& View )
{
    s32 Width, Height;
    eng_GetRes( Width, Height );

    View.SetViewport( 0, 0, Width, Height );
}

//=============================================================================

void eng_SetBackColor( xcolor Color )
{
    s.BackColor = Color;
}

//=============================================================================

void eng_SetView( const view& View )
{
    s.View = View;

    if( g_pd3dContext )
        eng_SetViewport( View );
}

//=============================================================================

const view* eng_GetView( void )
{
    return &s.View;
}

//=============================================================================

void DebugMessage( const char* FormatStr, ... )
{
    va_list Args;
    va_start( Args, FormatStr );

    OutputDebugString( xvfs( FormatStr, Args) );

    va_end( Args );
}

//=============================================================================

void eng_SetViewport( const view& View )
{
    if( !g_pd3dContext )
        return;

    D3D11_VIEWPORT vp;
    s32 L, T, R, B;

    View.GetViewport( L, T, R, B );

    vp.TopLeftX = L;
    vp.TopLeftY = T;
    vp.Width    = R-L; //R-L-1;
    vp.Height   = B-T; //B-T-1;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    if( vp.Width <= 0.0f || vp.Height <= 0.0f )
    {
        x_DebugMsg( "Engine: WARNING - Invalid viewport dimensions!\n" );
        return;
    }

    g_pd3dContext->RSSetViewports( 1, &vp );
}

//=============================================================================
//
// Legacy code - START
//
// This code is either of no value or is used only in a few places in the program.
// It should be removed, moved, or otherwise addressed in the future.
//
// It should not be here.
//
//=============================================================================

void eng_Reboot( reboot_reason Reason )
{
    exit(Reason);
}

//=============================================================================

datestamp eng_GetDate( void )
{
    SYSTEMTIME  Time;
    datestamp   DateStamp;

    ASSERT( sizeof(DateStamp) == sizeof(FILETIME) );

    GetLocalTime( &Time );
    SystemTimeToFileTime( &Time, (FILETIME*)&DateStamp );
    return DateStamp;
}

//=============================================================================

split_date eng_SplitDate( datestamp DateStamp )
{
    SYSTEMTIME Time;
    split_date SplitDate;

    FileTimeToSystemTime( (FILETIME*)&DateStamp, &Time );
    SplitDate.Year          = Time.wYear;
    SplitDate.Month         = (u8)Time.wMonth;
    SplitDate.Day           = (u8)Time.wDay;
    SplitDate.Hour          = (u8)Time.wHour;
    SplitDate.Minute        = (u8)Time.wMinute;
    SplitDate.Second        = (u8)Time.wSecond;
    SplitDate.CentiSecond   = (u8)(Time.wMilliseconds/100);
    return SplitDate;
}

//=============================================================================

datestamp eng_JoinDate( const split_date& SplitDate )
{
    SYSTEMTIME  Time;
    datestamp   DateStamp;

    ASSERT( sizeof(datestamp) == sizeof(FILETIME) );
    Time.wYear          = SplitDate.Year;
    Time.wMonth         = SplitDate.Month;
    Time.wDay           = SplitDate.Day;
    Time.wHour          = SplitDate.Hour;
    Time.wMinute        = SplitDate.Minute;
    Time.wSecond        = SplitDate.Second;
    Time.wMilliseconds  = SplitDate.CentiSecond*100;
    SystemTimeToFileTime( &Time, (FILETIME*)&DateStamp );
    return DateStamp;
}

//=========================================================================

s32 eng_GetProductCode( void )
{
    return 0;
}

//=============================================================================

const char* eng_GetProductKey(void)
{
    return NULL;
}

//=============================================================================

void eng_ScreenShot( const char* pFileName )
{
    ASSERTS( 0, "Press Print screen for now" );
}

//=============================================================================

#if !defined(X_RETAIL) || defined(X_QA)
xbool eng_ScreenShotActive( void )
{
    return FALSE;
}
#endif  // !defined( X_RETAIL ) || defined( X_QA )