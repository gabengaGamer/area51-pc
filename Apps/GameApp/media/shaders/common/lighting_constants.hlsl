//==============================================================================
//
//  lighting_constants.hlsl
//
//  Shared geometry lighting storage.
//
//==============================================================================

#ifndef LIGHTING_CONSTANTS_HLSL
#define LIGHTING_CONSTANTS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================
//  TYPES
//==============================================================================

struct GeomLightingData
{
    float4 LightVec [MAX_GEOM_LIGHTS];  // xyz = position or direction, w = range
    float4 LightCol [MAX_GEOM_LIGHTS];  // rgb = color, a = radial falloff
    float4 LightDir [MAX_GEOM_LIGHTS];  // xyz = spot direction, w = 1 for spot / 0 for omni
    float4 LightCone[MAX_GEOM_LIGHTS];  // x = cos(inner * 0.5), y = cos(outer * 0.5)
    float4 LightCookieU[MAX_GEOM_LIGHTS];
    float4 LightCookieV[MAX_GEOM_LIGHTS];
    float4 LightCookieAtlas[MAX_GEOM_LIGHTS];
    uint4  LightCookieLayer;
    float4 LightCookieMaxMip;
    uint4  LightShadowIndex;
    float4 LightAmbCol;                 // rgb = ambient color
    uint4  LightCountPadding;           // x = light count
};

#if defined(A51_SHADER_BINDING_SDL) && defined(A51_SHADER_STAGE_PIXEL)
    #ifndef A51_GEOM_LIGHTING_BINDING
        #define A51_GEOM_LIGHTING_BINDING 9
    #endif
#else
    #ifndef A51_GEOM_LIGHTING_BINDING
        #define A51_GEOM_LIGHTING_BINDING 2
    #endif
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

A51_STORAGE_BUFFER_ATTR(26, A51_GEOM_LIGHTING_BINDING)
StructuredBuffer<GeomLightingData> GeomLighting
    A51_STORAGE_BUFFER_BIND(26, A51_GEOM_LIGHTING_BINDING);

//==============================================================================
#endif // LIGHTING_CONSTANTS_HLSL
//==============================================================================
