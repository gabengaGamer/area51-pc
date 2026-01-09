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
static HANDLE           s_hSwapChainWaitable = NULL;
static xbool            s_bSwapChainWaitable = FALSE;

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
    xbool           bBeginScene;
    xbool           bD3DBeginScene;

    //
    // View variables
    //
    view            View;

    //
    // Window manechment related varaibles
    //
    xbool           bActive;
    xbool           bReady;      
    RECT            rcWindowRect;
    xbool           bWindowed;   
    s32             MaxXRes;
    s32             MaxYRes;

    //
    // Input related variables
    //
    MSG             Msg;
    f32             ABSMouseX;
    f32             ABSMouseY;
    f32             MouseX;
    f32             MouseY;
    xbool           bMouseLeftButton;
    xbool           bMouseRightButton;
    xbool           bMouseMiddleButton;
    f32             MouseWheelAbs;
    f32             MouseWheelRel;
    xbool           bMouseDelta;
    mouse_mode      MouseMode;

    //
    // FPS variables
    //
    xtick           FPSFrameTime[8];
    xtick           FPSLastTime;
    s32             FPSIndex;
    xtimer          CPUTIMER;
    f32             CPUMS;
    f32             IMS;
    s32             TargetFPS;
    xtick           FrameLimiterLastTick;

    
} s;

static xarray<const eng_frame_stage*>   s_FrameStages;

//=========================================================================
// FUNCTIONS
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

    s.FPSFrameTime[0] = 0;
    s.FPSFrameTime[1] = 0;
    s.FPSFrameTime[2] = 0;
    s.FPSFrameTime[3] = 0;
    s.FPSFrameTime[4] = 0;
    s.FPSFrameTime[5] = 0;
    s.FPSFrameTime[6] = 0;
    s.FPSFrameTime[7] = 0;

    s.FPSLastTime = x_GetTime();

    x_Init(argc,argv);
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

void eng_Kill( void ) //Deprecated, but i still prefer to maintenance this func
{
    x_DebugMsg( "=== ENGINE SHUTDOWN START ===\n" );

    //
    // Free sub modules (in reverse order of initialization)
    //
    
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

    //
    // Stop the d3d engine
    //
    if( g_pSwapChain != NULL )
    {
        g_pSwapChain->Release();
        g_pSwapChain = NULL;
        x_DebugMsg( "Engine: Swap chain released\n" );
    }

    s_hSwapChainWaitable = NULL;
    s_bSwapChainWaitable = FALSE;
    
    if( g_pd3dContext != NULL )
    {
        g_pd3dContext->Release();
        g_pd3dContext = NULL;
        x_DebugMsg( "Engine: Device context released\n" );
    }
    
    if( g_pd3dDevice != NULL )
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
        x_DebugMsg( "Engine: Device released\n" );
    }

    UnregisterClass( "Render Window", s.hInst );
    x_DebugMsg( "Engine: Window class unregistered\n" );

    s_FrameStages.Clear();

    x_DebugMsg( "=== ENGINE SHUTDOWN COMPLETE ===\n" );
}

//=========================================================================

s32 d3deng_ExitPoint( void )
{
    //eng_Kill();
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
    xtick Sum = s.FPSFrameTime[0] +
                s.FPSFrameTime[1] +
                s.FPSFrameTime[2] +
                s.FPSFrameTime[3] +
                s.FPSFrameTime[4] +
                s.FPSFrameTime[5] +
                s.FPSFrameTime[6] +
                s.FPSFrameTime[7];

    return( (f32)(s32)(((8.0f / x_TicksToMs( Sum )) * 1000.0f) + 0.5f) );
}       

//=========================================================================

// For now let it be static and "d3deng", not eng.

static
void d3deng_SetTargetFPS( s32 TargetFPS )
{
    if( TargetFPS < 0 )
        TargetFPS = 0;

    s.TargetFPS = TargetFPS;
    s.FrameLimiterLastTick = 0;

    x_DebugMsg( "Engine: Target FPS set to %d\n", s.TargetFPS );
}

//=========================================================================

static
s32 d3deng_GetTargetFPS( void )
{
    return s.TargetFPS;
}

//=========================================================================

static 
void d3deng_SetupDepthStencilParams( void )
{
    if( (s.Mode & ENG_ACT_STENCILOFF) == 0 )
    {
        ASSERT( (s.Mode & ENG_ACT_16_BPP) == 0 );
    }
}

