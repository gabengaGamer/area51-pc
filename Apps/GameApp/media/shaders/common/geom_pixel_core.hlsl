//==============================================================================
//
//  geom_pixel_core.hlsl
//
//  Shared geometry pixel structs and depth helpers.
//
//==============================================================================

#ifndef GEOM_PIXEL_CORE_HLSL
#define GEOM_PIXEL_CORE_HLSL

struct GeomDiffuseResult
{
    float4 Color;
    float4 Sample;
    float  Alpha;
};

//==============================================================================

static float4 GeomEncodeLinearDepth( GEOM_PIXEL_INPUT input )
{
    const float4 viewPos     = mul( View, float4( input.WorldPos, 1.0f ) );
    const float  invRange    = rcp( max( FarZ - NearZ, 1e-5f ) );
    const float  linearDepth = saturate( (viewPos.z - NearZ) * invRange );
    return linearDepth.xxxx;
}

//==============================================================================
#endif // GEOM_PIXEL_CORE_HLSL
//==============================================================================
