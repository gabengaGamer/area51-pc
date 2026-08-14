//==============================================================================
//
//  rigid_instance_buffers.hlsl
//
//  Structured buffers for rigid instanced rendering.
//
//==============================================================================

#ifndef RIGID_INSTANCE_BUFFERS_HLSL
#define RIGID_INSTANCE_BUFFERS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================
//  TYPES
//==============================================================================

struct RigidInstanceData
{
    float4x4 World;
    uint     ShaderFlags;
    uint     ColorOffset;
    uint     LightingIndex;
    float    FadeAlpha;
};

//==============================================================================

#if defined(A51_SHADER_BINDING_SDL) && defined(A51_SHADER_STAGE_PIXEL)
    #define A51_RIGID_INSTANCES_BINDING     8
    #define A51_RIGID_VERTEX_COLORS_BINDING 9
#else
    #define A51_RIGID_INSTANCES_BINDING     0
    #define A51_RIGID_VERTEX_COLORS_BINDING 1
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

A51_STORAGE_BUFFER_ATTR(22, A51_RIGID_INSTANCES_BINDING)
StructuredBuffer<RigidInstanceData> RigidInstances
    A51_STORAGE_BUFFER_BIND(22, A51_RIGID_INSTANCES_BINDING);
A51_STORAGE_BUFFER_ATTR(23, A51_RIGID_VERTEX_COLORS_BINDING)
StructuredBuffer<uint> RigidVertexColors
    A51_STORAGE_BUFFER_BIND(23, A51_RIGID_VERTEX_COLORS_BINDING);

//==============================================================================
//  FUNCTIONS
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
