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
#define MAX_SKIN_LIGHTS 4

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
    float4   LightDir[MAX_SKIN_LIGHTS];
    float4   LightCol[MAX_SKIN_LIGHTS];
    float4   LightAmbCol;
    uint     LightCount;
    float3   Padding;
};

struct Bone
{
    row_major float4x4 L2W;
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
    
    // Apply Per-pixel lighting
    float3 diffuse = float3(0.0, 0.0, 0.0);
    for(uint i=0;i<LightCount;i++)
    {
        float NdotL = saturate(dot(input.Normal, -LightDir[i].xyz));
        diffuse += LightCol[i].rgb * NdotL;
    }
    float3 ambient = LightAmbCol.rgb;
    float4 litColor = float4(diffuse + ambient, 1.0) * texColor;    
    
    //if( MaterialFlags & MATERIAL_FLAG_PROJ_LIGHT )
    //{
        for( uint i = 0; i < ProjLightCount; i++ )
        {
            float4 projPos = mul( ProjLightMatrix[i], float4( input.WorldPos, 1.0 ) );
            float3 projUVW = projPos.xyz / projPos.w;
            if( projUVW.x >= 0.0 && projUVW.x <= 1.0 &&
                projUVW.y >= 0.0 && projUVW.y <= 1.0 &&
                projUVW.z >= 0.0 && projUVW.z <= 1.0 )
            {
                float fade = smoothstep( 0.0f, EdgeSize,
                                         min( min( projUVW.x, 1.0 - projUVW.x ),
                                              min( projUVW.y, 1.0 - projUVW.y ) ) );
                float4 projCol = txProjLight[i].Sample( samLinear, projUVW.xy );
                float  blend   = fade * projCol.a;
                float3 lit     = litColor.rgb * projCol.rgb;
                litColor.rgb   = lerp( litColor.rgb, lit, blend );
            }
        }
    //}

    //if( MaterialFlags & MATERIAL_FLAG_PROJ_SHADOW )
    //{
        for( uint i = 0; i < ProjShadowCount; i++ )
        {
            float4 projPos = mul( ProjShadowMatrix[i], float4( input.WorldPos, 1.0 ) );
            float3 projUVW = projPos.xyz / projPos.w;
            if( projUVW.x >= 0.0 && projUVW.x <= 1.0 &&
                projUVW.y >= 0.0 && projUVW.y <= 1.0 &&
                projUVW.z >= 0.0 && projUVW.z <= 1.0 )
            {
                float fade = smoothstep( 0.0f, EdgeSize,
                                         min( min( projUVW.x, 1.0 - projUVW.x ),
                                              min( projUVW.y, 1.0 - projUVW.y ) ) );
                float4 shadCol = txProjShadow[i].Sample( samLinear, projUVW.xy );
                float  sBlend  = fade * shadCol.a;
                float3 shaded  = litColor.rgb * shadCol.rgb;
                litColor.rgb   = lerp( litColor.rgb, shaded, sBlend );
            }
        }
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