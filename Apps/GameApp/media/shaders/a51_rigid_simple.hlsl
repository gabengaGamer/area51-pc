//==============================================================================
//
//  a51_rigid_simple.hlsl
//
//  Simple rigidgeom uber-shader for A51.
//
//==============================================================================

#include "common/material_flags.hlsl"

//==============================================================================
//  CONSTANT BUFFERS
//==============================================================================

#include "common/geom_buffers.hlsl"

cbuffer cbMatrices : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;

    float4   MaterialParams;   // x = flags, y = alpha ref, z = nearZ, w = farZ
    float4   CameraPosition;   // xyz = camera position
    float4   UVAnim;           // xy = uv animation offsets
    float4   EnvParams;        // x = fixed alpha, y = is cube map, z = is view-space env
};

//==============================================================================
//  TEXTURES AND SAMPLERS
//==============================================================================

Texture2D txDiffuse : register(t0);
Texture2D txDetail  : register(t1);
Texture2D txEnvironment : register(t2);
TextureCube txEnvironmentCube : register(t3);
Texture2D txProjLight[MAX_PROJ_LIGHTS]   : register(t4);
Texture2D txProjShadow[MAX_PROJ_SHADOWS] : register(t14);

SamplerState samLinear : register(s0);

#include "common/proj_common.hlsl"

//==============================================================================
//  VERTEX SHADER
//==============================================================================

struct VS_INPUT
{
    float3 Pos    : POSITION;
    float3 Normal : NORMAL;
    float4 Color  : COLOR;
    float2 UV     : TEXCOORD;
};

struct PS_INPUT
{
    float4 Pos         : SV_POSITION;
    float4 Color       : COLOR;
    float2 UV          : TEXCOORD;
    float3 WorldPos    : TEXCOORD1;
    float3 Normal      : TEXCOORD2;
    float  LinearDepth : TEXCOORD3;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;

    // Transform position through matrices
    float4 worldPos = mul(World, float4(input.Pos, 1.0));
    float4 viewPos  = mul(View, worldPos);
    output.Pos      = mul(Projection, viewPos);

    // Pass through vertex attributes
    output.WorldPos    = worldPos.xyz;
    output.Normal      = normalize(mul((float3x3)World, input.Normal));

    float2 depthParams = MaterialParams.zw;
    float nearZ = depthParams.x;
    float farZ  = depthParams.y;
    float invRange = rcp(max(farZ - nearZ, 1e-5f));
    float linearDepth = (viewPos.z - nearZ) * invRange;
    output.LinearDepth = saturate(linearDepth);
    float2 uvAnimOffset = UVAnim.xy;
    output.UV          = input.UV + uvAnimOffset;
    output.Color       = input.Color;
    
    return output;
}

//==============================================================================
//  PIXEL SHADER
//==============================================================================

struct PS_OUTPUT
{
    float4 FinalColor : SV_Target0;  // Back buffer output
    float4 Albedo     : SV_Target1;  // Base color for deferred lighting
    float4 Normal     : SV_Target2;  // World-space normals + material info
    float  DepthInfo  : SV_Target3;  // Linear depth for distance effects
    float4 Glow       : SV_Target4;  // Emissive color + intensity mask
};

