//==============================================================================
//
//  geom_shadow_common.hlsl
//
//  Shared local-shadow atlas sampling and projection helpers.
//
//==============================================================================

#ifndef GEOM_SHADOW_COMMON_HLSL
#define GEOM_SHADOW_COMMON_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float GetFaceShadowResolution( uint sourceIndex )
{
    const float  depthAtlasTexelSize = GetShadowParams().x;
    const float  sampleAtlasTexelSize = GetShadowFilterParams().x;
    const float4 atlasRect = GetFaceShadowAtlasRect( sourceIndex );
    const float  normalizedWidth = atlasRect.z - atlasRect.x + sampleAtlasTexelSize;
    return max( normalizedWidth / max( depthAtlasTexelSize, 1e-8f ), 1.0f );
}

//==============================================================================

float ComputeFaceShadowWorldTexelSize( uint sourceIndex, float3 worldPos )
{
    const float4 lightPosRadius  = GetFaceShadowLightPosRadius( sourceIndex );
    const float4 lightDirFalloff = GetFaceShadowLightDirFalloff( sourceIndex );
    const float4 lightData       = GetFaceShadowLightData( sourceIndex );
    const float3 faceDirection   = normalize( lightDirFalloff.xyz );
    const float  faceDepth       = max( dot( worldPos - lightPosRadius.xyz, faceDirection ), 0.0f );
    const float  resolution      = GetFaceShadowResolution( sourceIndex );

    float tanHalfFov;
    if( lightData.w > 0.5f )
    {
        tanHalfFov = 1.0f + GetShadowParams().z / resolution;
    }
    else
    {
        const float cosHalfFov = clamp( lightData.x, 1e-4f, 1.0f );
        tanHalfFov = sqrt( saturate( 1.0f - cosHalfFov * cosHalfFov ) ) / cosHalfFov;
    }

    return ( 2.0f * faceDepth * tanHalfFov ) / resolution;
}

//==============================================================================

float3 ComputeNormalBiasedShadowWorldPos( uint sourceIndex, float3 worldPos, float3 geometricNormal )
{
    const float3 lightPos        = GetFaceShadowLightPosRadius( sourceIndex ).xyz;
    const float3 pointToLightDir = normalize( lightPos - worldPos );
    const float3 normal          = normalize( geometricNormal );
    const float  normalCos       = saturate( dot( normal, pointToLightDir ) );
    const float  normalSin       = sqrt( saturate( 1.0f - normalCos * normalCos ) );
    const float  worldTexelSize  = ComputeFaceShadowWorldTexelSize( sourceIndex, worldPos );
    const float4 shadowParams    = GetShadowParams();

    return worldPos + normal * ( worldTexelSize * shadowParams.y * normalSin );
}

//==============================================================================

bool ProjectFaceShadowSource( uint sourceIndex,
                              float3 shadowWorldPos,
                              out float3 shadowUVW )
{
    shadowUVW = 0.0f;

    const float4 shadowPos = mul( FaceShadowMatrices[sourceIndex], float4( shadowWorldPos, 1.0f ) );
    if( shadowPos.w <= 0.0f )
    {
        return false;
    }

    shadowUVW.xy = shadowPos.xy / shadowPos.w;
    shadowUVW.z  = shadowPos.z / shadowPos.w;

    const float  halfTexel = GetShadowFilterParams().x * 0.5f;
    const float4 atlasRect = GetFaceShadowAtlasRect( sourceIndex );
    if( shadowUVW.x < atlasRect.x - halfTexel || shadowUVW.x > atlasRect.z + halfTexel ||
        shadowUVW.y < atlasRect.y - halfTexel || shadowUVW.y > atlasRect.w + halfTexel ||
        shadowUVW.z < 0.0f || shadowUVW.z > 1.0f )
    {
        return false;
    }

    return true;
}

//==============================================================================

float2 WarpEVSMDepth( float depth, float2 exponents )
{
    const float warpedDepth = depth * 2.0f - 1.0f;
    return float2( exp( exponents.x * warpedDepth ), -exp( -exponents.y * warpedDepth ) );
}

//==============================================================================

float ComputeEVSMChebyshevBound( float2 moments, float receiverDepth, float minimumVariance )
{
    if( receiverDepth <= moments.x )
    {
        return 1.0f;
    }

    const float variance = max( moments.y - moments.x * moments.x, minimumVariance );
    const float depthDelta = receiverDepth - moments.x;
    return variance / ( variance + depthDelta * depthDelta );
}

//==============================================================================

bool FaceShadowIsPointFace( uint sourceIndex )
{
    static const float kFaceShadowSourceTypePointFace = 1.0f;
    return GetFaceShadowLightData( sourceIndex ).w == kFaceShadowSourceTypePointFace;
}

//==============================================================================

float LinearizeFaceShadowDepth( uint sourceIndex, float depth )
{
    const float2 projectionNearFar = GetFaceShadowProjectionData( sourceIndex ).xy;
    const float  nearZ             = max( projectionNearFar.x, 1.0e-4f );
    const float  farZ              = max( projectionNearFar.y, nearZ + 1.0e-4f );
    const float  denominator       = max( farZ - depth * ( farZ - nearZ ), 1.0e-4f );
    return ( nearZ * farZ ) / denominator;
}

//==============================================================================

