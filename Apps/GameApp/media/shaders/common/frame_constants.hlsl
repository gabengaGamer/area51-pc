//==============================================================================
//
//  frame_constants.hlsl
//
//  View and camera parameters shared across geometry shaders.
//
//==============================================================================

#ifndef FRAME_CONSTANTS_HLSL
#define FRAME_CONSTANTS_HLSL

cbuffer cbFrameConstants : register(b0)
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
};

//==============================================================================
#endif // FRAME_CONSTANTS_HLSL
//==============================================================================
