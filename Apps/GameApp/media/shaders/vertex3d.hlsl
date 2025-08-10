cbuffer cbMatrices : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
};

struct VS_INPUT
{
    float3 Pos : POSITION;
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
    
    float4 worldPos = mul(World, float4(input.Pos, 1.0));
    float4 viewPos = mul(View, worldPos);
    output.Pos = mul(Projection, viewPos);
    
    output.Color = input.Color;
    output.UV = input.UV;
    
    return output;
}