//=========================================================================
//
// d3deng_video.cpp
//
//=========================================================================

#include "e_Engine.hpp"
#include "d3deng_video.hpp"

//=========================================================================
// LOCALS
//=========================================================================

struct resolution_mode
{
    s32         Width;
    s32         Height;
    s32         RefreshRate;
    D3DFORMAT   Format;
};

static
struct video_locals
{
    xbool                   bInitialized;
    
    // Video settings
    f32                     Brightness;
    f32                     Contrast;
    f32                     Gamma;
    
    // DirectX resources
    IDirect3DVertexShader9* pVertexShader;
    IDirect3DPixelShader9*  pPixelShader;
    IDirect3DTexture9*      pBackbufferCopy;
    IDirect3DSurface9*      pBackbufferSurface;
    
    s32                     BackbufferWidth;
    s32                     BackbufferHeight;
    
    // Resolution management - ДОБАВИТЬ ЭТИ ПОЛЯ
    resolution_mode         SupportedModes[256];
    s32                     nSupportedModes;
    s32                     CurrentModeIndex;
    s32                     DesktopWidth;
    s32                     DesktopHeight;
    
    video_locals( void ) 
    { 
        bInitialized        = FALSE;
        Brightness          = 2.0f;
        Contrast            = 1.0f;
        Gamma               = 1.0f;
        pVertexShader       = NULL;
        pPixelShader        = NULL;
        pBackbufferCopy     = NULL;
        pBackbufferSurface  = NULL;
        BackbufferWidth     = 0;
        BackbufferHeight    = 0;
        
        // Resolution init
        nSupportedModes     = 0;
        CurrentModeIndex    = -1;
        DesktopWidth        = 0;
        DesktopHeight       = 0;
    }
    
} s;

//=========================================================================
// VIDEO ADJUSTMENT SHADER - SM 3.0
//=========================================================================

static const char s_VideoVS[] = 
"vs_3_0\n"
"dcl_position0 v0\n"
"dcl_texcoord0 v1\n"
"dcl_position o0\n"
"dcl_texcoord0 o1\n"
";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
";                                                                             \n"
";   VS_VIDEO.VSH: Video adjustments vertex shader                             \n"
";                                                                             \n"
";   v0.xyzw   = Position (transformed)                                        \n"
";   v1.xy     = UV coordinates                                                \n"
";                                                                             \n"
";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
";==----------------------------------------------------------------------------\n"
";\n"
";   Pass through transformed vertex position and UV coordinates\n"
";\n"
";==----------------------------------------------------------------------------\n"
"\n"
"    mov o0, v0        ; Pass through transformed position\n"
"    mov o1, v1        ; Pass through UV coordinates\n";