//=========================================================================

static 
xbool d3deng_SetupWindowedParams( void )
{
    g_SwapChainDesc.Windowed                           = TRUE;
    g_SwapChainDesc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    g_SwapChainDesc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    g_SwapChainDesc.BufferDesc.Width                   = s.MaxXRes;
    g_SwapChainDesc.BufferDesc.Height                  = s.MaxYRes;
    g_SwapChainDesc.BufferDesc.RefreshRate.Numerator   = 60;
    g_SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    g_SwapChainDesc.BufferCount                        = 1;

    return TRUE;
}

//=========================================================================

static 
void d3deng_SetupFullscreenParams( s32 XRes, s32 YRes )
{
    g_SwapChainDesc.Windowed                           = FALSE;
    g_SwapChainDesc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;    
    g_SwapChainDesc.BufferDesc.Format                  = (s.Mode & ENG_ACT_16_BPP) ? DXGI_FORMAT_B5G5R5A1_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;    
    g_SwapChainDesc.BufferDesc.Width                   = s.MaxXRes = XRes;
    g_SwapChainDesc.BufferDesc.Height                  = s.MaxYRes = YRes;
    g_SwapChainDesc.BufferDesc.RefreshRate.Numerator   = 60;
    g_SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
}

//=========================================================================

static 
xbool d3deng_SetupPresentationParams( s32 XRes, s32 YRes )
{
    char buffer[256];
    
    // Clear presentation parameters
    ZeroMemory( &g_SwapChainDesc, sizeof(g_SwapChainDesc) );

    x_sprintf( buffer, "Engine: Setting up presentation params %dx%d\n", XRes, YRes );
    x_DebugMsg( buffer );

    // Setup depth/stencil
    d3deng_SetupDepthStencilParams();

    // Determine windowed/fullscreen mode
    s.bWindowed = (s.Mode & ENG_ACT_FULLSCREEN) != ENG_ACT_FULLSCREEN;
    
    x_sprintf( buffer, "Engine: Mode = %s\n", s.bWindowed ? "Windowed" : "Fullscreen" );
    x_DebugMsg( buffer );
    
    if( s.bWindowed )
    {
        if( !d3deng_SetupWindowedParams() )
        {
            x_DebugMsg( "Engine: ERROR - Failed to setup windowed params\n" );
            return FALSE;
        }
    }
    else
    {
        d3deng_SetupFullscreenParams( XRes, YRes );
    }

    // Setup common parameters
    g_SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    g_SwapChainDesc.SampleDesc.Count = 1;
    g_SwapChainDesc.SampleDesc.Quality = 0;

    x_DebugMsg( "Engine: Presentation params setup complete\n" );
    return TRUE;
}

//=========================================================================

static 
void d3deng_CheckDeviceCaps( void )
{
    char buffer[256];
    
    D3D_FEATURE_LEVEL featureLevel = g_pd3dDevice->GetFeatureLevel();

    x_sprintf( buffer, "Engine: Checking device capabilities (Feature Level %d.%d)\n", 
             (featureLevel >> 12) & 0xF, (featureLevel >> 8) & 0xF );
    x_DebugMsg( buffer );

    // GS: In DX11, software vertex processing doesn't exist, all is done via shaders on GPU
    // I keep the flag for compatibility but it doesn't affect device creation
    if( featureLevel < D3D_FEATURE_LEVEL_10_0 )
    {
        s.Mode |= ENG_ACT_SHADERS_IN_SOFTWARE;
        x_DebugMsg( "Engine: WARNING - Feature level below 10.0, marking shaders as software\n" );
    }

    if( s.Mode & ENG_ACT_SOFTWARE )
    {
        s.Mode |= ENG_ACT_SHADERS_IN_SOFTWARE;
        x_DebugMsg( "Engine: Software mode requested, marking shaders as software\n" );
    }

    x_sprintf( buffer, "Engine: Mode flags = 0x%08X\n", s.Mode );
    x_DebugMsg( buffer );
}

//=========================================================================

static 
UINT d3deng_GetDeviceCreationFlags( void )
{
    UINT createDeviceFlags = 0;

#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // GS: DX11 doesn't have software vertex processing concept,
    // i keep this code for compatibility.

    return createDeviceFlags;
}

//=========================================================================