PS_OUTPUT PSMain(PS_INPUT input)
{
    PS_OUTPUT output;

    uint  materialFlags = (uint)MaterialParams.x;
    float alphaRef      = MaterialParams.y;
    float3 cameraPos    = CameraPosition.xyz;

    // Sample diffuse texture
    float4 diffuseSample = txDiffuse.Sample(samLinear, input.UV);
    float4 diffuseColor  = diffuseSample;

    // Apply detail texture
    if (materialFlags & MATERIAL_FLAG_DETAIL)
    {
        float4 detailColor = txDetail.Sample(samLinear, input.UV * 4.0);
        diffuseColor *= detailColor * 2.0;
    }

    float4 baseColor;

    // Apply vertex color modulation
    if (materialFlags & MATERIAL_FLAG_VERTEX_COLOR)
    {
        diffuseColor.a *= input.Color.a;
    }

    float alphaValue = diffuseColor.a;

    // Perform alpha testing
    if (materialFlags & MATERIAL_FLAG_ALPHA_TEST)
    {
        if (diffuseSample.a < alphaRef)
            discard;
    }

    // Environment mapping
    if (materialFlags & MATERIAL_FLAG_ENVIRONMENT)
    {
        float3 worldNormal = normalize(input.Normal);
        float3 viewDir     = normalize(cameraPos - input.WorldPos);
        float3 reflection  = normalize(reflect(-viewDir, worldNormal));

        float3 envColor = 0.0;

        if (materialFlags & MATERIAL_FLAG_ENV_CUBEMAP)
        {
            envColor = txEnvironmentCube.Sample(samLinear, reflection).rgb;
        }
        else
        {
            float2 envUV = reflection.xy * 0.5f + 0.5f;
            envColor = txEnvironment.Sample(samLinear, envUV).rgb;
        }

        if (materialFlags & MATERIAL_FLAG_PERPIXEL_ENV)
        {
            float blend = saturate(diffuseSample.a);
            diffuseColor.rgb = lerp(diffuseColor.rgb, envColor, blend);
        }
        else if (materialFlags & MATERIAL_FLAG_PERPOLY_ENV)
        {
            float blend = saturate(EnvParams.x);
            diffuseColor.rgb = lerp(diffuseColor.rgb, envColor, blend);
        }
    }

    diffuseColor.a = alphaValue;
    baseColor      = diffuseColor;

    // Apply per-pixel lighting
    float3 PerPixelLight = float3( 0.0, 0.0, 0.0 );
    for( uint i = 0; i < LightCount; i++ )
    {
        float3 L     = LightVec[i].xyz - input.WorldPos;
        float  dist  = length( L );
        float  atten = saturate( 1.0 - dist / LightVec[i].w );
        float3 Ldir  = L / max( dist, 0.0001 );
        float  NdotL = max( dot( input.Normal, Ldir ) + 0.5f, 0.0f );
        PerPixelLight    += LightCol[i].rgb * NdotL * atten;
    }

    float3 totalLight = LightAmbCol.rgb + PerPixelLight;
    if( materialFlags & MATERIAL_FLAG_VERTEX_COLOR )
    {
        totalLight += input.Color.rgb;
    }

    float4 finalColor = float4( diffuseColor.rgb * totalLight, diffuseColor.a );

    // Apply projection lights and shadows
    if( materialFlags & MATERIAL_FLAG_PROJ_LIGHT )
    {
        finalColor.rgb = ApplyProjLights( finalColor.rgb, input.WorldPos );
    }

    if( materialFlags & MATERIAL_FLAG_PROJ_SHADOW )
    {
        finalColor.rgb = ApplyProjShadows( finalColor.rgb, input.WorldPos );
    }

    output.Glow = float4(0.0, 0.0, 0.0, 0.0);

    // Apply per-pixel illumination
    if (materialFlags & MATERIAL_FLAG_PERPIXEL_ILLUM)
    {
        float4 texColor = txDiffuse.Sample(samLinear, input.UV);
        float  intensity = texColor.a;
        float3 emissive  = texColor.rgb;
        float  emissiveStrength = max( max( emissive.r, emissive.g ), emissive.b );
        float  glowMask         = saturate( max( intensity, emissiveStrength ) );

        output.Glow.rgb += emissive;
        output.Glow.a    = max(output.Glow.a, glowMask);

        finalColor.rgb = lerp(finalColor.rgb, texColor.rgb, intensity);
        finalColor.a   = intensity;
    }

    // Apply per-poly illumination
    if (materialFlags & MATERIAL_FLAG_PERPOLY_ILLUM)
    {
        float  intensity = diffuseColor.a;
        float3 emissive  = diffuseColor.rgb;
        float  emissiveStrength = max( max( emissive.r, emissive.g ), emissive.b );
        float  glowMask         = saturate( max( intensity, emissiveStrength ) );

        output.Glow.rgb += emissive;
        output.Glow.a    = max(output.Glow.a, glowMask);
    }

    if( (materialFlags & MATERIAL_FLAG_ALPHA_BLEND) == 0 )
    {
        finalColor.a *= 0.64;
    }

    // Fill G-Buffer outputs
    output.FinalColor = finalColor;
    output.Albedo     = baseColor;
    output.Normal     = float4(input.Normal * 0.5 + 0.5, 0.0);
    output.DepthInfo  = input.LinearDepth;

    return output;
}
