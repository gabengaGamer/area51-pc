//==============================================================================
//
//  a51_shadow_cast_skin.hlsl
//
//  Shadow depth caster shader for skinned geometry.
//
//==============================================================================

#define MAX_SKIN_BONES 96

#include "common/material_flags.hlsl"
#include "common/skin_instance_buffers.hlsl"

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
    uint   InstanceID : SV_InstanceID;
};

//------------------------------------------------------------------------------

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

//==============================================================================

VS_OUTPUT VSMain( VS_INPUT input )
{
    VS_OUTPUT output;
    uint  index1  = (uint)input.PosIndex.w;
    uint  index2  = (uint)input.NormIndex.w;
    float weight1 = input.UVWeights.z;
    float weight2 = input.UVWeights.w;

    float3 pos1 = mul( SkinGetBoneL2W( input.InstanceID, index1 ), float4( input.PosIndex.xyz, 1.0 ) ).xyz;
    float3 pos2 = mul( SkinGetBoneL2W( input.InstanceID, index2 ), float4( input.PosIndex.xyz, 1.0 ) ).xyz;
    float3 skinnedPos = pos1 * weight1 + pos2 * weight2;

    output.Position = mul( ShadowViewProjection, float4( skinnedPos, 1.0 ) );
    output.UV       = input.UVWeights.xy;
    return output;
}
