//==============================================================================
//
//  a51_shadow_evsm.hlsl
//
//  EVSM shadow caster and blur shaders for PC shadows.
//
//==============================================================================

static const float kEVSMPositiveExponent = 5.0f;
static const float kEVSMNegativeExponent = 5.0f;

cbuffer cbShadowBlur : register(b0)
{
    float4 ShadowBlurParams;
};

Texture2D<float4> txShadowSource : register(t0);
SamplerState      samShadowSource : register(s0);

struct PS_CAST_INPUT
{
    float4 Position : SV_POSITION;
};

struct PS_BLUR_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

float WarpDepthPositive( float Depth )
{
    return exp( kEVSMPositiveExponent * Depth );
}

float WarpDepthNegative( float Depth )
{
    return -exp( -kEVSMNegativeExponent * Depth );
}

float4 ComputeEVSMMoments( float Depth )
{
    const float positive = WarpDepthPositive( Depth );
    const float negative = WarpDepthNegative( Depth );
    return float4( positive,
                   positive * positive,
                   negative,
                   negative * negative );
}

float4 PSCastMoments( PS_CAST_INPUT input ) : SV_TARGET
{
    // In the pixel shader SV_POSITION.z is already post-projective depth.
    // Dividing by SV_POSITION.w distorts the stored moments and destabilizes
    // spotlight shadows as geometry/light distance changes.
    const float depth = saturate( input.Position.z );
    return ComputeEVSMMoments( depth );
}

float4 SampleBlurredMoments( float2 uv, float2 stepUV )
{
    float4 moments = txShadowSource.Sample( samShadowSource, uv ) * 0.22702703f;

    moments += txShadowSource.Sample( samShadowSource, uv + stepUV * 1.0f ) * 0.19459459f;
    moments += txShadowSource.Sample( samShadowSource, uv - stepUV * 1.0f ) * 0.19459459f;
    moments += txShadowSource.Sample( samShadowSource, uv + stepUV * 2.0f ) * 0.12162162f;
    moments += txShadowSource.Sample( samShadowSource, uv - stepUV * 2.0f ) * 0.12162162f;
    moments += txShadowSource.Sample( samShadowSource, uv + stepUV * 3.0f ) * 0.05405405f;
    moments += txShadowSource.Sample( samShadowSource, uv - stepUV * 3.0f ) * 0.05405405f;
    moments += txShadowSource.Sample( samShadowSource, uv + stepUV * 4.0f ) * 0.01621622f;
    moments += txShadowSource.Sample( samShadowSource, uv - stepUV * 4.0f ) * 0.01621622f;

    return moments;
}

float4 PSBlurHorizontal( PS_BLUR_INPUT input ) : SV_TARGET
{
    return SampleBlurredMoments( input.UV, float2( ShadowBlurParams.x, 0.0f ) );
}

float4 PSBlurVertical( PS_BLUR_INPUT input ) : SV_TARGET
{
    return SampleBlurredMoments( input.UV, float2( 0.0f, ShadowBlurParams.y ) );
}
