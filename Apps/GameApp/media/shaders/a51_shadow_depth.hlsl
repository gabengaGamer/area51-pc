//==============================================================================
//
//  a51_shadow_depth.hlsl
//
//  Depth-only shadow caster pixel shaders.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDiffuse A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samDiffuse A51_SAMPLER_BIND(0, 0);

//------------------------------------------------------------------------------

A51_CBUFFER_ATTR(1, 0) cbuffer cbShadowAlpha A51_CBUFFER_BIND(1, 0)
{
    float  AlphaRef;
    float  Padding;
    float2 UVOffset;
};

//==============================================================================
//  TYPES
//==============================================================================

struct PS_CAST_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

//==============================================================================
//  SHADERS
//==============================================================================

void PSCastOpaqueDepth( PS_CAST_INPUT input )
{
}

//==============================================================================

void PSCastAlphaDepth( PS_CAST_INPUT input )
{
    if( txDiffuse.Sample( samDiffuse, input.UV + UVOffset ).a < AlphaRef )
    {
        discard;
    }
}
