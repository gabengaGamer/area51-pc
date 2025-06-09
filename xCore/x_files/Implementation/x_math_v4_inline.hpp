//==============================================================================
//
//  x_math_v4_inline.hpp
//
//==============================================================================

#ifndef X_MATH_V4_INLINE_HPP
#define X_MATH_V4_INLINE_HPP
#else
#error "File " __FILE__ " has been included twice!"
#endif

#ifndef X_DEBUG_HPP
#include "../x_debug.hpp"
#endif

//==============================================================================

inline 
vector4::vector4( void )
{
    FORCE_ALIGNED_16( this );
}

//==============================================================================

inline 
vector4::vector4( const vector3& V )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    GetX() = V.GetX();
    GetY() = V.GetY();
    GetZ() = V.GetZ();
    GetW() = 0.0f;
}

//==============================================================================

inline 
vector4::vector4( f32 aX, f32 aY, f32 aZ, f32 aW )
{
    FORCE_ALIGNED_16( this );

    GetX() = aX;
    GetY() = aY;
    GetZ() = aZ;
    GetW() = aW;
}

//==============================================================================

inline
f32 vector4::GetX( void ) const
{
    return X;
}

//==============================================================================

inline
f32& vector4::GetX( void )
{
    return X;
}

//==============================================================================

inline
f32 vector4::GetY( void ) const
{
    return Y;
}

//==============================================================================

inline
f32& vector4::GetY( void )
{
    return Y;
}

//==============================================================================

inline
f32 vector4::GetZ( void ) const
{
    return Z;
}

//==============================================================================

inline
f32& vector4::GetZ( void )
{
    return Z;
}

//==============================================================================

inline
f32 vector4::GetW( void ) const
{
    return W;
}

//==============================================================================

inline
f32& vector4::GetW( void )
{
    return W;
}

//==============================================================================

inline
s32 vector4::GetIW( void ) const
{
    return iW;
}

//==============================================================================

inline
s32& vector4::GetIW( void )
{
    return iW;
}

//==============================================================================

inline 
void vector4::Set( f32 aX, f32 aY, f32 aZ, f32 aW )
{
    FORCE_ALIGNED_16( this );

    GetX() = aX;
    GetY() = aY;
    GetZ() = aZ;
    GetW() = aW;
}

//==============================================================================

inline 
void vector4::operator () ( f32 aX, f32 aY, f32 aZ, f32 aW )
{
    FORCE_ALIGNED_16( this );

    GetX() = aX;
    GetY() = aY;
    GetZ() = aZ;
    GetW() = aW;
}

//==============================================================================

inline
f32 vector4::operator [] ( s32 Index ) const
{
    FORCE_ALIGNED_16( this );

    ASSERT( (Index >= 0) && (Index < 4) );
    return( ((f32*)this)[Index] );
}

//==============================================================================

inline
f32& vector4::operator [] ( s32 Index )
{
    FORCE_ALIGNED_16( this );

    ASSERT( (Index >= 0) && (Index < 4) );
    return( ((f32*)this)[Index] );
}

//==============================================================================

inline 
void vector4::Zero( void )
{
    FORCE_ALIGNED_16( this );

    GetX() = GetY() = GetZ() = GetW() = 0.0f;
}

//==============================================================================

inline 
void vector4::Negate( void )
{
    FORCE_ALIGNED_16( this );

    GetX() = -GetX();
    GetY() = -GetY();
    GetZ() = -GetZ();
    GetW() = -GetW();
}

//==============================================================================

inline 
void vector4::Normalize( void )
{
    FORCE_ALIGNED_16( this );

    f32 N = x_1sqrt(GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ() + GetW()*GetW());
    ASSERT( x_isvalid(N) );

    GetX() *= N;
    GetY() *= N;
    GetZ() *= N;
    GetW() *= N;
}

//==============================================================================

inline 
void vector4::NormalizeAndScale( f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    f32 N = x_1sqrt(GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ() + GetW()*GetW());
    ASSERT( x_isvalid(N) );

    N      *= Scalar;
    GetX() *= N;
    GetY() *= N;
    GetZ() *= N;
    GetW() *= N;
}

//==============================================================================

inline 
xbool vector4::SafeNormalize( void )
{
    FORCE_ALIGNED_16( this );

    f32 N = x_1sqrt(GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ() + GetW()*GetW());

    if( x_isvalid(N) )
    {
        GetX() *= N;
        GetY() *= N;
        GetZ() *= N;
        GetW() *= N;
        return TRUE;
    }
    else
    {
        GetX() = 0;
        GetY() = 0;
        GetZ() = 0;
        GetW() = 0;
        return FALSE;
    }
}

//==============================================================================

inline 
xbool vector4::SafeNormalizeAndScale( f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    f32 N = x_1sqrt(GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ() + GetW()*GetW());

    if( x_isvalid(N) )
    {
        N  *= Scalar;
        GetX() *= N;
        GetY() *= N;
        GetZ() *= N;
        GetW() *= N;
        return TRUE;
    }
    else
    {
        GetX() = 0.0f;
        GetY() = 0.0f;
        GetZ() = 0.0f;
        GetW() = 0.0f;
        return FALSE;
    }
}

//==============================================================================

inline 
void vector4::Scale( f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    GetX() *= Scalar; 
    GetY() *= Scalar; 
    GetZ() *= Scalar;
    GetW() *= Scalar;
}

//==============================================================================

inline 
f32 vector4::Length( void ) const
{
    FORCE_ALIGNED_16( this );

    return( x_sqrt( GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ() + GetW()*GetW() ) );
}

