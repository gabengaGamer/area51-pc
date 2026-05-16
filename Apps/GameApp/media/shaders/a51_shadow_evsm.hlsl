//==============================================================================
//
//  a51_shadow_evsm.hlsl
//
//  Raw shadow-depth caster for PC shadows.
//
//==============================================================================

#include "common/material_flags.hlsl"

Texture2D    txDiffuse : register(t0);
SamplerState samLinear : register(s0);

//------------------------------------------------------------------------------

cbuffer cbShadowAlpha : register(b1)
{
    uint   MaterialFlags;
    float  AlphaRef;
    float2 UVOffset;
};

//------------------------------------------------------------------------------

struct PS_CAST_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

//==============================================================================

float PSCastMoments( PS_CAST_INPUT input ) : SV_TARGET
{
    if( MaterialFlags & MATERIAL_FLAG_ALPHA_TEST )
    {
        if( txDiffuse.Sample( samLinear, input.UV + UVOffset ).a < AlphaRef )
            discard;
    }

    return saturate( input.Position.z );
}
