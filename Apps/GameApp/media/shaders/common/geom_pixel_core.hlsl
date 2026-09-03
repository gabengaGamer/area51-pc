//==============================================================================
//
//  geom_pixel_core.hlsl
//
//  Shared geometry pixel structs and depth helpers.
//
//==============================================================================

#ifndef GEOM_PIXEL_CORE_HLSL
#define GEOM_PIXEL_CORE_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/fog_functions.hlsl"

// GS: TEMP: Disable only alpha-driven illumination.
#ifndef A51_DISABLE_ALPHA_ILLUMINATION
    #define A51_DISABLE_ALPHA_ILLUMINATION 0
#endif

//==============================================================================
//  TYPES
//==============================================================================

struct GeomDiffuseResult
{
    float4 Color;
    float4 Sample;
    float  Alpha;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

static float4 GeomEncodeLinearDepth( GEOM_PIXEL_INPUT input )
{
    const float4 viewPos     = mul( View, float4( input.WorldPos, 1.0f ) );
    const float  invRange    = rcp( max( FarZ - NearZ, 1e-5f ) );
    const float  linearDepth = saturate( (viewPos.z - NearZ) * invRange );
    return linearDepth.xxxx;
}

//==============================================================================

static float4 GeomApplyForwardFog( float4 color, GEOM_PIXEL_INPUT input )
{
    if( FogParams.w <= 0.0f )
    {
        return color;
    }

    const float fogAlpha = A51ComputePolynomialFogAlpha( GeomEncodeLinearDepth( input ).r,
                                                         FogParams.x,
                                                         FogParams.y,
                                                         FogParams.z,
                                                         FogCoeff );
    color.rgb = lerp( color.rgb, FogColor.rgb, fogAlpha );
    return color;
}

//==============================================================================
#endif // GEOM_PIXEL_CORE_HLSL
//==============================================================================