static 
xbool d3deng_CreateD3DDevice( HWND hWnd )
{
    HRESULT Error;
    UINT    dwFlags = d3deng_GetDeviceCreationFlags();

    x_DebugMsg( "Engine: Creating D3D device (flags=0x%08X)\n", dwFlags );

    s_hSwapChainWaitable = NULL;
    s_bSwapChainWaitable = FALSE;

    // Feature levels to try
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    D3D_FEATURE_LEVEL featureLevel;
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

    // Create the D3D device
    Error = D3D11CreateDevice(
        NULL,                       // Use default adapter
        driverType,                 // Hardware or WARP for software
        NULL,                       // No software device
        dwFlags,                    // Device creation flags
        featureLevels,              // Feature levels array
        numFeatureLevels,           // Number of feature levels
        D3D11_SDK_VERSION,          // SDK version
        &g_pd3dDevice,              // Output device
        &featureLevel,              // Output feature level
        &g_pd3dContext              // Output device context
    );

    // Handle creation errors
    if( Error != S_OK )
    {
        x_DebugMsg( "Engine: ERROR - Failed to create device (HRESULT=0x%08X)\n", Error );
        
        // Cleanup any partial creation
        if( g_pd3dContext ) { g_pd3dContext->Release(); g_pd3dContext = NULL; }
        if( g_pd3dDevice )  { g_pd3dDevice->Release();  g_pd3dDevice = NULL; }
        MessageBox( d3deng_GetWindowHandle(), 
                   xfs("Error creating device: %d", Error), 
                   "Device Error", MB_OK );
        return FALSE;
    }

    // Log success and feature level
    const char* featureLevelStr = "Unknown";
    switch( featureLevel )
    {
        case D3D_FEATURE_LEVEL_11_0: featureLevelStr = "11.0"; break;
        case D3D_FEATURE_LEVEL_10_1: featureLevelStr = "10.1"; break;
        case D3D_FEATURE_LEVEL_10_0: featureLevelStr = "10.0"; break;
        case D3D_FEATURE_LEVEL_9_3:  featureLevelStr = "9.3";  break;
        case D3D_FEATURE_LEVEL_9_2:  featureLevelStr = "9.2";  break;
        case D3D_FEATURE_LEVEL_9_1:  featureLevelStr = "9.1";  break;
    }
    
    x_DebugMsg( "Engine: Device created successfully (Feature Level %s)\n", featureLevelStr );

    // Create swap chain
    IDXGIDevice* pDXGIDevice = NULL;
    IDXGIAdapter* pAdapter = NULL;
    IDXGIFactory1* pFactory = NULL;
    IDXGIFactory2* pFactory2 = NULL;
    IDXGISwapChain1* pSwapChain1 = NULL;

    Error = g_pd3dDevice->QueryInterface( __uuidof(IDXGIDevice), (void**)&pDXGIDevice );
    if( SUCCEEDED(Error) )
    {
        Error = pDXGIDevice->GetAdapter( &pAdapter );
    }
    if( SUCCEEDED(Error) )
    {
        Error = pAdapter->GetParent( __uuidof(IDXGIFactory1), (void**)&pFactory );
    }
    if( SUCCEEDED(Error) )
    {
        pFactory->QueryInterface( __uuidof(IDXGIFactory2), (void**)&pFactory2 );
    }

    if( pFactory2 )
    {
        DXGI_SWAP_CHAIN_DESC1 Desc1;
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC FullDesc;
        ZeroMemory( &Desc1, sizeof(Desc1) );
        ZeroMemory( &FullDesc, sizeof(FullDesc) );

        Desc1.Width = g_SwapChainDesc.BufferDesc.Width;
        Desc1.Height = g_SwapChainDesc.BufferDesc.Height;
        Desc1.Format = g_SwapChainDesc.BufferDesc.Format;
        Desc1.Stereo = FALSE;
        Desc1.SampleDesc.Count = g_SwapChainDesc.SampleDesc.Count;
        Desc1.SampleDesc.Quality = g_SwapChainDesc.SampleDesc.Quality;
        Desc1.BufferUsage = g_SwapChainDesc.BufferUsage;
        Desc1.BufferCount = 2;
        Desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        Desc1.Scaling = DXGI_SCALING_STRETCH;
        Desc1.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        Desc1.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        FullDesc.RefreshRate = g_SwapChainDesc.BufferDesc.RefreshRate;
        FullDesc.Windowed = g_SwapChainDesc.Windowed;
        FullDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        FullDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        Error = pFactory2->CreateSwapChainForHwnd(
            g_pd3dDevice,
            hWnd,
            &Desc1,
            &FullDesc,
            NULL,
            &pSwapChain1 );

        if( SUCCEEDED(Error) )
        {
            Error = pSwapChain1->QueryInterface( __uuidof(IDXGISwapChain), (void**)&g_pSwapChain );
            if( SUCCEEDED(Error) )
            {
                IDXGISwapChain2* pSwapChain2 = NULL;
                if( SUCCEEDED( pSwapChain1->QueryInterface( __uuidof(IDXGISwapChain2), (void**)&pSwapChain2 ) ) )
                {
                    if( SUCCEEDED( pSwapChain2->SetMaximumFrameLatency( 1 ) ) )
                    {
                        s_hSwapChainWaitable = pSwapChain2->GetFrameLatencyWaitableObject();
                        s_bSwapChainWaitable = (s_hSwapChainWaitable != NULL);
                    }
                    pSwapChain2->Release();
                }
            }
        }
    }

    if( pSwapChain1 )  { pSwapChain1->Release(); pSwapChain1 = NULL; }
    if( pFactory2 )    { pFactory2->Release(); pFactory2 = NULL; }

    if( !g_pSwapChain && pFactory )
    {
        g_SwapChainDesc.OutputWindow = hWnd;
        Error = pFactory->CreateSwapChain( g_pd3dDevice, &g_SwapChainDesc, &g_pSwapChain );
        if( FAILED(Error) )
        {
            x_DebugMsg( "Engine: ERROR - Failed to create swap chain (HRESULT=0x%08X)\n", Error );
        }
    }

    if( pFactory )    { pFactory->Release(); pFactory = NULL; }
    if( pAdapter )    { pAdapter->Release(); pAdapter = NULL; }
    if( pDXGIDevice ) { pDXGIDevice->Release(); pDXGIDevice = NULL; }

    if( !g_pSwapChain )
    {
        x_DebugMsg( "Engine: ERROR - Failed to create swap chain\n" );
        if( g_pd3dContext ) { g_pd3dContext->Release(); g_pd3dContext = NULL; }
        if( g_pd3dDevice )  { g_pd3dDevice->Release();  g_pd3dDevice = NULL; }
        return FALSE;
    }

    if( s_bSwapChainWaitable )
        x_DebugMsg( "Engine: Waitable swap chain enabled\n" );
    else
        x_DebugMsg( "Engine: Waitable swap chain not available, using legacy present path\n" );

    return TRUE;
}

