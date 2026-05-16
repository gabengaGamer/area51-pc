//==============================================================================
//
//  geom_shadow_point.hlsl
//
//  Point-light shadow sampling helpers.
//
//==============================================================================

#ifndef GEOM_SHADOW_POINT_HLSL
#define GEOM_SHADOW_POINT_HLSL

float ComputePointShadowFaceAlignment( uint sourceIndex, float3 lightDir )
{
    const float3 faceDir = normalize( FaceShadowLightDirFalloff[sourceIndex].xyz );
    return dot( faceDir, lightDir );
}

//==============================================================================

float SamplePointShadowLight( uint lightIndex, float3 worldPos, float3 worldNormal )
{
    const float3 lightPos        = PointShadowLightPosRadius[lightIndex].xyz;
    const float  lightRadius     = PointShadowLightPosRadius[lightIndex].w;
    const float  lightFalloff    = PointShadowLightData[lightIndex].x;
    const float  nearZ           = PointShadowLightData[lightIndex].y;
    const uint   firstFaceIndex  = min( (uint)PointShadowLightParams[lightIndex].x,
                                        (uint)MAX_SHADOW_SOURCES );
    const uint   faceCount       = min( (uint)PointShadowLightParams[lightIndex].y,
                                        (uint)POINT_SHADOW_FACE_COUNT );

    if( ( faceCount == 0u ) || ( firstFaceIndex >= FaceShadowCount ) )
        return 1.0f;

    const float3 toLight         = worldPos - lightPos;
    const float  lightDistanceSq = dot( toLight, toLight );

    if( lightDistanceSq <= 1e-8f )
        return 1.0f;

    const float lightDistance   = sqrt( lightDistanceSq );
    const float shadowInfluence = GeomComputeRadialAttenuation( lightDistance, lightRadius, lightFalloff );
    if( shadowInfluence <= 0.0f )
        return 1.0f;

    const float nearInfluence = ComputeShadowNearInfluence( lightDistance, nearZ, lightRadius );
    if( nearInfluence <= 0.0f )
        return 1.0f;

    const float3 lightDir        = toLight / lightDistance;
    const float3 pointToLightDir = -lightDir;
    const float3 normal          = normalize( worldNormal );
    if( dot( normal, pointToLightDir ) <= 0.0f )
        return 1.0f;

    uint         bestSourceIndex  = MAX_SHADOW_SOURCES;
    float        bestAlignment    = -2.0f;

    [fastopt]
    [loop]
    for( uint iFace = 0; iFace < POINT_SHADOW_FACE_COUNT; iFace++ )
    {
        if( iFace >= faceCount )
            break;

        const uint sourceIndex = firstFaceIndex + iFace;
        if( sourceIndex >= FaceShadowCount )
            break;

        if( !FaceShadowIsPointFace( sourceIndex ) )
            continue;

        const float faceAlignment = ComputePointShadowFaceAlignment( sourceIndex, lightDir );
        if( faceAlignment > bestAlignment )
        {
            bestAlignment   = faceAlignment;
            bestSourceIndex = sourceIndex;
        }
    }

    if( bestSourceIndex >= FaceShadowCount )
        return 1.0f;

    const float visibility = SampleFaceShadowAtlasRaw( bestSourceIndex, worldPos );
    return lerp( 1.0f, visibility, nearInfluence );
}

//==============================================================================
#endif // GEOM_SHADOW_POINT_HLSL
//==============================================================================
