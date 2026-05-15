//==============================================================================
//
//  a51_shadow_evsm.hlsl
//
//  Raw shadow-depth caster for PC shadows.
//
//==============================================================================

struct PS_CAST_INPUT
{
    float4 Position : SV_POSITION;
};

float PSCastMoments( PS_CAST_INPUT input ) : SV_TARGET
{
    return saturate( input.Position.z );
}
