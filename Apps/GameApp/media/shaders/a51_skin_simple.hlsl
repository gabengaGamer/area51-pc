//==============================================================================
//  
//  a51_skin.hlsl
//  
//  Simple skindgeom shader for A51.
//
//==============================================================================

#define MATERIAL_FLAG_PROJ_LIGHT  (1u<<13)
#define MATERIAL_FLAG_PROJ_SHADOW (1u<<14)

#define MAX_PROJ_LIGHTS 10
#define MAX_PROJ_SHADOWS 10

//==============================================================================
//  CONSTANT BUFFERS
//==============================================================================

cbuffer cbSkinConsts : register(b0)
{
    row_major float4x4 W2C;
    float    Zero;
    float    One;
    float    MinusOne;
    float    Fog;
    float4   LightDirCol;
    float4   LightAmbCol;
    float2   Padding;
};

struct Bone
{
    row_major float4x4 L2W;
    float3   LightDir;
    float    Padding;
};

cbuffer cbBones : register(b1)
{
    Bone Bones[96];
};

cbuffer cbProjTextures : register(b2)
{
    float4x4 ProjLightMatrix[MAX_PROJ_LIGHTS];
    float4x4 ProjShadowMatrix[MAX_PROJ_SHADOWS];
    uint     ProjLightCount;
    uint     ProjShadowCount;
    float    EdgeSize;
    float3   ProjPadding;
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
    float4 PosIndex1  : POSITION;
    float4 NormIndex2 : NORMAL;
    float4 UVWeights  : TEXCOORD;
};

struct PS_INPUT
{
    float4 Pos        : SV_POSITION;
    float4 Color      : COLOR;
    float2 UV         : TEXCOORD;
    float3 WorldPos   : TEXCOORD1;
    float3 Normal     : TEXCOORD2;
    float  LinearDepth: TEXCOORD3;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    
    // Extract bone indices and weights (stub)
    int   index1  = 0;
    int   index2  = 0;
    float weight1 = 1.0;
    float weight2 = 0.0;
    
    // Blend positions using bone matrices
    float3 pos1 = mul(float4(input.PosIndex1.xyz, 1.0), Bones[index1].L2W).xyz;
    float3 pos2 = mul(float4(input.PosIndex1.xyz, 1.0), Bones[index2].L2W).xyz;
    float3 worldPos = pos1 * weight1 + pos2 * weight2;
    output.WorldPos = worldPos;
    
    // Blend normals using bone matrices
    float3 norm1 = mul(input.NormIndex2.xyz, (float3x3)Bones[index1].L2W);
    float3 norm2 = mul(input.NormIndex2.xyz, (float3x3)Bones[index2].L2W);
    output.Normal = normalize(norm1 * weight1 + norm2 * weight2);
    
    // Transform to clip space
    float4 clipPos = mul(float4(worldPos, 1.0), W2C);
    output.Pos = clipPos;
    
    // Calculate linear depth for distance-based effects
    output.LinearDepth = clipPos.w;
    
    // Calculate vertex lighting using blended bone light directions
    float3 lightDir1 = Bones[index1].LightDir;
    float3 lightDir2 = Bones[index2].LightDir;
    float3 blendedLightDir = normalize(lightDir1 * weight1 + lightDir2 * weight2);
    
    float NdotL = saturate(dot(output.Normal, -blendedLightDir));
    float3 diffuse = LightDirCol.rgb * NdotL;
    float3 ambient = LightAmbCol.rgb;
    
    output.Color = float4(diffuse + ambient, 1.0);
    output.UV = input.UVWeights.xy;
    
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
};

PS_OUTPUT PSMain(PS_INPUT input)
{
    PS_OUTPUT output;
    
    // Sample diffuse texture
    float4 texColor = txDiffuse.Sample(samLinear, input.UV);
    
    // Apply vertex lighting
    float4 litColor = texColor * input.Color;
    
	//if( MaterialFlags & MATERIAL_FLAG_PROJ_LIGHT )
    //{
        litColor.rgb = ApplyProjLights( litColor.rgb, input.WorldPos );
	//}

    //if( MaterialFlags & MATERIAL_FLAG_PROJ_SHADOW )
    //{
        litColor.rgb = ApplyProjShadows( litColor.rgb, input.WorldPos );
	//}
	
    // Fill G-Buffer outputs
    output.FinalColor = litColor;
    output.Albedo = texColor;
    output.Normal = float4(input.Normal * 0.5 + 0.5, 0.0);
    output.DepthInfo = float4(
        input.Pos.z / input.Pos.w,  // NDC depth for position reconstruction
        input.LinearDepth,          // Linear depth for distance effects
        1.0,                        // Mark as skinned geometry
        litColor.a                  // Alpha for transparency effects
    );
    
    return output;
}