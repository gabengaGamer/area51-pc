//==============================================================================
//
//  geom_textures.hlsl
//
//  Shared texture and sampler bindings for geometry shaders.
//
//==============================================================================

#ifndef GEOM_TEXTURES_HLSL
#define GEOM_TEXTURES_HLSL

Texture2D        txDiffuse                      : register(t0);
Texture2D        txDetail                       : register(t1);
Texture2D        txEnvironment                  : register(t2);
TextureCube      txEnvironmentCube              : register(t3);
Texture2D        txProjLight [MAX_PROJ_LIGHTS]  : register(t4);
Texture2D        txProjShadow[MAX_PROJ_SHADOWS] : register(t8);
Texture2D<float> txFaceShadowAtlas              : register(t20);
Texture2D        txDistortionScene              : register(t21);
Texture2D        txLightCookie[MAX_GEOM_LIGHTS] : register(t26);

SamplerState     samLinear                      : register(s0);
SamplerState     samFaceShadow                  : register(s8);

//==============================================================================
#endif // GEOM_TEXTURES_HLSL
//==============================================================================
