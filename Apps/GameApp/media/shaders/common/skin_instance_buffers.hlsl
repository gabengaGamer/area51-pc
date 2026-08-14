//==============================================================================
//
//  skin_instance_buffers.hlsl
//
//  Structured buffers for skinned instanced rendering.
//
//==============================================================================

#ifndef SKIN_INSTANCE_BUFFERS_HLSL
#define SKIN_INSTANCE_BUFFERS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================
//  TYPES
//==============================================================================

struct SkinInstanceData
{
    uint   ShaderFlags;
    uint   BoneOffset;
    uint   LightingIndex;
    uint   Padding0;
    float4 FadeAlphaPadding;
};

//------------------------------------------------------------------------------

struct SkinBoneMatrix
{
    float4x4 L2W;
};

//------------------------------------------------------------------------------

#if defined(A51_SHADER_BINDING_SDL) && defined(A51_SHADER_STAGE_PIXEL)
    #define A51_SKIN_INSTANCES_BINDING      8
    #define A51_SKIN_BONE_MATRICES_BINDING  9
    #define A51_SKIN_BONE_REMAPS_BINDING    10
#else
    #define A51_SKIN_INSTANCES_BINDING      0
    #define A51_SKIN_BONE_MATRICES_BINDING  1
    #define A51_SKIN_BONE_REMAPS_BINDING    2
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

A51_STORAGE_BUFFER_ATTR(24, A51_SKIN_INSTANCES_BINDING)
StructuredBuffer<SkinInstanceData> SkinInstances
    A51_STORAGE_BUFFER_BIND(24, A51_SKIN_INSTANCES_BINDING);
A51_STORAGE_BUFFER_ATTR(25, A51_SKIN_BONE_MATRICES_BINDING)
StructuredBuffer<SkinBoneMatrix> SkinBoneMatrices
    A51_STORAGE_BUFFER_BIND(25, A51_SKIN_BONE_MATRICES_BINDING);
A51_STORAGE_BUFFER_ATTR(26, A51_SKIN_BONE_REMAPS_BINDING)
StructuredBuffer<uint> SkinBoneRemaps
    A51_STORAGE_BUFFER_BIND(26, A51_SKIN_BONE_REMAPS_BINDING);

//==============================================================================
//  FUNCTIONS
//==============================================================================

float4x4 SkinGetBoneL2W( uint instanceID, uint boneRemapOffset, uint cacheIndex )
{
    const uint clampedCache = min( cacheIndex, (uint)( MAX_SKIN_BONES - 1 ) );
    const uint boneIndex = SkinBoneRemaps[boneRemapOffset + clampedCache];
    return SkinBoneMatrices[SkinInstances[instanceID].BoneOffset + boneIndex].L2W;
}

//==============================================================================
#endif // SKIN_INSTANCE_BUFFERS_HLSL
//==============================================================================
