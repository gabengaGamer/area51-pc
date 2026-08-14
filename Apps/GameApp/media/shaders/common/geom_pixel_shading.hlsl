//==============================================================================
//
//  geom_pixel_shading.hlsl
//
//  Final geometry pixel shader composition.
//
//==============================================================================

#ifndef GEOM_PIXEL_SHADING_HLSL
#define GEOM_PIXEL_SHADING_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

GEOM_PIXEL_OUTPUT ShadeGeometryPixel( GEOM_PIXEL_INPUT input, bool isFrontFace )
{
    GEOM_PIXEL_OUTPUT output;
    output.FinalColor  = 0.0f;
    output.NormalDepth = float4( 0.5f, 0.5f, 1.0f, GeomEncodeLinearDepth( input ).r );
    output.Glow        = 0.0f;

    uint  materialFlags = GeomGetMaterialFlags( input );
    float alphaRef      = AlphaRef;
    float fadeAlpha     = GeomGetFadeAlpha( input );

    if( EnvParams.w > 0.5f )
    {
        output.FinalColor  = float4( 0.0f, 0.0f, 0.0f, 0.0f );
        output.NormalDepth = float4( 0.5f, 0.5f, 1.0f, GeomEncodeLinearDepth( input ).r );
        output.Glow        = float4( 0.0f, 0.0f, 0.0f, 0.0f );
        return output;
    }

    input.Normal     = normalize( input.Normal );
    input.ViewNormal = normalize( input.ViewNormal );

    if( ( materialFlags & MATERIAL_FLAG_TWO_SIDED ) && isFrontFace )
    {
        input.Normal     = -input.Normal;
        input.ViewNormal = -input.ViewNormal;
    }

    if( materialFlags & ( MATERIAL_FLAG_DISTORTION | MATERIAL_FLAG_DISTORTION_PERPOLY_ENV ) )
    {
        return GeomShadeDistortionPixel( input, materialFlags, fadeAlpha );
    }

    const float3 worldPositionDx = ddx( input.WorldPos );
    const float3 worldPositionDy = ddy( input.WorldPos );
    float3       geometricNormal = cross( worldPositionDx, worldPositionDy );
    const float  geometricNormalLengthSq = dot( geometricNormal, geometricNormal );
    if( geometricNormalLengthSq > 1e-12f )
    {
        geometricNormal *= rsqrt( geometricNormalLengthSq );
        if( dot( geometricNormal, input.Normal ) < 0.0f )
        {
            geometricNormal = -geometricNormal;
        }
    }
    else
    {
        geometricNormal = input.Normal;
    }

    GeomDiffuseResult diffuse = GeomEvaluateDiffuse( input, materialFlags, alphaRef );
    GeomApplyEnvironment( diffuse, materialFlags, input.Normal, input.ViewVector );

    const GeomLightingResult lighting =
        GeomComputeLighting( input, materialFlags, diffuse.Sample.a, geometricNormal );
    float4 finalColor = float4( diffuse.Color.rgb * lighting.Diffuse + lighting.Specular, diffuse.Color.a );

    if( materialFlags & INSTANCE_FLAG_PROJ_LIGHT )
    {
        finalColor.rgb = ApplyProjLights( finalColor.rgb, input.WorldPos );
    }

    if( materialFlags & INSTANCE_FLAG_PROJ_SHADOW )
    {
        finalColor.rgb = ApplyProjShadows( finalColor.rgb, input.WorldPos );
    }

    finalColor.rgb = saturate( finalColor.rgb );

    output.Glow        = GeomComputeGlow( input, materialFlags, finalColor, diffuse.Sample );
    output.FinalColor  = finalColor;
    output.NormalDepth = float4( input.ViewNormal * 0.5 + 0.5, GeomEncodeLinearDepth( input ).r );

    if( fadeAlpha < 1.0f )
    {
        const bool  bFadeByTextureAlpha = ( materialFlags & ( MATERIAL_FLAG_ALPHA_BLEND | MATERIAL_FLAG_ALPHA_TEST ) ) != 0;
        const float blendAlpha          = bFadeByTextureAlpha ? saturate( output.FinalColor.a * fadeAlpha ) : fadeAlpha;

        output.FinalColor.a = blendAlpha;
        output.Glow.a       = saturate( output.Glow.a * fadeAlpha );
    }

    return output;
}

//==============================================================================
#endif // GEOM_PIXEL_SHADING_HLSL
//==============================================================================
