//==============================================================================
//
//  a51_shadow_cast_rigid.hlsl
//
//  Shadow depth caster shader for rigid geometry.
//
//==============================================================================

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
StructuredBuffer<float4x4> ShadowRigidWorld
    A51_STORAGE_BUFFER_BIND(0, 0);

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 UV       : TEXCOORD2;
    uint   InstanceIndex : TEXCOORD3;
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
    float4 worldPos = mul( ShadowRigidWorld[input.InstanceIndex],
                           float4( input.Position, 1.0f ) );
    output.Position = mul( ShadowViewProjection, worldPos );
    output.UV       = input.UV;
    return output;
}
