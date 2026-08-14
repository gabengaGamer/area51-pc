//==============================================================================
//
//  a51_decal.hlsl
//
//  Lit forward surface decals.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/material_flags.hlsl"
#include "common/shader_bindings.hlsl"

#if defined(A51_SHADER_BINDING_SDL) && defined(A51_SHADER_STAGE_PIXEL)
    #define A51_GEOM_LIGHTING_BINDING 5
    #define A51_GEOM_SHADOW_MATRICES_BINDING 6
    #define A51_GEOM_SHADOW_DATA_BINDING 7
#endif

#include "common/lighting_constants.hlsl"
#include "common/proj_buffers.hlsl"
#include "common/shadow_buffers.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbDecalDraw A51_CBUFFER_BIND(0, 0)
{
    float4x4 WorldToClip;
    float4   CameraPosition;
    float4   GeometricNormal;
    float4   EnvParams;
    uint     LightingIndex;
    uint     DecalFlags;
    uint     DecalBlendMode;
    uint     DecalOutputMode;
};

//------------------------------------------------------------------------------

#if defined(A51_SHADER_BINDING_SDL)
    A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDecal A51_SAMPLED_TEXTURE_BIND(0, 0);
    A51_SAMPLED_TEXTURE_ATTR(3, 1) TextureCube txEnvironmentCube A51_SAMPLED_TEXTURE_BIND(3, 1);
    A51_SAMPLED_TEXTURE_ATTR(20, 2) Texture2D<float4> txFaceShadowAtlas A51_SAMPLED_TEXTURE_BIND(20, 2);
    A51_SAMPLED_TEXTURE_ATTR(4, 3) Texture2DArray<float> txProjectionAtlas A51_SAMPLED_TEXTURE_BIND(4, 3);
    A51_SAMPLED_TEXTURE_ATTR(29, 4) Texture2D<float> txFaceShadowDepthAtlas A51_SAMPLED_TEXTURE_BIND(29, 4);
    
    A51_SAMPLER_ATTR(0, 0) SamplerState samDecal A51_SAMPLER_BIND(0, 0);
    A51_SAMPLER_ATTR(3, 1) SamplerState samEnvironmentCube A51_SAMPLER_BIND(3, 1);
    A51_SAMPLER_ATTR(20, 2) SamplerState samFaceShadow A51_SAMPLER_BIND(20, 2);
    A51_SAMPLER_ATTR(4, 3) SamplerState samProjectionAtlas A51_SAMPLER_BIND(4, 3);
    A51_SAMPLER_ATTR(9, 4) SamplerState samFaceShadowDepth A51_SAMPLER_BIND(9, 4);
#else
    A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDecal A51_SAMPLED_TEXTURE_BIND(0, 0);
    A51_SAMPLED_TEXTURE_ATTR(3, 3) TextureCube txEnvironmentCube A51_SAMPLED_TEXTURE_BIND(3, 3);
    A51_SAMPLED_TEXTURE_ATTR(20, 4) Texture2D<float4> txFaceShadowAtlas A51_SAMPLED_TEXTURE_BIND(20, 4);
    A51_SAMPLED_TEXTURE_ATTR(4, 6) Texture2DArray<float> txProjectionAtlas A51_SAMPLED_TEXTURE_BIND(4, 6);
    A51_SAMPLED_TEXTURE_ATTR(29, 7) Texture2D<float> txFaceShadowDepthAtlas A51_SAMPLED_TEXTURE_BIND(29, 7);
    
    A51_SAMPLER_ATTR(0, 0) SamplerState samLinear A51_SAMPLER_BIND(0, 0);
    A51_SAMPLER_ATTR(4, 4) SamplerState samProjectionAtlas A51_SAMPLER_BIND(4, 4);
    A51_SAMPLER_ATTR(8, 8) SamplerState samFaceShadow A51_SAMPLER_BIND(8, 8);
    A51_SAMPLER_ATTR(9, 9) SamplerState samFaceShadowDepth A51_SAMPLER_BIND(9, 9);
    #define samDecal samLinear
    #define samEnvironmentCube samLinear
#endif

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position : TEXCOORD0;
    float4 Color    : TEXCOORD1;
    float2 UV       : TEXCOORD2;
};

//------------------------------------------------------------------------------

struct GEOM_PIXEL_INPUT
{
    float4 Position   : SV_POSITION;
    float4 Color      : COLOR0;
    float2 UV         : TEXCOORD0;
    float3 WorldPos   : TEXCOORD1;
    float3 Normal     : TEXCOORD2;
    float3 ViewVector : TEXCOORD3;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT input )
{
    return INSTANCE_FLAG_RECEIVE_LOCAL_SHADOW | INSTANCE_FLAG_PROJ_LIGHT | INSTANCE_FLAG_PROJ_SHADOW;
}

//==============================================================================

uint GeomGetLightingIndex( GEOM_PIXEL_INPUT input )
{
    return LightingIndex;
}

