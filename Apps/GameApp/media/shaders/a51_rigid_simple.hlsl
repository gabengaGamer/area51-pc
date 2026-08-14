//==============================================================================
//
//  a51_rigid_simple.hlsl
//
//  Simple rigid geometry shader.
//
//==============================================================================

//==============================================================================
//  INCLUDES
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

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position   : TEXCOORD0;
    float3 Normal     : TEXCOORD1;
    float2 UV         : TEXCOORD2;
    uint   InstanceIndex : TEXCOORD3;
    uint   VertexIndex   : TEXCOORD4;
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
//  SHADERS
//==============================================================================

GEOM_PIXEL_INPUT VSMain( VS_INPUT input )
{
    GEOM_PIXEL_INPUT output;
    const uint instanceID = input.InstanceIndex;

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

    output.Color = 1.0f.xxxx;
    if( RigidInstances[instanceID].ColorOffset != 0xFFFFFFFFu )
    {
        const uint colorIndex = RigidInstances[instanceID].ColorOffset + input.VertexIndex;
        output.Color = DecodeRigidVertexColor( RigidVertexColors[colorIndex] );
    }

    return output;
}

//==============================================================================

GEOM_PIXEL_OUTPUT PSMain( GEOM_PIXEL_INPUT input, bool isFrontFace : SV_IsFrontFace )
{
    return ShadeGeometryPixel( input, isFrontFace );
}

//==============================================================================

float4 PSScene( GEOM_PIXEL_INPUT input, bool isFrontFace : SV_IsFrontFace ) : SV_Target0
{
    const GEOM_PIXEL_OUTPUT shaded = ShadeGeometryPixel( input, isFrontFace );
    return GeomApplyForwardFog( shaded.FinalColor, input );
}
