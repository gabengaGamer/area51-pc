//==============================================================================
//
//  vertexinput_geom.hlsl
//
//  Configurable vertex input declarations for geometry shaders.
//
//==============================================================================

#ifndef GEOM_VERTEX_INPUT_TYPES
#define GEOM_VERTEX_INPUT_TYPES
#define GEOM_INPUT_RIGID 0
#define GEOM_INPUT_SKIN  1
#endif

#ifndef GEOM_INPUT_TYPE
#error "GEOM_INPUT_TYPE must be defined before including vertexinput_geom.hlsl"
#endif

#if GEOM_INPUT_TYPE == GEOM_INPUT_RIGID
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR0;
    float2 UV       : TEXCOORD0;
};
#elif GEOM_INPUT_TYPE == GEOM_INPUT_SKIN
struct VS_INPUT
{
    float4 PosIndex  : POSITION;
    float4 NormIndex : NORMAL;
    float4 UVWeights : TEXCOORD0;
};
#else
#error "Unsupported GEOM_INPUT_TYPE"
#endif

#undef GEOM_INPUT_TYPE