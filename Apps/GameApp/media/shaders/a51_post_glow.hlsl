//==============================================================================
//
//  a51_post_glow.hlsl
//
//  Post-processing glow shaders.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(4, 0) cbuffer GlowParams A51_CBUFFER_BIND(4, 0)
{
    float4 GlowParams0;   // x=cutoff, y=accumulator retention, z=current-frame weight, w=unused
    float4 GlowParams1;   // x=step.x, y=step.y, z/w=unused
};

//------------------------------------------------------------------------------

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D GlowSource A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D GlowAux A51_SAMPLED_TEXTURE_BIND(1, 1);

A51_SAMPLER_ATTR(0, 0) SamplerState samGlowSource A51_SAMPLER_BIND(0, 0);
#if defined(A51_SHADER_BINDING_SDL)
    A51_SAMPLER_ATTR(1, 1) SamplerState samGlowAux A51_SAMPLER_BIND(1, 1);
#else
    #define samGlowAux samGlowSource
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

static const float kWeightNorm = 1.0f / 255.0f;
static const float kHorzWeightsRaw[5] = { 24.0f, 20.0f, 16.0f, 10.0f, 8.0f };
static const float kVertWeightsRaw[5] = { 96.0f, 40.0f, 32.0f, 20.0f, 16.0f };

//==============================================================================
//  SHADERS
//==============================================================================

float4 PS_Downsample( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float2 texel = GlowParams1.xy;

    float3 colorAccum = 0.0f;
    float  maxAlpha   = 0.0f;

    const float2 offsets[4] =
    {
        float2( -0.5f, -0.5f ),
        float2(  0.5f, -0.5f ),
        float2( -0.5f,  0.5f ),
        float2(  0.5f,  0.5f )
    };

    [unroll]
    for( int i = 0; i < 4; ++i )
    {
        float2 sampleUV = UV + offsets[i] * texel;
        float4 sample   = GlowSource.SampleLevel( samGlowSource, sampleUV, 0.0f );
        float  mask     = step( GlowParams0.x, sample.a );

        colorAccum += sample.rgb * mask;
        maxAlpha    = max( maxAlpha, sample.a * mask );
    }

    return float4( colorAccum * 0.25f, maxAlpha );
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

float4 SampleJitterHorizontal( float2 UV, float stepX )
{
    float4 center   = GlowSource.SampleLevel( samGlowSource, UV, 0.0f );
    float3 color    = center.rgb * kHorzWeightsRaw[0];
    float  alphaMax = center.a;

    [unroll]
    for( int i = 1; i < 5; ++i )
    {
        float offset = stepX * (float)i;
        float4 samplePos = GlowSource.SampleLevel( samGlowSource, UV + float2( offset, 0.0f ), 0.0f );
        float4 sampleNeg = GlowSource.SampleLevel( samGlowSource, UV - float2( offset, 0.0f ), 0.0f );

        color += ( samplePos.rgb + sampleNeg.rgb ) * kHorzWeightsRaw[i];
        alphaMax = max( alphaMax, max( samplePos.a, sampleNeg.a ) );
    }

    return float4( color * kWeightNorm, alphaMax );
}

//==============================================================================

float4 SampleJitterVertical( float2 UV, float stepY )
{
    float4 center   = GlowSource.SampleLevel( samGlowSource, UV, 0.0f );
    float3 color    = center.rgb * kVertWeightsRaw[0];
    float  alphaMax = center.a;

    [unroll]
    for( int i = 1; i < 5; ++i )
    {
        float offset = stepY * (float)i;
        float4 samplePos = GlowSource.SampleLevel( samGlowSource, UV + float2( 0.0f, offset ), 0.0f );
        float4 sampleNeg = GlowSource.SampleLevel( samGlowSource, UV - float2( 0.0f, offset ), 0.0f );

        color += ( samplePos.rgb + sampleNeg.rgb ) * kVertWeightsRaw[i];
        alphaMax = max( alphaMax, max( samplePos.a, sampleNeg.a ) );
    }

    return float4( color * kWeightNorm, alphaMax );
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PS_BlurHorizontal( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    return SampleJitterHorizontal( UV, GlowParams1.x );
}

//==============================================================================

float4 PS_BlurVertical( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    return SampleJitterVertical( UV, GlowParams1.y );
}

//==============================================================================

float4 PS_Combine( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float  retention = saturate( GlowParams0.y );
    float2 currentUV = UV + GlowParams1.xy * 0.5f;
    float4 current   = GlowSource.SampleLevel( samGlowSource, currentUV, 0.0f );
    float4 history   = GlowAux.SampleLevel( samGlowAux, UV, 0.0f );

    float4 result;
    result.rgb = saturate( current.rgb * GlowParams0.z + history.rgb * retention );
    result.a   = saturate( max( current.a, history.a * retention ) );
    return result;
}

//==============================================================================

float4 PS_Composite( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 glow = GlowSource.SampleLevel( samGlowSource, UV, 0.0f );
    return glow;
}
