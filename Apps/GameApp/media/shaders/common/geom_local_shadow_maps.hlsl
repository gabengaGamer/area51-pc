//==============================================================================
//
//  geom_local_shadow_maps.hlsl
//
//  Shared local-light shadow helpers.
//
//==============================================================================

#ifndef GEOM_LOCAL_SHADOW_MAPS_HLSL
#define GEOM_LOCAL_SHADOW_MAPS_HLSL

#include "common/geom_shadow_common.hlsl"
#include "common/geom_shadow_spot.hlsl"
#include "common/geom_shadow_point.hlsl"
#include "common/geom_shadow_match.hlsl"

//==============================================================================

float GeomComputeLocalLightShadowVisibility( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    const float4 lightDir = GeomGetLightDir( input, lightIndex );
    if( GeomIsCharFillLight( lightDir ) )
        return 1.0f;

    if( ( FaceShadowCount == 0u ) && ( PointShadowLightCount == 0u ) )
        return 1.0f;

    if( lightDir.w >= 0.5f )
    {
        [fastopt]
        [loop]
        for( uint i = 0; i < MAX_SHADOW_SOURCES; i++ )
        {
            if( i >= FaceShadowCount )
                break;

            if( !ShadowLightMatchesSpot( input, lightIndex, i ) )
                continue;

            return SampleFaceShadowSource( i, input.WorldPos, input.Normal );
        }

        return 1.0f;
    }

    [fastopt]
    [loop]
    for( uint i = 0; i < MAX_SHADOW_LIGHTS; i++ )
    {
        if( i >= PointShadowLightCount )
            break;

        if( !ShadowLightMatchesPoint( input, lightIndex, i ) )
            continue;

        return SamplePointShadowLight( i, input.WorldPos, input.Normal );
    }

    return 1.0f;
}

//==============================================================================
#endif // GEOM_LOCAL_SHADOW_MAPS_HLSL
//==============================================================================
