//==============================================================================
//
//  rigid_instance_buffers.hlsl
//
//  Structured buffers for rigid instanced rendering.
//
//==============================================================================

#ifndef RIGID_INSTANCE_BUFFERS_HLSL
#define RIGID_INSTANCE_BUFFERS_HLSL

struct RigidInstanceData
{
    float4x4 World;
    float4   LightVec[MAX_GEOM_LIGHTS];
    float4   LightCol[MAX_GEOM_LIGHTS];
    float4   LightDir[MAX_GEOM_LIGHTS];
    float4   LightCone[MAX_GEOM_LIGHTS];
    float4   LightAmbCol;
    uint     ShaderFlags;
    uint     ColorOffset;
    uint     BaseVertex;
    uint     LightCount;
    float    FadeAlpha;
    float3   Padding;
};

//==============================================================================

StructuredBuffer<RigidInstanceData> RigidInstances    : register(t22);
StructuredBuffer<uint>              RigidVertexColors : register(t23);

//==============================================================================

float4 DecodeRigidVertexColor( uint packedColor )
{
    return float4( (float)(( packedColor >> 16 ) & 0xFFu),
                   (float)(( packedColor >>  8 ) & 0xFFu),
                   (float)(  packedColor         & 0xFFu),
                   (float)(( packedColor >> 24 ) & 0xFFu) ) * (1.0f / 255.0f);
}

//==============================================================================
#endif // RIGID_INSTANCE_BUFFERS_HLSL
//==============================================================================
