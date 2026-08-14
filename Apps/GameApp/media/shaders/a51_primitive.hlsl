//==============================================================================
//
//  a51_primitive.hlsl
//
//  Basic renderer-owned primitive shader.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbPrimitiveDraw A51_CBUFFER_BIND(0, 0)
{
    float4x4 LocalToClip;
    uint   OutputMode;
    float  DistortionScale;
    float2 PrimitiveDrawPadding;
};

//------------------------------------------------------------------------------

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txPrimitive A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samPrimitive A51_SAMPLER_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D txPrimitiveScene A51_SAMPLED_TEXTURE_BIND(1, 1);
A51_SAMPLER_ATTR(1, 1) SamplerState samPrimitiveScene A51_SAMPLER_BIND(1, 1);

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position : TEXCOORD0;
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

    output.Position = mul( LocalToClip, float4( input.Position, 1.0f ) );
    output.Color    = input.Color.bgra;
    output.UV       = input.UV;

    return output;
}

//==============================================================================

float4 PSMain( PS_INPUT input ) : SV_Target0
{
    float4 surface = txPrimitive.Sample( samPrimitive, input.UV ) * input.Color;

    static const uint PRIMITIVE_OUTPUT_DISTORTION = 2u;
    if( OutputMode == PRIMITIVE_OUTPUT_DISTORTION )
    {
        uint width;
        uint height;
        txPrimitiveScene.GetDimensions( width, height );

        float2 inverseSize  = rcp( float2( width, height ) );
        float2 screenUV     = input.Position.xy * inverseSize;
        float2 displacement = (surface.rg * 2.0f - 1.0f) * surface.a * DistortionScale * inverseSize;
        float4 distorted    = txPrimitiveScene.Sample( samPrimitiveScene, screenUV + displacement );
        distorted.a         = surface.a;
        return distorted;
    }

    return surface;
}