//==============================================================================
//
//  post_glow.hlsl
//
//  Post-processing shaders for glow extraction and filtering.
//
//==============================================================================

cbuffer GlowParams : register(b4)
{
    float4 GlowParams0;   // x=cutoff, y=intensity scale, z=motion blend, w=unused
    float4 GlowParams1;   // x=step.x, y=step.y, z=unused, w=unused
};

static const float kGlowSoftKnee       = 1.0f;
static const float kThresholdEpsilon   = 1.0e-4f;

Texture2D GlowSource  : register(t0);
Texture2D GlowHistory : register(t1);

SamplerState samPoint : register(s0);

//==============================================================================

float4 PS_Downsample( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float2 texel = GlowParams1.xy;
    float  cutoff = GlowParams0.x;
    float  softKnee = kGlowSoftKnee;
    float  invRange = (cutoff < 1.0f) ? rcp( max( 1.0f - cutoff, kThresholdEpsilon ) ) : 0.0f;

    float3 colorAccum = 0.0;
    float  maxMask = 0.0;

    const float2 offsets[4] =
    {
        float2(-0.5, -0.5),
        float2( 0.5, -0.5),
        float2(-0.5,  0.5),
        float2( 0.5,  0.5)
    };

    [unroll]
    for( int i = 0; i < 4; ++i )
    {
        float2 sampleUV = UV + offsets[i] * texel;
        float4 sample = GlowSource.SampleLevel( samPoint, sampleUV, 0.0 );

        float  mask             = sample.a;
        float  knee             = max( cutoff * softKnee, 1e-4f );
        float  diff             = mask - cutoff;
        float  soft             = saturate( (diff + knee) / (2.0f * knee) );
        float  hard             = diff > 0.0f ? 1.0f : 0.0f;
        float  blend            = lerp( hard, soft, softKnee );
        float  intensity        = saturate( diff * invRange ) * blend;

        colorAccum += sample.rgb * intensity;
        maxMask = max( maxMask, intensity );
    }

    float glowAlpha = saturate( maxMask );
    float3 glowColor = colorAccum * 0.25f;

    return float4( glowColor, glowAlpha );
}

//==============================================================================

float4 SampleBlur( float2 UV, float2 Step )
{
    const float weights[5] =
    {
        0.181818182f,
        0.151515152f,
        0.121212121f,
        0.075757578f,
        0.060606062f
    };

    float4 center = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    float3 color = center.rgb * weights[0];
    float  alphaAccum = center.a * weights[0];
    float  alphaMax   = center.a;

    [unroll]
    for( int i = 1; i < 5; ++i )
    {
        float2 offset = Step * (float)i;
        float4 samplePos = GlowSource.SampleLevel( samPoint, UV + offset, 0.0 );
        float4 sampleNeg = GlowSource.SampleLevel( samPoint, UV - offset, 0.0 );
        color      += (samplePos.rgb + sampleNeg.rgb) * weights[i];
        alphaAccum += (samplePos.a   + sampleNeg.a)   * weights[i];
        alphaMax    = max( alphaMax, max( samplePos.a, sampleNeg.a ) );
    }

    float alpha = saturate( max( alphaAccum, alphaMax ) );
    return float4( color, alpha );
}

//==============================================================================

float4 PS_BlurHorizontal( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float2 stepDir = float2( GlowParams1.x, 0.0f );
    return SampleBlur( UV, stepDir );
}

//==============================================================================

float4 PS_BlurVertical( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float2 stepDir = float2( 0.0f, GlowParams1.y );
    return SampleBlur( UV, stepDir );
}

//==============================================================================

float4 PS_Accumulate( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float  weight = GlowParams0.y;
    float4 glow   = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    glow.rgb *= weight;
    glow.a   *= weight;
    return glow;
}

//==============================================================================

float4 PS_Combine( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float  blend  = saturate( GlowParams0.z );
    float4 current = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    float4 history = GlowHistory.SampleLevel( samPoint, UV, 0.0 );

    float4 result;
    result.rgb = lerp( current.rgb, history.rgb, blend );
    result.a   = lerp( current.a,   history.a,   blend );
    return result;
}

//==============================================================================

float4 PS_Composite( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 glow = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    glow.rgb *= GlowParams0.y;
    glow.a    = saturate( glow.a );
    return glow;
}

//==============================================================================