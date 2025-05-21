//==============================================================================
//
//  x_math_m3_inline.hpp
//
//==============================================================================

#ifndef X_MATH_M3_INLINE_HPP
#define X_MATH_M3_INLINE_HPP
#else
#error "File " __FILE__ " has been included twice!"
#endif

#ifndef X_DEBUG_HPP
#include "../x_debug.hpp"
#endif

#ifndef X_PLUS_HPP
#include "../x_plus.hpp"
#endif

#ifdef TARGET_XBOX
#include <xtl.h>
#include <xgmath.h>
#endif

//==============================================================================
//  DEFINES
//==============================================================================

#define M(row,column)   m_Cell[row][column]

//==============================================================================
//  FUNCTIONS
//==============================================================================

inline
f32 matrix3::operator () ( s32 Column, s32 Row ) const
{
    FORCE_ALIGNED_16( this );
    return( m_Cell[Column][Row] );
}

//==============================================================================

inline
f32& matrix3::operator () ( s32 Column, s32 Row )
{
    FORCE_ALIGNED_16( this );
    return( m_Cell[Column][Row] );
}

//==============================================================================

inline 
void matrix3::Zero( void )
{
    FORCE_ALIGNED_16( this );

#if USE_VU0
    asm( "vsub.xyzw  %0, vf00, vf00" : "=j" (m_Col0) );
    asm( "vsub.xyzw  %0, vf00, vf00" : "=j" (m_Col1) );
    asm( "vsub.xyzw  %0, vf00, vf00" : "=j" (m_Col2) );
#else
    M(0,0) = M(1,0) = M(2,0) = 0.0f;
    M(0,1) = M(1,1) = M(2,1) = 0.0f;
    M(0,2) = M(1,2) = M(2,2) = 0.0f;
#endif
}

//==============================================================================

inline 
void matrix3::Identity( void )
{    
    FORCE_ALIGNED_16( this );

#if USE_VU0
    asm( "vmove.xyzw    %0, vf00" : "=j" (m_Col0) );
    asm( "vmr32.xyzw    %0, vf00" : "=j" (m_Col1) );
    asm( "vmr32.xyzw    %0, %1"   : "=j" (m_Col2) : "j" (m_Col1) );
#else
    M(1,0) = M(2,0) = 0.0f;
    M(0,1) = M(2,1) = 0.0f;
    M(0,2) = M(1,2) = 0.0f;

    M(0,0) = 1.0f;
    M(1,1) = 1.0f;
    M(2,2) = 1.0f;
#endif
}

//==============================================================================

inline
void matrix3::Transpose( void )
{
    FORCE_ALIGNED_16( this );

#if USE_VU0
    u128    Tmp0, Tmp1, Tmp2, Tmp3;

    asm( "pextlw %0, %1, %2" : "=r" (Tmp0)   : "r" (m_Col1), "r" (m_Col0) );
    asm( "pextuw %0, %1, %2" : "=r" (Tmp1)   : "r" (m_Col1), "r" (m_Col0) );
    asm( "pextlw %0, $0, %1" : "=r" (Tmp2)   : "r" (m_Col2)                );
    asm( "pextuw %0, $0, %1" : "=r" (Tmp3)   : "r" (m_Col2)                );
    asm( "pcpyld %0, %1, %2" : "=r" (m_Col0) : "r" (Tmp2),   "r" (Tmp0)    );
    asm( "pcpyud %0, %1, %2" : "=r" (m_Col1) : "r" (Tmp0),   "r" (Tmp2)    );
    asm( "pcpyld %0, %1, %2" : "=r" (m_Col2) : "r" (Tmp3),   "r" (Tmp1)    );
#else
    f32 T;    
   
    T      = M(1,0);
    M(1,0) = M(0,1);
    M(0,1) = T;
    
    T      = M(2,0);
    M(2,0) = M(0,2);
    M(0,2) = T;
    
    T      = M(2,1);
    M(2,1) = M(1,2);
    M(1,2) = T;
#endif
}

//==============================================================================

