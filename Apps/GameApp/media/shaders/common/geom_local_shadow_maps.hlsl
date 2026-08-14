//==============================================================================
//
//  geom_local_shadow_maps.hlsl
//
//  Shared local-light shadow helpers.
//
//==============================================================================

#ifndef GEOM_LOCAL_SHADOW_MAPS_HLSL
#define GEOM_LOCAL_SHADOW_MAPS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/geom_shadow_common.hlsl"
#include "common/geom_shadow_spot.hlsl"
#include "common/geom_shadow_point.hlsl"

//==============================================================================
//  FUNCTIONS
//==============================================================================

float GeomComputeLocalLightShadowVisibility( GEOM_PIXEL_INPUT input,
                                             uint lightIndex,
                                             float3 geometricNormal )
{
    if( ( GeomGetMaterialFlags( input ) & INSTANCE_FLAG_RECEIVE_LOCAL_SHADOW ) == 0u )
    {
        return 1.0f;
    }

    const float4 lightDir = GeomGetLightDir( input, lightIndex );
    if( GeomIsCharFillLight( lightDir ) )
    {
        return 1.0f;
    }

    const uint shadowIndex = GeomGetLightShadowIndex( input, lightIndex );
    if( shadowIndex == 0xFFFFFFFFu )
    {
        return 1.0f;
    }

    if( lightDir.w >= 0.5f )
    {
        if( ( shadowIndex >= GetFaceShadowCount() ) || FaceShadowIsPointFace( shadowIndex ) )
        {
            return 1.0f;
        }

        return SampleFaceShadowSource( shadowIndex, input.WorldPos, input.Normal, geometricNormal );
    }

    if( shadowIndex >= GetPointShadowLightCount() )
    {
        return 1.0f;
    }

    return SamplePointShadowLight( shadowIndex, input.WorldPos, input.Normal, geometricNormal );
}

//==============================================================================
#endif // GEOM_LOCAL_SHADOW_MAPS_HLSL
//==============================================================================
