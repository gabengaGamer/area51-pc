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

//------------------------------------------------------------------------------

static float4 GeomEncodeLinearDepth( GEOM_PIXEL_INPUT input )
{
    const float4 viewPos     = mul( View, float4( input.WorldPos, 1.0f ) );
    const float  invRange    = rcp( max( FarZ - NearZ, 1e-5f ) );
    const float  linearDepth = saturate( (viewPos.z - NearZ) * invRange );
    return linearDepth.xxxx;
}

//==============================================================================

#if defined(GEOM_USE_RIGID_INSTANCE_DATA)
uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT input )
{
    return MaterialFlags | RigidInstances[input.InstanceID].ShaderFlags;
}

float GeomGetFadeAlpha( GEOM_PIXEL_INPUT input )
{
    return saturate( RigidInstances[input.InstanceID].FadeAlpha );
}

uint GeomGetLightCount( GEOM_PIXEL_INPUT input )
{
    return min( RigidInstances[input.InstanceID].LightCount, (uint)MAX_GEOM_LIGHTS );
}

float4 GeomGetLightVec( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return RigidInstances[input.InstanceID].LightVec[lightIndex];
}

float4 GeomGetLightCol( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return RigidInstances[input.InstanceID].LightCol[lightIndex];
}

float4 GeomGetLightDir( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return RigidInstances[input.InstanceID].LightDir[lightIndex];
}

float4 GeomGetLightCone( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return RigidInstances[input.InstanceID].LightCone[lightIndex];
}

float4 GeomGetLightAmbCol( GEOM_PIXEL_INPUT input )
{
    return RigidInstances[input.InstanceID].LightAmbCol;
}
#elif defined(GEOM_USE_SKIN_INSTANCE_DATA)
uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT input )
{
    return MaterialFlags | SkinInstances[input.InstanceID].ShaderFlags;
}

float GeomGetFadeAlpha( GEOM_PIXEL_INPUT input )
{
    return saturate( SkinInstances[input.InstanceID].FadeAlpha );
}

uint GeomGetLightCount( GEOM_PIXEL_INPUT input )
{
    return min( SkinInstances[input.InstanceID].LightCount, (uint)MAX_GEOM_LIGHTS );
}

float4 GeomGetLightVec( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return SkinInstances[input.InstanceID].LightVec[lightIndex];
}

float4 GeomGetLightCol( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return SkinInstances[input.InstanceID].LightCol[lightIndex];
}

float4 GeomGetLightDir( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return SkinInstances[input.InstanceID].LightDir[lightIndex];
}

float4 GeomGetLightCone( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return SkinInstances[input.InstanceID].LightCone[lightIndex];
}

float4 GeomGetLightAmbCol( GEOM_PIXEL_INPUT input )
{
    return SkinInstances[input.InstanceID].LightAmbCol;
}
#else
uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT input )
{
    return MaterialFlags;
}

float GeomGetFadeAlpha( GEOM_PIXEL_INPUT input )
{
    return saturate( EnvParams.z );
}

uint GeomGetLightCount( GEOM_PIXEL_INPUT input )
{
    return LightCount;
}

float4 GeomGetLightVec( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return LightVec[lightIndex];
}

float4 GeomGetLightCol( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return LightCol[lightIndex];
}

float4 GeomGetLightDir( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return LightDir[lightIndex];
}

float4 GeomGetLightCone( GEOM_PIXEL_INPUT input, uint lightIndex )
{
    return LightCone[lightIndex];
}

float4 GeomGetLightAmbCol( GEOM_PIXEL_INPUT input )
{
    return LightAmbCol;
}
#endif

//==============================================================================

float3 GeomGetDistortionViewNormal( float3 worldNormal )
{
    float3 distortionViewNormal = mul( (float3x3)DistortionNormalMatrix, normalize( worldNormal ) );
    return normalize( distortionViewNormal );
}

//==============================================================================

