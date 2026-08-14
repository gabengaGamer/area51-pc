//==============================================================================
//
//  geom_shadow_spot.hlsl
//
//  Spot-light shadow sampling helpers.
//
//==============================================================================

#ifndef GEOM_SHADOW_SPOT_HLSL
#define GEOM_SHADOW_SPOT_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float SampleFaceShadowSource( uint sourceIndex,
                              float3 worldPos,
                              float3 worldNormal,
                              float3 geometricNormal )
{
    if( sourceIndex >= GetFaceShadowCount() )
    {
        return 1.0f;
    }

    const float4 lightPosRadius  = GetFaceShadowLightPosRadius( sourceIndex );
    const float4 lightDirFalloff = GetFaceShadowLightDirFalloff( sourceIndex );
    const float4 lightData       = GetFaceShadowLightData( sourceIndex );
    const float3 lightPos        = lightPosRadius.xyz;
    const float  lightRadius     = lightPosRadius.w;
    const float3 lightDir        = normalize( lightDirFalloff.xyz );
    const float  lightFalloff    = lightDirFalloff.w;
    const float  cosOuter        = lightData.x;
    const float  nearZ           = lightData.y;
    const bool   isPointFace     = FaceShadowIsPointFace( sourceIndex );
    const float3 toLight         = lightPos - worldPos;
    const float  lightDistanceSq = dot( toLight, toLight );

    if( isPointFace )
    {
        return 1.0f;
    }

    if( lightDistanceSq <= 1e-8f )
    {
        return 1.0f;
    }

    const float lightDistance   = sqrt( lightDistanceSq );
    const float shadowInfluence = GeomComputeRadialAttenuation( lightDistance, lightRadius, lightFalloff );
    if( shadowInfluence <= 0.0f )
    {
        return 1.0f;
    }

    const float3 pointToLightDir = toLight / lightDistance;
    const float  coneCos         = dot( lightDir, -pointToLightDir );
    const float  coneRange       = max( 1.0f - cosOuter, 1e-4f );
    const float  coneInfluence   = saturate( ( coneCos - cosOuter ) / coneRange );
    if( coneInfluence <= 0.0f )
    {
        return 1.0f;
    }

    const float nearInfluence = ComputeShadowNearInfluence( lightDistance, nearZ, lightRadius );
    if( nearInfluence <= 0.0f )
    {
        return 1.0f;
    }

    const float3 normal = normalize( worldNormal );
    if( dot( normal, pointToLightDir ) <= 0.0f )
    {
        return 1.0f;
    }

    float visibility;
    if( !TrySampleFaceShadow( sourceIndex, worldPos, geometricNormal, visibility ) )
    {
        return 1.0f;
    }

    return lerp( 1.0f, visibility, nearInfluence );
}

//==============================================================================
#endif // GEOM_SHADOW_SPOT_HLSL
//==============================================================================