static const char s_VideoPS[] = 
"ps_3_0\n"
"dcl_2d s0\n"
"dcl_texcoord0 v0.xy\n"
"def c2, 0.0, 0.5, 1.0, -1.0\n"
"def c3, 3.0, 2.0, 0.25, 0.001\n"
"def c4, 0.999, 1.0, 0.0, 0.0\n"
";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
";                                                                             \n"
";   PS_VIDEO.PSH: Video adjustments pixel shader                              \n"
";                                                                             \n"
";   s0       = input texture (backbuffer copy)                                \n"
";   v0.xy    = UV coordinates                                                 \n"
";   c0.x     = exposure value                                                 \n"
";   c0.y     = contrast                                                       \n"
";   c0.z     = gamma (inverted)                                               \n"
";   c2       = constants (0.0, 0.5, 1.0, -1.0)                                \n"
";   c3       = constants (3.0, 2.0, 0.25, 0.001)                              \n"
";   c4       = constants (0.999, 1.0, 0.0, 0.0)                               \n"
";                                                                             \n"
";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
"\n"
"    texld r0, v0, s0            ; Sample input texture\n"
"    \n"
";==----------------------------------------------------------------------------\n"
";\n"
";   Exposure Brightness (no linear mode)\n"
";\n"
";==----------------------------------------------------------------------------\n"
"\n"
"    ;Exposure brightness: 1.0 - exp(-color * exposure)\n"
"    mul r1.rgb, r0, c0.x        ; r1 = color * exposure\n"
"    mul r1.rgb, r1, c2.w        ; r1 = -(color * exposure)\n"
"    exp r1.r, r1.r              ; r1.r = exp(-(color.r * exposure))\n"
"    exp r1.g, r1.g              ; r1.g = exp(-(color.g * exposure))\n"
"    exp r1.b, r1.b              ; r1.b = exp(-(color.b * exposure))\n"
"    sub r0.rgb, c2.z, r1        ; r0 = 1.0 - exp(-(color * exposure))\n"
"\n"
";==----------------------------------------------------------------------------\n"
";\n"
";   Enhanced S-Curve Contrast\n"
";\n"
";==----------------------------------------------------------------------------\n"
"\n"
"    ; Basic contrast adjustment\n"
"    sub r1.rgb, r0, c2.y        ; r1 = color - 0.5\n"
"    mul r1.rgb, r1, c0.y        ; r1 = (color-0.5) * contrast\n"
"    add r1.rgb, r1, c2.y        ; r1 = result + 0.5\n"
"    \n"
"    ; Clamp to valid range\n"
"    max r1.rgb, r1, c2.x        ; r1 = max(0, contrast_result)\n"
"    min r1.rgb, r1, c2.z        ; r1 = min(1, contrast_result)\n"
"    \n"
"    ; Smoothstep S-curve: t*t*(3-2*t) for natural look idk\n"
"    mul r2.rgb, r1, r1          ; r2 = t²\n"
"    mul r3.rgb, r1, c3.y        ; r3 = 2*t\n"
"    sub r3.rgb, c3.x, r3        ; r3 = 3 - 2*t\n"
"    mul r2.rgb, r2, r3          ; r2 = t² * (3-2*t) = smoothstep\n"
"    \n"
"    ; Calculate blend factor for S-curve (contrast strength based)\n"
"    mov r3.x, c0.y              ; r3.x = contrast\n"
"    sub r3.x, r3.x, c2.z        ; r3.x = contrast - 1.0\n"
"    mul r3.x, r3.x, c3.z        ; r3.x = (contrast-1.0) * 0.25\n"
"    add r3.x, r3.x, c2.y        ; r3.x = blend_factor (0.5 + strength)\n"
"    max r3.x, r3.x, c2.x        ; r3.x = max(0, blend_factor)\n"
"    min r3.x, r3.x, c2.z        ; r3.x = min(1, blend_factor)\n"
"    \n"
"    ; Blend simple contrast with S-curve\n"
"    lrp r0.rgb, r3.x, r2, r1    ; r0 = lerp(simple, s_curve, blend)\n"
"\n"
";==----------------------------------------------------------------------------\n"
";\n"
";   Gamma correction with proper clamping\n"
";\n"
";==----------------------------------------------------------------------------\n"
"\n"
"    ; Clamp to safe range to prevent pow() artifacts\n"
"    max r0.rgb, r0, c3.w        ; r0 = max(color, 0.001) - prevent artifacts\n"
"    min r0.rgb, r0, c4.x        ; r0 = min(color, 0.999) - prevent overflow\n"
"    \n"
"    ; Gamma correction using pow\n"
"    pow r0.r, r0.r, c0.z        ; r0.r = pow(color.r, gamma)\n"
"    pow r0.g, r0.g, c0.z        ; r0.g = pow(color.g, gamma)\n"
"    pow r0.b, r0.b, c0.z        ; r0.b = pow(color.b, gamma)\n"
"    \n"
"    mov r0.a, c2.z              ; Preserve alpha = 1.0\n"
"    \n"
"    mov oC0, r0                 ; Output final color\n";

