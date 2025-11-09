//==============================================================================
//
//  geom_textures.hlsl
//
//  Shared texture and sampler bindings for geometry shaders.
//
//==============================================================================

#ifndef GEOM_TEXTURES_HLSL
#define GEOM_TEXTURES_HLSL

Texture2D txDiffuse : register(t0);
Texture2D txDetail  : register(t1);
Texture2D txEnvironment : register(t2);
TextureCube txEnvironmentCube : register(t3);
Texture2D txProjLight[MAX_PROJ_LIGHTS]   : register(t4);
Texture2D txProjShadow[MAX_PROJ_SHADOWS] : register(t14);

SamplerState samLinear : register(s0);

#endif // GEOM_TEXTURES_HLSL