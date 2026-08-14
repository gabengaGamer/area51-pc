//==============================================================================
//
//  a51_post_filters.hlsl
//
//  Post-processing filter shaders.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D PostSource A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D FilterSource1 A51_SAMPLED_TEXTURE_BIND(1, 1);
A51_SAMPLED_TEXTURE_ATTR(2, 2) Texture2D FilterRamp A51_SAMPLED_TEXTURE_BIND(2, 2);

A51_SAMPLER_ATTR(0, 0) SamplerState samPostSource A51_SAMPLER_BIND(0, 0);
#if defined(A51_SHADER_BINDING_SDL)
    A51_SAMPLER_ATTR(1, 1) SamplerState samFilterSource1 A51_SAMPLER_BIND(1, 1);
    A51_SAMPLER_ATTR(2, 2) SamplerState samFilterRamp A51_SAMPLER_BIND(2, 2);
#else
    #define samFilterSource1 samPostSource
    #define samFilterRamp    samPostSource
#endif

//------------------------------------------------------------------------------

A51_CBUFFER_ATTR(4, 0) cbuffer FilterParams A51_CBUFFER_BIND(4, 0)
{
    float4 FilterParams0;  // x = motion intensity, y = zoom, z = sin(angle), w = cos(angle)
    float4 FilterParams1;  // x = alpha sub / 255, y = alpha scale / 255
    float4 FilterParams2;  // x = warp count, y = 1 / width, z = 1 / height
    float4 FilterParams3;  // x/y/z = color scale, w = alpha/intensity scale
    float4 FilterParams4;  // x = falloff fn, y = param1, z = param2, w = mip offset
    float4 FilterParams5;  // x = near z, y = far z, z = 1 / width, w = 1 / height
    float4 FilterParams6;  // x = aux0, y = aux1, z = aux2, w = aux3
    float4 ScreenWarps[8]; // xy = center in pixels, z = radius in pixels, w = warp amount
};

//==============================================================================
//  SHADERS
//==============================================================================

float4 PS_MotionBlur( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 color = PostSource.SampleLevel( samPostSource, UV, 0.0f );
    color.a = saturate( FilterParams0.x );
    return color;
}

//==============================================================================

float4 PS_RadialBlur( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float2 center = FilterParams6.xy;
    const float2 delta  = UV - center;
    const float2 scaled = delta * FilterParams0.y;
    const float2 rotated = float2( scaled.x * FilterParams0.w - scaled.y * FilterParams0.z,
                                   scaled.x * FilterParams0.z + scaled.y * FilterParams0.w );
    const float2 sampleUV = center + rotated;

    float4 color = PostSource.SampleLevel( samPostSource, sampleUV, 0.0f );
    float  dist  = length( delta ) * 2.0f;
    color.a = saturate( dist * FilterParams1.y - FilterParams1.x );
    return color;
}

//==============================================================================

float4 PS_ScreenWarp( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 color = FilterSource1.SampleLevel( samFilterSource1, UV, 0.0f );

    [fastopt]
    [loop]
    for( int i = 0; i < 8; ++i )
    {
        if( i >= (int)FilterParams2.x )
        {
            break;
        }

        float2 delta = Pos.xy - ScreenWarps[i].xy;
        float  dist  = length( delta );
        float  radius = ScreenWarps[i].z;

        if( (radius > 0.0f) && (dist < radius) )
        {
            float2 dir = (dist > 0.0001f) ? (delta / dist) : float2( 0.0f, 0.0f );
            float  t   = saturate( dist / radius );
            float  warpedRadius = pow( t, max( ScreenWarps[i].w, 0.001f ) ) * radius;
            float2 samplePixel  = ScreenWarps[i].xy + dir * warpedRadius;
            float2 sampleUV     = samplePixel * FilterParams2.yz;
            color = PostSource.SampleLevel( samPostSource, sampleUV, 0.0f );
        }
    }

    color.a = 1.0f;
    return color;
}

//==============================================================================

float4 PS_MipDownsample( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float weights[4] = { 0.125f, 0.375f, 0.375f, 0.125f };
    const float offsets[4] = { -1.5f, -0.5f, 0.5f, 1.5f };
    float4 result = 0.0f;

    [unroll]
    for( int y = 0; y < 4; ++y )
    {
        [unroll]
        for( int x = 0; x < 4; ++x )
        {
            const float2 sampleUV = UV + float2( offsets[x] * FilterParams5.z,
                                                  offsets[y] * FilterParams5.w );
            result += PostSource.SampleLevel( samPostSource, sampleUV, 0.0f ) * weights[x] * weights[y];
        }
    }

    return result;
}

//==============================================================================