//=========================================================================
// FUNCTIONS
//=========================================================================

static
xbool video_CreateShaders( void )
{
    dxerr               Error;
    LPD3DXBUFFER        pVSCode = NULL;
    LPD3DXBUFFER        pPSCode = NULL;
    LPD3DXBUFFER        pErrorMsgs = NULL;

    if( !g_pd3dDevice )
        return FALSE;

    // Compile vertex shader using assembler
    Error = D3DXAssembleShader( s_VideoVS, 
                               sizeof(s_VideoVS) - 1,
                               NULL, NULL, 0,
                               &pVSCode, &pErrorMsgs );
    if( FAILED( Error ) )
    {
        if( pErrorMsgs )
        {
            DebugMessage( "VS Compile Error: %s\n", (char*)pErrorMsgs->GetBufferPointer() );
            pErrorMsgs->Release();
        }
        return FALSE;
    }

    // Compile pixel shader using assembler
    Error = D3DXAssembleShader( s_VideoPS,
                               sizeof(s_VideoPS) - 1, 
                               NULL, NULL, 0,
                               &pPSCode, &pErrorMsgs );
    if( FAILED( Error ) )
    {
        if( pErrorMsgs )
        {
            DebugMessage( "PS Compile Error: %s\n", (char*)pErrorMsgs->GetBufferPointer() );
            pErrorMsgs->Release();
        }
        if( pVSCode ) pVSCode->Release();
        return FALSE;
    }

    // Create vertex shader
    Error = g_pd3dDevice->CreateVertexShader( (DWORD*)pVSCode->GetBufferPointer(), &s.pVertexShader );
    if( FAILED( Error ) )
    {
        pVSCode->Release();
        pPSCode->Release();
        return FALSE;
    }

    // Create pixel shader
    Error = g_pd3dDevice->CreatePixelShader( (DWORD*)pPSCode->GetBufferPointer(), &s.pPixelShader );
    if( FAILED( Error ) )
    {
        s.pVertexShader->Release();
        s.pVertexShader = NULL;
        pVSCode->Release();
        pPSCode->Release();
        return FALSE;
    }

    // Clean up
    pVSCode->Release();
    pPSCode->Release();

    return TRUE;
}

//=========================================================================

static
xbool video_CreateBackbufferCopy( void )
{
    dxerr Error;
    
    if( !g_pd3dDevice )
        return FALSE;

    // Get backbuffer dimensions
    s32 XRes, YRes;
    eng_GetRes( XRes, YRes );
    
    // Only recreate if size changed
    if( s.pBackbufferCopy && s.BackbufferWidth == XRes && s.BackbufferHeight == YRes )
        return TRUE;

    // Release old texture
    if( s.pBackbufferCopy )
    {
        s.pBackbufferCopy->Release();
        s.pBackbufferCopy = NULL;
    }
    
    if( s.pBackbufferSurface )
    {
        s.pBackbufferSurface->Release();
        s.pBackbufferSurface = NULL;
    }

    // Create texture for backbuffer copy
    Error = g_pd3dDevice->CreateTexture( XRes, YRes, 1, D3DUSAGE_RENDERTARGET,
                                        D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                        &s.pBackbufferCopy, NULL );
    if( FAILED( Error ) )
        return FALSE;

    // Get surface from texture
    Error = s.pBackbufferCopy->GetSurfaceLevel( 0, &s.pBackbufferSurface );
    if( FAILED( Error ) )
    {
        s.pBackbufferCopy->Release();
        s.pBackbufferCopy = NULL;
        return FALSE;
    }

    s.BackbufferWidth  = XRes;
    s.BackbufferHeight = YRes;

    return TRUE;
}

//=========================================================================

void video_Init( void )
{
    if( s.bInitialized )
        return;

    // Enumerate display modes first
    video_EnumerateDisplayModes();

    // Create shaders and backbuffer copy
    if( video_CreateShaders() && video_CreateBackbufferCopy() )
    {
        s.bInitialized = TRUE;
        
#ifndef X_RETAIL
        extern void video_DebugInfo( void );
        video_DebugInfo();
#endif
    }
}