inline
xbool matrix3::Invert( void )
{
    FORCE_ALIGNED_16( this );
    
    f32     Det;
    matrix3 Src = *this;

    // Calculate the determinant.
    Det = Src.M(0,0) * ( Src.M(1,1) * Src.M(2,2) - Src.M(1,2) * Src.M(2,1) ) -
          Src.M(0,1) * ( Src.M(1,0) * Src.M(2,2) - Src.M(1,2) * Src.M(2,0) ) +
          Src.M(0,2) * ( Src.M(1,0) * Src.M(2,1) - Src.M(1,1) * Src.M(2,0) );

    if( x_abs( Det ) < 0.00001f ) 
        return( FALSE );

    Det = 1.0f / Det;

    // Find the inverse of the matrix.
    M(0,0) =  Det * ( Src.M(1,1) * Src.M(2,2) - Src.M(1,2) * Src.M(2,1) );
    M(0,1) = -Det * ( Src.M(0,1) * Src.M(2,2) - Src.M(0,2) * Src.M(2,1) );
    M(0,2) =  Det * ( Src.M(0,1) * Src.M(1,2) - Src.M(0,2) * Src.M(1,1) );

    M(1,0) = -Det * ( Src.M(1,0) * Src.M(2,2) - Src.M(1,2) * Src.M(2,0) );
    M(1,1) =  Det * ( Src.M(0,0) * Src.M(2,2) - Src.M(0,2) * Src.M(2,0) );
    M(1,2) = -Det * ( Src.M(0,0) * Src.M(1,2) - Src.M(0,2) * Src.M(1,0) );

    M(2,0) =  Det * ( Src.M(1,0) * Src.M(2,1) - Src.M(1,1) * Src.M(2,0) );
    M(2,1) = -Det * ( Src.M(0,0) * Src.M(2,1) - Src.M(0,1) * Src.M(2,0) );
    M(2,2) =  Det * ( Src.M(0,0) * Src.M(1,1) - Src.M(0,1) * Src.M(1,0) );
  
    return( TRUE );
}

//==============================================================================

#ifdef TARGET_PS2
inline
u128 matrix3::GetCol0_U128( void ) const
{
    return m_Col0;
}

inline
u128 matrix3::GetCol1_U128( void ) const
{
    return m_Col1;
}

inline
u128 matrix3::GetCol2_U128( void ) const
{
    return m_Col2;
}
#endif

//==============================================================================

inline
matrix3& matrix3::operator += ( const matrix3& aM )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &aM );

#if USE_VU0
    asm( "vadd.xyzw DST0, MT10, MT20" : "=j DST0" (m_Col0) : "j MT10" (m_Col0), "j MT20" (aM.m_Col0) );
    asm( "vadd.xyzw DST1, MT11, MT21" : "=j DST1" (m_Col1) : "j MT11" (m_Col1), "j MT21" (aM.m_Col1) );
    asm( "vadd.xyzw DST2, MT12, MT22" : "=j DST2" (m_Col2) : "j MT12" (m_Col2), "j MT22" (aM.m_Col2) );
#else
    M(0,0) += aM.M(0,0);
    M(0,1) += aM.M(0,1);
    M(0,2) += aM.M(0,2);

    M(1,0) += aM.M(1,0);
    M(1,1) += aM.M(1,1);
    M(1,2) += aM.M(1,2);

    M(2,0) += aM.M(2,0);
    M(2,1) += aM.M(2,1);
    M(2,2) += aM.M(2,2);
#endif

    return( *this );
}

//==============================================================================

inline
matrix3& matrix3::operator -= ( const matrix3& aM )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &aM );

#if USE_VU0
    asm( "vsub.xyzw DST0, MT10, MT20" : "=j DST0" (m_Col0) : "j MT10" (m_Col0), "j MT20" (aM.m_Col0) );
    asm( "vsub.xyzw DST1, MT11, MT21" : "=j DST1" (m_Col1) : "j MT11" (m_Col1), "j MT21" (aM.m_Col1) );
    asm( "vsub.xyzw DST2, MT12, MT22" : "=j DST2" (m_Col2) : "j MT12" (m_Col2), "j MT22" (aM.m_Col2) );
#else
    M(0,0) -= aM.M(0,0);
    M(0,1) -= aM.M(0,1);
    M(0,2) -= aM.M(0,2);

    M(1,0) -= aM.M(1,0);
    M(1,1) -= aM.M(1,1);
    M(1,2) -= aM.M(1,2);

    M(2,0) -= aM.M(2,0);
    M(2,1) -= aM.M(2,1);
    M(2,2) -= aM.M(2,2);
