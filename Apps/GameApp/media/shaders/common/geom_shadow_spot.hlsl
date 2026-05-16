//==============================================================================
//
//  geom_shadow_spot.hlsl
//
//  Spot-light shadow sampling helpers.
//
//==============================================================================

#ifndef GEOM_SHADOW_SPOT_HLSL
#define GEOM_SHADOW_SPOT_HLSL

float SampleFaceShadowSource( uint sourceIndex, float3 worldPos, float3 worldNormal )
{
    if( sourceIndex >= FaceShadowCount )
        return 1.0f;

    const float3 lightPos        = FaceShadowLightPosRadius[sourceIndex].xyz;
    const float  lightRadius     = FaceShadowLightPosRadius[sourceIndex].w;
    const float3 lightDir        = normalize( FaceShadowLightDirFalloff[sourceIndex].xyz );
    const float  lightFalloff    = FaceShadowLightDirFalloff[sourceIndex].w;
    const float  cosOuter        = FaceShadowLightData[sourceIndex].x;
    const float  nearZ           = FaceShadowLightData[sourceIndex].y;
    const bool   isPointFace     = FaceShadowIsPointFace( sourceIndex );
    const float3 toLight         = lightPos - worldPos;
    const float  lightDistanceSq = dot( toLight, toLight );

    if( isPointFace )
        return 1.0f;

    if( lightDistanceSq <= 1e-8f )
        return 1.0f;

    const float lightDistance   = sqrt( lightDistanceSq );
    const float shadowInfluence = GeomComputeRadialAttenuation( lightDistance, lightRadius, lightFalloff );
    if( shadowInfluence <= 0.0f )
        return 1.0f;

    const float3 pointToLightDir = toLight / lightDistance;
    const float  coneCos         = dot( lightDir, -pointToLightDir );
    const float  coneRange       = max( 1.0f - cosOuter, 1e-4f );
    const float  coneInfluence   = saturate( ( coneCos - cosOuter ) / coneRange );
    if( coneInfluence <= 0.0f )
        return 1.0f;

    const float nearInfluence = ComputeShadowNearInfluence( lightDistance, nearZ, lightRadius );
    if( nearInfluence <= 0.0f )
        return 1.0f;

    const float3 normal = normalize( worldNormal );
    if( dot( normal, pointToLightDir ) <= 0.0f )
        return 1.0f;

    const float visibility = SampleFaceShadowAtlasRaw( sourceIndex, worldPos );
    return lerp( 1.0f, visibility, nearInfluence );
}

//==============================================================================
#endif // GEOM_SHADOW_SPOT_HLSL
//==============================================================================
