//==============================================================================
//
//  fog_functions.hlsl
//
//  Shared depth-based fog math used by post and forward geometry passes.
//
//==============================================================================

#ifndef A51_FOG_FUNCTIONS_HLSL
#define A51_FOG_FUNCTIONS_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float A51ComputePolynomialFogAlpha( float linearDepth, float nearZ, float farZ, float fogStart, float4 fogCoeff )
{
    const float viewZ     = nearZ + linearDepth * max( farZ - nearZ, 1e-5f );
    const float q         = farZ / max( farZ - nearZ, 1e-5f );
    const float xboxZ     = viewZ * q - nearZ * q;
    const float startZ2   = fogStart * fogStart;
    const float startZ3   = startZ2 * fogStart;
    const float startAlpha = fogCoeff.x + fogCoeff.y * fogStart + fogCoeff.z * startZ2 + fogCoeff.w * startZ3;

    if( xboxZ <= fogStart )
    {
        return 0.0f;
    }

    const float z2        = xboxZ * xboxZ;
    const float z3        = z2 * xboxZ;
    const float rawAlpha  = fogCoeff.x + fogCoeff.y * xboxZ + fogCoeff.z * z2 + fogCoeff.w * z3;
    return saturate( ( rawAlpha - startAlpha ) / max( 1.0f - startAlpha, 1e-5f ) );
}

//==============================================================================
#endif // A51_FOG_FUNCTIONS_HLSL
