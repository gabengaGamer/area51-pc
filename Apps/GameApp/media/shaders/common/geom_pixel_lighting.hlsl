//==============================================================================
//
//  geom_pixel_lighting.hlsl
//
//  Lighting, specular and projected light helpers for geometry shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_LIGHTING_HLSL
#define GEOM_PIXEL_LIGHTING_HLSL

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
        return 1.0f;

    float3 spotDir = lightDir.xyz;
    const float spotDirLenSq = dot( spotDir, spotDir );
    if( spotDirLenSq <= 1e-8f )
        return 1.0f;

    spotDir *= rsqrt( spotDirLenSq );

    const float coneCos  = dot( spotDir, lightToPointDir );
    float4 lightCone     = GeomGetLightCone( input, lightIndex );
    const float cosInner = lightCone.x;
    const float cosOuter = lightCone.y;
    const float cosRange = max( cosInner - cosOuter, 1e-4f );

    return saturate( ( coneCos - cosOuter ) / cosRange );
}

//==============================================================================

float4 GeomSampleLightCookieTexture( uint cookieSlot, float2 uv )
{
    if( cookieSlot == 1u ) return txLightCookie[0].Sample( samLinear, uv );
    if( cookieSlot == 2u ) return txLightCookie[1].Sample( samLinear, uv );
    if( cookieSlot == 3u ) return txLightCookie[2].Sample( samLinear, uv );
    return txLightCookie[3].Sample( samLinear, uv );
}

//==============================================================================

float GeomSampleLightCookie( GEOM_PIXEL_INPUT input,
                             uint             lightIndex,
                             float3           lightToPoint )
{
    float4 cookieU = GeomGetLightCookieU( input, lightIndex );
    const uint cookieSlot = (uint)cookieU.w;
    if( cookieSlot == 0u || cookieSlot > (uint)MAX_GEOM_LIGHTS )
        return 1.0f;

    float4 lightDir = GeomGetLightDir( input, lightIndex );
    if( lightDir.w < 0.5f || GeomIsCharFillLight( lightDir ) )
        return 1.0f;

    float3 spotDir = lightDir.xyz;
    const float spotDirLenSq = dot( spotDir, spotDir );
    if( spotDirLenSq <= 1e-8f )
        return 1.0f;

    spotDir *= rsqrt( spotDirLenSq );
    const float distAlong = dot( lightToPoint, spotDir );
    if( distAlong <= 1e-4f )
        return 0.0f;

    float4 lightCone = GeomGetLightCone( input, lightIndex );
    const float cosOuter = saturate( lightCone.y );
    const float sinOuter = sqrt( saturate( 1.0f - cosOuter * cosOuter ) );
    const float coneRadius = max( distAlong * sinOuter / max( cosOuter, 1e-4f ), 1e-4f );

    float4 cookieV = GeomGetLightCookieV( input, lightIndex );
    float2 uv = float2( dot( lightToPoint, cookieU.xyz ),
                        dot( lightToPoint, cookieV.xyz ) ) / ( 2.0f * coneRadius ) + 0.5f;

    if( uv.x < 0.0f || uv.x > 1.0f ||
        uv.y < 0.0f || uv.y > 1.0f )
    {
        return 0.0f;
    }

    const float4 cookie = GeomSampleLightCookieTexture( cookieSlot, uv );
    const float  gray   = dot( cookie.rgb, float3( 0.299f, 0.587f, 0.114f ) );
    return saturate( gray * cookie.a );
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

float GeomComputeLocalLightShadowVisibility( GEOM_PIXEL_INPUT input, uint lightIndex );

//==============================================================================

float3 GeomComputeLighting( GEOM_PIXEL_INPUT input, uint materialFlags )
{
    float3 perPixelLight  = float3( 0.0, 0.0, 0.0 );
    const uint lightCount = GeomGetLightCount( input );

    [fastopt]
    [loop]
    for( uint i = 0; i < lightCount; i++ )
    {
        float3 L;
        float  atten = GeomComputeLocalLightAttenuation( input, i, input.WorldPos, L );
        if( atten > 0.0f )
        {
            float ndotl = saturate( dot( input.Normal, L ) );
            if( ndotl > 0.0f )
            {
                const float visibility = GeomComputeLocalLightShadowVisibility( input, i );
                perPixelLight += GeomGetLightCol( input, i ).rgb * ( atten * ndotl * visibility );
            }
        }
    }

    float3 totalLight = GeomGetLightAmbCol( input ).rgb + perPixelLight;

#if GEOM_HAS_VERTEX_COLOR
    if( materialFlags & MATERIAL_FLAG_VERTEX_COLOR )
    {
        totalLight += input.Color.rgb;
    }
#endif

    return totalLight;
}

//==============================================================================

float3 GeomComputeSpecular( GEOM_PIXEL_INPUT input, uint materialFlags, float diffuseAlpha )
{
    float  specBlend = 0.0f;
    float3 specular  = float3( 0.0, 0.0, 0.0 );

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
            specBlend = diffuseAlpha * envStrength;
        else if( materialFlags & MATERIAL_FLAG_ALPHA_PERPOLY_ENV )
            specBlend = EnvParams.x * envStrength;

        float  specPower = 16.0f;
        float3 viewDir   = normalize( -input.ViewVector );
        const uint lightCount = GeomGetLightCount( input );

        [fastopt]
        [loop]
        for( uint i = 0; i < lightCount; i++ )
        {
            float3 L;
            float  atten = GeomComputeLocalLightAttenuation( input, i, input.WorldPos, L );
            if( atten > 0.0f )
            {
                float ndotl = saturate( dot( input.Normal, L ) );
                if( ndotl > 0.0f )
                {
                    float3 H = normalize( L + viewDir );
                    float  specTerm = pow( saturate( dot( input.Normal, H ) ), specPower ) * ndotl;
                    const float visibility = GeomComputeLocalLightShadowVisibility( input, i );
                    specular += GeomGetLightCol( input, i ).rgb * specTerm * atten * visibility;
                }
            }
        }
    }

    return specular * specBlend;
}

//==============================================================================

float3 ApplyProjLights( float3 color, float3 worldPos )
{
    for( uint i = 0; i < ProjLightCount; i++ )
    {
        float4 projPos = mul( ProjLightMatrix[i], float4( worldPos, 1.0 ) );
        if( projPos.w > 0.0 )
        {
            float2 uv = projPos.xy / projPos.w;
            if( uv.x >= 0.0 && uv.x <= 1.0 &&
                uv.y >= 0.0 && uv.y <= 1.0 )
            {
                float proj = txProjLight[i].Sample( samLinear, uv ).b;
                color = lerp( color, color * 2.0, saturate( proj ) );
            }
        }
    }

    return color;
}

//==============================================================================

float3 ApplyProjShadows( float3 color, float3 worldPos )
{
    for( uint i = 0; i < ProjShadowCount; i++ )
    {
        float4 projPos = mul( ProjShadowMatrix[i], float4( worldPos, 1.0 ) );
        if( projPos.w > 0.0 )
        {
            float2 uv = projPos.xy / projPos.w;
            if( uv.x >= 0.0 && uv.x <= 1.0 &&
                uv.y >= 0.0 && uv.y <= 1.0 )
            {
                float shade = txProjShadow[i].Sample( samLinear, uv ).b;
                color *= shade * 2.0;
            }
        }
    }

    return color;
}

//==============================================================================
#endif // GEOM_PIXEL_LIGHTING_HLSL
//==============================================================================
