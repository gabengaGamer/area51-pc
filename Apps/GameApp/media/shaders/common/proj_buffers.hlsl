//==============================================================================
//
//  proj_buffers.hlsl
//
//  Projection texture matrices shared by geometry shaders.
//
//==============================================================================

#ifndef PROJ_BUFFERS_HLSL
#define PROJ_BUFFERS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(4, 1) cbuffer cbProjTextures A51_CBUFFER_BIND(4, 1)
{
    float4x4 ProjLightMatrix [MAX_PROJ_LIGHTS];
    float4x4 ProjShadowMatrix[MAX_PROJ_SHADOWS];
    float4   ProjLightAtlas  [MAX_PROJ_LIGHTS];
    float4   ProjShadowAtlas [MAX_PROJ_SHADOWS];
    float4   ProjLightInfo   [MAX_PROJ_LIGHTS];
    float4   ProjShadowInfo  [MAX_PROJ_SHADOWS];
    uint     ProjLightCount;
    uint     ProjShadowCount;
    uint2    ProjPadding;
};

//==============================================================================
#endif // PROJ_BUFFERS_HLSL
//==============================================================================
