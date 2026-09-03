//==============================================================================
//
//  a51_dynamic_geometry.hlsl
//
//  Dynamic geometry G-buffer shader.
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
#include "common/cloth_damage.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbDynamicGeometry A51_CBUFFER_BIND(0, 0)
{
    float4x4 View;
    float4x4 Projection;
    float4   CameraPosition;
    float4   DepthParams;
    uint     LightingIndex;
    uint     ShaderFlags;
    uint2    DynamicPadding;
};

//------------------------------------------------------------------------------

#if defined(A51_SHADER_BINDING_SDL)
    A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDiffuse A51_SAMPLED_TEXTURE_BIND(0, 0);
    A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D txDamage A51_SAMPLED_TEXTURE_BIND(1, 1);
    A51_SAMPLED_TEXTURE_ATTR(20, 2) Texture2D<float4> txFaceShadowAtlas A51_SAMPLED_TEXTURE_BIND(20, 2);
    A51_SAMPLED_TEXTURE_ATTR(29, 3) Texture2D<float> txFaceShadowDepthAtlas A51_SAMPLED_TEXTURE_BIND(29, 3);
    A51_SAMPLED_TEXTURE_ATTR(4, 4) Texture2DArray<float> txProjectionAtlas A51_SAMPLED_TEXTURE_BIND(4, 4);
    
    A51_SAMPLER_ATTR(0, 0) SamplerState samDiffuse A51_SAMPLER_BIND(0, 0);
    A51_SAMPLER_ATTR(1, 1) SamplerState samDamage A51_SAMPLER_BIND(1, 1);
    A51_SAMPLER_ATTR(20, 2) SamplerState samFaceShadow A51_SAMPLER_BIND(20, 2);
    A51_SAMPLER_ATTR(9, 3) SamplerState samFaceShadowDepth A51_SAMPLER_BIND(9, 3);
    A51_SAMPLER_ATTR(4, 4) SamplerState samProjectionAtlas A51_SAMPLER_BIND(4, 4);
#else
    A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDiffuse A51_SAMPLED_TEXTURE_BIND(0, 0);
    A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D txDamage A51_SAMPLED_TEXTURE_BIND(1, 1);
    A51_SAMPLED_TEXTURE_ATTR(20, 2) Texture2D<float4> txFaceShadowAtlas A51_SAMPLED_TEXTURE_BIND(20, 2);
    A51_SAMPLED_TEXTURE_ATTR(29, 3) Texture2D<float> txFaceShadowDepthAtlas A51_SAMPLED_TEXTURE_BIND(29, 3);
    A51_SAMPLED_TEXTURE_ATTR(4, 4) Texture2DArray<float> txProjectionAtlas A51_SAMPLED_TEXTURE_BIND(4, 4);
    
    A51_SAMPLER_ATTR(0, 0) SamplerState samDiffuse A51_SAMPLER_BIND(0, 0);
    A51_SAMPLER_ATTR(1, 1) SamplerState samDamage A51_SAMPLER_BIND(1, 1);
    A51_SAMPLER_ATTR(8, 2) SamplerState samFaceShadow A51_SAMPLER_BIND(8, 2);
    A51_SAMPLER_ATTR(9, 3) SamplerState samFaceShadowDepth A51_SAMPLER_BIND(9, 3);
    A51_SAMPLER_ATTR(4, 4) SamplerState samProjectionAtlas A51_SAMPLER_BIND(4, 4);
#endif

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 UV       : TEXCOORD2;
    float4 Color    : TEXCOORD3;
};

//------------------------------------------------------------------------------

struct GEOM_PIXEL_INPUT
{
    float4 Position   : SV_POSITION;
    float2 UV         : TEXCOORD0;
    float4 Color      : COLOR0;
    float3 WorldPos   : TEXCOORD1;
    float3 Normal     : TEXCOORD2;
    float3 ViewNormal : TEXCOORD3;
    float3 ViewVector : TEXCOORD4;
    float  ViewDepth  : TEXCOORD5;
};

//------------------------------------------------------------------------------

struct PS_OUTPUT
{
    float4 FinalColor  : SV_Target0;
    float4 NormalDepth : SV_Target1;
    float4 Glow        : SV_Target2;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT input )
{
    return ShaderFlags;
}

//==============================================================================

uint GeomGetLightingIndex( GEOM_PIXEL_INPUT input )
{
    return LightingIndex;
}

//==============================================================================

uint GeomGetLightCount( GEOM_PIXEL_INPUT input )
{
    return min( GeomLighting[LightingIndex].LightCountPadding.x, (uint)MAX_GEOM_LIGHTS );
}

