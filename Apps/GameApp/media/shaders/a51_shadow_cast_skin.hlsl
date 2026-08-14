//==============================================================================
//
//  a51_shadow_cast_skin.hlsl
//
//  Shadow depth caster shader for skinned geometry.
//
//==============================================================================

//==============================================================================
//  DEFINES
//==============================================================================

#define MAX_SKIN_BONES 96

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbShadowCast A51_CBUFFER_BIND(0, 0)
{
    float4x4 ShadowViewProjection;
    uint4    ShadowCastPadding;
};

A51_STORAGE_BUFFER_ATTR(0, 0)
StructuredBuffer<float4x4> ShadowSkinBones
    A51_STORAGE_BUFFER_BIND(0, 0);

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float4 PosIndex  : TEXCOORD0;
    float4 NormIndex : TEXCOORD1;
    float4 UVWeights : TEXCOORD2;
    uint   BoneBase  : TEXCOORD3;
};

//------------------------------------------------------------------------------

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

//==============================================================================
//  SHADERS
//==============================================================================

VS_OUTPUT VSMain( VS_INPUT input )
{
    VS_OUTPUT output;
    int   index1  = (int)input.PosIndex.w;
    int   index2  = (int)input.NormIndex.w;
    float weight1 = input.UVWeights.z;
    float weight2 = input.UVWeights.w;

    float3 pos1 = mul( ShadowSkinBones[input.BoneBase + index1], float4( input.PosIndex.xyz, 1.0f ) ).xyz;
    float3 pos2 = mul( ShadowSkinBones[input.BoneBase + index2], float4( input.PosIndex.xyz, 1.0f ) ).xyz;
    float3 skinnedPos = pos1 * weight1 + pos2 * weight2;

    output.Position = mul( ShadowViewProjection, float4( skinnedPos, 1.0f ) );
    output.UV       = input.UVWeights.xy;
    return output;
}
