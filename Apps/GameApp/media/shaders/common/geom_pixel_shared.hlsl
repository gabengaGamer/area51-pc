//==============================================================================
//
//  geom_pixel_shared.hlsl
//
//  Shared shading routine for rigid and skinned geometry.
//
//==============================================================================

#ifndef GEOM_PIXEL_SHARED_HLSL
#define GEOM_PIXEL_SHARED_HLSL

struct GeomDiffuseResult
{
    float4 Color;
    float4 Sample;
    float  Alpha;
};

//==============================================================================

GeomDiffuseResult GeomEvaluateDiffuse( GEOM_PIXEL_INPUT input, uint materialFlags, float alphaRef )
{
    GeomDiffuseResult result;
    result.Sample = txDiffuse.Sample( samLinear, input.UV );
    result.Color  = result.Sample;

    if( materialFlags & MATERIAL_FLAG_DETAIL )
    {
        float4 detailColor = txDetail.Sample( samLinear, input.UV * 4.0 );
        result.Color *= detailColor * 2.0;
    }

#if GEOM_HAS_VERTEX_COLOR
    if( materialFlags & MATERIAL_FLAG_VERTEX_COLOR )
    {
        result.Color.a *= input.Color.a;
    }
#endif

    if( materialFlags & MATERIAL_FLAG_ALPHA_TEST )
    {
        if( result.Sample.a < alphaRef )
            discard;
    }

    result.Alpha = result.Color.a;
    return result;
}

//==============================================================================

