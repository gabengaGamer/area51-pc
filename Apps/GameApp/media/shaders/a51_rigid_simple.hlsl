//==============================================================================
//  
//  a51_rigid_simple.hlsl
//  
//  Simple rigidgeom shader for A51.
//
//==============================================================================

#define MATERIAL_FLAG_ALPHA_TEST        (1u<<0)
#define MATERIAL_FLAG_ADDITIVE          (1u<<1)
#define MATERIAL_FLAG_SUBTRACTIVE       (1u<<2)
#define MATERIAL_FLAG_VERTEX_COLOR      (1u<<3)
#define MATERIAL_FLAG_TWO_SIDED         (1u<<4)
#define MATERIAL_FLAG_ENVIRONMENT       (1u<<5)
#define MATERIAL_FLAG_DISTORTION        (1u<<6)
#define MATERIAL_FLAG_PERPIXEL_ILLUM    (1u<<7) 
#define MATERIAL_FLAG_PERPOLY_ILLUM     (1u<<8) 
#define MATERIAL_FLAG_PERPIXEL_ENV      (1u<<9) 
#define MATERIAL_FLAG_PERPOLY_ENV       (1u<<10)
#define MATERIAL_FLAG_HAS_DETAIL        (1u<<11)
#define MATERIAL_FLAG_HAS_ENVIRONMENT   (1u<<12) 

//==============================================================================
//  CONSTANT BUFFERS
//==============================================================================

cbuffer cbMatrices : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
    
    uint     MaterialFlags;
    float    AlphaRef;
    float3   CameraPosition;
    float    Padding1;
    float    Padding2;
};

//==============================================================================
//  TEXTURES AND SAMPLERS
//==============================================================================

Texture2D txDiffuse : register(t0);
Texture2D txDetail  : register(t1);
Texture2D txEnvironment : register(t2);

SamplerState samLinear : register(s0);

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
    float4 viewPos = mul(View, worldPos);
    output.Pos = mul(Projection, viewPos);
    
    // Pass through vertex attributes
    output.Color = input.Color;
    output.UV = input.UV; 
    output.WorldPos = worldPos.xyz;
    output.Normal = normalize(mul((float3x3)World, input.Normal));
    
    // Calculate linear depth for distance-based effects
    output.LinearDepth = length(viewPos.xyz);
    
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
    float4 DepthInfo  : SV_Target3;  // NDC depth + linear depth + flags
};

