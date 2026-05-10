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
    float4 LightVec [MAX_GEOM_LIGHTS];  // xyz = position or direction, w = range
    float4 LightCol [MAX_GEOM_LIGHTS];  // rgb = color, a = radial falloff
    float4 LightDir [MAX_GEOM_LIGHTS];  // xyz = spot direction, w = 1 for spot / 0 for omni
    float4 LightCone[MAX_GEOM_LIGHTS];  // x = cos(inner * 0.5), y = cos(outer * 0.5)
    float4 LightAmbCol;                 // rgb = ambient color
    uint   LightCount;
    float3 LightPadding;
};

#endif // LIGHTING_CONSTANTS_HLSL
