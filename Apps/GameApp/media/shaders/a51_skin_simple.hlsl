//==============================================================================
//  
//  a51_skin_simple.hlsl
//  
//  Simple skindgeom uber-shader for A51.
//
//==============================================================================

#include "common/material_flags.hlsl"

//==============================================================================
//  CONSTANT BUFFERS
//==============================================================================

#include "common/geom_buffers.hlsl"

cbuffer cbMatrices : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    
    uint     MaterialFlags;
    float    AlphaRef;
    float2   Padding;
};

struct Bone
{
    float4x4 L2W;
};

cbuffer cbBones : register(b2)
{
    Bone Bones[MAX_SKIN_BONES];
};

//==============================================================================
//  TEXTURES AND SAMPLERS
//==============================================================================

Texture2D txDiffuse : register(t0);
Texture2D txProjLight[MAX_PROJ_LIGHTS]   : register(t3);
Texture2D txProjShadow[MAX_PROJ_SHADOWS] : register(t13);
SamplerState samLinear : register(s0);

#include "common/proj_common.hlsl"

//==============================================================================
//  VERTEX SHADER
//==============================================================================

struct VS_INPUT
{
    float4 PosIndex   : POSITION;
    float4 NormIndex  : NORMAL;
    float4 UVWeights  : TEXCOORD;
};

struct PS_INPUT
{
    float4 Pos        : SV_POSITION;
    float2 UV         : TEXCOORD;
    float3 WorldPos   : TEXCOORD1;
    float3 Normal     : TEXCOORD2;
    float  LinearDepth: TEXCOORD3;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;

    // Extract bone indices and weights (stub)
    //int   index1  = 0;
    //int   index2  = 0;
    //float weight1 = 1.0;
    //float weight2 = 0.0;

    // Extract bone indices and weights
    int   index1  = (int)input.PosIndex.w;
    int   index2  = (int)input.NormIndex.w;
    float weight1 = input.UVWeights.z;
    float weight2 = input.UVWeights.w;

    // Blend positions using bone matrices
    float3 pos1 = mul(Bones[index1].L2W, float4(input.PosIndex.xyz, 1.0)).xyz;
    float3 pos2 = mul(Bones[index2].L2W, float4(input.PosIndex.xyz, 1.0)).xyz;
    float3 skinnedPos = pos1 * weight1 + pos2 * weight2;

    // Blend normals using bone matrices
    float3 norm1 = mul((float3x3)Bones[index1].L2W, input.NormIndex.xyz);
    float3 norm2 = mul((float3x3)Bones[index2].L2W, input.NormIndex.xyz);
    float3 skinnedNorm = normalize(norm1 * weight1 + norm2 * weight2);

    // Transform position through matrices
    float4 worldPos = float4(skinnedPos, 1.0);
    float4 viewPos  = mul(View, worldPos);
    output.Pos      = mul(Projection, viewPos);

    // Pass through vertex attributes
    output.WorldPos    = worldPos.xyz;
    output.Normal      = skinnedNorm;
    output.LinearDepth = length(viewPos.xyz);
    output.UV          = input.UVWeights.xy;

    return output;
}

//==============================================================================
//  PIXEL SHADER
//==============================================================================

struct PS_OUTPUT
{
    float4 FinalColor : SV_Target0;  // Back buffer output
    float4 Albedo     : SV_Target1;  // Base color for deferred lighting
    float4 Normal     : SV_Target2;  // World-space normals
    float4 DepthInfo  : SV_Target3;  // NDC depth + linear depth + flags
    float4 Glow       : SV_Target4;  // Emissive color + intensity mask
};

PS_OUTPUT PSMain(PS_INPUT input)
{
    PS_OUTPUT output;

    // Sample diffuse texture
    float4 diffuseColor = txDiffuse.Sample(samLinear, input.UV);

    // Perform alpha testing
    if (MaterialFlags & MATERIAL_FLAG_ALPHA_TEST)
    {
        if (diffuseColor.a < AlphaRef)
            discard;
    }

    // Apply per-pixel lighting
    float3 dynLight = float3(0.0, 0.0, 0.0);
    for( uint i = 0; i < LightCount; i++ )
    {
        float  ndotl = saturate(dot(input.Normal, -LightVec[i].xyz));
        dynLight    += LightCol[i].rgb * ndotl;
    }
    float3 totalLight = LightAmbCol.rgb + dynLight;

    float4 baseColor  = diffuseColor;
    
    float4 finalColor = float4(diffuseColor.rgb * totalLight, diffuseColor.a);

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

    // Fill G-Buffer outputs
    output.FinalColor = finalColor;
    output.Albedo     = baseColor;
    output.Normal     = float4(input.Normal * 0.5 + 0.5, 0.0);
    output.DepthInfo  = float4(
        input.Pos.z / input.Pos.w,  // NDC depth for position reconstruction
        input.LinearDepth,          // Linear depth for distance effects
        0.0,
        finalColor.a                // Alpha for transparency effects
    );

    return output;
}
