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

#define GEOM_USE_SKIN_INSTANCE_DATA 1
#define GEOM_USE_SKIN_LIGHTING
#include "common/geom_buffers.hlsl"
#include "common/skin_instance_buffers.hlsl"

struct VS_INPUT
{
    float4 PosIndex  : POSITION;
    float4 NormIndex : NORMAL;
    float4 UVWeights : TEXCOORD0;
    uint   InstanceID : SV_InstanceID;
};

struct GEOM_PIXEL_INPUT
{
    float4 Pos         : SV_POSITION;
    float2 UV          : TEXCOORD0;
    float3 WorldPos    : TEXCOORD1;
    float3 Normal      : TEXCOORD2;
    float3 ViewVector  : TEXCOORD3;
    float3 ViewNormal  : TEXCOORD4;
    nointerpolation uint InstanceID : TEXCOORD5;
};

#include "common/pixel_structs.hlsl"
#include "common/geom_textures.hlsl"
#include "common/geom_pixel_shared.hlsl"

//==============================================================================
//  VERTEX SHADER
//==============================================================================

GEOM_PIXEL_INPUT VSMain(VS_INPUT input)
{
    GEOM_PIXEL_INPUT output;

    uint  index1  = (uint)input.PosIndex.w;
    uint  index2  = (uint)input.NormIndex.w;
    float weight1 = input.UVWeights.z;
    float weight2 = input.UVWeights.w;

    float4x4 bone1 = SkinGetBoneL2W( input.InstanceID, index1 );
    float4x4 bone2 = SkinGetBoneL2W( input.InstanceID, index2 );

    float3 pos1 = mul( bone1, float4( input.PosIndex.xyz, 1.0f ) ).xyz;
    float3 pos2 = mul( bone2, float4( input.PosIndex.xyz, 1.0f ) ).xyz;
    float3 skinnedPos = pos1 * weight1 + pos2 * weight2;

    float3 norm1 = mul( (float3x3)bone1, input.NormIndex.xyz );
    float3 norm2 = mul( (float3x3)bone2, input.NormIndex.xyz );
    float3 skinnedNorm = normalize( norm1 * weight1 + norm2 * weight2 );

    float4 worldPos = float4( skinnedPos, 1.0f );
    float4 viewPos  = mul( View, worldPos );
    output.Pos      = mul( Projection, viewPos );

    float3 viewNormal = normalize( mul( (float3x3)View, skinnedNorm ) );
    output.WorldPos   = worldPos.xyz;
    output.Normal     = skinnedNorm;
    output.ViewNormal = viewNormal;
    output.ViewVector = worldPos.xyz - CameraPosition.xyz;
    output.UV         = input.UVWeights.xy + UVAnim.xy;
    output.InstanceID = input.InstanceID;

    return output;
}

//==============================================================================
//  PIXEL SHADER
//==============================================================================

GEOM_PIXEL_OUTPUT PSMain( GEOM_PIXEL_INPUT input )
{
    return ShadeGeometryPixel( input );
}
