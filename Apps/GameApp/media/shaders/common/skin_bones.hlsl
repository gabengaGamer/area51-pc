//==============================================================================
//
//  skin_bones.hlsl
//
//  Bone matrices for skinned geometry.
//
//==============================================================================

#ifndef SKIN_BONES_HLSL
#define SKIN_BONES_HLSL

struct Bone
{
    float4x4 L2W;
};

cbuffer cbSkinBones : register(b2)
{
    Bone Bones[MAX_SKIN_BONES];
};

#endif // SKIN_BONES_HLSL