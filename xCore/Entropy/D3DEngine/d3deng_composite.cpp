//==============================================================================
//  
//  d3deng_composite.cpp
//  
//  Composite manager for D3DENG
//
//==============================================================================

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

#include "d3deng_composite.hpp"
#include "d3deng_shader.hpp"
#include "d3deng_state.hpp"
#include "d3deng_rtarget.hpp"

#include "e_Engine.hpp"

//==============================================================================
//  COMPOSITE SHADERS
//==============================================================================

static const char* s_CompositeVertexShader = 
"struct VS_INPUT\n"
"{\n"
"    float3 Position : POSITION;\n"
"    float2 UV       : TEXCOORD0;\n"
"};\n"
"\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 Position : SV_POSITION;\n"
"    float2 UV       : TEXCOORD0;\n"
"};\n"
"\n"
"VS_OUTPUT main(VS_INPUT input)\n"
"{\n"
"    VS_OUTPUT output;\n"
"    output.Position = float4(input.Position, 1.0f);\n"
"    output.UV = input.UV;\n"
"    return output;\n"
"}\n";

//------------------------------------------------------------------------------

static const char* s_CompositePixelShader =
"Texture2D    g_Texture    : register(t0);\n"
"SamplerState g_Sampler    : register(s0);\n"
"\n"
"cbuffer CompositeParams : register(b1)\n"
"{\n"
"    float4 BlendColor;\n"
"    int    BlendMode;\n"
"    float3 Padding;\n"
"};\n"
"\n"
"struct PS_INPUT\n"
"{\n"
"    float4 Position : SV_POSITION;\n"
"    float2 UV       : TEXCOORD0;\n"
"};\n"
"\n"
"float4 main(PS_INPUT input) : SV_TARGET\n"
"{\n"
"    float4 texColor = g_Texture.Sample(g_Sampler, input.UV);\n"
"    \n"
"    texColor *= BlendColor;\n"
"    \n"
"    if (BlendMode == 3) // COMPOSITE_BLEND_OVERLAY\n"
"    {\n"
"        float3 overlay;\n"
"        overlay.r = texColor.r < 0.5 ? 2.0 * texColor.r * texColor.r : 1.0 - 2.0 * (1.0 - texColor.r) * (1.0 - texColor.r);\n"
"        overlay.g = texColor.g < 0.5 ? 2.0 * texColor.g * texColor.g : 1.0 - 2.0 * (1.0 - texColor.g) * (1.0 - texColor.g);\n"
"        overlay.b = texColor.b < 0.5 ? 2.0 * texColor.b * texColor.b : 1.0 - 2.0 * (1.0 - texColor.b) * (1.0 - texColor.b);\n"
"        texColor.rgb = overlay;\n"
"    }\n"
"    return texColor;\n"
"}\n";

//==============================================================================
//  STRUCTURES
//==============================================================================

struct composite_vertex
{
    f32 x, y, z;    // Position
    f32 u, v;       // UV
};

//------------------------------------------------------------------------------

struct cb_composite_params
{
    vector4 BlendColor;     // RGB + Alpha multiplier
    s32     BlendMode;      // Blend mode selector
    vector3 Padding;
};

//------------------------------------------------------------------------------

static struct composite_locals
{
    composite_locals( void ) { x_memset( this, 0, sizeof(composite_locals) ); }

    xbool                   bInitialized;
    
    // Geometry for full screen quad
    ID3D11Buffer*           pVertexBuffer;
    ID3D11Buffer*           pIndexBuffer;
    ID3D11InputLayout*      pInputLayout;
    
    // Shaders
    ID3D11VertexShader*     pVertexShader;
    ID3D11PixelShader*      pPixelShader;
    
    // Constant buffer
    ID3D11Buffer*           pConstantBuffer;
    
} s;

//==============================================================================
//  INPUT LAYOUT
//==============================================================================