float2 GeomGetDistortionSceneUV( GEOM_PIXEL_INPUT input, float3 worldNormal )
{
    float2 sceneUV = input.Pos.xy * DistortionParams.zw;
    float2 offset  = GeomGetDistortionViewNormal( worldNormal ).xy;
    float  lengthSq = dot( offset, offset );

    if( lengthSq > 1e-5f )
        offset *= rsqrt( lengthSq );
    else
        offset = 0.0f;

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
        envColor = txEnvironmentCube.Sample( samLinear, cubeDir ).rgb;
    }
    else
    {
        float2 envUV = GeomGetDistortionViewNormal( worldNormal ).xy * 0.5f + 0.5f;
        envColor = txEnvironment.Sample( samLinear, saturate( envUV ) ).rgb;
    }

    return envColor;
}

//==============================================================================

GEOM_PIXEL_OUTPUT GeomShadeDistortionPixel( GEOM_PIXEL_INPUT input, uint materialFlags, float fadeAlpha )
{
    GEOM_PIXEL_OUTPUT output;
    output.FinalColor = 0.0f;
    output.Albedo     = 0.0f;
    output.Normal     = 0.0f;
    output.LinearDepth= GeomEncodeLinearDepth( input );
    output.Glow       = 0.0f;

    const float2 sceneUV    = GeomGetDistortionSceneUV( input, input.Normal );
    const float3 sceneColor = txDistortionScene.Sample( samLinear, sceneUV ).rgb;

    float3 finalColor = sceneColor;

    if( materialFlags & MATERIAL_FLAG_DISTORTION_PERPOLY_ENV )
    {
        const float  envBlend    = saturate( EnvParams.x );
        const float  envStrength = envBlend * (1.0f - envBlend);
        const float3 envColor    = GeomSampleDistortionEnvironment( materialFlags, input.Normal, input.ViewVector );

        finalColor = saturate( sceneColor + envColor * envStrength );
    }

    finalColor = lerp( sceneColor, finalColor, fadeAlpha );

    output.FinalColor = float4( finalColor, fadeAlpha );
    output.Albedo     = float4( 0.0f, 0.0f, 0.0f, fadeAlpha );
    output.Normal     = float4( 0.5f, 0.5f, 1.0f, fadeAlpha );
    output.LinearDepth= GeomEncodeLinearDepth( input );
    output.Glow       = float4( 0.0f, 0.0f, 0.0f, 0.0f );

    return output;
}

//==============================================================================

