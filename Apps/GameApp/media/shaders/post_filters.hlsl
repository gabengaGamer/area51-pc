//==============================================================================
//
//  post_filters.hlsl
//
//  Filter shaders for the DX11 post pipeline.
//
//==============================================================================

Texture2D PostSource    : register(t0);
Texture2D FilterSource1 : register(t1);
Texture2D FilterRamp    : register(t2);

SamplerState samLinear : register(s0);

cbuffer FilterParams : register(b4)
{
    float4 FilterParams0; // x = motion intensity, y = zoom, z = sin(angle), w = cos(angle)
    float4 FilterParams1; // x = alpha sub / 255, y = alpha scale / 255
    float4 FilterParams2; // x = warp count, y = 1 / width, z = 1 / height
    float4 FilterParams3; // x/y/z = color scale, w = alpha/intensity scale
    float4 FilterParams4; // x = falloff fn, y = param1, z = param2, w = mip offset
    float4 FilterParams5; // x = near z, y = far z, z = 1 / width, w = 1 / height
    float4 FilterParams6; // x = aux0, y = aux1, z = aux2, w = aux3
    float4 ScreenWarps[8]; // xy = center in pixels, z = radius in pixels, w = warp amount
};

//==============================================================================

float4 PS_MotionBlur( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 color = PostSource.SampleLevel( samLinear, UV, 0.0f );
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

    float4 color = PostSource.SampleLevel( samLinear, sampleUV, 0.0f );
    float  dist  = length( delta ) * 2.0f;
    color.a = saturate( dist * FilterParams1.y - FilterParams1.x );
    return color;
}

//==============================================================================

float4 PS_ScreenWarp( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 color = FilterSource1.SampleLevel( samLinear, UV, 0.0f );

    [unroll]
    for( int i = 0; i < 8; ++i )
    {
        if( i >= (int)FilterParams2.x )
            break;

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
            color = PostSource.SampleLevel( samLinear, sampleUV, 0.0f );
        }
    }

    color.a = 1.0f;
    return color;
}

//==============================================================================

float ComputeMipVisibility( float ViewZ, float NearZ, float FarZ )
{
    const int   fn        = (int)(FilterParams4.x + 0.5f);
    const float param1    = FilterParams4.y;
    const float param2    = FilterParams4.z;
    const float baseAlpha = saturate( FilterParams3.w );
    const float range     = max( FarZ - NearZ, 1e-5f );
    float visibility      = 1.0f;

    switch( fn )
    {
        case 0:
            visibility = 1.0f - baseAlpha;
            break;

        case 1:
        {
            const float clampedZ = clamp( ViewZ, param1, param2 );
            visibility = (param2 - clampedZ) / max( param2 - param1, 1e-5f );
            break;
        }

        case 2:
        {
            const float d = (ViewZ - NearZ) / range;
            visibility = 1.0f / exp( d * param1 );
            break;
        }

        case 3:
        {
            float d = (ViewZ - NearZ) / range;
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
    float3 color = PostSource.SampleLevel( samLinear, UV, 0.0f ).rgb;
    if( abs( offset ) < 0.001f )
        return color;

    const float2 jitter = float2( offset * FilterParams5.z, offset * FilterParams5.w );

    color = 0.0f;
    color += PostSource.SampleLevel( samLinear, UV + float2( -jitter.x, -jitter.y ), 0.0f ).rgb;
    color += PostSource.SampleLevel( samLinear, UV + float2(  jitter.x, -jitter.y ), 0.0f ).rgb;
    color += PostSource.SampleLevel( samLinear, UV + float2(  jitter.x,  jitter.y ), 0.0f ).rgb;
    color += PostSource.SampleLevel( samLinear, UV + float2( -jitter.x,  jitter.y ), 0.0f ).rgb;
    return color * 0.25f;
}

//==============================================================================

float4 PS_MipComposite( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float3 blurred = SampleMipSource( UV );
    const float  linearDepth = saturate( FilterSource1.Load( int3( int2( Pos.xy ), 0 ) ).r );
    const int    fn = (int)(FilterParams4.x + 0.5f);

    if( (fn != 0) && (linearDepth >= 0.999f) )
        return 0.0f.xxxx;

    if( FilterParams6.x > 0.5f )
    {
        const float4 ramp = FilterRamp.SampleLevel( samLinear, float2( linearDepth, 0.5f ), 0.0f );
        return float4( blurred * ramp.rgb, saturate( ramp.a ) );
    }

    const float nearZ      = FilterParams5.x;
    const float farZ       = FilterParams5.y;
    const float viewZ      = lerp( nearZ, farZ, linearDepth );
    const float visibility = saturate( ComputeMipVisibility( viewZ, nearZ, farZ ) );
    const float alpha      = saturate( 1.0f - visibility );

    return float4( blurred * FilterParams3.rgb, alpha );
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
