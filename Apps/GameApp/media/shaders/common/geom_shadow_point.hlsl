//==============================================================================
//
//  geom_shadow_point.hlsl
//
//  Point-light shadow sampling helpers.
//
//==============================================================================

#ifndef GEOM_SHADOW_POINT_HLSL
#define GEOM_SHADOW_POINT_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float ComputePointShadowFaceAlignment( uint sourceIndex, float3 lightDir )
{
    const float3 faceDir = normalize( GetFaceShadowLightDirFalloff( sourceIndex ).xyz );
    return dot( faceDir, lightDir );
}

//==============================================================================

float SamplePointShadowLight( uint lightIndex,
                              float3 worldPos,
                              float3 worldNormal,
                              float3 geometricNormal )
{
    const float4 lightPosRadius  = GetPointShadowLightPosRadius( lightIndex );
    const float4 lightData       = GetPointShadowLightData( lightIndex );
    const float4 lightParams     = GetPointShadowLightParams( lightIndex );
    const float3 lightPos        = lightPosRadius.xyz;
    const float  lightRadius     = lightPosRadius.w;
    const float  lightFalloff    = lightData.x;
    const float  nearZ           = lightData.y;
    const uint   firstFaceIndex  = min( (uint)lightParams.x,
                                       (uint)MAX_SHADOW_SOURCES );
    const uint   faceCount       = min( (uint)lightParams.y,
                                       (uint)POINT_SHADOW_FACE_COUNT );

    if( ( faceCount == 0u ) || ( firstFaceIndex >= GetFaceShadowCount() ) )
    {
        return 1.0f;
    }

    const float3 toLight         = worldPos - lightPos;
    const float  lightDistanceSq = dot( toLight, toLight );

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

    const float nearInfluence = ComputeShadowNearInfluence( lightDistance, nearZ, lightRadius );
    if( nearInfluence <= 0.0f )
    {
        return 1.0f;
    }

    const float3 lightDir        = toLight / lightDistance;
    const float3 pointToLightDir = -lightDir;
    const float3 normal          = normalize( worldNormal );
    if( dot( normal, pointToLightDir ) <= 0.0f )
    {
        return 1.0f;
    }

    uint  bestSourceIndex   = MAX_SHADOW_SOURCES;
    uint  secondSourceIndex = MAX_SHADOW_SOURCES;
    float bestAlignment     = -2.0f;
    float secondAlignment   = -2.0f;

    [fastopt]
    [loop]
    for( uint iFace = 0; iFace < POINT_SHADOW_FACE_COUNT; ++iFace )
    {
        if( iFace >= faceCount )
        {
            break;
        }

        const uint sourceIndex = firstFaceIndex + iFace;
        if( sourceIndex >= GetFaceShadowCount() )
        {
            break;
        }

        if( !FaceShadowIsPointFace( sourceIndex ) )
        {
            continue;
        }

        const float faceAlignment = ComputePointShadowFaceAlignment( sourceIndex, lightDir );
        if( faceAlignment > bestAlignment )
        {
            secondAlignment   = bestAlignment;
            secondSourceIndex = bestSourceIndex;
            bestAlignment   = faceAlignment;
            bestSourceIndex = sourceIndex;
        }
        else if( faceAlignment > secondAlignment )
        {
            secondAlignment   = faceAlignment;
            secondSourceIndex = sourceIndex;
        }
    }

    if( bestSourceIndex >= GetFaceShadowCount() )
    {
        return 1.0f;
    }

    float bestVisibility;
    const bool bestValid =
        TrySampleFaceShadow( bestSourceIndex, worldPos, geometricNormal, bestVisibility );

    if( secondSourceIndex >= GetFaceShadowCount() )
    {
        return bestValid ? lerp( 1.0f, bestVisibility, nearInfluence ) : 1.0f;
    }

    const float seamBlendWidth = ( 2.0f * GetShadowParams().z ) / GetFaceShadowResolution( bestSourceIndex );
    const float alignmentGap   = bestAlignment - secondAlignment;
    if( bestValid && alignmentGap >= seamBlendWidth )
    {
        return lerp( 1.0f, bestVisibility, nearInfluence );
    }

    float secondVisibility;
    const bool secondValid =
        TrySampleFaceShadow( secondSourceIndex, worldPos, geometricNormal, secondVisibility );
    if( !bestValid )
    {
        return secondValid ? lerp( 1.0f, secondVisibility, nearInfluence ) : 1.0f;
    }
    if( !secondValid )
    {
        return lerp( 1.0f, bestVisibility, nearInfluence );
    }

    const float seamWeight = smoothstep( 0.0f, seamBlendWidth, alignmentGap );
    const float visibility = lerp( max( bestVisibility, secondVisibility ), bestVisibility, seamWeight );
    return lerp( 1.0f, visibility, nearInfluence );
}

//==============================================================================
#endif // GEOM_SHADOW_POINT_HLSL
//==============================================================================
