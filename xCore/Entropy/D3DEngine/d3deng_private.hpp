//==============================================================================
//  
//  d3deng_private.hpp
//  
//==============================================================================

#ifndef D3DENG_PRIVATE_HPP
#define D3DENG_PRIVATE_HPP

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

// Included this header only one time
#pragma once
#ifndef STRICT
#define STRICT
#endif

#include "..\3rdParty\DirectX9\D3D11.h"
#include "..\3rdParty\DirectX9\D3Dcompiler.h"
#include "..\3rdParty\DirectX9\DXGI.h"
#include "..\3rdParty\DirectXTex\DirectXTex\DirectXTex.h"
//#include "..\3rdParty\DirectX9\d3dx11tex.h"
//#include "..\3rdParty\DirectX9\D3DX11core.h"
#include <windows.h>
#include <mmsystem.h>
#include "x_files.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define WM_MOUSEWHEEL                   0x020A

//==============================================================================
//  ENUMS
//==============================================================================

// DX ERROR ENUM
#define DXERROR( A ) ENG_##A = (A<0) ? -A : A
enum dxerror_enum
{
    DXERROR( E_OUTOFMEMORY                                  ),
    DXERROR( DXGI_ERROR_INVALID_CALL                        ),
    DXERROR( DXGI_ERROR_NOT_FOUND                           ),
    DXERROR( DXGI_ERROR_MORE_DATA                           ),
    DXERROR( DXGI_ERROR_UNSUPPORTED                         ),
    DXERROR( DXGI_ERROR_DEVICE_REMOVED                      ),
    DXERROR( DXGI_ERROR_DEVICE_HUNG                         ),
    DXERROR( DXGI_ERROR_DEVICE_RESET                        ),
    DXERROR( DXGI_ERROR_WAS_STILL_DRAWING                   ),
    DXERROR( DXGI_ERROR_FRAME_STATISTICS_DISJOINT           ),
    DXERROR( DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE        ),
    DXERROR( DXGI_ERROR_DRIVER_INTERNAL_ERROR               ),
    DXERROR( DXGI_ERROR_NONEXCLUSIVE                        ),
    DXERROR( DXGI_ERROR_NOT_CURRENTLY_AVAILABLE             ),
    DXERROR( DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED          ),
    DXERROR( DXGI_ERROR_REMOTE_OUTOFMEMORY                  ),
    DXERROR( D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS      ),
    DXERROR( D3D11_ERROR_FILE_NOT_FOUND                     ),
    DXERROR( D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS       ),
    DXERROR( D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD ),

    ENG_END = (unsigned int)0xffffffff
};
#undef DXERROR

//------------------------------------------------------------------------------

enum d3deng_mode
{
    ENG_ACT_DEFAULT             = (0),      // Activates nothing
    ENG_ACT_FULLSCREEN          = (1<<0),   // Default is Window
    ENG_ACT_SOFTWARE            = (1<<1),   // Default is Hardware
    ENG_ACT_BACKBUFFER_LOCK     = (1<<2),   // Default is you can't lock back buffer
    ENG_ACT_STENCILOFF          = (1<<3),   // Default is you will use stencil
    ENG_ACT_16_BPP              = (1<<4),   // Default is you will use stencil
    ENG_ACT_SHADERS_IN_SOFTWARE = (1<<5),   // Default is that the shaders are done in hardware
    ENG_ACT_LOCK_WINDOW_SIZE    = (1<<6),
    ENG_ACT_MSAA_2X             = (1<<7),
    ENG_ACT_MSAA_4X             = (1<<8),
    ENG_ACT_MSAA_8X             = (1<<9),
    ENG_ACT_MSAA_MASK           = (ENG_ACT_MSAA_2X | ENG_ACT_MSAA_4X | ENG_ACT_MSAA_8X)
};

//------------------------------------------------------------------------------

enum mouse_mode
{
    MOUSE_MODE_BUTTONS,
    MOUSE_MODE_NEVER,
    MOUSE_MODE_ALWAYS,
    MOUSE_MODE_ABSOLUTE
};

//==============================================================================
//  STRUCTURES
//==============================================================================

struct dxerr
{
    dxerror_enum    Desc;
    xbool           bSign;

    inline operator HRESULT() { return (HRESULT) ((bSign) ? -Desc : Desc) ; }
    inline const dxerr& operator = ( HRESULT hRes ) 
    {         
        if( hRes < 0 ){ bSign=1; Desc = (dxerror_enum)-hRes; }  
        else          { bSign=0; Desc = (dxerror_enum)hRes;  } 

        return *this;
    }
};

//------------------------------------------------------------------------------

struct d3dvertex 
{
    vector3     Pos;
    vector3     Normal;
    vector2     UV;