#endif

    return( *this );
}

//==============================================================================

inline
matrix3& matrix3::operator *= ( const matrix3& aM )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &aM );

    *this = (*this) * aM;
    return( *this );
}

//==============================================================================

#if USE_VU0
inline
matrix3 operator * ( const matrix3& LHS, const matrix3& RHS )
{
    FORCE_ALIGNED_16( &LHS );
    FORCE_ALIGNED_16( &RHS );

    matrix3 Result;
    asm( "vmulaw.xyz  acc,  LHS2, RHS0w
          vmaddaz.xyz acc,  LHS1, RHS0z
          vmadday.xyz acc,  LHS0, RHS0y
          vmaddx.xyz  DST0, LHS0, RHS0x" :
         "=j DST0" (Result.m_Col0) :
         "j  LHS0" (LHS.m_Col0),
         "j  LHS1" (LHS.m_Col1),
         "j  LHS2" (LHS.m_Col2),
         "j  RHS0" (RHS.m_Col0) );
    asm( "vmulaw.xyz  acc,  LHS2, RHS1w
          vmaddaz.xyz acc,  LHS1, RHS1z
          vmadday.xyz acc,  LHS0, RHS1y
          vmaddx.xyz  DST1, LHS0, RHS1x" :
         "=j DST1" (Result.m_Col1) :
         "j  LHS0" (LHS.m_Col0),
         "j  LHS1" (LHS.m_Col1),
         "j  LHS2" (LHS.m_Col2),
         "j  RHS1" (RHS.m_Col1) );
    asm( "vmulaw.xyz  acc,  LHS2, RHS2w
          vmaddaz.xyz acc,  LHS1, RHS2z
          vmadday.xyz acc,  LHS0, RHS2y
          vmaddx.xyz  DST2, LHS0, RHS2x" :
         "=j DST2" (Result.m_Col2) :
         "j  LHS0" (LHS.m_Col0),
         "j  LHS1" (LHS.m_Col1),
         "j  LHS2" (LHS.m_Col2),
         "j  RHS2" (RHS.m_Col2) );

    return Result;
}
#elif defined TARGET_XBOX
__forceinline
matrix3 operator * ( const matrix3& LHS, const matrix3& RHS )
{
    matrix3 Result;
    // Xbox-specific implementation would go here
    // For now, default to standard implementation
    Result.M(0,0) = (LHS.M(0,0)*RHS.M(0,0)) + (LHS.M(1,0)*RHS.M(0,1)) + (LHS.M(2,0)*RHS.M(0,2));
    Result.M(1,0) = (LHS.M(0,0)*RHS.M(1,0)) + (LHS.M(1,0)*RHS.M(1,1)) + (LHS.M(2,0)*RHS.M(1,2));
    Result.M(2,0) = (LHS.M(0,0)*RHS.M(2,0)) + (LHS.M(1,0)*RHS.M(2,1)) + (LHS.M(2,0)*RHS.M(2,2));
    
    Result.M(0,1) = (LHS.M(0,1)*RHS.M(0,0)) + (LHS.M(1,1)*RHS.M(0,1)) + (LHS.M(2,1)*RHS.M(0,2));
    Result.M(1,1) = (LHS.M(0,1)*RHS.M(1,0)) + (LHS.M(1,1)*RHS.M(1,1)) + (LHS.M(2,1)*RHS.M(1,2));
    Result.M(2,1) = (LHS.M(0,1)*RHS.M(2,0)) + (LHS.M(1,1)*RHS.M(2,1)) + (LHS.M(2,1)*RHS.M(2,2));
    
    Result.M(0,2) = (LHS.M(0,2)*RHS.M(0,0)) + (LHS.M(1,2)*RHS.M(0,1)) + (LHS.M(2,2)*RHS.M(0,2));
    Result.M(1,2) = (LHS.M(0,2)*RHS.M(1,0)) + (LHS.M(1,2)*RHS.M(1,1)) + (LHS.M(2,2)*RHS.M(1,2));
    Result.M(2,2) = (LHS.M(0,2)*RHS.M(2,0)) + (LHS.M(1,2)*RHS.M(2,1)) + (LHS.M(2,2)*RHS.M(2,2));
    
    return Result;
}
#else
inline
matrix3 operator * ( const matrix3& LHS, const matrix3& RHS )
{
    matrix3 Result;
    for( s32 i=0; i<3; i++ )
    {
        Result.M(i,0) = (LHS.M(0,0)*RHS.M(i,0)) + (LHS.M(1,0)*RHS.M(i,1)) + (LHS.M(2,0)*RHS.M(i,2));
        Result.M(i,1) = (LHS.M(0,1)*RHS.M(i,0)) + (LHS.M(1,1)*RHS.M(i,1)) + (LHS.M(2,1)*RHS.M(i,2));
        Result.M(i,2) = (LHS.M(0,2)*RHS.M(i,0)) + (LHS.M(1,2)*RHS.M(i,1)) + (LHS.M(2,2)*RHS.M(i,2));
    }
    return Result;
}
#endif