PS_OUTPUT PSMain(PS_INPUT input)
{
    PS_OUTPUT output;
    
    // Sample base diffuse texture
    float4 diffuseColor = txDiffuse.Sample(samLinear, input.UV);

    // Apply detail texture if available
    if (MaterialFlags & MATERIAL_FLAG_HAS_DETAIL)
    {
        float4 detailColor = txDetail.Sample(samLinear, input.UV * 4.0);
        diffuseColor *= detailColor * 2.0;
    }
    
    float4 baseColor = diffuseColor;
    
    // Apply vertex color modulation
    if (MaterialFlags & MATERIAL_FLAG_VERTEX_COLOR)
    {
        diffuseColor *= input.Color;
    }
    
    // Perform alpha testing
    if (MaterialFlags & MATERIAL_FLAG_ALPHA_TEST)
    {
        if (diffuseColor.a < AlphaRef)
            discard;
    }

    float4 finalColor = diffuseColor;
      
	
	//---------------------------------------------------------------------------------------
	
	// DEBUG
	
    //if (MaterialFlags & MATERIAL_FLAG_DISTORTION)
    //{
    //    output.FinalColor = float4(0.5, 0.8, 1.0, 1.0);  // BrightBlue
    //    output.Albedo = float4(0.5, 0.8, 1.0, 1.0);
    //    output.Normal = float4(0.5, 0.5, 1.0, 0.25);     // Distortion material alpha marker
    //    output.DepthInfo = float4(input.Pos.z / input.Pos.w, input.LinearDepth, 0.0, 1.0);
    //    return output;
    //}
	//
    //if (MaterialFlags & MATERIAL_FLAG_PERPIXEL_ENV)
    //{
    //    output.FinalColor = float4(0.0, 1.0, 0.0, 1.0);  // Green
    //    output.Albedo = float4(0.0, 1.0, 0.0, 1.0);
    //    output.Normal = float4(0.5, 0.5, 1.0, 0.5);      // Environment material alpha marker
    //    output.DepthInfo = float4(input.Pos.z / input.Pos.w, input.LinearDepth, 0.0, 1.0);
    //    return output;
    //}
    //    
    //if (MaterialFlags & MATERIAL_FLAG_PERPOLY_ENV)
    //{
    //    output.FinalColor = float4(1.0, 1.0, 0.0, 1.0);  // Yellow
    //    output.Albedo = float4(1.0, 1.0, 0.0, 1.0);
    //    output.Normal = float4(0.5, 0.5, 1.0, 0.5);      // Environment material alpha marker
    //    output.DepthInfo = float4(input.Pos.z / input.Pos.w, input.LinearDepth, 0.0, 1.0);
    //    return output;
    //}
    //    
    //if (MaterialFlags & MATERIAL_FLAG_PERPIXEL_ILLUM)
    //{
    //    output.FinalColor = float4(0.0, 0.0, 1.0, 1.0);  // DarkBlue
    //    output.Albedo = float4(0.0, 0.0, 1.0, 1.0);
    //    output.Normal = float4(0.5, 0.5, 1.0, 1.0);      // Per-pixel illum alpha marker
    //    output.DepthInfo = float4(input.Pos.z / input.Pos.w, input.LinearDepth, 0.0, 1.0);
    //    return output;
    //}
    //    
    //if (MaterialFlags & MATERIAL_FLAG_PERPOLY_ILLUM)
    //{
    //    output.FinalColor = float4(0.0, 1.0, 1.0, 1.0);  // Cyan
    //    output.Albedo = float4(0.0, 1.0, 1.0, 1.0);
    //    output.Normal = float4(0.5, 0.5, 1.0, 1.0);      // Per-poly illum alpha marker
    //    output.DepthInfo = float4(input.Pos.z / input.Pos.w, input.LinearDepth, 0.0, 1.0);
    //    return output;
    //}
    //    
    //if (MaterialFlags & MATERIAL_FLAG_ADDITIVE)
    //{
    //    output.FinalColor = float4(1.0, 1.0, 1.0, 1.0);  // White
    //    output.Albedo = float4(1.0, 1.0, 1.0, 1.0);
    //    output.Normal = float4(0.5, 0.5, 1.0, 0.0);      // Additive alpha marker
    //    output.DepthInfo = float4(input.Pos.z / input.Pos.w, input.LinearDepth, 0.0, 1.0);
    //    return output;
    //}
    //    
    //if (MaterialFlags & MATERIAL_FLAG_SUBTRACTIVE)
    //{
    //    output.FinalColor = float4(0.0, 0.0, 0.0, 1.0);  // Black
    //    output.Albedo = float4(0.0, 0.0, 0.0, 1.0);
    //    output.Normal = float4(0.5, 0.5, 1.0, 0.0);      // Subtractive alpha marker
    //    output.DepthInfo = float4(input.Pos.z / input.Pos.w, input.LinearDepth, 0.0, 1.0);
    //    return output;
    //}
	
	//---------------------------------------------------------------------------------------
    
    // Special material handling for per-pixel illumination
    if (MaterialFlags & MATERIAL_FLAG_PERPIXEL_ILLUM)
    {
        float4 texColor = txDiffuse.Sample(samLinear, input.UV);
        float4 vertexColor = input.Color;

        float3 litColor = vertexColor.rgb * texColor.rgb;
        finalColor.rgb = lerp(litColor, texColor.rgb, texColor.a);
        finalColor.a = texColor.a;
    }
    
    // Fill G-Buffer outputs
    output.FinalColor = finalColor;
    output.Albedo = baseColor;
    output.Normal = float4(input.Normal * 0.5 + 0.5, 0.0);
    output.DepthInfo = float4(
        input.Pos.z / input.Pos.w,     // NDC depth
        input.LinearDepth,             // Linear depth
        0.0,                           // Reserved
        finalColor.a                   // Alpha
    );
    
    return output;
}