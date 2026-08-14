//==============================================================================
//
//  geom_pixel_lighting.hlsl
//
//  Lighting, specular and projected light helpers for geometry shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_LIGHTING_HLSL
#define GEOM_PIXEL_LIGHTING_HLSL

//==============================================================================
//  TYPES
//==============================================================================

struct GeomLightingResult
{
    float3 Diffuse;
    float3 Specular;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

float GeomComputeRadialAttenuation( float lightDistance, float lightRadius, float lightFalloff )
{
    const float radius      = max( lightRadius, 1e-4f );
    const float falloff     = saturate( lightFalloff );
    const float innerRadius = radius * ( 1.0f - falloff );
    return 1.0f - saturate( ( lightDistance - innerRadius ) / max( radius - innerRadius, 1e-4f ) );
}

//==============================================================================

bool GeomIsCharFillLight( float4 lightDir )
{
    return lightDir.w >= 1.5f;
}

//==============================================================================

float GeomComputeSpotAttenuation( GEOM_PIXEL_INPUT input, uint lightIndex, float3 lightToPointDir )
{
    float4 lightDir = GeomGetLightDir( input, lightIndex );
    if( GeomIsCharFillLight( lightDir ) || ( lightDir.w < 0.5f ) )
    {
        return 1.0f;
    }

    float3 spotDir = lightDir.xyz;
    const float spotDirLenSq = dot( spotDir, spotDir );
    if( spotDirLenSq <= 1e-8f )
    {
        return 1.0f;
    }

    spotDir *= rsqrt( spotDirLenSq );

    const float coneCos  = dot( spotDir, lightToPointDir );
    float4 lightCone     = GeomGetLightCone( input, lightIndex );
    const float cosInner = lightCone.x;
    const float cosOuter = lightCone.y;
    const float cosRange = max( cosInner - cosOuter, 1e-4f );

    return saturate( ( coneCos - cosOuter ) / cosRange );
}

//==============================================================================

float SampleProjectionAtlas( float2 localUV,
                             float4 atlasRegion,
                             float  atlasLayer,
                             float  maxMip )
{
    const float2 atlasUV = localUV * atlasRegion.xy + atlasRegion.zw;

    uint atlasWidth;
    uint atlasHeight;
    uint atlasLayers;
    uint atlasMipCount;
    txProjectionAtlas.GetDimensions( 0, atlasWidth, atlasHeight, atlasLayers, atlasMipCount );

    const float2 atlasSize = float2( atlasWidth, atlasHeight );
    const float2 dx = ddx( atlasUV ) * atlasSize;
    const float2 dy = ddy( atlasUV ) * atlasSize;
    const float footprint = max( dot( dx, dx ), dot( dy, dy ) );
    const float availableMaxMip = (float)(atlasMipCount - 1u);
    const float clampedMaxMip = min( maxMip, availableMaxMip );
    const float lod = min( clampedMaxMip,
                           max( 0.0f, 0.5f * log2( max( footprint, 1e-8f ) ) ) );

    const float tileSize = exp2( clampedMaxMip );
    const float2 imageSize = atlasRegion.xy * atlasSize + 1.0f;
    const float2 imageMin = atlasRegion.zw * atlasSize - 0.5f;
    const float2 tileMin = imageMin - floor( (tileSize - imageSize) * 0.5f );
    const float2 tileMax = tileMin + tileSize;
    const float2 halfMipTexel = 0.5f * exp2( ceil( lod ) );
    const float2 safeAtlasUV = clamp( atlasUV * atlasSize,
                                      tileMin + halfMipTexel,
                                      tileMax - halfMipTexel ) / atlasSize;

    return txProjectionAtlas.SampleLevel( samProjectionAtlas,
                                          float3( safeAtlasUV,
                                                  min( atlasLayer, (float)(atlasLayers - 1u) ) ),
                                          lod ).r;
}

//==============================================================================

float GeomSampleLightCookie( GEOM_PIXEL_INPUT input,
                             uint             lightIndex,
                             float3           lightToPoint )
{
    const uint encodedLayer = GeomGetLightCookieLayer( input, lightIndex );
    if( encodedLayer == 0u )
    {
        return 1.0f;
    }

    float4 lightDir = GeomGetLightDir( input, lightIndex );
    if( lightDir.w < 0.5f || GeomIsCharFillLight( lightDir ) )
    {
        return 1.0f;
    }

    float3 spotDir = lightDir.xyz;
    const float spotDirLenSq = dot( spotDir, spotDir );
    if( spotDirLenSq <= 1e-8f )
    {
        return 1.0f;
    }

    spotDir *= rsqrt( spotDirLenSq );
    const float distAlong = dot( lightToPoint, spotDir );
    if( distAlong <= 1e-4f )
    {
        return 0.0f;
    }

    const float4 lightCone = GeomGetLightCone( input, lightIndex );
    const float cosOuter = saturate( lightCone.y );
    const float sinOuter = sqrt( saturate( 1.0f - cosOuter * cosOuter ) );
    const float coneRadius = max( distAlong * sinOuter / max( cosOuter, 1e-4f ), 1e-4f );

    const float4 cookieU = GeomGetLightCookieU( input, lightIndex );
    const float4 cookieV = GeomGetLightCookieV( input, lightIndex );
    const float2 uv = float2( dot( lightToPoint, cookieU.xyz ),
                              dot( lightToPoint, cookieV.xyz ) ) /
                      (2.0f * coneRadius) + 0.5f;

    if( any( uv < 0.0f ) || any( uv > 1.0f ) )
    {
        return 0.0f;
    }

    return saturate( SampleProjectionAtlas( uv,
                                             GeomGetLightCookieAtlas( input, lightIndex ),
                                             (float)(encodedLayer - 1u),
                                             GeomGetLightCookieMaxMip( input, lightIndex ) ) );
}

//==============================================================================

float GeomComputeLocalLightAttenuation( GEOM_PIXEL_INPUT input, uint lightIndex, float3 worldPos, out float3 pointToLightDir )
{
    float4 lightVec = GeomGetLightVec( input, lightIndex );
    float4 lightCol = GeomGetLightCol( input, lightIndex );
    float4 lightDir = GeomGetLightDir( input, lightIndex );

    if( GeomIsCharFillLight( lightDir ) )
    {
        const float dirLenSq = dot( lightVec.xyz, lightVec.xyz );
        if( dirLenSq <= 1e-8f )
        {
            pointToLightDir = 0.0f;
            return 0.0f;
        }

        pointToLightDir = -lightVec.xyz * rsqrt( dirLenSq );
        return 1.0f;
    }

    const float3 lightToPoint = worldPos - lightVec.xyz;
    const float3 toLight = -lightToPoint;
    const float  distSq  = dot( toLight, toLight );

    if( distSq <= 1e-8f )
    {
        pointToLightDir = float3( 0.0f, 0.0f, 1.0f );
        return 1.0f;
    }

    const float dist   = sqrt( distSq );
    const float radial = GeomComputeRadialAttenuation( dist, lightVec.w, lightCol.a );
    if( radial <= 0.0f )
    {
        pointToLightDir = 0.0f;
        return 0.0f;
    }

    pointToLightDir = toLight / dist;
    return radial *
           GeomComputeSpotAttenuation( input, lightIndex, -pointToLightDir ) *
           GeomSampleLightCookie( input, lightIndex, lightToPoint );
}

//==============================================================================

float GeomComputeLocalLightShadowVisibility( GEOM_PIXEL_INPUT input,
                                             uint lightIndex,
                                             float3 geometricNormal );

GeomLightingResult GeomComputeLighting( GEOM_PIXEL_INPUT input,
                                        uint materialFlags,
                                        float diffuseAlpha,
                                        float3 geometricNormal )
{
    GeomLightingResult result;
    result.Diffuse  = GeomGetLightAmbCol( input ).rgb;
    result.Specular = 0.0f;

#if GEOM_HAS_VERTEX_COLOR
    result.Diffuse += input.Color.rgb;
#endif

    float specBlend = 0.0f;
    if( materialFlags & MATERIAL_FLAG_ENVIRONMENT )
    {
        float envStrength = 1.0f;

        if( materialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ENV )
        {
            if( materialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
            {
                envStrength = 4.0f * EnvParams.y;
            }
            else
            {
                envStrength = EnvParams.y;
            }
        }

        if( materialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ENV )
        {
            specBlend = diffuseAlpha * envStrength;
        }
        else if( materialFlags & MATERIAL_FLAG_ALPHA_PERPOLY_ENV )
        {
            specBlend = EnvParams.x * envStrength;
        }
    }

    const bool   computeSpecular = specBlend > 0.0f;
    const float3 viewDir        = computeSpecular ? normalize( -input.ViewVector ) : 0.0f;
    const uint   lightCount     = GeomGetLightCount( input );

    [fastopt]
    [loop]
    for( uint i = 0; i < lightCount; ++i )
    {
        float3 L;
        const float atten = GeomComputeLocalLightAttenuation( input, i, input.WorldPos, L );
        if( atten > 0.0f )
        {
            const float ndotl = saturate( dot( input.Normal, L ) );
            if( ndotl > 0.0f )
            {
                const float3 lightColor = GeomGetLightCol( input, i ).rgb;
                const float visibility =
                    GeomComputeLocalLightShadowVisibility( input, i, geometricNormal );
                const float  lightScale = atten * ndotl * visibility;
                result.Diffuse += lightColor * lightScale;

                if( computeSpecular )
                {
                    const float3 H        = normalize( L + viewDir );
                    const float  specTerm = pow( saturate( dot( input.Normal, H ) ), 16.0f );
                    result.Specular += lightColor * ( specTerm * lightScale );
                }
            }
        }
    }

    result.Specular *= specBlend;
    return result;
}

//==============================================================================

float3 ApplyProjLights( float3 color, float3 worldPos )
{
    for( uint i = 0; i < ProjLightCount; ++i )
    {
        const float4 projPos = mul( ProjLightMatrix[i], float4( worldPos, 1.0f ) );
        if( projPos.w <= 0.0f )
        {
            continue;
        }

        const float2 uv = projPos.xy / projPos.w;
        if( any( uv < 0.0f ) || any( uv > 1.0f ) )
        {
            continue;
        }

        const float projection = SampleProjectionAtlas( uv,
                                                         ProjLightAtlas[i],
                                                         ProjLightInfo[i].x,
                                                         ProjLightInfo[i].y );
        color = lerp( color, color * 2.0f, saturate( projection ) );
    }

    return color;
}

//==============================================================================

float3 ApplyProjShadows( float3 color, float3 worldPos )
{
    for( uint i = 0; i < ProjShadowCount; ++i )
    {
        const float4 projPos = mul( ProjShadowMatrix[i], float4( worldPos, 1.0f ) );
        if( projPos.w <= 0.0f )
        {
            continue;
        }

        const float2 uv = projPos.xy / projPos.w;
        if( any( uv < 0.0f ) || any( uv > 1.0f ) )
        {
            continue;
        }

        const float shade = SampleProjectionAtlas( uv,
                                                    ProjShadowAtlas[i],
                                                    ProjShadowInfo[i].x,
                                                    ProjShadowInfo[i].y );
        color *= shade * 2.0f;
    }

    return color;
}

//==============================================================================
#endif // GEOM_PIXEL_LIGHTING_HLSL
//==============================================================================