//==============================================================================

inline
matrix3 operator + ( const matrix3& M1, const matrix3& M2 )
{
    FORCE_ALIGNED_16( &M1 );
    FORCE_ALIGNED_16( &M2 );

    matrix3 Result;

#if USE_VU0
    asm( "vadd.xyzw DST0, MT10, MT20" : "=j DST0" (Result.m_Col0) : "j MT10" (M1.m_Col0), "j MT20" (M2.m_Col0) );
    asm( "vadd.xyzw DST1, MT11, MT21" : "=j DST1" (Result.m_Col1) : "j MT11" (M1.m_Col1), "j MT21" (M2.m_Col1) );
    asm( "vadd.xyzw DST2, MT12, MT22" : "=j DST2" (Result.m_Col2) : "j MT12" (M1.m_Col2), "j MT22" (M2.m_Col2) );
#else
    Result.M(0,0) = M1.M(0,0) + M2.M(0,0);
    Result.M(0,1) = M1.M(0,1) + M2.M(0,1);
    Result.M(0,2) = M1.M(0,2) + M2.M(0,2);
    
    Result.M(1,0) = M1.M(1,0) + M2.M(1,0);
    Result.M(1,1) = M1.M(1,1) + M2.M(1,1);
    Result.M(1,2) = M1.M(1,2) + M2.M(1,2);
    
    Result.M(2,0) = M1.M(2,0) + M2.M(2,0);
    Result.M(2,1) = M1.M(2,1) + M2.M(2,1);
    Result.M(2,2) = M1.M(2,2) + M2.M(2,2);
#endif

    return( Result );
}

//==============================================================================

inline
matrix3 operator - ( const matrix3& M1, const matrix3& M2 )
{
    FORCE_ALIGNED_16( &M1 );
    FORCE_ALIGNED_16( &M2 );

    matrix3 Result;

#if USE_VU0
    asm( "vsub.xyzw DST0, MT10, MT20" : "=j DST0" (Result.m_Col0) : "j MT10" (M1.m_Col0), "j MT20" (M2.m_Col0) );
    asm( "vsub.xyzw DST1, MT11, MT21" : "=j DST1" (Result.m_Col1) : "j MT11" (M1.m_Col1), "j MT21" (M2.m_Col1) );
    asm( "vsub.xyzw DST2, MT12, MT22" : "=j DST2" (Result.m_Col2) : "j MT12" (M1.m_Col2), "j MT22" (M2.m_Col2) );
#else
    Result.M(0,0) = M1.M(0,0) - M2.M(0,0);
    Result.M(0,1) = M1.M(0,1) - M2.M(0,1);
    Result.M(0,2) = M1.M(0,2) - M2.M(0,2);
    
    Result.M(1,0) = M1.M(1,0) - M2.M(1,0);
    Result.M(1,1) = M1.M(1,1) - M2.M(1,1);
    Result.M(1,2) = M1.M(1,2) - M2.M(1,2);
    
    Result.M(2,0) = M1.M(2,0) - M2.M(2,0);
    Result.M(2,1) = M1.M(2,1) - M2.M(2,1);
    Result.M(2,2) = M1.M(2,2) - M2.M(2,2);
#endif

    return( Result );
}

//==============================================================================

inline
f32 matrix3::Difference( const matrix3& M ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &M );

