//==============================================================================
//
//  geom_textures.hlsl
//
//  Shared texture and sampler bindings for geometry shaders.
//
//==============================================================================

#ifndef GEOM_TEXTURES_HLSL
#define GEOM_TEXTURES_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDiffuse A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D txDetail A51_SAMPLED_TEXTURE_BIND(1, 1);
A51_SAMPLED_TEXTURE_ATTR(2, 2) Texture2D txEnvironment A51_SAMPLED_TEXTURE_BIND(2, 2);
A51_SAMPLED_TEXTURE_ATTR(3, 3) TextureCube txEnvironmentCube A51_SAMPLED_TEXTURE_BIND(3, 3);
A51_SAMPLED_TEXTURE_ATTR(20, 4) Texture2D<float4> txFaceShadowAtlas A51_SAMPLED_TEXTURE_BIND(20, 4);
A51_SAMPLED_TEXTURE_ATTR(21, 5) Texture2D txDistortionScene A51_SAMPLED_TEXTURE_BIND(21, 5);
A51_SAMPLED_TEXTURE_ATTR(4, 6) Texture2DArray<float> txProjectionAtlas A51_SAMPLED_TEXTURE_BIND(4, 6);
A51_SAMPLED_TEXTURE_ATTR(29, 7) Texture2D<float> txFaceShadowDepthAtlas A51_SAMPLED_TEXTURE_BIND(29, 7);

#if defined(A51_SHADER_BINDING_SDL)
    A51_SAMPLER_ATTR(0, 0) SamplerState samDiffuse A51_SAMPLER_BIND(0, 0);
    A51_SAMPLER_ATTR(1, 1) SamplerState samDetail A51_SAMPLER_BIND(1, 1);
    A51_SAMPLER_ATTR(2, 2) SamplerState samEnvironment A51_SAMPLER_BIND(2, 2);
    A51_SAMPLER_ATTR(3, 3) SamplerState samEnvironmentCube A51_SAMPLER_BIND(3, 3);
    A51_SAMPLER_ATTR(20, 4) SamplerState samFaceShadow A51_SAMPLER_BIND(20, 4);
    A51_SAMPLER_ATTR(21, 5) SamplerState samDistortion A51_SAMPLER_BIND(21, 5);
    A51_SAMPLER_ATTR(4, 6) SamplerState samProjectionAtlas A51_SAMPLER_BIND(4, 6);
    A51_SAMPLER_ATTR(9, 7) SamplerState samFaceShadowDepth A51_SAMPLER_BIND(9, 7);
#else
    A51_SAMPLER_ATTR(0, 0) SamplerState samLinear A51_SAMPLER_BIND(0, 0);
    A51_SAMPLER_ATTR(4, 4) SamplerState samProjectionAtlas A51_SAMPLER_BIND(4, 4);
    A51_SAMPLER_ATTR(8, 8) SamplerState samFaceShadow A51_SAMPLER_BIND(8, 8);
    A51_SAMPLER_ATTR(9, 9) SamplerState samFaceShadowDepth A51_SAMPLER_BIND(9, 9);
    
    #define samDiffuse         samLinear
    #define samDetail          samLinear
    #define samEnvironment     samLinear
    #define samEnvironmentCube samLinear
    #define samDistortion      samLinear
#endif

//==============================================================================
#endif // GEOM_TEXTURES_HLSL
//==============================================================================
