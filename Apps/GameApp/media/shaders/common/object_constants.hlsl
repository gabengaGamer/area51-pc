//==============================================================================
//
//  object_constants.hlsl
//
//  Per-object transform data shared by rigid geometry shaders.
//
//==============================================================================

#ifndef OBJECT_CONSTANTS_HLSL
#define OBJECT_CONSTANTS_HLSL

cbuffer cbObjectConstants : register(b1)
{
    float4x4 World;   // Local to world matrix
};

#endif // OBJECT_CONSTANTS_HLSL