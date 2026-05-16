//==============================================================================
//
//  post_fog.hlsl
//
//  Depth-based fog composite for the DX11 post pipeline.
//
//==============================================================================

Texture2D LinearDepthSource : register(t1);
Texture2D FogPalette  : register(t2);

SamplerState samLinear : register(s0);

cbuffer FogParams : register(b4)
{
    float4 FogColor;
    float4 FogCoeff;
    float4 FogParams0; // x = near, y = far, z = use polynomial, w = fog start
};

//==============================================================================

float4 PSMain( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const int2   depthTexel   = int2( Pos.xy );
    const float  linearDepth  = saturate( LinearDepthSource.Load( int3( depthTexel, 0 ) ).r );
    const float  nearZ        = FogParams0.x;
    const float  farZ         = FogParams0.y;
    const float  viewZ        = nearZ + linearDepth * max( farZ - nearZ, 1e-5f );
    
    if( FogParams0.z > 0.5f )
    {
        const float fogStart = FogParams0.w;
        const float q     = farZ / max( farZ - nearZ, 1e-5f );
        const float xboxZ = viewZ * q - nearZ * q;
        const float startZ2 = fogStart * fogStart;
        const float startZ3 = startZ2 * fogStart;
        const float startAlpha = FogCoeff.x + FogCoeff.y * fogStart + FogCoeff.z * startZ2 + FogCoeff.w * startZ3;

        if( xboxZ <= fogStart )
            return float4( FogColor.rgb, 0.0f );

        const float z2    = xboxZ * xboxZ;
        const float z3    = z2 * xboxZ;
        const float rawAlpha = FogCoeff.x + FogCoeff.y * xboxZ + FogCoeff.z * z2 + FogCoeff.w * z3;
        const float alpha = saturate( (rawAlpha - startAlpha) / max(1.0f - startAlpha, 1e-5f) );

        return float4( FogColor.rgb, alpha );
    }

    const float4 fogSample = FogPalette.SampleLevel( samLinear, float2( linearDepth, 0.5f ), 0.0f );
    return float4( fogSample.rgb, saturate( fogSample.a ) );
}