//==============================================================================

float4 GeomGetLightVec( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightVec[i];
}

//==============================================================================

float4 GeomGetLightCol( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightCol[i];
}

//==============================================================================

float4 GeomGetLightDir( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightDir[i];
}

//==============================================================================

float4 GeomGetLightCone( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightCone[i];
}

//==============================================================================

float4 GeomGetLightCookieU( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightCookieU[i];
}

//==============================================================================

float4 GeomGetLightCookieV( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightCookieV[i];
}

//==============================================================================

float4 GeomGetLightCookieAtlas( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightCookieAtlas[i];
}

//==============================================================================

uint GeomGetLightCookieLayer( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightCookieLayer[i];
}

//==============================================================================

float GeomGetLightCookieMaxMip( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightCookieMaxMip[i];
}

//==============================================================================

uint GeomGetLightShadowIndex( GEOM_PIXEL_INPUT input, uint i )
{
    return GeomLighting[LightingIndex].LightShadowIndex[i];
}

//==============================================================================

float4 GeomGetLightAmbCol( GEOM_PIXEL_INPUT input )
{
    return GeomLighting[LightingIndex].LightAmbCol;
}

static const float4 EnvParams = 0.0f;

#define GEOM_HAS_VERTEX_COLOR 1

//------------------------------------------------------------------------------

#include "common/geom_pixel_lighting.hlsl"
#include "common/geom_local_shadow_maps.hlsl"

//==============================================================================
//  SHADERS
//==============================================================================

GEOM_PIXEL_INPUT VSMain( VS_INPUT input )
{
    GEOM_PIXEL_INPUT output;
    const float4 viewPosition = mul( View, float4( input.Position, 1.0f ) );
    output.Position = mul( Projection, viewPosition );
    output.UV = input.UV;
    output.Color = input.Color.bgra;
    output.WorldPos = input.Position;
    output.Normal = normalize( input.Normal );
    output.ViewNormal = normalize( mul( (float3x3)View, input.Normal ) );
    output.ViewVector = input.Position - CameraPosition.xyz;
    output.ViewDepth = viewPosition.z;
    return output;
}

//==============================================================================

PS_OUTPUT PSMain( GEOM_PIXEL_INPUT input, bool isFrontFace : SV_IsFrontFace )
{
    float4 diffuse = txDiffuse.Sample( samDiffuse, input.UV );
    const float damage = ClothDamageCoverage( txDamage, samDamage, input.UV );
    clip( diffuse.a * damage - 0.05f );

    input.Normal = normalize( input.Normal );
    input.ViewNormal = normalize( input.ViewNormal );
    if( isFrontFace )
    {
        input.Normal = -input.Normal;
        input.ViewNormal = -input.ViewNormal;
    }

    float3 geometricNormal = cross( ddx( input.WorldPos ), ddy( input.WorldPos ) );
    const float geometricNormalLength = length( geometricNormal );
    geometricNormal = ( geometricNormalLength > 1e-5f ) ? ( geometricNormal / geometricNormalLength ) : input.Normal;
    if( dot( geometricNormal, input.Normal ) < 0.0f )
    {
        geometricNormal = -geometricNormal;
    }

    const GeomLightingResult lighting = GeomComputeLighting( input, ShaderFlags, diffuse.a, geometricNormal );
    float3 finalColor = diffuse.rgb * lighting.Diffuse + lighting.Specular;
    if( ShaderFlags & INSTANCE_FLAG_PROJ_LIGHT )
    {
        finalColor = ApplyProjLights( finalColor, input.WorldPos );
    }
    if( ShaderFlags & INSTANCE_FLAG_PROJ_SHADOW )
    {
        finalColor = ApplyProjShadows( finalColor, input.WorldPos );
    }

    float const fray = ClothDamageEdge( txDamage, samDamage, input.UV, damage );
    finalColor = lerp( finalColor, saturate( finalColor * 0.35f + 0.45f ), fray * 0.8f );
    finalColor = saturate( finalColor );

    PS_OUTPUT output;
    output.FinalColor = float4( finalColor, 1.0f );
    output.NormalDepth = float4( input.ViewNormal * 0.5f + 0.5f,
                                 saturate( ( input.ViewDepth - DepthParams.x ) /
                                           max( DepthParams.y - DepthParams.x, 1e-5f ) ) );
    output.Glow = ( ShaderFlags & INSTANCE_FLAG_GLOWING ) ? output.FinalColor : 0.0f;
    return output;
}
