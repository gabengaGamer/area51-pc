//==============================================================================
//
//  frame_constants.hlsl
//
//  View and camera parameters shared across geometry shaders.
//
//==============================================================================

#ifndef FRAME_CONSTANTS_HLSL
#define FRAME_CONSTANTS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbFrameConstants A51_CBUFFER_BIND(0, 0)
{
    float4x4 View;                   // World to view matrix
    float4x4 Projection;             // View to clip matrix

    uint     MaterialFlags;          // material and instance flags
    float    AlphaRef;               // alpha test reference
    float    NearZ;                  // view near plane
    float    FarZ;                   // view far plane
    float4   UVAnim;                 // xy = uv animation offsets, z = detail scale
    float4   CameraPosition;         // xyz = camera position, w = 1
    float4   EnvParams;              // x = fixed alpha, y = cubemap intensity, z = fade alpha, w = z-prime override
    float4x4 DistortionNormalMatrix; // World normal -> rotated view-space normal
    float4   DistortionParams;       // x = pixel offset scale, zw = inverse scene size
    float4   FogColor;               // rgb = fog color
    float4   FogCoeff;               // polynomial fog coefficients
    float4   FogParams;              // x = near, y = far, z = fog start, w = enabled
};

//==============================================================================
#endif // FRAME_CONSTANTS_HLSL
//==============================================================================