//=========================================================================

void video_Kill( void )
{
    if( !s.bInitialized )
        return;

    // Release shaders
    if( s.pVertexShader )
    {
        s.pVertexShader->Release();
        s.pVertexShader = NULL;
    }
    
    if( s.pPixelShader )
    {
        s.pPixelShader->Release();
        s.pPixelShader = NULL;
    }

    // Release backbuffer copy resources
    if( s.pBackbufferSurface )
    {
        s.pBackbufferSurface->Release();
        s.pBackbufferSurface = NULL;
    }
    
    if( s.pBackbufferCopy )
    {
        s.pBackbufferCopy->Release();
        s.pBackbufferCopy = NULL;
    }

    s.bInitialized = FALSE;
}

//=========================================================================

void video_OnDeviceLost( void )
{
    // Default pool resources will be automatically lost
    s.pVertexShader       = NULL;
    s.pPixelShader        = NULL;
    s.pBackbufferCopy     = NULL;
    s.pBackbufferSurface  = NULL;
}

//=========================================================================

void video_OnDeviceReset( void )
{
    if( s.bInitialized )
    {
        // Recreate shaders and backbuffer copy
        video_CreateShaders();
        video_CreateBackbufferCopy();
        
        // Re-enumerate modes in case adapter changed
        video_EnumerateDisplayModes();
    }
}

//=========================================================================

void video_SetBrightness( f32 Brightness )
{
    s.Brightness = MAX( 0.10f, MIN( 4.0f, Brightness ) );
}

//=========================================================================

f32 video_GetBrightness( void )
{
    return s.Brightness;
}

//=========================================================================

void video_SetContrast( f32 Contrast )
{
    s.Contrast = MAX( 0.10f, MIN( 2.0f, Contrast ) );
}

//=========================================================================

f32 video_GetContrast( void )
{
    return s.Contrast;
}

//=========================================================================

void video_SetGamma( f32 Gamma )
{
    s.Gamma = MAX( 0.10f, MIN( 2.0f, Gamma ) );
}

//=========================================================================

f32 video_GetGamma( void )
{
    return s.Gamma;
}

//=========================================================================

void video_ResetDefaults( void )
{
    s.Brightness     = 2.0f;
    s.Contrast       = 1.0f;
    s.Gamma          = 1.0f;
}

//=========================================================================

xbool video_IsEnabled( void )
{
    // Check if any video setting is different from default
    return s.bInitialized;
}

//=========================================================================