//=========================================================================

static 
xbool d3deng_CreateRenderTargets( void )
{
    x_DebugMsg( "Engine: Creating render targets\n" );

    // Set back buffer as active
    rtarget_SetBackBuffer();

    x_DebugMsg( "Engine: Render targets initialized successfully\n" );
    return TRUE;
}

//=========================================================================

static 
void d3deng_SetupWindowedViewport( HWND hWnd, s32 XRes, s32 YRes )
{
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = (f32)XRes;
    vp.Height   = (f32)YRes;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_pd3dContext->RSSetViewports( 1, &vp );

    // Calculate proper window size
    RECT Window;
    GetWindowRect( s.Wnd, &Window );
    GetClientRect( s.Wnd, &s.rcWindowRect );
    
    s32 W = (Window.right  - Window.left) * 2 - (s.rcWindowRect.right  - s.rcWindowRect.left);
    s32 H = (Window.bottom - Window.top)  * 2 - (s.rcWindowRect.bottom - s.rcWindowRect.top);
    
    MoveWindow( s.Wnd, 0, 0, W, H, TRUE );
    GetClientRect( s.Wnd, &s.rcWindowRect );

    // Make window active
    ShowWindow( s.Wnd, SW_SHOWNORMAL );
    UpdateWindow( s.Wnd );
    SetActiveWindow( s.Wnd );
}

//=========================================================================

static 
void d3deng_PostDeviceSetup( HWND hWnd, s32 XRes, s32 YRes )
{
    if( !g_pd3dDevice )
        return;

    // Setup viewport for windowed mode
    if( s.bWindowed )
    {
        d3deng_SetupWindowedViewport( hWnd, XRes, YRes );
    }
}

