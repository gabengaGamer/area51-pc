//==============================================================================
//
//  pixel_structs.hlsl
//
//  Shared pixel shader output structures.
//
//==============================================================================

#ifndef GEOM_PIXEL_STRUCTS_HLSL
#define GEOM_PIXEL_STRUCTS_HLSL

struct GEOM_PIXEL_OUTPUT
{
    float4 FinalColor : SV_Target0;
    float4 Albedo     : SV_Target1;
    float4 Normal     : SV_Target2;
    float  DepthInfo  : SV_Target3;
    float4 Glow       : SV_Target4;
};

#endif // GEOM_PIXEL_STRUCTS_HLSL