void video_ApplyPostEffect( void )
{
    if( !s.bInitialized || !g_pd3dDevice )
        return;

    // Check if we have valid resources
    if( !s.pVertexShader || !s.pPixelShader || !s.pBackbufferCopy )
        return;

    // Make sure backbuffer copy is correct size
    if( !video_CreateBackbufferCopy() )
        return;

    dxerr Error;

    // Get current backbuffer
    IDirect3DSurface9* pCurrentBackbuffer = NULL;
    Error = g_pd3dDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pCurrentBackbuffer );
    if( FAILED( Error ) || !pCurrentBackbuffer )
        return;

    // Copy backbuffer to our texture
    Error = g_pd3dDevice->StretchRect( pCurrentBackbuffer, NULL, s.pBackbufferSurface, NULL, D3DTEXF_NONE );
    if( FAILED( Error ) )
    {
        pCurrentBackbuffer->Release();
        return;
    }

    // Get viewport info
    const view* pView = eng_GetView();
    s32 vportL, vportT, vportR, vportB;
    pView->GetViewport( vportL, vportT, vportR, vportB );

    // Save current render states
    DWORD dwZEnable, dwZWrite, dwAlphaBlend, dwCullMode;
    g_pd3dDevice->GetRenderState( D3DRS_ZENABLE, &dwZEnable );
    g_pd3dDevice->GetRenderState( D3DRS_ZWRITEENABLE, &dwZWrite );
    g_pd3dDevice->GetRenderState( D3DRS_ALPHABLENDENABLE, &dwAlphaBlend );
    g_pd3dDevice->GetRenderState( D3DRS_CULLMODE, &dwCullMode );

    // Set render states for fullscreen pass
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Setup texture sampling with better filtering
    g_pd3dDevice->SetTexture( 0, s.pBackbufferCopy );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    // Create fullscreen quad
    struct video_vertex
    {
        f32 x, y, z, w;    // Transformed position
        f32 u, v;          // UV coordinates
    };

    video_vertex TriList[4] = {
        // Top-left
        { (f32)vportL - 0.5f, (f32)vportT - 0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
        // Top-right                   
        { (f32)vportR - 0.5f, (f32)vportT - 0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
        // Bottom-right                
        { (f32)vportR - 0.5f, (f32)vportB - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
        // Bottom-left                 
        { (f32)vportL - 0.5f, (f32)vportB - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f }
    };

    // Set shaders
    g_pd3dDevice->SetVertexShader( s.pVertexShader );
    g_pd3dDevice->SetPixelShader( s.pPixelShader );

    // Set shader constants for simplified video processing
    float constants[4];
    
    // c0: exposure_value, contrast, inverse_gamma, unused
    constants[0] = s.Brightness;                // exposure value
    constants[1] = s.Contrast;                  // contrast
    constants[2] = 1.0f / s.Gamma;              // gamma (inverted for pow function)
    constants[3] = 0.0f;                        // unused
    g_pd3dDevice->SetPixelShaderConstantF( 0, constants, 1 );

    // Render fullscreen quad
    g_pd3dDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_TEX1 );
    g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, TriList, sizeof( video_vertex ) );

    // Cleanup
    pCurrentBackbuffer->Release();

    // Restore render states
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, dwZEnable );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, dwZWrite );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, dwAlphaBlend );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, dwCullMode );

    // Restore texture sampling
    g_pd3dDevice->SetTexture( 0, NULL );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    // Clear shaders
    g_pd3dDevice->SetVertexShader( NULL );
    g_pd3dDevice->SetPixelShader( NULL );
}

//=========================================================================