//==============================================================================

static 
void InitializeD3D( HWND hWnd, s32 XRes, s32 YRes )
{
    char buffer[256];
    
    x_sprintf( buffer, "Engine: Initializing DirectX 11 (%dx%d)\n", XRes, YRes );
    x_DebugMsg( buffer );

    // Setup presentation parameters
    if( !d3deng_SetupPresentationParams( XRes, YRes ) )
    {
        x_DebugMsg( "Engine: ERROR - Failed to setup presentation parameters\n" );
        return;
    }

    // Create D3D device
    if( !d3deng_CreateD3DDevice( hWnd ) )
    {
        x_DebugMsg( "Engine: ERROR - Failed to create D3D device\n" );
        return;
    }

    // Initialize render target system
    rtarget_Init();
    x_DebugMsg( "Engine: Render target system initialized\n" );

    // Create render targets
    if( !d3deng_CreateRenderTargets() )
    {
        x_DebugMsg( "Engine: ERROR - Failed to create render targets\n" );
        ASSERT( FALSE && "Failed to create render targets" );
        return;
    }

    // Check device capabilities
    d3deng_CheckDeviceCaps();

    // Post-creation setup
    d3deng_PostDeviceSetup( hWnd, XRes, YRes );

    x_DebugMsg( "Engine: DirectX 11 initialization complete\n" );
}

//=========================================================================

