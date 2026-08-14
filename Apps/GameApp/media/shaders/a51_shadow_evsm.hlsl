//==============================================================================
//
//  a51_shadow_evsm.hlsl
//
//  EVSM4 depth-to-moment conversion.
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
//  SHADERS
//==============================================================================

VS_OUTPUT VSMain( uint vertexID : SV_VertexID )
{
    VS_OUTPUT output;

    const float2 uv = float2( ( vertexID << 1 ) & 2, vertexID & 2 );
    output.Position = float4( uv * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f ), 0.5f, 1.0f );
    output.UV = uv;
    return output;
}

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D<float> txShadowDepth A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samShadowDepth A51_SAMPLER_BIND(0, 0);

A51_CBUFFER_ATTR(1, 0) cbuffer cbShadowFilter A51_CBUFFER_BIND(1, 0)
{
    float4 TextureParams;          // xy = input texel size, zw = output texel size
    float4 SourceClampRect;        // xy = minimum source texel center, zw = maximum source texel center
    float4 FilterParams;           // x = positive exponent, y = negative exponent, z = blur scale
    float4 DepthParams;            // x/y = projection near/far, z = light radius, w = point-face flag
    float4 SourceProjectionParams; // xy = source UV center, z = UV-to-NDC scale, w = corner coverage cosine
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

float LinearizeShadowDepth( float depth )
{
    const float nearZ = max( DepthParams.x, 1.0e-4f );
    const float farZ = max( DepthParams.y, nearZ + 1.0e-4f );
    return ( nearZ * farZ ) / max( farZ - depth * ( farZ - nearZ ), 1.0e-4f );
}

//==============================================================================

float ComputePointShadowRadialDepth( float depth, float2 sourceUV )
{
    const float2 faceNDC = ( sourceUV - SourceProjectionParams.xy ) * SourceProjectionParams.z;
    const float cornerCos = max( SourceProjectionParams.w, 1.0e-4f );
    const float tanHalfFov = sqrt( max( ( rcp( cornerCos * cornerCos ) - 1.0f ) * 0.5f, 0.0f ) );
    const float2 facePlane = faceNDC * tanHalfFov;
    const float radialDistance = LinearizeShadowDepth( depth ) * sqrt( 1.0f + dot( facePlane, facePlane ) );
    return saturate( radialDistance / max( DepthParams.z, 1.0e-4f ) );
}

//==============================================================================

float4 ComputeEVSM4Moments( float depth, float2 sourceUV )
{
    const float evsmDepth =
        ( DepthParams.w > 0.5f ) ? ComputePointShadowRadialDepth( depth, sourceUV ) : depth;
    const float warpedDepth = evsmDepth * 2.0f - 1.0f;
    const float positive = exp( FilterParams.x * warpedDepth );
    const float negative = -exp( -FilterParams.y * warpedDepth );
    return float4( positive, positive * positive, negative, negative * negative );
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PSConvert( VS_OUTPUT input ) : SV_Target
{
    const float2 outputUV = input.Position.xy * TextureParams.zw;
    const float2 sampleOffset = TextureParams.zw * 0.25f;
    static const float2 kSampleDirections[4] =
    {
        float2( -1.0f, -1.0f ),
        float2(  1.0f, -1.0f ),
        float2( -1.0f,  1.0f ),
        float2(  1.0f,  1.0f )
    };

    float4 moments = 0.0f;
    [unroll]
    for( uint i = 0u; i < 4u; ++i )
    {
        const float2 sourceUV =
            clamp( outputUV + kSampleDirections[i] * sampleOffset, SourceClampRect.xy, SourceClampRect.zw );
        const float depth = txShadowDepth.SampleLevel( samShadowDepth, sourceUV, 0.0f );
        moments += ComputeEVSM4Moments( depth, sourceUV );
    }

    return moments * 0.25f;
}