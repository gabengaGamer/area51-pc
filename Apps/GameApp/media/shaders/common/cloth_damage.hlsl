//==============================================================================
//
//  cloth_damage.hlsl
//
//  Shared runtime cloth cutout and frayed-edge helpers.
//
//==============================================================================

#ifndef CLOTH_DAMAGE_HLSL
#define CLOTH_DAMAGE_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float ClothDamageCoverage( Texture2D damageTexture, SamplerState damageSampler, float2 uv )
{
    return damageTexture.Sample( damageSampler, uv ).r;
}

//==============================================================================

float ClothDamageEdge( Texture2D damageTexture, SamplerState damageSampler, float2 uv, float coverage )
{
    uint width;
    uint height;
    damageTexture.GetDimensions( width, height );

    float2 const texelSize = 1.0f / float2( width, height );
    float2 const edgeStep = texelSize * 1.5f;

    float neighbour = 0.0f;
    neighbour = max( neighbour, damageTexture.SampleLevel( damageSampler, uv + float2( edgeStep.x, 0.0f ), 0.0f ).r );
    neighbour = max( neighbour, damageTexture.SampleLevel( damageSampler, uv - float2( edgeStep.x, 0.0f ), 0.0f ).r );
    neighbour = max( neighbour, damageTexture.SampleLevel( damageSampler, uv + float2( 0.0f, edgeStep.y ), 0.0f ).r );
    neighbour = max( neighbour, damageTexture.SampleLevel( damageSampler, uv - float2( 0.0f, edgeStep.y ), 0.0f ).r );
    return smoothstep( 0.0f, 0.65f, saturate( neighbour - coverage ) );
}

//==============================================================================
#endif // CLOTH_DAMAGE_HLSL
//==============================================================================
