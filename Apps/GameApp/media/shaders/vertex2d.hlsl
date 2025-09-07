//==============================================================================
//  
//  vertex2d.hlsl
//  
//  Generic 2D shader for A51.
//
//==============================================================================

cbuffer cbProjection : register(b1)
{
    float ScreenWidth;
    float ScreenHeight;
    float pad0;
    float pad1;
};

struct VS_INPUT
{
    float3 Pos : POSITION;
    float RHW : RHW;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    
    float adjustedX = input.Pos.x + 0.5;  // Half-pixel offset
    float adjustedY = input.Pos.y + 0.5;  // Half-pixel offset
    
    output.Pos.x = (adjustedX * 2.0 / ScreenWidth) - 1.0;
    output.Pos.y = 1.0 - (adjustedY * 2.0 / ScreenHeight);
    output.Pos.z = input.Pos.z;
    output.Pos.w = 1.0;
    
    output.Color = input.Color;
    output.UV = input.UV;
    
    return output;
}