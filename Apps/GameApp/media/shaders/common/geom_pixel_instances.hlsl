//==============================================================================
//
//  geom_pixel_instances.hlsl
//
//  Geometry instance accessors for rigid and skinned shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_INSTANCES_HLSL
#define GEOM_PIXEL_INSTANCES_HLSL

#if defined(GEOM_USE_RIGID_INSTANCE_DATA)

uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT input )
{
    return MaterialFlags | RigidInstances[input.InstanceID].ShaderFlags;
}

//==============================================================================

float GeomGetFadeAlpha( GEOM_PIXEL_INPUT input )
{
    return saturate( RigidInstances[input.InstanceID].FadeAlpha );
}

//==============================================================================

uint GeomGetLightCount( GEOM_PIXEL_INPUT input )
{
    return min( RigidInstances[input.InstanceID].LightCount, (uint)MAX_GEOM_LIGHTS );
}

//==============================================================================

float4 GeomGetLightVec( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return RigidInstances[input.InstanceID].LightVec[lightIndex];
}

//==============================================================================

float4 GeomGetLightCol( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return RigidInstances[input.InstanceID].LightCol[lightIndex];
}

//==============================================================================

float4 GeomGetLightDir( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return RigidInstances[input.InstanceID].LightDir[lightIndex];
}

//==============================================================================

float4 GeomGetLightCone( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return RigidInstances[input.InstanceID].LightCone[lightIndex];
}

//==============================================================================

float4 GeomGetLightAmbCol( GEOM_PIXEL_INPUT input )
{
    return RigidInstances[input.InstanceID].LightAmbCol;
}

#elif defined(GEOM_USE_SKIN_INSTANCE_DATA)

uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT input )
{
    return MaterialFlags | SkinInstances[input.InstanceID].ShaderFlags;
}

//==============================================================================

float GeomGetFadeAlpha( GEOM_PIXEL_INPUT input )
{
    return saturate( SkinInstances[input.InstanceID].FadeAlpha );
}

//==============================================================================

uint GeomGetLightCount( GEOM_PIXEL_INPUT input )
{
    return min( SkinInstances[input.InstanceID].LightCount, (uint)MAX_GEOM_LIGHTS );
}

//==============================================================================

float4 GeomGetLightVec( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return SkinInstances[input.InstanceID].LightVec[lightIndex];
}

//==============================================================================

float4 GeomGetLightCol( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return SkinInstances[input.InstanceID].LightCol[lightIndex];
}

//==============================================================================

float4 GeomGetLightDir( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return SkinInstances[input.InstanceID].LightDir[lightIndex];
}

//==============================================================================

float4 GeomGetLightCone( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return SkinInstances[input.InstanceID].LightCone[lightIndex];
}

//==============================================================================

float4 GeomGetLightAmbCol( GEOM_PIXEL_INPUT input )
{
    return SkinInstances[input.InstanceID].LightAmbCol;
}

#else
#error "Geometry shader must define GEOM_USE_RIGID_INSTANCE_DATA or GEOM_USE_SKIN_INSTANCE_DATA before including geom_pixel_instances.hlsl"
#endif

//==============================================================================
#endif // GEOM_PIXEL_INSTANCES_HLSL
//==============================================================================