float4 PS_PainBlur( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float2 stepUV = FilterParams5.zw;
    float3 result = 0.0f;
    float totalWeight = 0.0f;

    [unroll]
    for( int tap = -8; tap <= 8; ++tap )
    {
        const float tapPosition = (float)tap;
        const float weight = exp( -0.5f * tapPosition * tapPosition / 16.0f );
        result += PostSource.SampleLevel( samPostSource, UV + stepUV * tapPosition, 0.0f ).rgb * weight;
        totalWeight += weight;
    }
    result /= totalWeight;

    if( FilterParams6.x > 0.5f )
    {
        const float blend = saturate( FilterParams3.w );
        const float tintWeight = pow( 1.0f - blend, 4.0f );
        result *= lerp( 1.0f.xxx, FilterParams3.rgb, tintWeight );
    }

    return float4( result, 1.0f );
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

float ComputeMipVisibility( float ViewZ, float NearZ, float FarZ )
{
    const int   fn        = (int)(FilterParams4.x + 0.5f);
    const float param1    = FilterParams4.y;
    const float param2    = FilterParams4.z;
    const float range     = max( FarZ - NearZ, 1e-5f );
    float visibility      = 1.0f;

    switch( fn )
    {
        case 1:
        {
            const float clampedZ = clamp( ViewZ, param1, param2 );
            visibility = ( param2 - clampedZ ) / max( param2 - param1, 1e-5f );
            break;
        }

        case 2:
        {
            const float d = ( ViewZ - NearZ ) / range;
            visibility = 1.0f / exp( d * param1 );
            break;
        }

        case 3:
        {
            float d = ( ViewZ - NearZ ) / range;
            d *= param1;
            visibility = 1.0f / exp( d * d );
            break;
        }
    }

    return visibility;
}

//==============================================================================

float3 SampleMipSource( float2 UV )
{
    const float offset = FilterParams4.w;
    float3 color = PostSource.SampleLevel( samPostSource, UV, 0.0f ).rgb;
    if( abs( offset ) < 0.001f )
    {
        return color;
    }

    const float2 jitter = float2( offset * FilterParams5.z, offset * FilterParams5.w );

    color = 0.0f;
    color += PostSource.SampleLevel( samPostSource, UV + float2( -jitter.x, -jitter.y ), 0.0f ).rgb;
    color += PostSource.SampleLevel( samPostSource, UV + float2(  jitter.x, -jitter.y ), 0.0f ).rgb;
    color += PostSource.SampleLevel( samPostSource, UV + float2(  jitter.x,  jitter.y ), 0.0f ).rgb;
    color += PostSource.SampleLevel( samPostSource, UV + float2( -jitter.x,  jitter.y ), 0.0f ).rgb;
    return color * 0.25f;
}

//==============================================================================

float SampleMipDepth( float4 Pos, float2 UV )
{
#if defined(A51_SHADER_BINDING_SDL)
    return saturate( FilterSource1.SampleLevel( samFilterSource1, UV, 0.0f ).a );
#else
    return saturate( FilterSource1.Load( int3( int2( Pos.xy ), 0 ) ).a );
#endif
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PS_MipComposite( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float3 blurred     = SampleMipSource( UV );
    const float  linearDepth = SampleMipDepth( Pos, UV );
    const int    fn = (int)(FilterParams4.x + 0.5f);

    if( ( fn != 0 ) && ( linearDepth >= 0.999f ) )
    {
        return 0.0f.xxxx;
    }

    const float nearZ      = FilterParams5.x;
    const float farZ       = FilterParams5.y;
    const float viewZ      = lerp( nearZ, farZ, linearDepth );
    const float visibility = saturate( ComputeMipVisibility( viewZ, nearZ, farZ ) );
    const float paletteAlpha = (128.0f - floor( visibility * 128.0f )) / 255.0f;
    const float passCount    = max( floor( FilterParams6.x + 0.5f ), 1.0f );
    const float alpha        = 1.0f - pow( 1.0f - paletteAlpha, passCount );

    return float4( blurred * FilterParams3.rgb, alpha );
}

//==============================================================================

float4 PS_MipCompositeCustom( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float3 blurred     = SampleMipSource( UV );
    const float  linearDepth = SampleMipDepth( Pos, UV );

    if( linearDepth >= 0.999f )
    {
        return 0.0f.xxxx;
    }

    const float4 ramp = FilterRamp.SampleLevel( samFilterRamp,
                                                 float2( linearDepth, 0.5f ),
                                                 0.0f );
    return float4( blurred * ramp.rgb, saturate( ramp.a ) );
}

//==============================================================================

uint PCGHash( uint2 Pixel )
{
    uint state = Pixel.x * 1664525u + Pixel.y * 1013904223u;
    state ^= (state >> 17u);
    state *= 0xbf324c81u;
    state ^= (state >> 11u);
    state *= 0x9f324c81u;
    state ^= (state >> 16u);
    return state;
}

//==============================================================================

float4 PS_Noise( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const uint2 screenPixel = (uint2)Pos.xy + uint2( FilterParams6.xy );
    const float noise       = (float)(PCGHash( screenPixel ) & 255u) / 255.0f;
    const float intensity  = saturate( FilterParams3.w );
    const float alpha      = saturate( (1.0f - noise) * intensity );
    const float3 grainColor = saturate( FilterParams3.xyz );

    return float4( grainColor, alpha );
}

//==============================================================================

float4 PS_ScreenFade( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    return FilterParams3;
}
