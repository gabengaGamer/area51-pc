//==============================================================================
//
//  InterpolationMath.hpp
//
//==============================================================================

#ifndef INTERPOLATION_MATH_HPP
#define INTERPOLATION_MATH_HPP

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

inline 
f32 ClampInterpAlpha( f32 T )
{
    return MAX( 0.0f, MIN( T, 1.0f ) );
}

//==============================================================================

inline 
f32 InterpScalar( f32 A, f32 B, f32 T )
{
    return A + ((B - A) * T);
}

//==============================================================================

inline 
radian InterpAngle( radian A, radian B, f32 T )
{
    return A + (x_MinAngleDiff( B, A ) * T);
}

//==============================================================================

inline 
radian3 InterpRotation( const radian3& A, const radian3& B, f32 T )
{
    return radian3( InterpAngle( A.Pitch, B.Pitch, T ),
                    InterpAngle( A.Yaw,   B.Yaw,   T ),
                    InterpAngle( A.Roll,  B.Roll,  T ) );
}

//==============================================================================

inline 
vector3 InterpVector( const vector3& A, const vector3& B, f32 T )
{
    return A + ((B - A) * T);
}

//==============================================================================

inline
vector3 TransformInterpDirection( const matrix4& M, const vector3& V )
{
    matrix4 R = M;
    R.ClearTranslation();
    return R * V;
}

//==============================================================================

inline 
matrix4 BuildInterpL2W( const vector3& Pos, const radian3& Rot )
{
    matrix4 L2W;
    L2W.Identity();
    L2W.SetRotation( Rot );
    L2W.SetTranslation( Pos );
    return L2W;
}

//==============================================================================

inline 
matrix4 InterpMatrix( const matrix4& A, const matrix4& B, f32 T )
{
    return BuildInterpL2W( InterpVector( A.GetTranslation(), B.GetTranslation(), T ),
                           InterpRotation( A.GetRotation(), B.GetRotation(), T ) );
}

//==============================================================================

inline 
xbool ShouldSnapInterpL2W( const matrix4& PrevL2W, const matrix4& CurrL2W )
{
    const vector3 Delta   = CurrL2W.GetTranslation() - PrevL2W.GetTranslation();
    const radian3 PrevRot = PrevL2W.GetRotation();
    const radian3 CurrRot = CurrL2W.GetRotation();

    return (Delta.LengthSquared() > x_sqr( 250.0f )) ||
           (x_abs( x_MinAngleDiff( CurrRot.Pitch, PrevRot.Pitch ) ) > R_90) ||
           (x_abs( x_MinAngleDiff( CurrRot.Yaw,   PrevRot.Yaw   ) ) > R_90) ||
           (x_abs( x_MinAngleDiff( CurrRot.Roll,  PrevRot.Roll  ) ) > R_90);
}

//==============================================================================

inline 
void UpdateInterpMatrices( matrix4*       pInterp,
                                  const matrix4* pPrev,
                                  s32            PrevCount,
                                  const matrix4* pCurr,
                                  s32            CurrCount,
                                  f32            T )
{
    if( (PrevCount == CurrCount) && (CurrCount > 0) )
    {
        for( s32 i = 0; i < CurrCount; i++ )
            pInterp[i] = InterpMatrix( pPrev[i], pCurr[i], T );
    }
    else
    {
        for( s32 i = 0; i < CurrCount; i++ )
            pInterp[i] = pCurr[i];
    }
}

//==============================================================================
#endif //INTERPOLATION_MATH_HPP
//==============================================================================
