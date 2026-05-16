//==============================================================================
//
//  geom_shadow_common.hlsl
//
//  Shared local-shadow atlas sampling and projection helpers.
//
//==============================================================================

#ifndef GEOM_SHADOW_COMMON_HLSL
#define GEOM_SHADOW_COMMON_HLSL

float SampleFaceShadowAtlas( float3 shadowUVW )
{
    if( shadowUVW.x < 0.0 || shadowUVW.x > 1.0 ||
        shadowUVW.y < 0.0 || shadowUVW.y > 1.0 ||
        shadowUVW.z < 0.0 || shadowUVW.z > 1.0 )
    {
        return 1.0;
    }

    const float depth       = saturate( shadowUVW.z );
    const float shadowDepth = txFaceShadowAtlas.SampleLevel( samFaceShadow, shadowUVW.xy, 0.0f );
    return ( depth <= shadowDepth ) ? 1.0f : 0.0f;
}

//==============================================================================

bool ComputeFaceShadowUVW( uint sourceIndex, float3 shadowWorldPos, out float3 shadowUVW )
{
    shadowUVW = 0.0f;

    float4 shadowPos = mul( FaceShadowMatrix[sourceIndex], float4( shadowWorldPos, 1.0 ) );
    if( shadowPos.w <= 0.0f )
        return false;

    shadowUVW.xy = shadowPos.xy / shadowPos.w;
    shadowUVW.z  = shadowPos.z / shadowPos.w;

    return true;
}

//==============================================================================

bool ProjectFaceShadowSource( uint sourceIndex, float3 shadowWorldPos, out float3 shadowUVW )
{
    if( !ComputeFaceShadowUVW( sourceIndex, shadowWorldPos, shadowUVW ) )
        return false;

    if( shadowUVW.x < 0.0f || shadowUVW.x > 1.0f ||
        shadowUVW.y < 0.0f || shadowUVW.y > 1.0f ||
        shadowUVW.z < 0.0f || shadowUVW.z > 1.0f )
    {
        return false;
    }

    return true;
}

//==============================================================================

float SampleFaceShadowAtlasRaw( uint sourceIndex, float3 shadowWorldPos )
{
    float3 shadowUVW;
    if( !ProjectFaceShadowSource( sourceIndex, shadowWorldPos, shadowUVW ) )
        return 1.0f;

    return SampleFaceShadowAtlas( shadowUVW );
}

//==============================================================================

bool FaceShadowIsPointFace( uint sourceIndex )
{
    static const float kFaceShadowSourceTypePointFace = 1.0f;
    return FaceShadowLightData[sourceIndex].w == kFaceShadowSourceTypePointFace;
}

//==============================================================================

float ComputeShadowNearInfluence( float lightDistance, float nearZ, float lightRadius )
{
    const float nearFadeRange = max( nearZ, 0.05f );
    return saturate( ( lightDistance - nearZ ) / nearFadeRange );
}

//==============================================================================

#endif // GEOM_SHADOW_COMMON_HLSL
//==============================================================================
