//==============================================================================
//
//  pixelinput_geom.hlsl
//
//  Shared pixel shader input for geometry pipelines.
//
//==============================================================================

#ifndef GEOM_PIXEL_INPUT_HLSL
#define GEOM_PIXEL_INPUT_HLSL

#ifndef GEOM_HAS_VERTEX_COLOR
#define GEOM_HAS_VERTEX_COLOR 0
#endif

struct GEOM_PIXEL_INPUT
{
    float4 Pos         : SV_POSITION;
    float2 UV          : TEXCOORD0;
#if GEOM_HAS_VERTEX_COLOR
    float4 Color       : COLOR0;
#endif
    float3 WorldPos    : TEXCOORD1;
    float3 Normal      : TEXCOORD2;
    float  LinearDepth : TEXCOORD3;
    float3 ViewVector  : TEXCOORD4;
    float3 ViewNormal  : TEXCOORD5;
};

#endif // GEOM_PIXEL_INPUT_HLSL