float4 GetFaceShadowDepthClampRect( uint sourceIndex )
{
    const float  depthAtlasTexelSize  = GetShadowParams().x;
    const float  sampleAtlasTexelSize = GetShadowFilterParams().x;
    const float  insetDifference      = max( sampleAtlasTexelSize - depthAtlasTexelSize, 0.0f ) * 0.5f;
    const float4 sampleRect           = GetFaceShadowAtlasRect( sourceIndex );
    return sampleRect + float4( -insetDifference, -insetDifference, insetDifference, insetDifference );
}

//==============================================================================

float ApplyPointShadowContactHardening( uint sourceIndex,
                                        float3 shadowWorldPos,
                                        float3 shadowUVW,
                                        float evsmVisibility )
{
    const float4 depthClampRect = GetFaceShadowDepthClampRect( sourceIndex );
    const float blockerDepth = txFaceShadowDepthAtlas.SampleLevel(
        samFaceShadowDepth, clamp( shadowUVW.xy, depthClampRect.xy, depthClampRect.zw ), 0.0f );

    if( shadowUVW.z <= blockerDepth )
    {
        return evsmVisibility;
    }

    const float receiverDistance = LinearizeFaceShadowDepth( sourceIndex, shadowUVW.z );
    const float blockerDistance  = LinearizeFaceShadowDepth( sourceIndex, blockerDepth );
    const float blockerSeparation = max( receiverDistance - blockerDistance, 0.0f );

    const float depthAtlasTexelSize  = max( GetShadowParams().x, 1.0e-8f );
    const float sampleAtlasTexelSize = GetShadowFilterParams().x;
    const float momentToDepthScale   = sampleAtlasTexelSize / depthAtlasTexelSize;
    const float filterRadiusTexels   = GetShadowContactParams().x * momentToDepthScale;

    const float filterWorldRadius =
        ComputeFaceShadowWorldTexelSize( sourceIndex, shadowWorldPos ) * filterRadiusTexels;
    const float contactWeight =
        smoothstep( 0.0f, max( filterWorldRadius, 1.0e-4f ), blockerSeparation );
    return evsmVisibility * contactWeight;
}

//==============================================================================

float SampleFaceShadowEVSM( uint sourceIndex, float3 shadowWorldPos, float3 shadowUVW )
{
    static const float kMinimumVariance = 1.0e-5f;

    const float4 atlasRect = GetFaceShadowAtlasRect( sourceIndex );
    const float4 evsmParams = GetShadowFilterParams();
    const float2 exponents = evsmParams.yz;
    const float4 moments = txFaceShadowAtlas.SampleLevel(
        samFaceShadow, clamp( shadowUVW.xy, atlasRect.xy, atlasRect.zw ), 0.0f );
    float receiverDepth = shadowUVW.z;
    if( FaceShadowIsPointFace( sourceIndex ) )
    {
        const float4 lightPosRadius = GetFaceShadowLightPosRadius( sourceIndex );
        receiverDepth =
            saturate( length( shadowWorldPos - lightPosRadius.xyz ) / max( lightPosRadius.w, 1.0e-4f ) );
    }
    const float2 warpedDepth = WarpEVSMDepth( receiverDepth, exponents );
    const float2 depthScale = exponents * abs( warpedDepth );
    const float2 minimumVariance = kMinimumVariance * depthScale * depthScale;
    const float positiveVisibility =
        ComputeEVSMChebyshevBound( moments.xy, warpedDepth.x, minimumVariance.x );
    const float negativeVisibility =
        ComputeEVSMChebyshevBound( moments.zw, warpedDepth.y, minimumVariance.y );
    const float visibility = min( positiveVisibility, negativeVisibility );
    const float evsmVisibility =
        saturate( ( visibility - evsmParams.w ) / max( 1.0f - evsmParams.w, 1.0e-4f ) );
    if( !FaceShadowIsPointFace( sourceIndex ) )
    {
        return evsmVisibility;
    }

    return ApplyPointShadowContactHardening( sourceIndex, shadowWorldPos, shadowUVW, evsmVisibility );
}

//==============================================================================

float SampleFaceShadowHard( uint sourceIndex, float3 shadowUVW )
{
    const float4 atlasRect = GetFaceShadowAtlasRect( sourceIndex );
    const float shadowDepth = txFaceShadowAtlas.SampleLevel(
        samFaceShadow, clamp( shadowUVW.xy, atlasRect.xy, atlasRect.zw ), 0.0f ).x;
    return shadowUVW.z <= shadowDepth ? 1.0f : 0.0f;
}

//==============================================================================

bool TrySampleFaceShadow( uint sourceIndex,
                          float3 worldPos,
                          float3 geometricNormal,
                          out float visibility )
{
    const float3 biasedWorldPos = ComputeNormalBiasedShadowWorldPos( sourceIndex, worldPos, geometricNormal );

    float3 shadowUVW;
    if( !ProjectFaceShadowSource( sourceIndex, biasedWorldPos, shadowUVW ) )
    {
        visibility = 1.0f;
        return false;
    }

    if( GetShadowParams().w > 0.5f )
    {
        visibility = SampleFaceShadowEVSM( sourceIndex, biasedWorldPos, shadowUVW );
    }
    else
    {
        visibility = SampleFaceShadowHard( sourceIndex, shadowUVW );
    }
    return true;
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
