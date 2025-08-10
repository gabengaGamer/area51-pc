Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

cbuffer cbFlags : register(b1)
{
    int UseTexture;
    int UseAlpha;
    int pad0;
    int pad1;
};

float4 main(PS_INPUT input) : SV_Target
{
    float4 finalColor;
    
    if (UseTexture > 0)
    {
        float4 texColor = txDiffuse.Sample(samLinear, input.UV);
        finalColor = texColor * input.Color;
    }
    else
    {
        finalColor = input.Color;
    }
    
    return finalColor;
}