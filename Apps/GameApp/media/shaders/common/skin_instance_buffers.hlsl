//==============================================================================
//
//  skin_instance_buffers.hlsl
//
//  Structured buffers for skinned instanced rendering.
//
//==============================================================================

#ifndef SKIN_INSTANCE_BUFFERS_HLSL
#define SKIN_INSTANCE_BUFFERS_HLSL

struct SkinInstanceData
{
    float4 LightVec[MAX_GEOM_LIGHTS];
    float4 LightCol[MAX_GEOM_LIGHTS];
    float4 LightDir[MAX_GEOM_LIGHTS];
    float4 LightCone[MAX_GEOM_LIGHTS];
    float4 LightAmbCol;
    uint   ShaderFlags;
    uint   BoneOffset;
    uint   LightCount;
    uint   Padding0;
    float  FadeAlpha;
    float3 Padding;
};

struct SkinBoneMatrix
{
    float4x4 L2W;
};

StructuredBuffer<SkinInstanceData> SkinInstances    : register(t24);
StructuredBuffer<SkinBoneMatrix>   SkinBoneMatrices : register(t25);

cbuffer cbSkinSection : register(b2)
{
    uint4 SkinSectionBoneRemapPacked[MAX_SKIN_BONES / 4];
};

uint SkinGetSectionBoneIndex( uint cacheIndex )
{
    uint clampedCache = min( cacheIndex, (uint)( MAX_SKIN_BONES - 1 ) );
    uint4 packed      = SkinSectionBoneRemapPacked[clampedCache >> 2];
    uint  lane        = clampedCache & 3u;

    if( lane == 0u ) return packed.x;
    if( lane == 1u ) return packed.y;
    if( lane == 2u ) return packed.z;
    return packed.w;
}

float4x4 SkinGetBoneL2W( uint instanceID, uint cacheIndex )
{
    uint boneIndex = SkinGetSectionBoneIndex( cacheIndex );
    return SkinBoneMatrices[SkinInstances[instanceID].BoneOffset + boneIndex].L2W;
}

#endif // SKIN_INSTANCE_BUFFERS_HLSL
