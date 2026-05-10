//==============================================================================
//
//  post_glow.hlsl
//
//  Post-processing shaders for glow extraction and filtering.
//  Xbox-style glow: 9-tap separable blur, single-res accumulation,
//  artifact wipe, and alpha-masked composite.
//
//==============================================================================

cbuffer GlowParams : register(b4)
{
    float4 GlowParams0;   // x=cutoff, y=intensity scale, z=motion blend, w=unused
    float4 GlowParams1;   // x=step.x, y=step.y, z=composite weight, w=unused
};

static const float kWeightNorm = 1.0f / 255.0f;

Texture2D GlowSource  : register(t0);
Texture2D GlowAux     : register(t1);

SamplerState samPoint : register(s0);

//==============================================================================
//  Xbox-style 9-tap separable blur weights
//==============================================================================

static const float kHorzWeightsRaw[5] = { 24.0f, 20.0f, 16.0f, 10.0f, 8.0f };
static const float kVertWeightsRaw[5] = { 96.0f, 40.0f, 32.0f, 20.0f, 16.0f };

float4 PS_Downsample( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float2 texel = GlowParams1.xy;

    float3 colorAccum = 0.0;
    float  maxAlpha   = 0.0;

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
        float4 sample   = GlowSource.SampleLevel( samPoint, sampleUV, 0.0 );

        colorAccum += sample.rgb;
        maxAlpha    = max( maxAlpha, sample.a );
    }

    return float4( colorAccum * 0.25f, maxAlpha );
}

//==============================================================================

float4 SampleBlurH( float2 UV, float stepX )
{
    float4 center   = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    float3 color    = center.rgb * kHorzWeightsRaw[0];
    float  alphaMax = center.a;

    [unroll]
    for( int i = 1; i < 5; ++i )
    {
        float offset = stepX * (float)i;
        float4 samplePos = GlowSource.SampleLevel( samPoint, UV + float2( offset, 0.0f ), 0.0 );
        float4 sampleNeg = GlowSource.SampleLevel( samPoint, UV - float2( offset, 0.0f ), 0.0 );

        color += (samplePos.rgb + sampleNeg.rgb) * kHorzWeightsRaw[i];
        alphaMax = max( alphaMax, max( samplePos.a, sampleNeg.a ) );
    }

    return float4( color * kWeightNorm, alphaMax );
}

//==============================================================================

float4 SampleBlurV( float2 UV, float stepY )
{
    float4 center   = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    float3 color    = center.rgb * kVertWeightsRaw[0];
    float  alphaMax = center.a;

    [unroll]
    for( int i = 1; i < 5; ++i )
    {
        float offset = stepY * (float)i;
        float4 samplePos = GlowSource.SampleLevel( samPoint, UV + float2( 0.0f, offset ), 0.0 );
        float4 sampleNeg = GlowSource.SampleLevel( samPoint, UV - float2( 0.0f, offset ), 0.0 );

        color += (samplePos.rgb + sampleNeg.rgb) * kVertWeightsRaw[i];
        alphaMax = max( alphaMax, max( samplePos.a, sampleNeg.a ) );
    }

    return float4( color * kWeightNorm, alphaMax );
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
//  Accumulate + Xbox-style streak & artifact wipe
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
//  Motion-blur blend for glow (kept for compatibility, unused in Xbox-style path)
//==============================================================================

float4 PS_Combine( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float  blend   = saturate( GlowParams0.z );
    float4 current = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    float4 history = GlowAux.SampleLevel( samPoint, UV, 0.0 );

    float4 result;
    result.rgb = lerp( current.rgb, history.rgb, blend );
    result.a   = lerp( current.a,   history.a,   blend );
    return result;
}

//==============================================================================
//  Final composite with Xbox-style glow cutoff
//==============================================================================

float4 PS_Composite( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 glow      = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    float  frameMask = GlowAux.SampleLevel( samPoint, UV, 0.0 ).a;
    float  cutoff    = GlowParams0.x;
    // Old smooth gate:
    //float  gate      = max( glow.a, frameMask );
    //float  apply     = saturate( (gate - cutoff) * 2.0f );

    // DX9 PS0009-style gate: branch on frame alpha cutoff, then x2 the glow contribution.
    float  gate      = frameMask - cutoff;
    float  apply     = (gate >= 0.0f) ? 1.0f : 0.0f;

    glow.rgb *= GlowParams1.z * apply * 2.0f;
    glow.a    = apply;

    return glow;
}

//==============================================================================
