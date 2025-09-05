//==============================================================================
//
//  proj_common.hlsl
//
//  Shared projection texture utilities.
//
//==============================================================================

float3 ApplyProjLights(float3 color, float3 worldPos)
{
    for(uint i = 0; i < ProjLightCount; i++)
    {
        float4 projPos = mul(ProjLightMatrix[i], float4(worldPos, 1.0));
        float3 uvw = projPos.xyz / projPos.w;
        if(uvw.x >= 0.0 && uvw.x <= 1.0 &&
           uvw.y >= 0.0 && uvw.y <= 1.0 &&
           uvw.z >= 0.0 && uvw.z <= 1.0)
        {
            float4 proj = txProjLight[i].Sample(samLinear, uvw.xy);
            float3 lit = proj.rgb * 2.0;
            color = lerp(color, color * lit, proj.a);
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
        float3 uvw = projPos.xyz / projPos.w;
        if(uvw.x >= 0.0 && uvw.x <= 1.0 &&
           uvw.y >= 0.0 && uvw.y <= 1.0 &&
           uvw.z >= 0.0 && uvw.z <= 1.0)
        {
            float4 proj = txProjShadow[i].Sample(samLinear, uvw.xy);
            float shade = proj.b * 2.0;
            color = lerp(color, color * shade, proj.a);
        }
    }
    return color;
}