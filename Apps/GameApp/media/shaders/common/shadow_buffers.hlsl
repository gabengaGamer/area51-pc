//==============================================================================
//
//  shadow_buffers.hlsl
//
//  Shadow map matrices shared by geometry shaders.
//
//==============================================================================

#ifndef SHADOW_BUFFERS_HLSL
#define SHADOW_BUFFERS_HLSL

#define MAX_SHADOW_SOURCES 64
#define MAX_SHADOW_LIGHTS 8
#define POINT_SHADOW_FACE_COUNT 6

//==============================================================================

cbuffer cbShadowMaps : register(b5)
{
    float4x4 FaceShadowMatrix[MAX_SHADOW_SOURCES];
    float4   FaceShadowLightPosRadius[MAX_SHADOW_SOURCES];
    float4   FaceShadowLightDirFalloff[MAX_SHADOW_SOURCES];
    float4   FaceShadowLightData[MAX_SHADOW_SOURCES];        // x = cone/coverage cosine, y = receive near z, z = far z, w = 0 spot / 1 point face
    float4   PointShadowLightPosRadius[MAX_SHADOW_LIGHTS];
    float4   PointShadowLightData[MAX_SHADOW_LIGHTS];        // x = falloff, y = receive near z, z = far z
    float4   PointShadowLightParams[MAX_SHADOW_LIGHTS];      // x = first face source, y = face count
    uint     FaceShadowCount;
    uint     PointShadowLightCount;
    float2   ShadowPadding;
    float4   ShadowParams;                         // z = min variance, w = light bleed reduction
};

//==============================================================================
#endif // SHADOW_BUFFERS_HLSL
//==============================================================================
