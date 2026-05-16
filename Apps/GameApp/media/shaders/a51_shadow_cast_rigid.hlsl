//==============================================================================
//
//  a51_shadow_cast_rigid.hlsl
//
//  Shadow depth caster shader for rigid geometry.
//
//==============================================================================

cbuffer cbShadowCast : register(b0)
{
    float4x4 ShadowViewProjection;
    float4x4 World;
};

//------------------------------------------------------------------------------

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR0;
    float2 UV       : TEXCOORD0;
};

//==============================================================================

float4 VSMain( VS_INPUT input ) : SV_Position
{
    float4 worldPos = mul( World, float4( input.Position, 1.0f ) );
    return mul( ShadowViewProjection, worldPos );
}