void GeomApplyEnvironment( inout GeomDiffuseResult diffuse, uint materialFlags, float3 normal, float3 viewVector )
{
    float alphaValue = diffuse.Alpha;

    if( materialFlags & MATERIAL_FLAG_ENVIRONMENT )
    {
        float3 worldNormal = normalize( normal );
        float3 envColor    = 0.0;

        if( materialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
        {
            float3 viewToSurface = normalize( viewVector );
            float3 reflectionVec = normalize( reflect( viewToSurface, worldNormal ) );

            if( materialFlags & MATERIAL_FLAG_ENV_VIEWSPACE )
            {
                reflectionVec = normalize( mul( (float3x3)View, reflectionVec ) );
            }

            envColor = txEnvironmentCube.Sample( samLinear, reflectionVec ).rgb;
        }
        else
        {
            float3 envVector = worldNormal;

            if( materialFlags & MATERIAL_FLAG_ENV_VIEWSPACE )
            {
                envVector = mul( (float3x3)View, envVector );
            }

            envVector = normalize( envVector );

            float2 envUV = envVector.xy * 0.5f + 0.5f;
            envColor = txEnvironment.Sample( samLinear, envUV ).rgb;
        }

        float envStrength = 1.0f;
        if( materialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
        {
            envStrength = saturate( EnvParams.y );
        }

        float envBlend = 0.0f;

        if( materialFlags & MATERIAL_FLAG_PERPIXEL_ENV )
        {
            envBlend = saturate( diffuse.Sample.a ) * envStrength;
        }
        else if( materialFlags & MATERIAL_FLAG_PERPOLY_ENV )
        {
            envBlend = saturate( EnvParams.x ) * envStrength;
        }

        if( envBlend > 0.0f )
        {
            if( materialFlags & MATERIAL_FLAG_ADDITIVE )
            {
                diffuse.Color.rgb = saturate( diffuse.Color.rgb + envColor * envBlend );
            }
            else
            {
                diffuse.Color.rgb = lerp( diffuse.Color.rgb, envColor, envBlend );
            }
        }
    }

    diffuse.Color.a = alphaValue;
}

//==============================================================================

float3 GeomComputeLighting( GEOM_PIXEL_INPUT input, uint materialFlags )
{
#ifdef GEOM_USE_SKIN_LIGHTING
    float3 dynLight = float3( 0.0, 0.0, 0.0 );
    for( uint i = 0; i < LightCount; i++ )
    {
        float ndotl = saturate( dot( input.Normal, -LightVec[i].xyz ) );
        dynLight += LightCol[i].rgb * ndotl;
    }
    return LightAmbCol.rgb + dynLight;
#else
    float3 perPixelLight = float3( 0.0, 0.0, 0.0 );
    for( uint i = 0; i < LightCount; i++ )
    {
        float3 L     = LightVec[i].xyz - input.WorldPos;
        float  dist  = length( L );
        float  atten = saturate( 1.0 - dist / max( LightVec[i].w, 1e-4f ) );
        float3 Ldir  = L / max( dist, 1e-4f );
        float  NdotL = max( dot( input.Normal, Ldir ) + 0.5f, 0.0f );
        perPixelLight += LightCol[i].rgb * NdotL * atten;
    }

    float3 totalLight = LightAmbCol.rgb + perPixelLight;
#if GEOM_HAS_VERTEX_COLOR
    if( materialFlags & MATERIAL_FLAG_VERTEX_COLOR )
    {
        totalLight += input.Color.rgb;
    }
#endif
    return totalLight;
#endif
}

//==============================================================================

float4 GeomComputeGlow( GEOM_PIXEL_INPUT input,
                        uint materialFlags,
                        inout float4 finalColor,
                        float4 diffuseColor )
{
    float4 glow = float4( 0.0, 0.0, 0.0, 0.0 );

    if( materialFlags & MATERIAL_FLAG_PERPIXEL_ILLUM )
    {
        float4 texColor = txDiffuse.Sample( samLinear, input.UV );
        float  intensity = texColor.a;
        float3 emissive  = texColor.rgb;
        float  emissiveStrength = max( max( emissive.r, emissive.g ), emissive.b );
        float  glowMask         = saturate( max( intensity, emissiveStrength ) );

        glow.rgb += emissive;
        glow.a    = max( glow.a, glowMask );

        finalColor.rgb = lerp( finalColor.rgb, texColor.rgb, intensity );
        finalColor.a   = intensity;
    }

    if( materialFlags & MATERIAL_FLAG_PERPOLY_ILLUM )
    {
        float  intensity = diffuseColor.a;
        float3 emissive  = diffuseColor.rgb;
        float  emissiveStrength = max( max( emissive.r, emissive.g ), emissive.b );
        float  glowMask         = saturate( max( intensity, emissiveStrength ) );

        glow.rgb += emissive;
        glow.a    = max( glow.a, glowMask );
    }

    return glow;
}

//==============================================================================

float3 ApplyProjLights(float3 color, float3 worldPos)
{
    for(uint i = 0; i < ProjLightCount; i++)
    {
        float4 projPos = mul(ProjLightMatrix[i], float4(worldPos, 1.0));
        float3 uvw = projPos.xyz / projPos.w;
        if(uvw.x >= 0.0 && uvw.x <= 1.0 &&
           uvw.y >= 0.0 && uvw.y <= 1.0 &&
           uvw.z >= 0.0 && uvw.z <= 1.0)
        {
            float4 proj = txProjLight[i].Sample(samLinear, uvw.xy);
            float3 lit = proj.rgb * 2.0;
            color = lerp(color, color * lit, proj.a);
        }
    }
    return color;
}

//==============================================================================

float3 ApplyProjShadows(float3 color, float3 worldPos)
{
    for(uint i = 0; i < ProjShadowCount; i++)
    {
        float4 projPos = mul(ProjShadowMatrix[i], float4(worldPos, 1.0));
        float3 uvw = projPos.xyz / projPos.w;
        if(uvw.x >= 0.0 && uvw.x <= 1.0 &&
           uvw.y >= 0.0 && uvw.y <= 1.0 &&
           uvw.z >= 0.0 && uvw.z <= 1.0)
        {
            float shade = txProjShadow[i].Sample(samLinear, uvw.xy).b;
            color *= shade * 2.0;
        }
    }
    return color;
}

//==============================================================================

GEOM_PIXEL_OUTPUT ShadeGeometryPixel( GEOM_PIXEL_INPUT input )
{
    GEOM_PIXEL_OUTPUT output;

    uint  materialFlags = (uint)MaterialParams.x;
    float alphaRef      = MaterialParams.y;

    GeomDiffuseResult diffuse = GeomEvaluateDiffuse( input, materialFlags, alphaRef );
    GeomApplyEnvironment( diffuse, materialFlags, input.Normal, input.ViewVector );

    float4 baseColor = diffuse.Color;
    float3 totalLight = GeomComputeLighting( input, materialFlags );
    float4 finalColor = float4( diffuse.Color.rgb * totalLight, diffuse.Color.a );

    if( materialFlags & MATERIAL_FLAG_PROJ_LIGHT )
    {
        finalColor.rgb = ApplyProjLights( finalColor.rgb, input.WorldPos );
    }

    if( materialFlags & MATERIAL_FLAG_PROJ_SHADOW )
    {
        finalColor.rgb = ApplyProjShadows( finalColor.rgb, input.WorldPos );
    }

    output.Glow = GeomComputeGlow( input, materialFlags, finalColor, diffuse.Color );

    output.FinalColor = finalColor;
    output.Albedo     = baseColor;
    output.Normal     = float4( input.ViewNormal * 0.5 + 0.5, 0.0 );
    output.DepthInfo  = input.LinearDepth;

    return output;
}

#endif // GEOM_PIXEL_SHARED_HLSL