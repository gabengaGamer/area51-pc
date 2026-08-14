//==============================================================================
//
//  geom_pixel_distortion.hlsl
//
//  Distortion helpers for geometry shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_DISTORTION_HLSL
#define GEOM_PIXEL_DISTORTION_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float3 GeomGetDistortionViewNormal( float3 worldNormal )
{
    float3 distortionViewNormal = mul( (float3x3)DistortionNormalMatrix, normalize( worldNormal ) );
    return normalize( distortionViewNormal );
}

//==============================================================================

float2 GeomGetDistortionSceneUV( GEOM_PIXEL_INPUT input, float3 worldNormal )
{
    float2 sceneUV  = input.Pos.xy * DistortionParams.zw;
    float2 offset   = GeomGetDistortionViewNormal( worldNormal ).xy;
    float  lengthSq = dot( offset, offset );

    if( lengthSq > 1e-5f )
    {
        offset *= rsqrt( lengthSq );
    }
    else
    {
        offset = 0.0f;
    }

    sceneUV += offset * DistortionParams.x * DistortionParams.zw;
    return saturate( sceneUV );
}

//==============================================================================

float3 GeomSampleDistortionEnvironment( uint materialFlags, float3 worldNormal, float3 viewVector )
{
    float3 envColor = 0.0f;

    if( materialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
    {
        float3 cubeDir = normalize( viewVector );
        envColor = txEnvironmentCube.Sample( samEnvironmentCube, cubeDir ).rgb;
    }
    else
    {
        float2 envUV = GeomGetDistortionViewNormal( worldNormal ).xy * 0.5f + 0.5f;
        envColor = txEnvironment.Sample( samEnvironment, saturate( envUV ) ).rgb;
    }

    return envColor;
}

//==============================================================================

GEOM_PIXEL_OUTPUT GeomShadeDistortionPixel( GEOM_PIXEL_INPUT input, uint materialFlags, float fadeAlpha )
{
    GEOM_PIXEL_OUTPUT output;
    output.FinalColor  = 0.0f;
    output.NormalDepth = float4( 0.5f, 0.5f, 1.0f, GeomEncodeLinearDepth( input ).r );
    output.Glow        = 0.0f;

    const float2 sceneUV    = GeomGetDistortionSceneUV( input, input.Normal );
    const float3 sceneColor = txDistortionScene.Sample( samDistortion, sceneUV ).rgb;

    float3 finalColor = sceneColor;

    if( materialFlags & MATERIAL_FLAG_DISTORTION_PERPOLY_ENV )
    {
        const float  envBlend    = saturate( EnvParams.x );
        const float  envStrength = envBlend * (1.0f - envBlend);
        const float3 envColor    = GeomSampleDistortionEnvironment( materialFlags, input.Normal, input.ViewVector );

        finalColor = saturate( sceneColor + envColor * envStrength );
    }

    finalColor = lerp( sceneColor, finalColor, fadeAlpha );

    output.FinalColor  = float4( finalColor, fadeAlpha );
    output.NormalDepth = float4( 0.5f, 0.5f, 1.0f, GeomEncodeLinearDepth( input ).r );
    output.Glow        = float4( 0.0f, 0.0f, 0.0f, 0.0f );

    return output;
}

//==============================================================================
#endif // GEOM_PIXEL_DISTORTION_HLSL
//==============================================================================
