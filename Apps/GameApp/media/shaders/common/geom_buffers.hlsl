//==============================================================================
//
//  geom_buffers.hlsl
//
//  Shared constant buffers for lighting and projection textures.
//
//==============================================================================

cbuffer cbProjTextures : register(b1)
{
    float4x4 ProjLightMatrix[MAX_PROJ_LIGHTS];
    float4x4 ProjShadowMatrix[MAX_PROJ_SHADOWS];
    uint     ProjLightCount;
    uint     ProjShadowCount;
    float    EdgeSize;
    float3   ProjPadding;
};

cbuffer cbLightConsts : register(b2)
{
    float4 LightVec[MAX_GEOM_LIGHTS];   // position or direction
    float4 LightCol[MAX_GEOM_LIGHTS];
    float4 LightAmbCol;
    uint   LightCount;
    float3 LightPadding;
};