GeomDiffuseResult GeomEvaluateDiffuse( GEOM_PIXEL_INPUT input, uint materialFlags, float alphaRef )
{
    GeomDiffuseResult result;
    result.Sample = txDiffuse.Sample( samLinear, input.UV );
    result.Color  = result.Sample;

    if( materialFlags & MATERIAL_FLAG_DETAIL )
    {
        float  detailScale = max( UVAnim.z, 1.0f );
        float4 detailColor = txDetail.Sample( samLinear, input.UV * detailScale );
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
            float3 cubeDir = normalize( viewVector );
            envColor = txEnvironmentCube.Sample( samLinear, cubeDir ).rgb;
        }
        else
        {
            float3 envVector;

            if( materialFlags & MATERIAL_FLAG_ENV_VIEWSPACE )
            {
                envVector = mul( (float3x3)View, worldNormal );
            }
            else //MATERIAL_FLAG_ENV_WORLDSPACE
            {
                envVector = worldNormal;
            }

            envVector = normalize( envVector );

            float2 envUV = envVector.xy * 0.5f + 0.5f;
            envColor = txEnvironment.Sample( samLinear, envUV ).rgb;
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

float GeomComputeRadialAttenuation( float lightDistance, float lightRadius, float lightFalloff )
{
    const float radius = max( lightRadius, 1e-4f );
    const float falloff = saturate( lightFalloff );
    const float innerRadius = radius * ( 1.0f - falloff );
    return 1.0f - saturate( ( lightDistance - innerRadius ) / max( radius - innerRadius, 1e-4f ) );
}

//==============================================================================

float GeomComputeSpotAttenuation( GEOM_PIXEL_INPUT input, uint lightIndex, float3 lightToPointDir )
{
    float4 lightDir = GeomGetLightDir( input, lightIndex );
    if( lightDir.w < 0.5f )
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

float GeomComputeLocalLightAttenuation( GEOM_PIXEL_INPUT input, uint lightIndex, float3 worldPos, out float3 pointToLightDir )
{
    float4 lightVec      = GeomGetLightVec( input, lightIndex );
    float4 lightCol      = GeomGetLightCol( input, lightIndex );
    const float3 toLight = lightVec.xyz - worldPos;
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
    return radial * GeomComputeSpotAttenuation( input, lightIndex, -pointToLightDir );
}

//==============================================================================

float3 GeomComputeLighting( GEOM_PIXEL_INPUT input, uint materialFlags )
{
#ifdef GEOM_USE_SKIN_LIGHTING
    float3 dynLight = float3( 0.0, 0.0, 0.0 );
    const uint lightCount = GeomGetLightCount( input );
    for( uint i = 0; i < lightCount; i++ )
    {
        float ndotl = saturate( dot( input.Normal, -GeomGetLightVec( input, i ).xyz ) );
        dynLight += GeomGetLightCol( input, i ).rgb * ndotl;
    }
    float3 totalLight = GeomGetLightAmbCol( input ).rgb + dynLight;
#else
    float3 perPixelLight = float3( 0.0, 0.0, 0.0 );
    const uint lightCount = GeomGetLightCount( input );
    for( uint i = 0; i < lightCount; i++ )
    {
        float3 L;
        float  atten = GeomComputeLocalLightAttenuation( input, i, input.WorldPos, L );
        if( atten > 0.0f )
        {
            float ndotl = saturate( dot( input.Normal, L ) );
            perPixelLight += GeomGetLightCol( input, i ).rgb * ( atten * ndotl );
        }
    }
    float3 totalLight = perPixelLight;
    #if GEOM_HAS_VERTEX_COLOR
        if( materialFlags & MATERIAL_FLAG_VERTEX_COLOR )
        {
            totalLight += input.Color.rgb;
        }
    #endif
#endif
    return totalLight;
}

//==============================================================================

// Experimental hacky specular.

float3 GeomComputeSpecular( GEOM_PIXEL_INPUT input, uint materialFlags, float diffuseAlpha )
{
    float  specBlend    = 0.0f;
    float3 specular     = float3( 0.0, 0.0, 0.0 );

    if( materialFlags & MATERIAL_FLAG_ENVIRONMENT )
    {
        float  envStrength  = 1.0f;
        
        if( materialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ENV )
        {
            if( materialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
            {
                envStrength  = 4.0f * EnvParams.y;
            }
            else
            {
                envStrength  = EnvParams.y;            
            }
        }
    
        if( materialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ENV )
            specBlend = diffuseAlpha * envStrength;
        else if( materialFlags & MATERIAL_FLAG_ALPHA_PERPOLY_ENV )
            specBlend = EnvParams.x * envStrength;
    
        float specPower = 16.0f;
        float3 viewDir = normalize( -input.ViewVector );

    #ifdef GEOM_USE_SKIN_LIGHTING
        const uint lightCount = GeomGetLightCount( input );
        for( uint i = 0; i < lightCount; i++ )
        {
            float3 L = normalize( -GeomGetLightVec( input, i ).xyz );
            float  ndotl = saturate( dot( input.Normal, L ) );
            if( ndotl > 0.0f )
            {
                float3 H = normalize( L + viewDir );
                float  specTerm = pow( saturate( dot( input.Normal, H ) ), specPower ) * ndotl;
                specular += GeomGetLightCol( input, i ).rgb * specTerm;
            }
        }
    #else
        const uint lightCount = GeomGetLightCount( input );
        for( uint i = 0; i < lightCount; i++ )
        {
            float3 L;
            float  atten = GeomComputeLocalLightAttenuation( input, i, input.WorldPos, L );
            if( atten > 0.0f )
            {
                float  ndotl = saturate( dot( input.Normal, L ) );
                if( ndotl > 0.0f )
                {
                    float3 H = normalize( L + viewDir );
                    float  specTerm = pow( saturate( dot( input.Normal, H ) ), specPower ) * ndotl;
                    specular += GeomGetLightCol( input, i ).rgb * specTerm * atten;
                }
            }
        }
    #endif
    } 
    
    return specular * specBlend;
}

//==============================================================================

float4 GeomComputeGlow( GEOM_PIXEL_INPUT input,
                        uint materialFlags,
                        inout float4 finalColor,
                        float4 diffuseSample )
{
    float4 glow = float4( 0.0, 0.0, 0.0, 0.0 );
    const bool bUseDiffuse           = (materialFlags & MATERIAL_FLAG_ILLUM_USE_DIFFUSE) != 0;
    const bool bDiffusePerPixelIllum = (materialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ILLUM) != 0;
    const bool bAlphaPerPixelIllum   = (materialFlags & MATERIAL_FLAG_ALPHA_PERPIXEL_ILLUM) != 0;
    const bool bAlphaPerPolyIllum    = (materialFlags & MATERIAL_FLAG_ALPHA_PERPOLY_ILLUM) != 0;

    if( bAlphaPerPixelIllum )
    {
        glow = diffuseSample;

        if( !bUseDiffuse )
            finalColor.rgb = diffuseSample.rgb;
    }
    else if( bDiffusePerPixelIllum || bAlphaPerPolyIllum )
    {
        const float intensity = bAlphaPerPolyIllum ? saturate( EnvParams.x ) : saturate( diffuseSample.a );
        const float3 illumRgb = lerp( finalColor.rgb, diffuseSample.rgb, intensity );

        glow.rgb = illumRgb;
        glow.a   = 1.0f;  //glow.a   = intensity;

        if( !bUseDiffuse )
        {
            finalColor.rgb = illumRgb;
            //finalColor.a   = intensity;
        }
    }

    return glow;
}

//==============================================================================

float3 ApplyProjLights(float3 color, float3 worldPos)
{
    for(uint i = 0; i < ProjLightCount; i++)
    {
        float4 projPos = mul(ProjLightMatrix[i], float4(worldPos, 1.0));
        if( projPos.w > 0.0 )
        {
            float2 uv = projPos.xy / projPos.w;
            if( uv.x >= 0.0 && uv.x <= 1.0 &&
                uv.y >= 0.0 && uv.y <= 1.0 )
            {
                float proj = txProjLight[i].Sample(samLinear, uv).b;
                color = lerp( color, color * 2.0, saturate(proj) );
            }
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
        if( projPos.w > 0.0 )
        {
            float2 uv = projPos.xy / projPos.w;
            if( uv.x >= 0.0 && uv.x <= 1.0 &&
                uv.y >= 0.0 && uv.y <= 1.0 )
            {
                float shade = txProjShadow[i].Sample(samLinear, uv).b;
                color *= shade * 2.0;
            }
        }
    }
    return color;
}

//==============================================================================

static const float kFaceShadowPositiveExponent = 5.0f;
static const float kFaceShadowNegativeExponent = 5.0f;

float WarpFaceShadowPositive( float depth )
{
    return exp( kFaceShadowPositiveExponent * depth );
}

float WarpFaceShadowNegative( float depth )
{
    return -exp( -kFaceShadowNegativeExponent * depth );
}

float ComputeFaceShadowVisibility( float2 moments, float depth )
{
    const float lit = ( depth <= moments.x ) ? 1.0f : 0.0f;
    const float variance = max( moments.y - ( moments.x * moments.x ), ShadowParams.z );
    const float delta    = depth - moments.x;
    float visibility     = variance / ( variance + ( delta * delta ) );
    visibility           = saturate( ( visibility - ShadowParams.w ) / max( 1.0f - ShadowParams.w, 1e-4f ) );
    return max( lit, visibility );
}

float SampleFaceShadowAtlas(float3 shadowUVW, float depthBias)
{
    if( shadowUVW.x < 0.0 || shadowUVW.x > 1.0 ||
        shadowUVW.y < 0.0 || shadowUVW.y > 1.0 ||
        shadowUVW.z < 0.0 || shadowUVW.z > 1.0 )
    {
        return 1.0;
    }

    const float depth = saturate( shadowUVW.z - depthBias );
    const float4 moments = txFaceShadowAtlas.Sample( samFaceShadow, shadowUVW.xy );
    const float positiveDepth = WarpFaceShadowPositive( depth );
    const float negativeDepth = WarpFaceShadowNegative( depth );
    const float positiveVisibility = ComputeFaceShadowVisibility( moments.xy, positiveDepth );
    const float negativeVisibility = ComputeFaceShadowVisibility( moments.zw, negativeDepth );
    return min( positiveVisibility, negativeVisibility );
}

//==============================================================================

float SampleFaceShadowSource(uint sourceIndex, float3 worldPos, float3 worldNormal)
{
    if( sourceIndex >= FaceShadowCount )
        return 1.0f;

    const float3 lightPos      = FaceShadowLightPosRadius[sourceIndex].xyz;
    const float  lightRadius   = FaceShadowLightPosRadius[sourceIndex].w;
    const float3 lightDir      = normalize( FaceShadowLightDirFalloff[sourceIndex].xyz );
    const float  lightFalloff  = FaceShadowLightDirFalloff[sourceIndex].w;
    const float  cosOuter      = FaceShadowLightData[sourceIndex].x;
    const float  nearZ         = FaceShadowLightData[sourceIndex].y;
    const float3 toLight       = lightPos - worldPos;
    const float  lightDistanceSq = dot( toLight, toLight );

    if( lightDistanceSq <= 1e-8f )
        return 1.0f;

    const float lightDistance   = sqrt( lightDistanceSq );
    const float shadowInfluence = GeomComputeRadialAttenuation( lightDistance, lightRadius, lightFalloff );
    if( shadowInfluence <= 0.0f )
        return 1.0f;

    const float3 pointToLightDir = toLight / lightDistance;
    const float  coneCos         = dot( lightDir, -pointToLightDir );
    const float  coneRange       = max( 1.0f - cosOuter, 1e-4f );
    const float  coneInfluence   = saturate( ( coneCos - cosOuter ) / coneRange );
    if( coneInfluence <= 0.0f )
        return 1.0f;

    const float  nearFadeRange   = max( nearZ, lightRadius * 0.02f );
    const float  nearInfluence   = saturate( ( lightDistance - nearZ ) / nearFadeRange );
    if( nearInfluence <= 0.0f )
        return 1.0f;

    const float3 normal          = normalize( worldNormal );
    const float  ndotl           = dot( normal, pointToLightDir );
    if( ndotl <= 0.0f )
        return 1.0f;

    const float  visibilityScale = shadowInfluence * coneInfluence * nearInfluence;
    const float  normalBias      = ShadowParams.x * lightRadius * ( 0.5f + ( 1.5f * ( 1.0f - saturate( ndotl ) ) ) );
    const float  depthBias       = ShadowParams.x * ( 1.0f + ( 2.0f * ( 1.0f - saturate( ndotl ) ) ) );
    const float3 shadowWorldPos  = worldPos + normal * normalBias;

    float4 shadowPos = mul(FaceShadowMatrix[sourceIndex], float4(shadowWorldPos, 1.0));
    if( shadowPos.w <= 0.0f )
        return 1.0f;

    float3 shadowUVW;
    shadowUVW.xy = shadowPos.xy / shadowPos.w;
    shadowUVW.z  = shadowPos.z  / shadowPos.w;

    if( shadowUVW.x < 0.0f || shadowUVW.x > 1.0f ||
        shadowUVW.y < 0.0f || shadowUVW.y > 1.0f ||
        shadowUVW.z < 0.0f || shadowUVW.z > 1.0f )
    {
        return 1.0f;
    }

    const float visibility = SampleFaceShadowAtlas( shadowUVW, depthBias );
    return lerp( 1.0f, visibility, visibilityScale );
}

//==============================================================================

float ComputePointShadowInfluence( float lightDistance, float lightRadius, float lightFalloff )
{
    if( lightRadius <= 0.0f || lightDistance >= lightRadius )
        return 0.0f;

    const float falloff     = saturate( lightFalloff );
    const float innerRadius = lightRadius * ( 1.0f - falloff );
    return 1.0f - saturate( ( lightDistance - innerRadius ) / max( lightRadius - innerRadius, 1e-4f ) );
}

//==============================================================================

float ComputePointShadowCompareDepth( float faceDepth, float nearZ, float farZ )
{
    const float safeDepth = max( faceDepth, 1e-4f );
    const float denom     = max( farZ - nearZ, 1e-4f );
    return saturate( ( farZ / denom ) - ( ( nearZ * farZ ) / ( denom * safeDepth ) ) );
}

//==============================================================================

float SamplePointShadowBucket( uint bucketIndex, float3 lightDir, float cubeIndex, float compareDepth )
{
    const float4 shadowCoord = float4( lightDir, cubeIndex );

    if( bucketIndex == 0u )
        return txPointShadowCube[0].SampleCmpLevelZero( samPointShadowCmp, shadowCoord, compareDepth );

    if( bucketIndex == 1u )
        return txPointShadowCube[1].SampleCmpLevelZero( samPointShadowCmp, shadowCoord, compareDepth );

    if( bucketIndex == 2u )
        return txPointShadowCube[2].SampleCmpLevelZero( samPointShadowCmp, shadowCoord, compareDepth );

    return txPointShadowCube[3].SampleCmpLevelZero( samPointShadowCmp, shadowCoord, compareDepth );
}

//==============================================================================

float SamplePointShadowLight(uint lightIndex, float3 worldPos, float3 worldNormal)
{
    const float3 lightPos     = PointShadowLightPosRadius[lightIndex].xyz;
    const float  lightRadius  = PointShadowLightPosRadius[lightIndex].w;
    const float  lightFalloff = PointShadowLightData[lightIndex].x;
    const float  nearZ        = PointShadowLightData[lightIndex].y;
    const float  farZ         = PointShadowLightData[lightIndex].z;
    const float  cubeIndex    = PointShadowLightData[lightIndex].w;
    const uint   bucketIndex  = min( (uint)PointShadowLightParams[lightIndex].x,
                                     (uint)( POINT_SHADOW_BUCKET_COUNT - 1 ) );
    const float3 toLight      = worldPos - lightPos;
    const float  lightDistanceSq = dot( toLight, toLight );

    if( lightDistanceSq <= 1e-8f )
        return 1.0f;

    const float lightDistance   = sqrt( lightDistanceSq );
    const float shadowInfluence = ComputePointShadowInfluence( lightDistance, lightRadius, lightFalloff );
    if( shadowInfluence <= 0.0f )
        return 1.0f;

    const float3 lightDir = toLight / lightDistance;
    const float3 normal   = normalize( worldNormal );
    const float  ndotl    = dot( normal, -lightDir );

    // Do not project point-light shadows onto the receiver back side.
    if( ndotl <= 0.0f )
        return 1.0f;

    const float faceDepth    = max( abs( toLight.x ), max( abs( toLight.y ), abs( toLight.z ) ) );
    const float depthBias    = ShadowParams.x * farZ;
    const float compareDepth = ComputePointShadowCompareDepth( max( faceDepth - depthBias, nearZ ),
                                                               nearZ,
                                                               farZ );
    const float visibility   = SamplePointShadowBucket( bucketIndex,
                                                        lightDir,
                                                        cubeIndex,
                                                        compareDepth );

    return lerp( 1.0f, visibility, shadowInfluence );
}

//==============================================================================

float3 ApplyShadowMaps(float3 color, float3 worldPos, float3 worldNormal)
{
    if( ( FaceShadowCount == 0 ) && ( PointShadowLightCount == 0 ) )
        return color;

    float visibility = 1.0;

    if( PointShadowLightCount > 0 )
    {
        [unroll]
        for( uint i = 0; i < MAX_SHADOW_LIGHTS; i++ )
        {
            if( i >= PointShadowLightCount )
                break;

            visibility = min( visibility, SamplePointShadowLight( i, worldPos, worldNormal ) );
        }
    }

    [unroll]
    for( uint i = 0; i < MAX_SHADOW_SOURCES; i++ )
    {
        if( i >= FaceShadowCount )
            break;

        visibility = min( visibility, SampleFaceShadowSource( i, worldPos, worldNormal ) );
    }

    return color * lerp( ShadowParams.y, 1.0, visibility );
}

//==============================================================================

GEOM_PIXEL_OUTPUT ShadeGeometryPixel( GEOM_PIXEL_INPUT input )
{
    GEOM_PIXEL_OUTPUT output;
    output.FinalColor = 0.0f;
    output.Albedo     = 0.0f;
    output.Normal     = 0.0f;
    output.LinearDepth= GeomEncodeLinearDepth( input );
    output.Glow       = 0.0f;

    uint  materialFlags = GeomGetMaterialFlags( input );
    float alphaRef      = AlphaRef;
    float fadeAlpha     = GeomGetFadeAlpha( input );

    if( EnvParams.w > 0.5f )
    {
        output.FinalColor = float4( 0.0, 0.0, 0.0, 0.0 );
        output.Albedo     = float4( 0.0, 0.0, 0.0, 0.0 );
        output.Normal     = float4( 0.5, 0.5, 1.0, 0.0 );
        output.LinearDepth= GeomEncodeLinearDepth( input );
        output.Glow       = float4( 0.0, 0.0, 0.0, 0.0 );
        return output;
    }

    if( materialFlags & (MATERIAL_FLAG_DISTORTION | MATERIAL_FLAG_DISTORTION_PERPOLY_ENV) )
    {
        return GeomShadeDistortionPixel( input, materialFlags, fadeAlpha );
    }

    GeomDiffuseResult diffuse = GeomEvaluateDiffuse( input, materialFlags, alphaRef );
    GeomApplyEnvironment( diffuse, materialFlags, input.Normal, input.ViewVector );

    float4 baseColor = diffuse.Color;
    float4 finalColor;

    float3 totalLight = GeomComputeLighting( input, materialFlags );
    float3 specular = GeomComputeSpecular( input, materialFlags, diffuse.Sample.a );
    finalColor = float4( diffuse.Color.rgb * totalLight + specular, diffuse.Color.a );

    if( materialFlags & INSTANCE_FLAG_PROJ_LIGHT )
        finalColor.rgb = ApplyProjLights( finalColor.rgb, input.WorldPos );

    if( materialFlags & INSTANCE_FLAG_PROJ_SHADOW )
        finalColor.rgb = ApplyProjShadows( finalColor.rgb, input.WorldPos );

    finalColor.rgb = ApplyShadowMaps( finalColor.rgb, input.WorldPos, input.Normal );
    finalColor.rgb = saturate(finalColor.rgb);

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
#endif // GEOM_PIXEL_SHARED_HLSL
//==============================================================================