static
void video_EnumerateDisplayModes( void )
{
    s.nSupportedModes = 0;
    s.CurrentModeIndex = -1;

    extern IDirect3D9* g_pD3D;  // Нужен доступ к D3D объекту
    if( !g_pD3D )
        return;

    // Get current display mode first
    D3DDISPLAYMODE currentMode;
	
	dxerr Error;
    Error = g_pD3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &currentMode );
    if( SUCCEEDED( Error ) )
    {
        s.DesktopWidth = currentMode.Width;
        s.DesktopHeight = currentMode.Height;
    }

    // Enumerate X8R8G8B8 modes
    UINT modeCount = g_pD3D->GetAdapterModeCount( D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8 );
    
    for( UINT i = 0; i < modeCount && s.nSupportedModes < 256; i++ )
    {
        D3DDISPLAYMODE mode;
        Error = g_pD3D->EnumAdapterModes( D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &mode );
        
        if( SUCCEEDED( Error ) )
        {
            // Filter minimum resolution
            if( mode.Width >= 640 && mode.Height >= 480 )
            {
                // Check for duplicates (ignore refresh rate)
                xbool bDuplicate = FALSE;
                for( s32 j = 0; j < s.nSupportedModes; j++ )
                {
                    if( s.SupportedModes[j].Width == (s32)mode.Width && 
                        s.SupportedModes[j].Height == (s32)mode.Height )
                    {
                        bDuplicate = TRUE;
                        break;
                    }
                }
                
                if( !bDuplicate )
                {
                    s.SupportedModes[s.nSupportedModes].Width = mode.Width;
                    s.SupportedModes[s.nSupportedModes].Height = mode.Height;
                    s.SupportedModes[s.nSupportedModes].RefreshRate = mode.RefreshRate;
                    s.SupportedModes[s.nSupportedModes].Format = mode.Format;
                    
                    // Check if this matches current mode
                    if( mode.Width == currentMode.Width && mode.Height == currentMode.Height )
                    {
                        s.CurrentModeIndex = s.nSupportedModes;
                    }
                    
                    s.nSupportedModes++;
                }
            }
        }
    }

    // Also enumerate A8R8G8B8 modes for better compatibility
    modeCount = g_pD3D->GetAdapterModeCount( D3DADAPTER_DEFAULT, D3DFMT_A8R8G8B8 );
    
    for( UINT i = 0; i < modeCount && s.nSupportedModes < 256; i++ )
    {
        D3DDISPLAYMODE mode;
        Error = g_pD3D->EnumAdapterModes( D3DADAPTER_DEFAULT, D3DFMT_A8R8G8B8, i, &mode );
        
        if( SUCCEEDED( Error ) )
        {
            if( mode.Width >= 640 && mode.Height >= 480 )
            {
                // Check for duplicates
                xbool bDuplicate = FALSE;
                for( s32 j = 0; j < s.nSupportedModes; j++ )
                {
                    if( s.SupportedModes[j].Width == (s32)mode.Width && 
                        s.SupportedModes[j].Height == (s32)mode.Height )
                    {
                        bDuplicate = TRUE;
                        break;
                    }
                }
                
                if( !bDuplicate )
                {
                    s.SupportedModes[s.nSupportedModes].Width = mode.Width;
                    s.SupportedModes[s.nSupportedModes].Height = mode.Height;
                    s.SupportedModes[s.nSupportedModes].RefreshRate = mode.RefreshRate;
                    s.SupportedModes[s.nSupportedModes].Format = mode.Format;
                    s.nSupportedModes++;
                }
            }
        }
    }

    // Sort modes by resolution (smallest first)
    for( s32 i = 0; i < s.nSupportedModes - 1; i++ )
    {
        for( s32 j = i + 1; j < s.nSupportedModes; j++ )
        {
            if( (s.SupportedModes[i].Width * s.SupportedModes[i].Height) > 
                (s.SupportedModes[j].Width * s.SupportedModes[j].Height) )
            {
                resolution_mode temp = s.SupportedModes[i];
                s.SupportedModes[i] = s.SupportedModes[j];
                s.SupportedModes[j] = temp;
            }
        }
    }
}

//=========================================================================

void video_GetDesktopResolution( s32& Width, s32& Height )
{
    if( s.DesktopWidth > 0 && s.DesktopHeight > 0 )
    {
        Width = s.DesktopWidth;
        Height = s.DesktopHeight;
        return;
    }
    
    // Fallback to system metrics
    Width = GetSystemMetrics( SM_CXSCREEN );
    Height = GetSystemMetrics( SM_CYSCREEN );
    
    if( Width > 0 && Height > 0 )
    {
        s.DesktopWidth = Width;
        s.DesktopHeight = Height;
    }
}

//=========================================================================

s32 video_GetSupportedModeCount( void )
{
    return s.nSupportedModes;
}

//=========================================================================

xbool video_GetSupportedMode( s32 Index, s32& Width, s32& Height, s32& RefreshRate )
{
    if( Index < 0 || Index >= s.nSupportedModes )
        return FALSE;
        
    Width = s.SupportedModes[Index].Width;
    Height = s.SupportedModes[Index].Height;
    RefreshRate = s.SupportedModes[Index].RefreshRate;
    return TRUE;
}

//=========================================================================

s32 video_FindResolutionMode( s32 Width, s32 Height )
{
    for( s32 i = 0; i < s.nSupportedModes; i++ )
    {
        if( s.SupportedModes[i].Width == Width && s.SupportedModes[i].Height == Height )
            return i;
    }
    return -1;
}

