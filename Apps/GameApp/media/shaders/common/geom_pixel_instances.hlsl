//==============================================================================
//
//  geom_pixel_instances.hlsl
//
//  Geometry instance accessors for rigid and skinned shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_INSTANCES_HLSL
#define GEOM_PIXEL_INSTANCES_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

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

uint GeomGetLightingIndex( GEOM_PIXEL_INPUT input )
{
    return RigidInstances[input.InstanceID].LightingIndex;
}

//==============================================================================

#elif defined(GEOM_USE_SKIN_INSTANCE_DATA)

uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT input )
{
    return MaterialFlags | SkinInstances[input.InstanceID].ShaderFlags;
}

//==============================================================================

float GeomGetFadeAlpha( GEOM_PIXEL_INPUT input )
{
    return saturate( SkinInstances[input.InstanceID].FadeAlphaPadding.x );
}

//==============================================================================

uint GeomGetLightingIndex( GEOM_PIXEL_INPUT input )
{
    return SkinInstances[input.InstanceID].LightingIndex;
}

#else
    #error "Geometry shader must define GEOM_USE_RIGID_INSTANCE_DATA or GEOM_USE_SKIN_INSTANCE_DATA before including geom_pixel_instances.hlsl"
#endif

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

//==============================================================================
#endif // GEOM_PIXEL_INSTANCES_HLSL
//==============================================================================