//==============================================================================

uint GeomGetLightCount( GEOM_PIXEL_INPUT input )
{
    return min( GeomLighting[GeomGetLightingIndex( input )].LightCountPadding.x, (uint)MAX_GEOM_LIGHTS );
}

//==============================================================================

float4 GeomGetLightVec( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightVec[lightIndex];
}

//==============================================================================

float4 GeomGetLightCol( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightCol[lightIndex];
}

//==============================================================================

float4 GeomGetLightDir( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightDir[lightIndex];
}

//==============================================================================

float4 GeomGetLightCone( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightCone[lightIndex];
}

//==============================================================================

float4 GeomGetLightCookieU( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightCookieU[lightIndex];
}

//==============================================================================

float4 GeomGetLightCookieV( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightCookieV[lightIndex];
}

//==============================================================================

float4 GeomGetLightCookieAtlas( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightCookieAtlas[lightIndex];
}

//==============================================================================

uint GeomGetLightCookieLayer( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightCookieLayer[lightIndex];
}

//==============================================================================

float GeomGetLightCookieMaxMip( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightCookieMaxMip[lightIndex];
}

//==============================================================================

uint GeomGetLightShadowIndex( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightShadowIndex[lightIndex];
}

//==============================================================================

float4 GeomGetLightAmbCol( GEOM_PIXEL_INPUT input )
{
    return GeomLighting[GeomGetLightingIndex( input )].LightAmbCol;
}

#define GEOM_HAS_VERTEX_COLOR 0

//------------------------------------------------------------------------------

#include "common/geom_pixel_lighting.hlsl"
#include "common/geom_local_shadow_maps.hlsl"

//==============================================================================
//  SHADERS
//==============================================================================

GEOM_PIXEL_INPUT VSMain( VS_INPUT input )
{
    GEOM_PIXEL_INPUT output;
    output.Position = mul( WorldToClip, float4( input.Position, 1.0f ) );
    output.Color = input.Color.bgra;
    output.UV = input.UV;
    output.WorldPos = input.Position;
    output.Normal = GeometricNormal.xyz;
    output.ViewVector = input.Position - CameraPosition.xyz;
    return output;
}

//==============================================================================

float4 PSMain( GEOM_PIXEL_INPUT input ) : SV_Target0
{
    static const uint DECAL_BLEND_ADDITIVE = 1u;
    static const uint DECAL_BLEND_INTENSITY = 3u;
    static const uint DECAL_DRAW_FLAG_ENV_MAPPED = 2u;
    static const uint DECAL_OUTPUT_GLOW = 1u;

    float4 textureSample = txDecal.Sample( samDecal, input.UV );
    float4 surface = textureSample * input.Color;
    if( DecalOutputMode == DECAL_OUTPUT_GLOW )
    {
        float glowMask = surface.a > 0.0f ? 1.0f : 0.0f;
        return float4( surface.rgb * surface.a, glowMask );
    }

    float3 geometricNormal = cross( ddx( input.WorldPos ), ddy( input.WorldPos ) );
    float normalLengthSquared = dot( geometricNormal, geometricNormal );
    if( normalLengthSquared > 1e-12f )
    {
        geometricNormal *= rsqrt( normalLengthSquared );
        if( dot( geometricNormal, GeometricNormal.xyz ) < 0.0f )
        {
            geometricNormal = -geometricNormal;
        }
    }
    else
    {
        geometricNormal = normalize( GeometricNormal.xyz );
    }
    input.Normal = geometricNormal;

    const bool isIntensity = DecalBlendMode == DECAL_BLEND_INTENSITY;
    const bool isAdditive = DecalBlendMode == DECAL_BLEND_ADDITIVE;

    if( isIntensity )
    {
        surface = textureSample;
    }
    else if( !isAdditive )
    {
        GeomLightingResult lighting =
            GeomComputeLighting( input, GeomGetMaterialFlags( input ), surface.a, geometricNormal );
        surface.rgb *= lighting.Diffuse;
        surface.rgb += lighting.Specular;
        surface.rgb = ApplyProjLights( surface.rgb, input.WorldPos );
        surface.rgb = ApplyProjShadows( surface.rgb, input.WorldPos );
    }

    if( ( DecalFlags & DECAL_DRAW_FLAG_ENV_MAPPED ) != 0u )
    {
        float3 cubeDirection = normalize( input.ViewVector );
        float3 environment = txEnvironmentCube.Sample( samEnvironmentCube, cubeDirection ).rgb;
        float envBlend = saturate( textureSample.a * EnvParams.w );
        if( isAdditive )
        {
            surface.rgb += environment * envBlend;
        }
        else
        {
            surface.rgb = lerp( surface.rgb, environment, envBlend );
        }
    }

    if( !isIntensity )
    {
        surface.rgb = saturate( surface.rgb );
    }
    return surface;
}
