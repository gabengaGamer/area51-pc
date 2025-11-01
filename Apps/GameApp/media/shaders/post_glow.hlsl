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

float4 SampleBlur( float2 UV, float2 Step )
{
    // Standard 5-tap Gaussian blur
    const float weights[5] =
    {
        0.227027f,  // center (stronger to preserve brightness)
        0.194595f,
        0.121622f,
        0.054054f,
        0.016216f
    };

    float4 center = GlowSource.SampleLevel( samPoint, UV, 0.0 );
    float3 color = center.rgb * weights[0];
    float  alphaMax = center.a;

    [unroll]
    for( int i = 1; i < 5; ++i )
    {
        float2 offset = Step * (float)i;
        float4 samplePos = GlowSource.SampleLevel( samPoint, UV + offset, 0.0 );
        float4 sampleNeg = GlowSource.SampleLevel( samPoint, UV - offset, 0.0 );
        
        color += (samplePos.rgb + sampleNeg.rgb) * weights[i];
        alphaMax = max( alphaMax, max( samplePos.a, sampleNeg.a ) );
    }

    return float4( color, alphaMax );
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
    
    // Apply x2 boost here to match cnd_x2 behavior
    // GlowParams0.y is the intensity scale (kGlowIntensityScale)
    glow.rgb *= GlowParams0.y * 2.0f;
    glow.a = saturate( glow.a );
    
    return glow;
}

//==============================================================================