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

    uint     MaterialFlags;
    float    AlphaRef;
    float3   CameraPosition;
    float2   DepthParams;
    float4   UVAnim;
};


//==============================================================================
//  TEXTURES AND SAMPLERS
//==============================================================================

Texture2D txDiffuse : register(t0);
Texture2D txDetail  : register(t1);
Texture2D txEnvironment : register(t2);
Texture2D txProjLight[MAX_PROJ_LIGHTS]   : register(t3);
Texture2D txProjShadow[MAX_PROJ_SHADOWS] : register(t13);

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

    float nearZ = DepthParams.x;
    float farZ  = DepthParams.y;
    float invRange = rcp(max(farZ - nearZ, 1e-5f));
    float linearDepth = (viewPos.z - nearZ) * invRange;
    output.LinearDepth = saturate(linearDepth);
    output.UV          = input.UV + UVAnim.xy;
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

    // Sample diffuse texture
    float4 diffuseColor = txDiffuse.Sample(samLinear, input.UV);

    // Apply detail texture
    if (MaterialFlags & MATERIAL_FLAG_DETAIL)
    {
        float4 detailColor = txDetail.Sample(samLinear, input.UV * 4.0);
        diffuseColor *= detailColor * 2.0;
    }

    float4 baseColor = diffuseColor;

    // Apply vertex color modulation 
    if (MaterialFlags & MATERIAL_FLAG_VERTEX_COLOR)
    {
        diffuseColor.a *= input.Color.a;
    }

    // Perform alpha testing
    if (MaterialFlags & MATERIAL_FLAG_ALPHA_TEST)
    {
        if (diffuseColor.a < AlphaRef)
            discard;
    }

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
    if( MaterialFlags & MATERIAL_FLAG_VERTEX_COLOR )
    {
        totalLight += input.Color.rgb;
    }

    float4 finalColor = float4( diffuseColor.rgb * totalLight, diffuseColor.a );

    // Apply projection lights and shadows
    if( MaterialFlags & MATERIAL_FLAG_PROJ_LIGHT )
    {
        finalColor.rgb = ApplyProjLights( finalColor.rgb, input.WorldPos );
    }

    if( MaterialFlags & MATERIAL_FLAG_PROJ_SHADOW )
    {
        finalColor.rgb = ApplyProjShadows( finalColor.rgb, input.WorldPos );
    }

    output.Glow = float4(0.0, 0.0, 0.0, 0.0);

    // Apply per-pixel illumination
    if (MaterialFlags & MATERIAL_FLAG_PERPIXEL_ILLUM)
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
    if (MaterialFlags & MATERIAL_FLAG_PERPOLY_ILLUM)
    {
        float  intensity = diffuseColor.a;
        float3 emissive  = diffuseColor.rgb;
        float  emissiveStrength = max( max( emissive.r, emissive.g ), emissive.b );
        float  glowMask         = saturate( max( intensity, emissiveStrength ) );

        output.Glow.rgb += emissive;
        output.Glow.a    = max(output.Glow.a, glowMask);
    }

    if( (MaterialFlags & MATERIAL_FLAG_ALPHA_BLEND) == 0 )
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
