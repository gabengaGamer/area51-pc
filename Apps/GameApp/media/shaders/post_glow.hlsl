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

static const float kThresholdEpsilon = 1.0e-4f;
static const float kWeightNorm       = 1.0f / 255.0f;

Texture2D GlowSource  : register(t0);
Texture2D GlowHistory : register(t1);

SamplerState samPoint : register(s0);

//==============================================================================

float4 PS_Downsample( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float2 texel = GlowParams1.xy;
    float  cutoff = GlowParams0.x;

    float3 colorAccum = 0.0;
    float  maxAlpha = 0.0;

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

        // Hard cutoff: if alpha > cutoff, use it, else reject
        float mask = sample.a;
        float accepted = (mask > cutoff) ? 1.0f : 0.0f;
        
        // Just extract bright areas, boost will be applied at composite stage
        colorAccum += sample.rgb * accepted;
        maxAlpha = max( maxAlpha, mask * accepted );
    }

    float3 glowColor = colorAccum * 0.25f;
    float glowAlpha = saturate( maxAlpha );

    return float4( glowColor, glowAlpha );
}

//==============================================================================

float4 SampleBlurH( float2 UV, float stepX )
{
    // Horizontal jitter weights scaled by 1/255
    const float weights[5] =
    {
        24.0f * kWeightNorm,
        20.0f * kWeightNorm,
        16.0f * kWeightNorm,
        10.0f * kWeightNorm,
        8.0f  * kWeightNorm
    };

    float4 center = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    float3 color = center.rgb * weights[0];
    float  alphaMax = center.a;

    [unroll]
    for( int i = 1; i < 5; ++i )
    {
        float offset = stepX * (float)i;
        float4 samplePos = GlowSource.SampleLevel( samPoint, UV + float2( offset, 0.0f ), 0.0 );
        float4 sampleNeg = GlowSource.SampleLevel( samPoint, UV - float2( offset, 0.0f ), 0.0 );

        color += (samplePos.rgb + sampleNeg.rgb) * weights[i];
        alphaMax = max( alphaMax, max( samplePos.a, sampleNeg.a ) );
    }

    return float4( color, alphaMax );
}

//==============================================================================

float4 SampleBlurV( float2 UV, float stepY )
{
    // Vertical jitter weights scaled by 1/255
    const float weights[5] =
    {
        96.0f * kWeightNorm,
        40.0f * kWeightNorm,
        32.0f * kWeightNorm,
        20.0f * kWeightNorm,
        16.0f * kWeightNorm
    };

    float4 center = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    float3 color = center.rgb * weights[0];
    float  alphaMax = center.a;

    [unroll]
    for( int i = 1; i < 5; ++i )
    {
        float offset = stepY * (float)i;
        float4 samplePos = GlowSource.SampleLevel( samPoint, UV + float2( 0.0f, offset ), 0.0 );
        float4 sampleNeg = GlowSource.SampleLevel( samPoint, UV - float2( 0.0f, offset ), 0.0 );

        color += (samplePos.rgb + sampleNeg.rgb) * weights[i];
        alphaMax = max( alphaMax, max( samplePos.a, sampleNeg.a ) );
    }

    return float4( color, alphaMax );
}

//==============================================================================

float4 PS_BlurHorizontal( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    return SampleBlurH( UV, GlowParams1.x );
}

//==============================================================================

float4 PS_BlurVertical( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    return SampleBlurV( UV, GlowParams1.y );
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

    glow.rgb *= GlowParams0.y * GlowParams1.z;
    glow.a = saturate( glow.a );

    return glow;
}

//==============================================================================