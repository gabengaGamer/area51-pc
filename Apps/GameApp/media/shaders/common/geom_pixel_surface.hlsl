//==============================================================================
//
//  geom_pixel_surface.hlsl
//
//  Surface sampling, environment and glow helpers for geometry shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_SURFACE_HLSL
#define GEOM_PIXEL_SURFACE_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

GeomDiffuseResult GeomEvaluateDiffuse( GEOM_PIXEL_INPUT input, uint materialFlags, float alphaRef )
{
    GeomDiffuseResult result;
    result.Sample = txDiffuse.Sample( samDiffuse, input.UV );
    result.Color  = result.Sample;

    if( materialFlags & MATERIAL_FLAG_DETAIL )
    {
        float  detailScale = max( UVAnim.z, 1.0f );
        float4 detailColor = txDetail.Sample( samDetail, input.UV * detailScale );
        result.Color *= detailColor * 2.0f;
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
        {
            discard;
        }
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
        float3 envColor    = 0.0f;

        if( materialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
        {
            float3 cubeDir = normalize( viewVector );
            envColor = txEnvironmentCube.Sample( samEnvironmentCube, cubeDir ).rgb;
        }
        else
        {
            float3 envVector;

            if( materialFlags & MATERIAL_FLAG_ENV_VIEWSPACE )
            {
                envVector = mul( (float3x3)View, worldNormal );
            }
            else
            {
                envVector = worldNormal;
            }

            envVector = normalize( envVector );

            float2 envUV = envVector.xy * 0.5f + 0.5f;
            envColor = txEnvironment.Sample( samEnvironment, envUV ).rgb;
        }

        float envStrength = 1.0f;
        if( materialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
        {
            envStrength = EnvParams.y;
        }

        float envBlend = 0.0f;

        if( materialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ENV )
        {
            envBlend = diffuse.Sample.a * envStrength;
        }
        else if( materialFlags & MATERIAL_FLAG_ALPHA_PERPOLY_ENV )
        {
            envBlend = EnvParams.x * envStrength;
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

float4 GeomComputeGlow( GEOM_PIXEL_INPUT input,
                        uint materialFlags,
                        inout float4 finalColor,
                        float4 diffuseSample )
{
    float4 glow = float4( 0.0f, 0.0f, 0.0f, 0.0f );
    const float4 ambient = GeomGetLightAmbCol( input );

    if( materialFlags & INSTANCE_FLAG_GLOWING )
    {
        const float3 forcedGlowColor = saturate( ambient.rgb * diffuseSample.rgb * 2.0f );
        finalColor.rgb = forcedGlowColor;
        glow.rgb = forcedGlowColor;
        glow.a   = 1.0f;
        return glow;
    }

    const bool bUseDiffuse           = ( materialFlags & MATERIAL_FLAG_ILLUM_USE_DIFFUSE ) != 0;
    const bool bDiffusePerPixelIllum = ( materialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ILLUM ) != 0;
    const bool bAlphaPerPixelIllum   = ( materialFlags & MATERIAL_FLAG_ALPHA_PERPIXEL_ILLUM ) != 0;
    const bool bAlphaPerPolyIllum    = ( materialFlags & MATERIAL_FLAG_ALPHA_PERPOLY_ILLUM ) != 0;

    if( bAlphaPerPixelIllum )
    {
        glow = diffuseSample;

        if( !bUseDiffuse )
        {
            finalColor.rgb = diffuseSample.rgb;
        }
    }
    else if( bDiffusePerPixelIllum || bAlphaPerPolyIllum )
    {
        const float intensity = bAlphaPerPolyIllum ? saturate( EnvParams.x ) : saturate( diffuseSample.a );
        const float3 illumRgb = lerp( finalColor.rgb, diffuseSample.rgb, intensity );

        glow.rgb = illumRgb;
        glow.a   = 1.0f;

        if( !bUseDiffuse )
        {
            finalColor.rgb = illumRgb;
        }
    }

    return glow;
}

//==============================================================================
#endif // GEOM_PIXEL_SURFACE_HLSL
//==============================================================================
