//==============================================================================
//  
//  a51_skin_simple.hlsl
//  
//  Simple skindgeom uber-shader for A51.
//
//==============================================================================

#include "common/material_flags.hlsl"

//==============================================================================
//  GEOMETRY RESOURCES
//==============================================================================

#define GEOM_INPUT_TYPE GEOM_INPUT_SKIN
#include "common/vertexinput_geom.hlsl"

#include "common/geom_buffers.hlsl"
#include "common/skin_bones.hlsl"

#include "common/pixelinput_geom.hlsl"
#include "common/pixel_structs.hlsl"

#define GEOM_USE_SKIN_LIGHTING
#include "common/geom_textures.hlsl"
#include "common/geom_pixel_shared.hlsl"

//==============================================================================
//  VERTEX SHADER
//==============================================================================

GEOM_PIXEL_INPUT VSMain(VS_INPUT input)
{
    GEOM_PIXEL_INPUT output;

    // Extract bone indices and weights (stub)
    //int   index1  = 0;
    //int   index2  = 0;
    //float weight1 = 1.0;
    //float weight2 = 0.0;

    // Extract bone indices and weights
    int   index1  = (int)input.PosIndex.w;
    int   index2  = (int)input.NormIndex.w;
    float weight1 = input.UVWeights.z;
    float weight2 = input.UVWeights.w;

    // Blend positions using bone matrices
    float3 pos1 = mul(Bones[index1].L2W, float4(input.PosIndex.xyz, 1.0)).xyz;
    float3 pos2 = mul(Bones[index2].L2W, float4(input.PosIndex.xyz, 1.0)).xyz;
    float3 skinnedPos = pos1 * weight1 + pos2 * weight2;

    // Blend normals using bone matrices
    float3 norm1 = mul((float3x3)Bones[index1].L2W, input.NormIndex.xyz);
    float3 norm2 = mul((float3x3)Bones[index2].L2W, input.NormIndex.xyz);
    float3 skinnedNorm = normalize(norm1 * weight1 + norm2 * weight2);

    // Transform position through matrices
    float4 worldPos = float4(skinnedPos, 1.0);
    float4 viewPos  = mul(View, worldPos);
    output.Pos      = mul(Projection, viewPos);

    // Pass through vertex attributes
    output.WorldPos    = worldPos.xyz;
    output.Normal      = skinnedNorm;

    float3 viewVector = worldPos.xyz - CameraPosition.xyz;
    output.ViewVector = viewVector;

    float2 depthParams = MaterialParams.zw;
    float nearZ = depthParams.x;
    float farZ  = depthParams.y;
    float invRange = rcp(max(farZ - nearZ, 1e-5f));
    float linearDepth = (viewPos.z - nearZ) * invRange;
    output.LinearDepth = saturate(linearDepth);
    float2 uvAnimOffset = UVAnim.xy;
    output.UV          = input.UVWeights.xy + uvAnimOffset;

    return output;
}

//==============================================================================
//  PIXEL SHADER
//==============================================================================

GEOM_PIXEL_OUTPUT PSMain( GEOM_PIXEL_INPUT input )
{
    return ShadeGeometryPixel( input );
}