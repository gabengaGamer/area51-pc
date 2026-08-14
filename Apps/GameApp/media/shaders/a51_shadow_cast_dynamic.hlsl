//==============================================================================
//
//  a51_shadow_cast_dynamic.hlsl
//
//  Shadow depth caster shader for runtime cloth geometry.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "common/cloth_damage.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbShadowCast A51_CBUFFER_BIND(0, 0)
{
    float4x4 ShadowViewProjection;
    uint4    ShadowCastPadding;
};

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDiffuse A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D txDamage A51_SAMPLED_TEXTURE_BIND(1, 1);
A51_SAMPLER_ATTR(0, 0) SamplerState samDiffuse A51_SAMPLER_BIND(0, 0);
A51_SAMPLER_ATTR(1, 1) SamplerState samDamage A51_SAMPLER_BIND(1, 1);

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position : TEXCOORD0;
    float2 UV       : TEXCOORD1;
};

//------------------------------------------------------------------------------

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

//==============================================================================
//  SHADERS
//==============================================================================

VS_OUTPUT VSMain( VS_INPUT input )
{
    VS_OUTPUT output;
    output.Position = mul( ShadowViewProjection, float4( input.Position, 1.0f ) );
    output.UV = input.UV;
    return output;
}

//==============================================================================

void PSMain( VS_OUTPUT input )
{
    float const diffuseAlpha = txDiffuse.Sample( samDiffuse, input.UV ).a;
    float const damage = ClothDamageCoverage( txDamage, samDamage, input.UV );
    clip( diffuseAlpha * damage - 0.05f );
}