//==============================================================================
//
//  a51_shadow_evsm_blur.hlsl
//
//  Separable EVSM4 moment-atlas filtering.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"

//==============================================================================
//  TYPES
//==============================================================================

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D<float4> txShadowMoments A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samShadowMoments A51_SAMPLER_BIND(0, 0);

A51_CBUFFER_ATTR(1, 0) cbuffer cbShadowFilter A51_CBUFFER_BIND(1, 0)
{
    float4 TextureParams;
    float4 SourceClampRect;
    float4 FilterParams; // x/y = EVSM exponents, z = blur scale
    float4 DepthParams;
    float4 SourceProjectionParams;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

float4 SampleGaussian9( float2 uv, float2 direction )
{
    float4 result = txShadowMoments.SampleLevel( samShadowMoments, uv, 0.0f ) * 0.2270270270f;
    result +=
        txShadowMoments.SampleLevel( samShadowMoments, uv + direction * 1.3846153846f, 0.0f ) * 0.3162162162f;
    result +=
        txShadowMoments.SampleLevel( samShadowMoments, uv - direction * 1.3846153846f, 0.0f ) * 0.3162162162f;
    result +=
        txShadowMoments.SampleLevel( samShadowMoments, uv + direction * 3.2307692308f, 0.0f ) * 0.0702702703f;
    result +=
        txShadowMoments.SampleLevel( samShadowMoments, uv - direction * 3.2307692308f, 0.0f ) * 0.0702702703f;
    return result;
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PSBlurHorizontal( VS_OUTPUT input ) : SV_Target
{
    const float2 uv = input.Position.xy * TextureParams.xy;
    return SampleGaussian9( uv, float2( TextureParams.x, 0.0f ) * FilterParams.z );
}

//==============================================================================

float4 PSBlurVertical( VS_OUTPUT input ) : SV_Target
{
    const float2 uv = input.Position.xy * TextureParams.xy;
    return SampleGaussian9( uv, float2( 0.0f, TextureParams.y ) * FilterParams.z );
}