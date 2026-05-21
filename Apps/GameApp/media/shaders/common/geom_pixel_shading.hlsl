//==============================================================================
//
//  geom_pixel_shading.hlsl
//
//  Final geometry pixel shader composition.
//
//==============================================================================

#ifndef GEOM_PIXEL_SHADING_HLSL
#define GEOM_PIXEL_SHADING_HLSL

GEOM_PIXEL_OUTPUT ShadeGeometryPixel( GEOM_PIXEL_INPUT input )
{
    GEOM_PIXEL_OUTPUT output;
    output.FinalColor  = 0.0f;
    output.Albedo      = 0.0f;
    output.Normal      = 0.0f;
    output.LinearDepth = GeomEncodeLinearDepth( input );
    output.Glow        = 0.0f;

    uint  materialFlags = GeomGetMaterialFlags( input );
    float alphaRef      = AlphaRef;
    float fadeAlpha     = GeomGetFadeAlpha( input );

    if( EnvParams.w > 0.5f )
    {
        output.FinalColor  = float4( 0.0, 0.0, 0.0, 0.0 );
        output.Albedo      = float4( 0.0, 0.0, 0.0, 0.0 );
        output.Normal      = float4( 0.5, 0.5, 1.0, 0.0 );
        output.LinearDepth = GeomEncodeLinearDepth( input );
        output.Glow        = float4( 0.0, 0.0, 0.0, 0.0 );
        return output;
    }

    if( materialFlags & (MATERIAL_FLAG_DISTORTION | MATERIAL_FLAG_DISTORTION_PERPOLY_ENV) )
    {
        return GeomShadeDistortionPixel( input, materialFlags, fadeAlpha );
    }

    GeomDiffuseResult diffuse = GeomEvaluateDiffuse( input, materialFlags, alphaRef );
    GeomApplyEnvironment( diffuse, materialFlags, input.Normal, input.ViewVector );

    float4 baseColor  = diffuse.Color;
    float3 totalLight = GeomComputeLighting( input, materialFlags );
    float3 specular   = GeomComputeSpecular( input, materialFlags, diffuse.Sample.a );
    float4 finalColor = float4( diffuse.Color.rgb * totalLight + specular, diffuse.Color.a );

    if( materialFlags & INSTANCE_FLAG_PROJ_LIGHT )
        finalColor.rgb = ApplyProjLights( finalColor.rgb, input.WorldPos );

    if( materialFlags & INSTANCE_FLAG_PROJ_SHADOW )
        finalColor.rgb = ApplyProjShadows( finalColor.rgb, input.WorldPos );

    finalColor.rgb = saturate( finalColor.rgb );

    output.Glow       = GeomComputeGlow( input, materialFlags, finalColor, diffuse.Sample );
    output.FinalColor = finalColor;
    output.Albedo     = baseColor;
    output.Normal     = float4( input.ViewNormal * 0.5 + 0.5, 0.0 );
    output.LinearDepth= GeomEncodeLinearDepth( input );

    if( fadeAlpha < 1.0f )
    {
        const bool  bFadeByTextureAlpha = (materialFlags & (MATERIAL_FLAG_ALPHA_BLEND | MATERIAL_FLAG_ALPHA_TEST)) != 0;
        const float blendAlpha          = bFadeByTextureAlpha ? saturate( output.FinalColor.a * fadeAlpha ) : fadeAlpha;

        output.FinalColor.a = blendAlpha;
        output.Albedo.a     = blendAlpha;
        output.Normal.a     = blendAlpha;
        output.Glow.a       = saturate( output.Glow.a * fadeAlpha );
    }

    return output;
}

//==============================================================================
#endif // GEOM_PIXEL_SHADING_HLSL
//==============================================================================