//=========================================================================

xbool video_ChangeResolution( s32 NewWidth, s32 NewHeight, xbool bFullscreen )
{
    if( !g_pd3dDevice )
        return FALSE;

    // Check if resolution is supported for fullscreen mode
    if( !bFullscreen || video_FindResolutionMode( NewWidth, NewHeight ) != -1 )
    {
        // Get current resolution for rollback
        s32 OldWidth, OldHeight;
        eng_GetRes( OldWidth, OldHeight );
        
        // Call engine's resolution change function
        extern xbool d3deng_InternalChangeResolution( s32 Width, s32 Height, xbool bFullscreen );
        if( d3deng_InternalChangeResolution( NewWidth, NewHeight, bFullscreen ) )
        {
            // Update backbuffer copy for new resolution
            if( !video_CreateBackbufferCopy() )
            {
                // If backbuffer creation fails, try to rollback
                d3deng_InternalChangeResolution( OldWidth, OldHeight, FALSE );
                return FALSE;
            }
            
            return TRUE;
        }
    }
    
    return FALSE;
}

//=========================================================================

xbool video_ToggleFullscreen( void )
{
    s32 currentWidth, currentHeight;
    eng_GetRes( currentWidth, currentHeight );
    
    // Get current windowed state from engine
    extern xbool d3deng_GetWindowedState( void );
    xbool bCurrentlyWindowed = d3deng_GetWindowedState();
    
    return video_ChangeResolution( currentWidth, currentHeight, bCurrentlyWindowed );
}

//=========================================================================

void video_SetResolutionMode( s32 ModeIndex, xbool bFullscreen )
{
    if( ModeIndex >= 0 && ModeIndex < s.nSupportedModes )
    {
        video_ChangeResolution( s.SupportedModes[ModeIndex].Width, 
                               s.SupportedModes[ModeIndex].Height, 
                               bFullscreen );
    }
}

//=========================================================================

s32 video_GetCurrentModeIndex( void )
{
    s32 currentWidth, currentHeight;
    eng_GetRes( currentWidth, currentHeight );
    return video_FindResolutionMode( currentWidth, currentHeight );
}

//=========================================================================

#ifndef X_RETAIL
void video_DebugInfo( void )
{
    DebugMessage( "****************************\n" );
    DebugMessage( "*** VIDEO SETTINGS DEBUG ***\n" );
    DebugMessage( "****************************\n" );
    DebugMessage( "Initialized: %s\n", s.bInitialized ? "YES" : "NO" );
    DebugMessage( "Effect enabled: %s\n", video_IsEnabled() ? "YES" : "NO" );
    DebugMessage( "Exposure: %f\n", s.Brightness );
    DebugMessage( "Contrast: %f\n", s.Contrast );
    DebugMessage( "Gamma: %f\n", s.Gamma );
    DebugMessage( "Vertex shader: %p\n", s.pVertexShader );
    DebugMessage( "Pixel shader: %p\n", s.pPixelShader );
    DebugMessage( "Backbuffer copy: %p\n", s.pBackbufferCopy );
    DebugMessage( "Backbuffer size: %dx%d\n", s.BackbufferWidth, s.BackbufferHeight );
    DebugMessage( "Desktop resolution: %dx%d\n", s.DesktopWidth, s.DesktopHeight );
    DebugMessage( "Supported modes: %d\n", s.nSupportedModes );
    
    for( s32 i = 0; i < s.nSupportedModes && i < 10; i++ )
    {
        DebugMessage( "  Mode %d: %dx%d @ %dHz\n", i, 
                     s.SupportedModes[i].Width, 
                     s.SupportedModes[i].Height,
                     s.SupportedModes[i].RefreshRate );
    }
    if( s.nSupportedModes > 10 )
        DebugMessage( "  ... and %d more modes\n", s.nSupportedModes - 10 );
}
#endif