    inline d3dvertex( void ) {}
    inline d3dvertex( const vector3& P, const vector3& N, const vector2& TUV ) : Pos( P ), Normal( N ), UV( TUV ) {}
    inline void Set( const vector3& P, const vector3& N, const vector2& TUV ) 
    {
        Pos     = P;
        Normal  = N;
        UV      = TUV;
    }
};

//------------------------------------------------------------------------------

struct d3dtlvertex 
{
    vector3    Screen;
    f32        Rhw;
    xcolor     Color;
    f32        Specular;
    vector2    UV;

    inline void Set( const vector3& P, xcolor C, const vector2& TUV  ) 
    {
        Screen      = P;
        UV          = TUV;
        Color       = C;
        Specular    = 1;
        Rhw         = 1;
    }
};

//------------------------------------------------------------------------------

struct d3dlvertex 
{
    vector3    Pos;
    xcolor     Color;
    vector2    UV;
    f32        Specular;

    inline void Set( const vector3& P, xcolor C, const vector2& TUV  ) 
    {
        Pos         = P;
        UV          = TUV;
        Color       = C;
        Specular    = 1;
    }
};

//------------------------------------------------------------------------------

struct eng_frame_stage
{
    void    (*OnBeginFrame)     ( void );
    void    (*OnBeforePresent)  ( void );
};

//==============================================================================
//  GLOBAL VARIABLES
//==============================================================================

extern ID3D11Device*            g_pd3dDevice;        // Main DX11 device
extern ID3D11DeviceContext*     g_pd3dContext;       // Device context  
extern IDXGISwapChain*          g_pSwapChain;        // Swap chain

//==============================================================================
//  CORE FUNCTIONS
//==============================================================================

void        d3deng_EntryPoint           ( s32& argc, char**& argv, HINSTANCE h1, HINSTANCE h2, LPSTR str1, INT i1 );
s32         d3deng_ExitPoint            ( void );
void        d3deng_SetPresets           ( u32 Mode = ENG_ACT_DEFAULT );
u32         d3deng_GetMode              ( void );

//==============================================================================
//  RTARGET MANAGEMENT
//==============================================================================

void        d3deng_RegisterFrameStage   ( const eng_frame_stage& Stage );
void        d3deng_UnregisterFrameStage ( const eng_frame_stage& Stage );

//==============================================================================
//  WINDOW MANAGEMENT
//==============================================================================

HWND        d3deng_GetWindowHandle      ( void );
void        d3deng_UpdateDisplayWindow  ( HWND hWindow );
void        d3deng_SetWindowHandle      ( HWND hWindow );
void        d3deng_SetParentHandle      ( HWND hWindow );
void        d3deng_SetResolution        ( s32 Width, s32 Height );
HINSTANCE   d3deng_GetInstance          ( void );

LRESULT CALLBACK eng_D3DWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

//==============================================================================
//  MSAA FUNCTIONS
//==============================================================================

s32         d3deng_GetMsaa              ( void );
void        d3deng_SetMsaa              ( s32 SampleCount );

//==============================================================================
//  INPUT FUNCTIONS
//==============================================================================

void        d3deng_SetMouseMode         ( mouse_mode Mode );
void        d3deng_ComputeMousePos      ( void );

//==============================================================================
//  DEBUG FUNCTIONS
//==============================================================================

void        DebugMessage                ( const char* FormatStr, ... );

//==============================================================================
//  APPLICATION ENTRY POINT MACRO
//==============================================================================

// Magical macro which defines the entry point of the app. Make sure that 
// the user has the entry point: void AppMain( s32 argc, char* argv[] ) 
// defined somewhere. MFC apps don't need the user entry point.

#define AppMain AppMain( s32 argc, char* argv[] );                                  \
                                                                                    \
s32 __stdcall WinMain( HINSTANCE hInstance,                                         \
                       HINSTANCE hPrevInstance,                                     \
                       LPSTR     lpCmdLine,                                         \
                       INT       nCmdShow )                                         \
{                                                                                   \
    s32    argc;                                                                    \
    char** argv;                                                                    \
                                                                                    \
    /* Initialize D3D engine and parse command line */                              \
    d3deng_EntryPoint( argc, argv, hInstance, hPrevInstance, lpCmdLine, nCmdShow ); \
                                                                                    \
    /* Start main application */                                                    \
    x_StartMain( AppMain, argc, argv );                                             \
                                                                                    \
    /* Cleanup and return */                                                        \
    return d3deng_ExitPoint();                                                      \
}                                                                                   \
                                                                                    \
void AppMain                                                            

//==============================================================================
#endif // D3DENG_PRIVATE_HPP
//==============================================================================