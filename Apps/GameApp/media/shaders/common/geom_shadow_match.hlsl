//==============================================================================
//
//  geom_shadow_match.hlsl
//
//  Matching helpers between geometry lights and cached local shadows.
//
//==============================================================================

#ifndef GEOM_SHADOW_MATCH_HLSL
#define GEOM_SHADOW_MATCH_HLSL

float ComputeShadowLightMatchEpsilon( float lightRadius )
{
    return max( 0.05f, lightRadius * 0.002f );
}

//==============================================================================

bool ShadowLightMatchesPosition( float3 a, float3 b, float epsilon )
{
    const float3 delta = a - b;
    return dot( delta, delta ) <= ( epsilon * epsilon );
}

//==============================================================================

bool ShadowLightMatchesDirection( float3 a, float3 b )
{
    const float aLenSq = dot( a, a );
    const float bLenSq = dot( b, b );
    if( ( aLenSq <= 1e-8f ) || ( bLenSq <= 1e-8f ) )
        return true;

    const float3 normA = a * rsqrt( aLenSq );
    const float3 normB = b * rsqrt( bLenSq );
    return dot( normA, normB ) >= 0.999f;
}

//==============================================================================

bool ShadowLightMatchesPoint( GEOM_PIXEL_INPUT input, uint lightIndex, uint shadowLightIndex )
{
    const float4 lightDir = GeomGetLightDir( input, lightIndex );
    if( GeomIsCharFillLight( lightDir ) || ( lightDir.w >= 0.5f ) )
        return false;

    const float4 lightVec     = GeomGetLightVec( input, lightIndex );
    const float4 lightCol     = GeomGetLightCol( input, lightIndex );
    const float  matchEpsilon = ComputeShadowLightMatchEpsilon( lightVec.w );

    if( !ShadowLightMatchesPosition( lightVec.xyz, PointShadowLightPosRadius[shadowLightIndex].xyz, matchEpsilon ) )
        return false;

    if( abs( lightVec.w - PointShadowLightPosRadius[shadowLightIndex].w ) > matchEpsilon )
        return false;

    return abs( lightCol.a - PointShadowLightData[shadowLightIndex].x ) <= 0.01f;
}

//==============================================================================

bool ShadowLightMatchesSpot( GEOM_PIXEL_INPUT input, uint lightIndex, uint sourceIndex )
{
    if( FaceShadowIsPointFace( sourceIndex ) )
        return false;

    const float4 lightDir = GeomGetLightDir( input, lightIndex );
    if( GeomIsCharFillLight( lightDir ) || ( lightDir.w < 0.5f ) )
        return false;

    const float4 lightVec     = GeomGetLightVec( input, lightIndex );
    const float4 lightCol     = GeomGetLightCol( input, lightIndex );
    const float4 lightCone    = GeomGetLightCone( input, lightIndex );
    const float  matchEpsilon = ComputeShadowLightMatchEpsilon( lightVec.w );

    if( !ShadowLightMatchesPosition( lightVec.xyz, FaceShadowLightPosRadius[sourceIndex].xyz, matchEpsilon ) )
        return false;

    if( abs( lightVec.w - FaceShadowLightPosRadius[sourceIndex].w ) > matchEpsilon )
        return false;

    if( abs( lightCol.a - FaceShadowLightDirFalloff[sourceIndex].w ) > 0.01f )
        return false;

    if( abs( lightCone.y - FaceShadowLightData[sourceIndex].x ) > 0.01f )
        return false;

    return ShadowLightMatchesDirection( lightDir.xyz, FaceShadowLightDirFalloff[sourceIndex].xyz );
}

//==============================================================================
#endif // GEOM_SHADOW_MATCH_HLSL
//==============================================================================
