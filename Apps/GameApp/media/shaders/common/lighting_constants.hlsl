//==============================================================================
//
//  lighting_constants.hlsl
//
//  Geometry lighting data shared between vertex and pixel shaders.
//
//==============================================================================

#ifndef LIGHTING_CONSTANTS_HLSL
#define LIGHTING_CONSTANTS_HLSL

cbuffer cbLightConsts : register(b3)
{
    float4 LightVec[MAX_GEOM_LIGHTS];   // position or direction, w = range
    float4 LightCol[MAX_GEOM_LIGHTS];   // rgb = color, a = intensity
    float4 LightAmbCol;                 // rgb = ambient color
    uint   LightCount;
    float3 LightPadding;
};

#endif // LIGHTING_CONSTANTS_HLSL