static D3D11_INPUT_ELEMENT_DESC s_CompositeInputLayout[] = 
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static state_blend_mode composite_GetHardwareBlendMode( composite_blend_mode BlendMode )
{
    switch( BlendMode )
    {
        case COMPOSITE_BLEND_ALPHA:     return STATE_BLEND_ALPHA;
        case COMPOSITE_BLEND_ADDITIVE:  return STATE_BLEND_ADD;
        case COMPOSITE_BLEND_MULTIPLY:  return STATE_BLEND_MULTIPLY;
        case COMPOSITE_BLEND_OVERLAY:   return STATE_BLEND_PREMULT_ALPHA;
        default:                        return STATE_BLEND_ALPHA;
    }
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

void composite_Init( void )
{
    if( s.bInitialized )
    {
        x_DebugMsg( "CompositeMgr: WARNING - Already initialized\n" );
        return;
    }

    x_DebugMsg( "CompositeMgr: Initializing composite system\n" );

    if( !g_pd3dDevice )
    {
        x_DebugMsg( "CompositeMgr: ERROR - No D3D device available\n" );
        return;
    }

    // Create full screen quad vertices  
    composite_vertex vertices[4] = 
    {
        { -1.0f,  1.0f, 0.5f,   0.0f, 0.0f }, // Top-left
        { -1.0f, -1.0f, 0.5f,   0.0f, 1.0f }, // Bottom-left  
        {  1.0f, -1.0f, 0.5f,   1.0f, 1.0f }, // Bottom-right
        {  1.0f,  1.0f, 0.5f,   1.0f, 0.0f }  // Top-right
    };

    u16 indices[6] = { 0, 2, 1, 0, 3, 2 };

    // Create vertex buffer
    D3D11_BUFFER_DESC vbd;
    x_memset( &vbd, 0, sizeof(vbd) );
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = 0;
    vbd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vInitData;
    vInitData.pSysMem = vertices;
    vInitData.SysMemPitch = 0;
    vInitData.SysMemSlicePitch = 0;

    HRESULT hr = g_pd3dDevice->CreateBuffer( &vbd, &vInitData, &s.pVertexBuffer );
    if( FAILED(hr) )
    {
        x_DebugMsg( "CompositeMgr: ERROR - Failed to create vertex buffer\n" );
        return;
    }

    // Create index buffer
    D3D11_BUFFER_DESC ibd;
    x_memset( &ibd, 0, sizeof(ibd) );
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;
    ibd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA iInitData;
    iInitData.pSysMem = indices;
    iInitData.SysMemPitch = 0;
    iInitData.SysMemSlicePitch = 0;

    hr = g_pd3dDevice->CreateBuffer( &ibd, &iInitData, &s.pIndexBuffer );
    if( FAILED(hr) )
    {
        x_DebugMsg( "CompositeMgr: ERROR - Failed to create index buffer\n" );
        composite_Kill();
        return;
    }

    // Create shaders
    s.pVertexShader = shader_CompileVertexWithLayout( s_CompositeVertexShader,
                                                     &s.pInputLayout,
                                                     s_CompositeInputLayout,
                                                     ARRAYSIZE(s_CompositeInputLayout),
                                                     "main",
                                                     "vs_5_0" );

    s.pPixelShader = shader_CompilePixel( s_CompositePixelShader,
                                         "main", 
                                         "ps_5_0" );

    if( !s.pVertexShader || !s.pPixelShader )
    {
        x_DebugMsg( "CompositeMgr: ERROR - Failed to create shaders\n" );
        composite_Kill();
        return;
    }

    // Create constant buffer
    s.pConstantBuffer = shader_CreateConstantBuffer( sizeof(cb_composite_params) );
    if( !s.pConstantBuffer )
    {
        x_DebugMsg( "CompositeMgr: ERROR - Failed to create constant buffer\n" );
        composite_Kill();
        return;
    }

    s.bInitialized = TRUE;

    x_DebugMsg( "CompositeMgr: Initialization complete\n" );
}

//==============================================================================

void composite_Kill( void )
{
    if( !s.bInitialized )
        return;

    x_DebugMsg( "CompositeMgr: Shutting down composite system\n" );

    if( s.pConstantBuffer )     { s.pConstantBuffer->Release();    s.pConstantBuffer = NULL; }
    if( s.pPixelShader )        { s.pPixelShader->Release();       s.pPixelShader = NULL; }
    if( s.pVertexShader )       { s.pVertexShader->Release();      s.pVertexShader = NULL; }  
    if( s.pInputLayout )        { s.pInputLayout->Release();       s.pInputLayout = NULL; }
    if( s.pIndexBuffer )        { s.pIndexBuffer->Release();       s.pIndexBuffer = NULL; }
    if( s.pVertexBuffer )       { s.pVertexBuffer->Release();      s.pVertexBuffer = NULL; }

    s.bInitialized = FALSE;

    x_DebugMsg( "CompositeMgr: Shutdown complete\n" );
}

//==============================================================================

void composite_Blit( const rtarget& Source, composite_blend_mode BlendMode, f32 Alpha, ID3D11PixelShader* pCustomShader )
{
    if( !s.bInitialized )
    {
        x_DebugMsg( "CompositeMgr: ERROR - Not initialized\n" );
        return;
    }

    if( !g_pd3dContext || !Source.pShaderResourceView )
        return;

    // Set viewport for current render target
    const view* pView = eng_GetView();
    if( pView )
    {
        eng_SetViewport( *pView );
    }

   // Limit the composite blind viewport for better working with downscaled buffers
   if( g_pd3dContext )
   {
       const rtarget* pCurrentTarget = rtarget_GetCurrentTarget( 0 );
       if( pCurrentTarget && pCurrentTarget->Desc.Width && pCurrentTarget->Desc.Height )
       {
           UINT viewportCount = 1;
           D3D11_VIEWPORT viewport;
           g_pd3dContext->RSGetViewports( &viewportCount, &viewport );
   
           if( viewportCount > 0 )
           {
               f32 maxWidth  = (f32)pCurrentTarget->Desc.Width;
               f32 maxHeight = (f32)pCurrentTarget->Desc.Height;
               f32 width     = MIN( viewport.Width,  maxWidth );
               f32 height    = MIN( viewport.Height, maxHeight );
               f32 topLeftX  = MAX( viewport.TopLeftX, 0.0f );
               f32 topLeftY  = MAX( viewport.TopLeftY, 0.0f );
   
               if( (width != viewport.Width) || (height != viewport.Height) ||
                   (topLeftX != viewport.TopLeftX) || (topLeftY != viewport.TopLeftY) )
               {
                   viewport.TopLeftX = topLeftX;
                   viewport.TopLeftY = topLeftY;
                   viewport.Width    = width;
                   viewport.Height   = height;
                   g_pd3dContext->RSSetViewports( 1, &viewport );
               }
           }
       }
   }

    // Set up for composite rendering
    state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_DISABLED_NO_WRITE );
    state_SetState( STATE_TYPE_RASTERIZER, STATE_RASTER_SOLID_NO_CULL );

    // Set blend mode based on composite blend mode
    state_blend_mode HardwareBlendMode = composite_GetHardwareBlendMode( BlendMode );
    state_SetState( STATE_TYPE_BLEND, HardwareBlendMode );

    // Set sampler
    state_SetState( STATE_TYPE_SAMPLER, STATE_SAMPLER_POINT_CLAMP );

    // Set vertex buffer
    UINT stride = sizeof(composite_vertex);
    UINT offset = 0;
    g_pd3dContext->IASetVertexBuffers( 0, 1, &s.pVertexBuffer, &stride, &offset );
    g_pd3dContext->IASetIndexBuffer( s.pIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );
    g_pd3dContext->IASetInputLayout( s.pInputLayout );
    g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Set shaders
    g_pd3dContext->VSSetShader( s.pVertexShader, NULL, 0 );
    
    ID3D11PixelShader* pPixelShader = pCustomShader ? pCustomShader : s.pPixelShader;
    g_pd3dContext->PSSetShader( pPixelShader, NULL, 0 );

    // Update constant buffer with proper blend mode
    cb_composite_params cbData;
    cbData.BlendColor = vector4( 1.0f, 1.0f, 1.0f, Alpha );
    cbData.BlendMode = (s32)BlendMode;  // Now properly using the BlendMode parameter
    cbData.Padding = vector3( 0.0f, 0.0f, 0.0f );

    shader_UpdateConstantBuffer( s.pConstantBuffer, &cbData, sizeof(cbData) );
    g_pd3dContext->PSSetConstantBuffers( 1, 1, &s.pConstantBuffer );

    // Set texture
    g_pd3dContext->PSSetShaderResources( 0, 1, &Source.pShaderResourceView );

    // Render full screen quad
    g_pd3dContext->DrawIndexed( 6, 0, 0 );

    // Clear texture binding
    ID3D11ShaderResourceView* nullSRV = NULL;
    g_pd3dContext->PSSetShaderResources( 0, 1, &nullSRV );

    // Restore states
    state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_NORMAL );
    state_SetState( STATE_TYPE_RASTERIZER, STATE_RASTER_SOLID );
}