//==============================================================================

inline
f32 vector4::LengthSquared( void ) const
{
    FORCE_ALIGNED_16( this );

    return( GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ() + GetW()*GetW() );
}

//==============================================================================

inline
vector4::operator const f32* ( void ) const
{
    FORCE_ALIGNED_16( this );
    return( (f32*)this );
}

//==============================================================================

inline 
vector4 vector4::operator - ( void ) const
{
    FORCE_ALIGNED_16( this );
    return( vector4( -GetX(), -GetY(), -GetZ(), -GetW() ) );
}

//==============================================================================

inline 
const vector4& vector4::operator = ( const vector3& V )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    GetX() = V.GetX();
    GetY() = V.GetY();
    GetZ() = V.GetZ();
    GetW() = 0.0f;
    return( *this );
}

//==============================================================================

inline 
vector4& vector4::operator += ( const vector4& V )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    GetX() += V.GetX();
    GetY() += V.GetY();
    GetZ() += V.GetZ();
    GetW() += V.GetW();
    return( *this );
}

//==============================================================================

inline 
vector4& vector4::operator -= ( const vector4& V )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    GetX() -= V.GetX();
    GetY() -= V.GetY();
    GetZ() -= V.GetZ();
    GetW() -= V.GetW();
    return( *this );
}

//==============================================================================

inline 
vector4& vector4::operator *= ( const f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    GetX() *= Scalar; 
    GetY() *= Scalar; 
    GetZ() *= Scalar;
    GetW() *= Scalar;
    return( *this );
}

//==============================================================================

inline 
vector4& vector4::operator /= ( const f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    ASSERT( Scalar != 0.0f );
    f32 d = 1.0f / Scalar;

    GetX() *= d; 
    GetY() *= d; 
    GetZ() *= d;
    GetW() *= d;
    return( *this );
}

//==============================================================================

inline 
xbool vector4::operator == ( const vector4& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return( (GetX() == V.GetX()) && (GetY() == V.GetY()) && (GetZ() == V.GetZ()) && (GetW() == V.GetW()) );
}

//==============================================================================

inline 
xbool vector4::operator != ( const vector4& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return( (GetX() != V.GetX()) || (GetY() != V.GetY()) || (GetZ() != V.GetZ()) || (GetW() != V.GetW()) );
}

//==============================================================================

inline 
vector4 operator + ( const vector4& V1, const vector4& V2 )
{
    FORCE_ALIGNED_16( &V1 );
    FORCE_ALIGNED_16( &V2 );

    return( vector4( V1.GetX() + V2.GetX(),
                     V1.GetY() + V2.GetY(),
                     V1.GetZ() + V2.GetZ(),
                     V1.GetW() + V2.GetW() ) );
}

//==============================================================================

inline 
vector4 operator - ( const vector4& V1, const vector4& V2 )
{
    FORCE_ALIGNED_16( &V1 );
    FORCE_ALIGNED_16( &V2 );

    return( vector4( V1.GetX() - V2.GetX(),
                     V1.GetY() - V2.GetY(),
                     V1.GetZ() - V2.GetZ(),
                     V1.GetW() - V2.GetW() ) );
}

//==============================================================================

inline 
vector4 operator / ( const vector4& V, f32 S )
{
    FORCE_ALIGNED_16( &V );

    ASSERT( (S > 0.00001f) || (S < -0.00001f) );
    S = 1.0f / S;

    return( vector4( V.GetX() * S, 
                     V.GetY() * S, 
                     V.GetZ() * S, 
                     V.GetW() * S ) );
}

//==============================================================================

inline 
vector4 operator * ( const vector4& V, f32 S )
{
    FORCE_ALIGNED_16( &V );

    return( vector4( V.GetX() * S, 
                     V.GetY() * S, 
                     V.GetZ() * S, 
                     V.GetW() * S ) );
}

//==============================================================================

inline 
vector4 operator * ( f32 S, const vector4& V )
{
    FORCE_ALIGNED_16( &V );

    return( vector4( V.GetX() * S, 
                     V.GetY() * S, 
                     V.GetZ() * S, 
                     V.GetW() * S ) );
}

//==============================================================================

inline
vector4 operator * ( const vector4& V1, const vector4& V2 )
{
    FORCE_ALIGNED_16( &V1 );
    FORCE_ALIGNED_16( &V2 );

    return( vector4( V1.GetX() * V2.GetX(),
                     V1.GetY() * V2.GetY(),
                     V1.GetZ() * V2.GetZ(),
                     V1.GetW() * V2.GetW() ) );
}

//==============================================================================

inline
f32 vector4::Dot( const vector4& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return (V.GetX()*GetX() + V.GetY()*GetY() + V.GetZ()*GetZ() + V.GetW()*GetW());
}

//==============================================================================

inline
f32 vector4::Difference( const vector4& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return (*this - V).LengthSquared();
}

//==============================================================================

inline
xbool vector4::InRange( f32 aMin, f32 aMax ) const
{
    FORCE_ALIGNED_16( this );

    return( (GetX()>=aMin) && (GetX()<=aMax) &&
            (GetY()>=aMin) && (GetY()<=aMax) &&
            (GetZ()>=aMin) && (GetZ()<=aMax) &&
            (GetW()>=aMin) && (GetW()<=aMax) );
}

//==============================================================================

inline
xbool vector4::IsValid( void ) const
{
    FORCE_ALIGNED_16( this );

    return( x_isvalid(GetX()) && x_isvalid(GetY()) && x_isvalid(GetZ()) && x_isvalid(GetW()) );
}

//==============================================================================

