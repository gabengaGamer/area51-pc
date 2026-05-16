//==============================================================================
//
//  a51_shadow_cast_skin.hlsl
//
//  Shadow depth caster shader for skinned geometry.
//
//==============================================================================

#define MAX_SKIN_BONES 96

#include "common/skin_bones.hlsl"

//------------------------------------------------------------------------------

cbuffer cbShadowCast : register(b0)
{
    float4x4 ShadowViewProjection;
};

//------------------------------------------------------------------------------

struct VS_INPUT
{
    float4 PosIndex  : POSITION;
    float4 NormIndex : NORMAL;
    float4 UVWeights : TEXCOORD0;
};

//==============================================================================

float4 VSMain( VS_INPUT input ) : SV_Position
{
    int   index1  = (int)input.PosIndex.w;
    int   index2  = (int)input.NormIndex.w;
    float weight1 = input.UVWeights.z;
    float weight2 = input.UVWeights.w;

    float3 pos1 = mul( Bones[index1].L2W, float4( input.PosIndex.xyz, 1.0 ) ).xyz;
    float3 pos2 = mul( Bones[index2].L2W, float4( input.PosIndex.xyz, 1.0 ) ).xyz;
    float3 skinnedPos = pos1 * weight1 + pos2 * weight2;

    return mul( ShadowViewProjection, float4( skinnedPos, 1.0 ) );
}
