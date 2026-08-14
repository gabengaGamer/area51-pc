//==============================================================================
//
//  a51_ui.hlsl
//
//  Screen-space UI shader.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbUIDraw A51_CBUFFER_BIND(0, 0)
{
    float2 LogicalOrigin;
    float2 InverseLogicalSize;
};

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txUI A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samUI A51_SAMPLER_BIND(0, 0);

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float2 Position : TEXCOORD0;
    float4 Color    : TEXCOORD1;
    float2 UV       : TEXCOORD2;
};

//------------------------------------------------------------------------------

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
    float2 UV       : TEXCOORD0;
};

//==============================================================================
//  SHADERS
//==============================================================================

PS_INPUT VSMain( VS_INPUT input )
{
    PS_INPUT output;
    const float2 normalized = (input.Position - LogicalOrigin) * InverseLogicalSize;
    output.Position = float4( normalized.x * 2.0f - 1.0f,
                              1.0f - normalized.y * 2.0f,
                              0.0f,
                              1.0f );
    output.Color = input.Color.bgra;
    output.UV = input.UV;
    return output;
}

//==============================================================================

float4 PSMain( PS_INPUT input ) : SV_Target0
{
    return txUI.Sample( samUI, input.UV ) * input.Color;
}