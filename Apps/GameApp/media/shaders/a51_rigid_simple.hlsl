//==============================================================================
//
//  a51_rigid_simple.hlsl
//
//  Simple rigidgeom uber-shader for A51.
//
//==============================================================================

#include "common/material_flags.hlsl"

//==============================================================================
//  GEOMETRY RESOURCES
//==============================================================================

#define GEOM_USE_RIGID_INSTANCE_DATA 1
#include "common/frame_constants.hlsl"
#include "common/lighting_constants.hlsl"
#include "common/proj_buffers.hlsl"
#include "common/shadow_buffers.hlsl"
#include "common/rigid_instance_buffers.hlsl"

#define GEOM_HAS_VERTEX_COLOR 1

//------------------------------------------------------------------------------

struct VS_INPUT
{
    float3 Position   : POSITION;
    float3 Normal     : NORMAL;
    float4 Color      : COLOR0;
    float2 UV         : TEXCOORD0;
    uint   VertexID   : SV_VertexID;
    uint   InstanceID : SV_InstanceID;
};

//------------------------------------------------------------------------------

struct GEOM_PIXEL_INPUT
{
    float4 Pos         : SV_POSITION;
    float2 UV          : TEXCOORD0;
    float4 Color       : COLOR0;
    float3 WorldPos    : TEXCOORD1;
    float3 Normal      : TEXCOORD2;
    float3 ViewVector  : TEXCOORD3;
    float3 ViewNormal  : TEXCOORD4;
    nointerpolation uint InstanceID : TEXCOORD5;
};

//------------------------------------------------------------------------------

#include "common/pixel_structs.hlsl"
#include "common/geom_textures.hlsl"
#include "common/geom_pixel_shared.hlsl"
#include "common/geom_local_shadow_maps.hlsl"

//==============================================================================
//  VERTEX SHADER
//==============================================================================

GEOM_PIXEL_INPUT VSMain( VS_INPUT input )
{
    GEOM_PIXEL_INPUT output;
    const uint instanceID = input.InstanceID;

    float4 worldPos    = mul( RigidInstances[instanceID].World, float4( input.Position, 1.0f ) );
    float4 viewPos     = mul( View, worldPos );
    float3 worldNormal = normalize( mul( (float3x3)RigidInstances[instanceID].World, input.Normal ) );
    float3 viewNormal  = normalize( mul( (float3x3)View, worldNormal ) );

    output.Pos        = mul( Projection, viewPos );
    output.UV         = input.UV + UVAnim.xy;
    output.WorldPos   = worldPos.xyz;
    output.Normal     = worldNormal;
    output.ViewVector = worldPos.xyz - CameraPosition.xyz;
    output.ViewNormal = viewNormal;
    output.InstanceID = instanceID;

    output.Color = input.Color;
    if( RigidInstances[instanceID].ColorOffset != 0xFFFFFFFFu )
    {
        const uint localVertex = input.VertexID - RigidInstances[instanceID].BaseVertex;
        output.Color = DecodeRigidVertexColor( RigidVertexColors[RigidInstances[instanceID].ColorOffset + localVertex] );
    }

    return output;
}

//==============================================================================
//  PIXEL SHADER
//==============================================================================

GEOM_PIXEL_OUTPUT PSMain( GEOM_PIXEL_INPUT input )
{
    return ShadeGeometryPixel( input );
}
