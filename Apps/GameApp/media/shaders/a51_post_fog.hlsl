//==============================================================================
//
//  a51_post_fog.hlsl
//
//  Depth-based fog composite shader.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "common/fog_functions.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D NormalDepthSource A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(2, 1) Texture2D FogPalette A51_SAMPLED_TEXTURE_BIND(2, 1);

A51_SAMPLER_ATTR(0, 0) SamplerState samNormalDepthSource A51_SAMPLER_BIND(0, 0);
#if defined(A51_SHADER_BINDING_SDL)
    A51_SAMPLER_ATTR(2, 1) SamplerState samFogPalette A51_SAMPLER_BIND(2, 1);
#else
    #define samFogPalette samNormalDepthSource
#endif

//------------------------------------------------------------------------------

A51_CBUFFER_ATTR(4, 0) cbuffer FogParams A51_CBUFFER_BIND(4, 0)
{
    float4 FogColor;
    float4 FogCoeff;
    float4 FogParams0; // x = near, y = far, z = use polynomial, w = fog start
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

float SampleLinearDepth( float2 UV )
{
    return saturate( NormalDepthSource.SampleLevel( samNormalDepthSource, UV, 0.0f ).a );
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PSMain( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float  linearDepth = SampleLinearDepth( UV );
    const float4 fogSample   = FogPalette.SampleLevel( samFogPalette, float2( linearDepth, 0.5f ), 0.0f );
    return float4( fogSample.rgb, saturate( fogSample.a ) );
}

//==============================================================================

float4 PSPolynomial( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float  linearDepth  = SampleLinearDepth( UV );
    const float  alpha        = A51ComputePolynomialFogAlpha( linearDepth, FogParams0.x, FogParams0.y,
                                                               FogParams0.w, FogCoeff );

    return float4( FogColor.rgb, alpha );
}
