//==============================================================================
//
//  a51_shadow_cast_rigid.hlsl
//
//  Shadow depth caster shader for rigid geometry.
//
//==============================================================================

#include "common/material_flags.hlsl"
#include "common/rigid_instance_buffers.hlsl"

//==============================================================================

cbuffer cbShadowCast : register(b0)
{
    float4x4 ShadowViewProjection;
};

//------------------------------------------------------------------------------

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR0;
    float2 UV       : TEXCOORD0;
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
    float4 worldPos = mul( RigidInstances[input.InstanceID].World, float4( input.Position, 1.0f ) );
    output.Position = mul( ShadowViewProjection, worldPos );
    output.UV       = input.UV;
    return output;
}