#if USE_VU0
    u128 Tmp0;
    u128 Tmp1;
    u128 Tmp2;
    f32  Diff;
    asm( "vsub.xyzw TMP0, MT10, MT20"  : "=j TMP0" (Tmp0) : "j MT10" (m_Col0), "j MT20" (M.m_Col0) );
    asm( "vsub.xyzw TMP1, MT11, MT21"  : "=j TMP1" (Tmp1) : "j MT11" (m_Col1), "j MT21" (M.m_Col1) );
    asm( "vsub.xyzw TMP2, MT12, MT22"  : "=j TMP2" (Tmp2) : "j MT12" (m_Col2), "j MT22" (M.m_Col2) );
    asm( "vmul.xyzw TMP0, VIN,  VIN"   : "=j TMP0" (Tmp0) : "j VIN"  (Tmp0) );
    asm( "vmul.xyzw TMP1, VIN,  VIN"   : "=j TMP1" (Tmp1) : "j VIN"  (Tmp1) );
    asm( "vmul.xyzw TMP2, VIN,  VIN"   : "=j TMP2" (Tmp2) : "j VIN"  (Tmp2) );
    asm( "vadd.xyzw TMP0, VIN0, VIN1"  : "=j TMP0" (Tmp0) : "j VIN0" (Tmp0), "j VIN1" (Tmp1) );
    asm( "vadd.xyzw TMP0, VIN0, VIN2"  : "=j TMP0" (Tmp0) : "j VIN0" (Tmp0), "j VIN2" (Tmp2) );
    asm( "vaddw.x   TMP0, VIN0, VIN0w
          vaddz.x   TMP0, TMP0, VIN0z
          vaddy.x   TMP0, TMP0, VIN0y" : "=j TMP0" (Tmp0) : "j VIN0" (Tmp0) );
    asm( "qmfc2     %0,   %1"          : "=r"      (Diff) : "j"      (Tmp0) );
    return Diff;
#else
    f32 Diff=0.0f;
    for( s32 i=0; i<9; i++ )
        Diff += (((f32*)this)[i] - ((f32*)(&M))[i])*
                (((f32*)this)[i] - ((f32*)(&M))[i]);
    return Diff;
#endif
}

//==============================================================================

inline
xbool matrix3::IsIdentity( void ) const
{
    FORCE_ALIGNED_16( this );

    matrix3 I;
    I.Identity();
    return (this->Difference(I) < 0.0000001f);
}

//==============================================================================

