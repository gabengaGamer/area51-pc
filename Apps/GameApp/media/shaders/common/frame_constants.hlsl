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
    float4x4 View;            // World to view matrix
    float4x4 Projection;      // View to clip matrix

    float4   MaterialParams;  // x = flags, y = alpha ref, z = nearZ, w = farZ
    float4   UVAnim;          // xy = uv animation offsets
    float4   CameraPosition;  // xyz = camera position, w = 1
    float4   EnvParams;       // x = fixed alpha, y = cubemap intensity, zw = unused
};

#endif // FRAME_CONSTANTS_HLSL