void d3deng_UpdateDisplayWindow( HWND hWindow )
{
    s.WndDisplay = hWindow;

    // Get the new size of the window
    GetClientRect( s.WndDisplay, &s.rcWindowRect );

    // Resize the view port
    DWORD dwRenderWidth  = s.rcWindowRect.right - s.rcWindowRect.left;
    DWORD dwRenderHeight = s.rcWindowRect.bottom - s.rcWindowRect.top;

    dwRenderWidth  = MIN( (DWORD)s.MaxXRes, dwRenderWidth );
    dwRenderHeight = MIN( (DWORD)s.MaxYRes, dwRenderHeight );

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

//=============================================================================

LRESULT CALLBACK eng_D3DWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch( uMsg )
    {
        case WM_ACTIVATE:
            {
                xbool bWasActive = s.bActive;
                s.bActive = LOWORD(wParam) != WA_INACTIVE;
                
                if( s.bActive )
                {
                    d3deng_ComputeMousePos();
                    d3deng_ComputeMousePos();
                }
            }
            break;
            
        case WM_ACTIVATEAPP:
            {
                s.bActive = (BOOL)wParam;
            }
            break;

        case WM_SIZE:
            // Check to see if we are losing or gaining our window. Set the
            // active flag to match.
            if( SIZE_MAXHIDE==wParam || SIZE_MINIMIZED==wParam )
                s.bActive = FALSE;
            else s.bActive = TRUE;

            if( s.bActive && s.bReady )
            {
                d3deng_UpdateDisplayWindow( hWnd );
                s.bReady = TRUE;
            }
            break;

        case WM_GETMINMAXINFO:
            ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 100;
            ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 100;
            break;

        case WM_SETCURSOR:
            // We turn the cursor off for fullscreen modes
            if( s.bActive && s.bReady && (!s.bWindowed) )
            {
                SetCursor(NULL);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            DestroyWindow( hWnd );
            return 0;
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0L;

        case WM_MOUSEMOVE:
            s.ABSMouseX = s.MouseX = LOWORD(lParam);  // horizontal position of cursor 
            s.ABSMouseY = s.MouseY = HIWORD(lParam);  // vertical position of cursor 

            // Compute the abs position of the mouse
            {
                s32 w = (s.rcWindowRect.right-s.rcWindowRect.left);
                s32 h = (s.rcWindowRect.bottom-s.rcWindowRect.top);
                if( w > s.MaxXRes )
                {
                    s.ABSMouseX = (s.ABSMouseX*s.MaxXRes)/w;
                }

                if( h > s.MaxYRes )
                {
                    s.ABSMouseY = (s.ABSMouseY*s.MaxYRes)/h;
                }
            }
            return 0;
            
         case WM_LBUTTONDOWN:
            s.bMouseLeftButton = true;
            return 0;
            
         case WM_MBUTTONDOWN:
             s.bMouseMiddleButton = true;
            return 0;
            
         case WM_RBUTTONDOWN:
             s.bMouseRightButton = true;
            return 0;
            
         case WM_LBUTTONUP:
             s.bMouseLeftButton = false;
            return 0;
            
         case WM_MBUTTONUP:
             s.bMouseMiddleButton = false;
            return 0;
            
         case WM_RBUTTONUP:
             s.bMouseRightButton = false;
            return 0;
            
         case WM_MOUSEWHEEL:
            {
                 f32 Wheel = (f32)((s16)(wParam >> 16)) / 120.0f ;
                 s.MouseWheelAbs += Wheel ;
                 s.MouseWheelRel = Wheel ;
            }
            return 0;
            
         case WM_KEYDOWN:
             break;
    }
    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

//=========================================================================

static
HWND CreateWin( s32 Width, s32 Height )
{
    // Register the window class
    WNDCLASS wndClass = { CS_HREDRAW | CS_VREDRAW, eng_D3DWndProc, 0, 0, s.hInst,
                          NULL, LoadCursor(NULL, IDC_ARROW),
                          (HBRUSH)GetStockObject(WHITE_BRUSH), NULL,
                          TEXT("Render Window") };
    
    RegisterClass( &wndClass );
    
    s32 screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    s32 screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    RECT rect = { 0, 0, Width, Height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    
    s32 windowWidth  = rect.right - rect.left;
    s32 windowHeight = rect.bottom - rect.top;
   
    if (windowWidth >= screenWidth || windowHeight >= screenHeight)
    {
        // Fallback to the native resolution of the monitor, because something is wrong with the desired resolution.
        d3deng_SetResolution( screenWidth, screenHeight );
        
        // Create our main boardless window
        return CreateWindow(TEXT("Render Window"), TEXT("Dreamlnd 51"),
                           WS_POPUP | WS_VISIBLE, 0, 0, 
                           screenWidth, screenHeight,
                           0L, 0L, s.hInst, 0L);
    }
    else
    {
        // Everything is fine, set the desired resolution
        d3deng_SetResolution( windowWidth, windowHeight );
        
        // Create our main window
        return CreateWindow(TEXT("Render Window"), TEXT("Dreamlnd 51"),
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           windowWidth, windowHeight,
                           0L, 0L, s.hInst, 0L);
    }
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

void d3deng_SetResolution( s32 Width,  s32 Height )
{
    ASSERT( Width  > 0 );
    ASSERT( Width  < 40000 );
    ASSERT( Height > 0 );
    ASSERT( Height < 40000 );

    s.MaxXRes = Width;
    s.MaxYRes = Height;
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

    // Ofc is temp solution.
    d3deng_SetTargetFPS(120);

    if( s.MaxXRes == 0 )
    {
        //TODO: GS: Ofc, made settings and .inl loader, but for now this code is good.
        #ifdef X_RETAIL
            // Get ready to the retail.
            s.MaxXRes = GetSystemMetrics(SM_CXSCREEN);
            s.MaxYRes = GetSystemMetrics(SM_CYSCREEN);
        #else
            // Use desktop resolution as default
            s.MaxXRes = 1024;
            s.MaxYRes = 768;
            x_DebugMsg( "Engine: Using default resolution 1024x768\n" );    
        #endif
    }
    else
    {
        x_sprintf( buffer, "Engine: Using resolution %dx%d\n", s.MaxXRes, s.MaxYRes );
        x_DebugMsg( buffer );
    }

    // Initialize the engine
    if( s.Wnd == 0 )
    {
        s.Wnd = CreateWin( s.MaxXRes, s.MaxYRes );
        x_DebugMsg( "Engine: Created main window\n" );
    }
    else
    {
        x_DebugMsg( "Engine: Using external window\n" );
    }

    // The default window to display will be the current window
    s.WndDisplay = s.Wnd;

    // Init D3D and render target system
    InitializeD3D( s.Wnd, s.MaxXRes, s.MaxYRes );

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
    text_SetParams( s.MaxXRes, s.MaxYRes, 0, 0,
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
    s.bReady = TRUE;

    // Set default modes
    d3deng_SetDefaultStates();

    x_DebugMsg( "=== ENGINE INITIALIZATION COMPLETE ===\n" );
}

//=============================================================================

static const char* pPrevName = NULL;

xbool eng_Begin( const char* pTaskName )
{
    ASSERT( s.bBeginScene == FALSE );
    pPrevName = pTaskName;

    if( !g_pd3dDevice )
        return FALSE;

    if( s.bD3DBeginScene == FALSE )
    {
        // Use rtarget system for clearing and setting targets
        f32 clearColor[4] = { s.BackColor.R/255.0f, s.BackColor.G/255.0f, 
                              s.BackColor.B/255.0f, s.BackColor.A/255.0f };

        rtarget_SetBackBuffer();
        rtarget_Clear( RTARGET_CLEAR_ALL, clearColor, 1.0f, 0 );

        for( s32 iStage = 0; iStage < s_FrameStages.GetCount(); )
        {
            const eng_frame_stage* pStage = s_FrameStages[iStage];
            if( pStage && pStage->OnBeginFrame )
            {
                pStage->OnBeginFrame();
            }

            if( (iStage < s_FrameStages.GetCount()) && (s_FrameStages[iStage] == pStage) )
            {
                iStage++;
            }
        }
        
        s.bD3DBeginScene = TRUE;
    }

    s.bBeginScene = TRUE;
    draw_ClearL2W();
    return TRUE;
}

//=============================================================================

void eng_End( void )
{
    s.bBeginScene = FALSE;
}

//=============================================================================

xbool eng_InBeginEnd( void )
{
    return s.bBeginScene;
}

//=============================================================================

void eng_PrintStats( void )
{
    x_DebugMsg("CPU:%1.1f  Pageflip:%1.1f  FPS:%1.1f\n",s.CPUMS,s.IMS,1000.0f/(s.CPUMS+s.IMS));
}

//=========================================================================

static 
void d3deng_EnsureSceneBegun( void )
{
    // If the user hasn't called eng_Begin then force a call
    if( s.bBeginScene == FALSE )
    {
        if( eng_Begin( NULL ) )
        {
            eng_End();
        }
    }
}

//=========================================================================

static 
void d3deng_RenderBufferedText( void )
{
    // Check if we have a valid device before rendering text
    if( g_pd3dDevice )
    {
        text_Render();
    }
    
    text_ClearBuffers();
}

//=========================================================================

static 
void d3deng_ThrottleFrame( void )
{
    if( s.TargetFPS <= 0 )
        return;

    s64 TicksPerSec = x_GetTicksPerSecond();
    if( TicksPerSec <= 0 )
        return;

    xtick TargetTicks = TicksPerSec / s.TargetFPS;
    if( TargetTicks <= 0 )
        return;

    xtick Current = x_GetTime();
    if( s.FrameLimiterLastTick != 0 )
    {
        xtick Elapsed = Current - s.FrameLimiterLastTick;
        if( Elapsed < TargetTicks )
        {
            xtick Remaining = TargetTicks - Elapsed;
            s64 TicksPerMs = x_GetTicksPerMs();
            if( TicksPerMs > 0 )
            {
                s32 WaitMs = (s32)(Remaining / TicksPerMs);
                if( WaitMs > 0 )
                    Sleep( (DWORD)WaitMs );
            }
            Current = x_GetTime();
        }
    }

    s.FrameLimiterLastTick = Current;
}

//=========================================================================

static 
xbool d3deng_PresentFrame( void )
{
    HRESULT Error;
    static s32 frameCount = 0;
    static s32 errorCount = 0;
    char buffer[256];
    
    if( !g_pSwapChain )
    {
        x_DebugMsg( "Engine: WARNING - No swap chain available for present\n" );
        return TRUE;
    }

    // Ensure rendering to back buffer before present
    rtarget_SetBackBuffer();

    d3deng_ThrottleFrame();

    if( s_bSwapChainWaitable && s_hSwapChainWaitable )
    {
        DWORD waitResult = WaitForSingleObjectEx( s_hSwapChainWaitable, 1000, TRUE );
        if( waitResult == WAIT_TIMEOUT )
        {
            x_DebugMsg( "Engine: WARNING - Waitable swap chain wait timeout\n" );
        }
    }

    Error = g_pSwapChain->Present( 1, 0 );  // 0 = immediate present, VSYNC
    
    frameCount++;
    
    if( Error != S_OK )
    {
        errorCount++;
        x_sprintf( buffer, "Engine: ERROR - Present failed (HRESULT=0x%08X) [Frame %d, Errors %d]\n", 
                 Error, frameCount, errorCount );
        x_DebugMsg( buffer );
        return FALSE;
    }

    if( (frameCount % 1024) == 0 )
    {
        x_sprintf( buffer, "Engine: Frame %d presented successfully (Errors: %d)\n", frameCount, errorCount );
        x_DebugMsg( buffer );
    }

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

static 
void d3deng_TickleScreensaver( void )
{
    SystemParametersInfo( SPI_SETSCREENSAVEACTIVE, FALSE, 0, FALSE );
    SystemParametersInfo( SPI_SETSCREENSAVEACTIVE, TRUE, 0, FALSE );
}

//=========================================================================

void eng_PageFlip( void )
{
    CONTEXT( "eng_PageFlip" );

    d3deng_TickleScreensaver();
    xtimer InternalTime;
    InternalTime.Start();

    d3deng_EnsureSceneBegun();
    d3deng_RenderBufferedText();

    
    for( s32 iStage = 0; iStage < s_FrameStages.GetCount(); )
    {
        const eng_frame_stage* pStage = s_FrameStages[iStage];
        if( pStage && pStage->OnBeforePresent )
        {
            pStage->OnBeforePresent();
        }

        if( (iStage < s_FrameStages.GetCount()) && (s_FrameStages[iStage] == pStage) )
        {
            iStage++;
        }
    }

    d3deng_PresentFrame();
    s.bD3DBeginScene = FALSE;

    InternalTime.Stop();
    s.IMS = InternalTime.ReadMs();
    d3deng_UpdateTimers();
}

//=============================================================================

void eng_ResetAfterException( void )
{
    // Clear the in scene flag
    s.bBeginScene = FALSE;
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
    s32 XRes, YRes;
    eng_GetRes( XRes, YRes );

    s32 Width  = MIN( s.MaxXRes, s.rcWindowRect.right-s.rcWindowRect.left );
    s32 Height = MIN( s.MaxYRes, s.rcWindowRect.bottom-s.rcWindowRect.top );

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
    {
        eng_SetViewport( View );
    }
}

//=============================================================================

const view* eng_GetView( void )
{
    return &s.View;
}

//=============================================================================

xbool d3deng_GetWindowedState( void )
{
    return s.bWindowed;
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

//=============================================================================

void eng_Sync ( void )
{
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

    vp.Width  = MIN( (u32)s.MaxXRes, vp.Width );
    vp.Height = MIN( (u32)s.MaxYRes, vp.Height );

    g_pd3dContext->RSSetViewports( 1, &vp );
}

//=========================================================================

void  d3deng_SetMouseMode( mouse_mode Mode )
{
    s.MouseMode = Mode;
}

//=========================================================================

void d3deng_ComputeMousePos( void )
{
    static xbool WasActive = FALSE;
    xbool  LastTimeActive;
    s.MouseX = 0;
    s.MouseY = 0;

    LastTimeActive = WasActive;
    WasActive      = FALSE;

    if( s.bActive == FALSE )
        return;

    if( s.MouseMode == MOUSE_MODE_NEVER )
        return;

    if( s.MouseMode == MOUSE_MODE_BUTTONS )
    {
        if( s.bMouseLeftButton == 0 && s.bMouseRightButton == 0 && s.bMouseMiddleButton == 0 )
            return;
    }

    WasActive = TRUE;

    RECT    Rect;
    POINT   MousePos;
    GetCursorPos ( &MousePos );
    GetWindowRect( s.Wnd, &Rect );

    s32 CenterX = (Rect.right + Rect.left) >> 1;
    s32 CenterY = (Rect.bottom + Rect.top) >> 1;

    if(s.MouseMode != MOUSE_MODE_ABSOLUTE)
    {
        SetCursorPos( CenterX, CenterY );
    }

    if( LastTimeActive == FALSE )        
        return;
    if(s.MouseMode != MOUSE_MODE_ABSOLUTE)
    {
        s.MouseX = (f32)(MousePos.x - CenterX);
        s.MouseY = (f32)(MousePos.y - CenterY);
    }
    else
    {
        s.MouseX = (f32)(MousePos.x - Rect.left);
        s.MouseY = (f32)(MousePos.y - Rect.top);
    }
}

//=============================================================================

HINSTANCE d3deng_GetInstace( void )
{
    return s.hInst;
}

//=============================================================================

HWND d3deng_GetWindowHandle( void )
{
    return s.Wnd;
}

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