inline
xbool matrix3::InRange( f32 Min, f32 Max ) const
{
    FORCE_ALIGNED_16( this );

    for( s32 i=0; i<9; i++ )
    {
        if( (((f32*)this)[i] < Min) || (((f32*)this)[i] > Max) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================

inline
xbool matrix3::IsValid( void ) const
{
    FORCE_ALIGNED_16( this );

    for( s32 i=0; i<9; i++ )
    {
        if( !x_isvalid( ((f32*)this)[i] ) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================

inline
matrix3 m3_Transpose( const matrix3& M )
{
    FORCE_ALIGNED_16( &M );

    matrix3 Mout = M;
    Mout.Transpose();
    return Mout;
}

//==============================================================================

inline
vector3 matrix3::operator * ( const vector3& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

#if USE_VU0
    u128 Ret;
    asm( "vmulaz.xyz  acc,  COL2, VINz
          vmadday.xyz acc,  COL1, VINy
          vmaddx.xyz  VOUT, COL0, VINx" :
         "=j VOUT" (Ret) :
         "j  VIN"  (V.GetU128()),
         "j  COL0" (m_Col0),
         "j  COL1" (m_Col1),
         "j  COL2" (m_Col2) );
    return vector3( Ret );
#else
    return( vector3( (M(0,0)*V.GetX()) + (M(1,0)*V.GetY()) + (M(2,0)*V.GetZ()),
                     (M(0,1)*V.GetX()) + (M(1,1)*V.GetY()) + (M(2,1)*V.GetZ()),
                     (M(0,2)*V.GetX()) + (M(1,2)*V.GetY()) + (M(2,2)*V.GetZ()) ) );
#endif
}

//==============================================================================

inline
vector3 matrix3::Transform( const vector3& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return( (*this) * V );
}

//==============================================================================

inline
void matrix3::Transform(       vector3* pDest, 
                       const vector3* pSource, 
                             s32      NVerts ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( pDest );
    FORCE_ALIGNED_16( pSource );

    ASSERT( pDest   );
    ASSERT( pSource );

    while( NVerts > 0 )
    {
        *pDest = Transform( *pSource );

        pDest++;
        pSource++;
        NVerts--;
    }
}

//==============================================================================

// Rotates vector by matrix rotation
inline
vector3 matrix3::RotateVector( const vector3& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return Transform(V);
}

//==============================================================================

// Inverse rotates vector by matrix rotation
inline
vector3 matrix3::InvRotateVector( const vector3& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

#if USE_VU0
    u128 Tmp0;
    u128 Tmp1;
    u128 Tmp2;
    u128 Tmp3;
    u128 Row0;
    u128 Row1;
    u128 Row2;
    u128 OutVec;
    asm( "pextlw %0, %1, %2" : "=r" (Tmp0) : "r" (m_Col1), "r" (m_Col0) );
    asm( "pextuw %0, %1, %2" : "=r" (Tmp1) : "r" (m_Col1), "r" (m_Col0) );
    asm( "pextlw %0, $0, %1" : "=r" (Tmp2) : "r" (m_Col2)              );
    asm( "pextuw %0, $0, %1" : "=r" (Tmp3) : "r" (m_Col2)              );
    asm( "pcpyld %0, %1, %2" : "=r" (Row0) : "r" (Tmp2),   "r" (Tmp0)  );
    asm( "pcpyud %0, %1, %2" : "=r" (Row1) : "r" (Tmp0),   "r" (Tmp2)  );
    asm( "pcpyld %0, %1, %2" : "=r" (Row2) : "r" (Tmp3),   "r" (Tmp1)  );
    asm( "vmulaz.xyz  acc,  ROW2, VINz
          vmadday.xyz acc,  ROW1, VINy
          vmaddx.xyz  VOUT, ROW0, VINx" :
         "=j VOUT" (OutVec) :
         "j  VIN"  (V.GetU128()),
         "j  ROW0" (Row0),
         "j  ROW1" (Row1),
         "j  ROW2" (Row2) );
    return vector3( OutVec );
#else
    return( vector3( (M(0,0)*V.GetX()) + (M(0,1)*V.GetY()) + (M(0,2)*V.GetZ()),
                     (M(1,0)*V.GetX()) + (M(1,1)*V.GetY()) + (M(1,2)*V.GetZ()),
                     (M(2,0)*V.GetX()) + (M(2,1)*V.GetY()) + (M(2,2)*V.GetZ()) ) );
#endif
}

//==============================================================================

inline 
matrix3::matrix3( void )
{
    FORCE_ALIGNED_16( this );
}

//==============================================================================

inline 
matrix3::matrix3( const quaternion& Q )
{   
    FORCE_ALIGNED_16( this );

    f32 tx  = 2.0f * Q.X;   // 2x
    f32 ty  = 2.0f * Q.Y;   // 2y
    f32 tz  = 2.0f * Q.Z;   // 2z
    f32 txw =   tx * Q.W;   // 2x * w
    f32 tyw =   ty * Q.W;   // 2y * w
    f32 tzw =   tz * Q.W;   // 2z * w
    f32 txx =   tx * Q.X;   // 2x * x
    f32 tyx =   ty * Q.X;   // 2y * x
    f32 tzx =   tz * Q.X;   // 2z * x
    f32 tyy =   ty * Q.Y;   // 2y * y
    f32 tzy =   tz * Q.Y;   // 2z * y
    f32 tzz =   tz * Q.Z;   // 2z * z
                                
    // Fill out 3x3 rotations.
    M(0,0) = 1.0f-(tyy+tzz); M(0,1) = tyx + tzw;      M(0,2) = tzx - tyw;           
    M(1,0) = tyx - tzw;      M(1,1) = 1.0f-(txx+tzz); M(1,2) = tzy + txw;           
    M(2,0) = tzx + tyw;      M(2,1) = tzy - txw;      M(2,2) = 1.0f-(txx+tyy);    
}

//==============================================================================
//  CLEAR DEFINES
//==============================================================================

#undef M

//==============================================================================