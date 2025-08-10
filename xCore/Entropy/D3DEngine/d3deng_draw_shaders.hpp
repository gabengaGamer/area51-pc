//==============================================================================
//  
//  d3deng_draw_shaders.hpp
//  
//  Basic shaders liblary for d3deng_draw
//
//==============================================================================

// VERY IMPORTANT NOTE: README README README README!!!!!
//
// These are basic solutions, mostly not related to the game rendering system,
// they are intended as basic modules for Entropy (D3DENG_DRAW).
// Please use your own solutions, in OTHER files.
//

#ifndef D3DENG_DRAW_SHADERS_HPP
#define D3DENG_DRAW_SHADERS_HPP

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  D3DENG DRAW CONSTANT BUFFER STRUCTURES
//==============================================================================

struct cb_matrices
{
    matrix4 World;
    matrix4 View;
    matrix4 Projection;
};

struct cb_projection_2d
{
    f32 ScreenWidth;
    f32 ScreenHeight;
    f32 pad0;
    f32 pad1;
};

struct cb_render_flags
{
    s32 UseTexture;
    s32 UseAlpha;
    s32 UsePremultipliedAlpha;
    s32 pad1;
};

//==============================================================================
//  D3DENG DRAW SHADER SOURCES
//==============================================================================

// Vertex shader for 3D rendering
static const char* s_VertexShader3D = 
"cbuffer cbMatrices : register(b0)\n"
"{\n"
"    float4x4 World;\n"
"    float4x4 View;\n"
"    float4x4 Projection;\n"
"};\n"
"struct VS_INPUT\n"
"{\n"
"    float3 Pos : POSITION;\n"
"    float4 Color : COLOR;\n"
"    float2 UV : TEXCOORD;\n"
"};\n"
"struct PS_INPUT\n"
"{\n"
"    float4 Pos : SV_POSITION;\n"
"    float4 Color : COLOR;\n"
"    float2 UV : TEXCOORD;\n"
"};\n"
"PS_INPUT main(VS_INPUT input)\n"
"{\n"
"    PS_INPUT output;\n"
"    float4 worldPos = mul(World, float4(input.Pos, 1.0));\n" 
"    float4 viewPos = mul(View, worldPos);\n"  
"    output.Pos = mul(Projection, viewPos);\n" 
"    output.Color = input.Color;\n"
"    output.UV = input.UV;\n"
"    return output;\n"
"}\n";

// Vertex shader for 2D rendering
static const char* s_VertexShader2D = 
"cbuffer cbProjection : register(b1)\n"
"{\n"
"    float ScreenWidth;\n"
"    float ScreenHeight;\n"
"    float pad0;\n"
"    float pad1;\n"
"};\n"
"struct VS_INPUT\n"
"{\n"
"    float3 Pos : POSITION;\n"
"    float RHW : RHW;\n"
"    float4 Color : COLOR;\n"
"    float2 UV : TEXCOORD;\n"
"};\n"
"struct PS_INPUT\n"
"{\n"
"    float4 Pos : SV_POSITION;\n"
"    float4 Color : COLOR;\n"
"    float2 UV : TEXCOORD;\n"
"};\n"
"PS_INPUT main(VS_INPUT input)\n"
"{\n"
"    PS_INPUT output;\n"
"    float adjustedX = input.Pos.x + 0.5;\n"  // Half-pixel offset
"    float adjustedY = input.Pos.y + 0.5;\n"  // Half-pixel offset
"    \n"
"    output.Pos.x = (adjustedX * 2.0 / ScreenWidth) - 1.0;\n"
"    output.Pos.y = 1.0 - (adjustedY * 2.0 / ScreenHeight);\n"
"    output.Pos.z = input.Pos.z;\n"
"    output.Pos.w = 1.0;\n"
"    output.Color = input.Color;\n"
"    output.UV = input.UV;\n"
"    return output;\n"
"}\n";

// Basic pixel shader
static const char* s_PixelShaderBasic = 
"Texture2D txDiffuse : register(t0);\n"
"SamplerState samLinear : register(s0);\n"
"struct PS_INPUT\n"
"{\n"
"    float4 Pos : SV_POSITION;\n"
"    float4 Color : COLOR;\n"
"    float2 UV : TEXCOORD;\n"
"};\n"
"cbuffer cbFlags : register(b1)\n"
"{\n"
"    int UseTexture;\n"
"    int UseAlpha;\n"
"    int UsePremultipliedAlpha;\n"
"    int pad1;\n"
"};\n"
"float4 main(PS_INPUT input) : SV_Target\n"
"{\n"
"    float4 finalColor;\n"
"    \n"
"    if (UseTexture > 0)\n"
"    {\n"
"        float4 texColor = txDiffuse.Sample(samLinear, input.UV);\n"
"        finalColor = texColor * input.Color;\n"
"    }\n"
"    else\n"
"    {\n"
"        finalColor = input.Color;\n"
"    }\n"
"    \n"
"    if (UsePremultipliedAlpha > 0)\n"
"    {\n"
"        finalColor.rgb *= finalColor.a;\n"
"    }\n"
"    \n"
"    return finalColor;\n"
"}\n";

//==============================================================================
//  D3DENG DRAW INPUT LAYOUTS
//==============================================================================

// Standard 3D vertex layout: Position, Color, UV
static const D3D11_INPUT_ELEMENT_DESC s_InputLayout3D[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

// Standard 2D vertex layout: Position, RHW, Color, UV
static const D3D11_INPUT_ELEMENT_DESC s_InputLayout2D[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "RHW",      0, DXGI_FORMAT_R32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

//==============================================================================
#endif // D3DENG_DRAW_SHADERS_HPP
//==============================================================================