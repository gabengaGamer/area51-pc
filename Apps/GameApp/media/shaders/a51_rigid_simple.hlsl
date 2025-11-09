//==============================================================================
//
//  a51_rigid_simple.hlsl
//
//  Simple rigidgeom uber-shader for A51.
//
//==============================================================================

#include "common/material_flags.hlsl"

//==============================================================================
//  GEOMETRY RESOURCES
//==============================================================================

#define GEOM_INPUT_TYPE GEOM_INPUT_RIGID
#include "common/vertexinput_geom.hlsl"

#define GEOM_INCLUDE_OBJECT_BUFFER
#include "common/geom_buffers.hlsl"

#define GEOM_HAS_VERTEX_COLOR 1
#include "common/pixelinput_geom.hlsl"
#include "common/pixel_structs.hlsl"
#include "common/geom_textures.hlsl"
#include "common/geom_pixel_shared.hlsl"

//==============================================================================
//  VERTEX SHADER
//==============================================================================

GEOM_PIXEL_INPUT VSMain(VS_INPUT input)
{
    GEOM_PIXEL_INPUT output;

    // Transform position through matrices
    float4 worldPos = mul(World, float4(input.Position, 1.0));
    float4 viewPos  = mul(View, worldPos);
    output.Pos      = mul(Projection, viewPos);

    // Pass through vertex attributes
    float3 worldNormal = normalize(mul((float3x3)World, input.Normal));
    output.WorldPos    = worldPos.xyz;
    output.Normal      = worldNormal;

    float3 viewVector = worldPos.xyz - CameraPosition.xyz;
    output.ViewVector = viewVector;

    float2 depthParams = MaterialParams.zw;
    float nearZ = depthParams.x;
    float farZ  = depthParams.y;
    float invRange = rcp(max(farZ - nearZ, 1e-5f));
    float linearDepth = (viewPos.z - nearZ) * invRange;
    output.LinearDepth = saturate(linearDepth);
    float2 uvAnimOffset = UVAnim.xy;
    output.UV          = input.UV + uvAnimOffset;
    output.Color       = input.Color;

    return output;
}

//==============================================================================
//  PIXEL SHADER
//==============================================================================

GEOM_PIXEL_OUTPUT PSMain( GEOM_PIXEL_INPUT input )
{
    return ShadeGeometryPixel( input );
}