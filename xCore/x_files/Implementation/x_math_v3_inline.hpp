//==============================================================================
//
//  x_math_v3_inline.hpp
//
//==============================================================================

#ifndef X_MATH_V3_INLINE_HPP
#define X_MATH_V3_INLINE_HPP
#else
#error "File " __FILE__ " has been included twice!"
#endif

#ifndef X_DEBUG_HPP
#include "../x_debug.hpp"
#endif

//==============================================================================

inline
f32 vector3::GetX( void ) const
{
    return X;
}

//==============================================================================

inline
f32& vector3::GetX( void )
{
    return X;
}

//==============================================================================

inline
f32 vector3::GetY( void ) const
{
    return Y;
}

//==============================================================================

inline
f32& vector3::GetY( void )
{
    return Y;
}

//==============================================================================

inline
f32 vector3::GetZ( void ) const
{
    return Z;
}

//==============================================================================

inline
f32& vector3::GetZ( void )
{
    return Z;
}

//==============================================================================

inline
s32 vector3::GetIW( void ) const
{
    return ((s32*)this)[3];
}

//==============================================================================

inline
s32& vector3::GetIW( void )
{
    return ((s32*)this)[3];
}

//==============================================================================

inline
vector3& vector3::Snap( f32 GridX, f32 GridY, f32 GridZ )
{
    FORCE_ALIGNED_16( this );

    GetX() = x_round( GetX(), GridX );
    GetY() = x_round( GetY(), GridY );
    GetZ() = x_round( GetZ(), GridZ );

    return *this;
}

//==============================================================================

inline 
vector3::vector3( void )
{
    FORCE_ALIGNED_16( this );
}

//==============================================================================

inline 
vector3::vector3( f32 aX, f32 aY, f32 aZ )
{
    FORCE_ALIGNED_16( this );

    GetX() = aX;
    GetY() = aY;
    GetZ() = aZ;
}

//==============================================================================

inline 
void vector3::RotateX( radian Angle )
{
    FORCE_ALIGNED_16( this );

    radian S, C;
    x_sincos( Angle, S, C );

    f32    y, z;
    y      = GetY();
    z      = GetZ();
    GetY() = (C * y) - (S * z);
    GetZ() = (C * z) + (S * y);
}

//==============================================================================

inline 
void vector3::RotateY( radian Angle )
{
    FORCE_ALIGNED_16( this );

    radian S, C;
    x_sincos( Angle, S, C );

    f32    x, z;
    x   = GetX();
    z   = GetZ();
    GetX() = (C * x) + (S * z);
    GetZ() = (C * z) - (S * x);
}

//==============================================================================

inline 
void vector3::RotateZ( radian Angle )
{
    FORCE_ALIGNED_16( this );

    radian S, C;
    x_sincos( Angle, S, C );

    f32    x, y;
    x      = GetX();
    y      = GetY();
    GetX() = (C * x) - (S * y);
    GetY() = (C * y) + (S * x);
}

//==============================================================================

inline 
void vector3::Rotate( const radian3& R )
{
    FORCE_ALIGNED_16( this );

    f32 sx, cx;
    f32 sy, cy;
    f32 sz, cz;

    x_sincos( R.Pitch, sx, cx );
    x_sincos( R.Yaw,   sy, cy );
    x_sincos( R.Roll,  sz, cz );

    // fill out 3x3 rotations...this would give you the columns of a rotation
    // matrix that is roll, pitch, then yaw
    f32 sxsz = sx * sz;
    f32 sxcz = sx * cz;

    vector3 Mat0( cy*cz+sy*sxsz,   cx*sz,  cy*sxsz-sy*cz );
    vector3 Mat1( sy*sxcz-sz*cy,   cx*cz,  sy*sz+sxcz*cy );
    vector3 Mat2( cx*sy,           -sx,    cx*cy         );

    f32 x = GetX();
    f32 y = GetY();
    f32 z = GetZ();

    GetX() = Mat0.GetX()*x + Mat1.GetX()*y + Mat2.GetX()*z;
    GetY() = Mat0.GetY()*x + Mat1.GetY()*y + Mat2.GetY()*z;
    GetZ() = Mat0.GetZ()*x + Mat1.GetZ()*y + Mat2.GetZ()*z;
}


//==============================================================================

inline 
void vector3::Set( f32 aX, f32 aY, f32 aZ )
{
    FORCE_ALIGNED_16( this );

    GetX() = aX; 
    GetY() = aY; 
    GetZ() = aZ;
}

//==============================================================================

inline
void vector3::Set( radian Pitch, radian Yaw )
{
    FORCE_ALIGNED_16( this );

    // If you take the vector (0,0,1) and pass it through a rotation matrix
    // created from pitch and yaw, this would be the resulting vector.
    // By knowing that the x and y components of the vector are 0, we have
    // removed lots of extra operations.
    f32 PS, PC;
    f32 YS, YC;

    x_sincos( Pitch, PS, PC );
    x_sincos( Yaw,   YS, YC );

    Set( (YS * PC), -(PS), (YC * PC) );
}

//==============================================================================

inline
void vector3::Set( const radian3& R )
{
    FORCE_ALIGNED_16( this );

    // Rotations are applied in the order roll, pitch, and yaw, and setting
    // this vector with a radian3 has the effect of rotating the vector (0,0,1)
    // by R. Which means the roll is useless.
    Set( R.Pitch, R.Yaw );
}

//==============================================================================

inline
vector3::vector3( radian Pitch, radian Yaw )
{
    FORCE_ALIGNED_16( this );

    Set( Pitch, Yaw );
}

//==============================================================================

inline
vector3::vector3( const radian3& R )
{
    FORCE_ALIGNED_16( this );

    Set( R );
}

//==============================================================================

inline 
void vector3::operator () ( f32 aX, f32 aY, f32 aZ )
{
    FORCE_ALIGNED_16( this );

    GetX() = aX; 
    GetY() = aY; 
    GetZ() = aZ;
}

//==============================================================================

inline
f32 vector3::operator [] ( s32 Index ) const
{
    FORCE_ALIGNED_16( this );

    ASSERT( (Index >= 0) && (Index < 3) );
    return( ((f32*)this)[Index] );
}

//==============================================================================

inline
f32& vector3::operator [] ( s32 Index )
{
    FORCE_ALIGNED_16( this );

    ASSERT( (Index >= 0) && (Index < 3) );
    return( ((f32*)this)[Index] );
}

//==============================================================================

inline 
void vector3::Zero( void )
{
    FORCE_ALIGNED_16( this );

    GetX() = GetY() = GetZ() = 0.0f;
}

//==============================================================================

inline 
void vector3::Negate( void )
{
    FORCE_ALIGNED_16( this );

    GetX() = -GetX(); 
    GetY() = -GetY(); 
    GetZ() = -GetZ();
}

//==============================================================================

inline
void vector3::Min( f32 Min )
{
    GetX() = MIN( GetX(), Min );
    GetY() = MIN( GetY(), Min );
    GetZ() = MIN( GetZ(), Min );
}

//==============================================================================

inline
void vector3::Max( f32 Max )
{
    GetX() = MAX( GetX(), Max );
    GetY() = MAX( GetY(), Max );
    GetZ() = MAX( GetZ(), Max );
}

//==============================================================================

inline
void vector3::Min( const vector3& V )
{
    GetX() = MIN( GetX(), V.GetX() );
    GetY() = MIN( GetY(), V.GetY() );
    GetZ() = MIN( GetZ(), V.GetZ() );
}

//==============================================================================

inline
void vector3::Max( const vector3& V )
{
    GetX() = MAX( GetX(), V.GetX() );
    GetY() = MAX( GetY(), V.GetY() );
    GetZ() = MAX( GetZ(), V.GetZ() );
}

//==============================================================================

inline
void vector3::Normalize( void )
{
    FORCE_ALIGNED_16( this );

    f32 LengthSquared = GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ();
    if( LengthSquared > 0.0000001f )
    {
        f32 N = x_1sqrt( LengthSquared );
        ASSERT( x_isvalid(N) );

        GetX() *= N;
        GetY() *= N;
        GetZ() *= N;
    }
}

//==============================================================================

inline
void vector3::NormalizeAndScale( f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    f32 F = GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ();
    if( F ) // F was often found to be zero
    {
        f32 N = x_1sqrt( F );
        ASSERT( x_isvalid( N ));
        N      *= Scalar;
        GetX() *= N;
        GetY() *= N;
        GetZ() *= N;
    }
}

//==============================================================================

inline 
xbool vector3::SafeNormalize( void )
{
    FORCE_ALIGNED_16( this );

    f32 N = x_1sqrt(GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ());

    if( x_isvalid(N) )
    {
        GetX() *= N;
        GetY() *= N;
        GetZ() *= N;
        return TRUE;
    }
    else
    {
        GetX() = 0.0f;
        GetY() = 0.0f;
        GetZ() = 0.0f;
        return FALSE;
    }
}

//==============================================================================

inline 
xbool vector3::SafeNormalizeAndScale( f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    f32 N = x_1sqrt(GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ());

    if( x_isvalid(N) )
    {
        N   *= Scalar;
        GetX() *= N;
        GetY() *= N;
        GetZ() *= N;
        return TRUE;
    }
    else
    {
        GetX() = 0.0f;
        GetY() = 0.0f;
        GetZ() = 0.0f;
        return FALSE;
    }
}

//==============================================================================

inline 
void vector3::Scale( f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    ASSERT( x_isvalid( Scalar ) );

    GetX() *= Scalar; 
    GetY() *= Scalar; 
    GetZ() *= Scalar;
}

//==============================================================================

inline 
f32 vector3::Length( void ) const
{
    FORCE_ALIGNED_16( this );

    return( x_sqrt( GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ() ) );
}

//==============================================================================

inline
f32 vector3::LengthSquared( void ) const
{
    FORCE_ALIGNED_16( this );

    return( GetX()*GetX() + GetY()*GetY() + GetZ()*GetZ() );
}

//==============================================================================

inline 
f32 vector3::Dot( const vector3& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return( (GetX() * V.GetX()) + (GetY() * V.GetY()) + (GetZ() * V.GetZ()) );
}

//==============================================================================

inline 
vector3 vector3::Cross( const vector3& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return( vector3( (GetY() * V.GetZ()) - (GetZ() * V.GetY()),
                     (GetZ() * V.GetX()) - (GetX() * V.GetZ()),
                     (GetX() * V.GetY()) - (GetY() * V.GetX()) ) );
}

//==============================================================================

inline
vector3 vector3::Reflect( const vector3& Normal ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &Normal );

    return *this - 2*(Normal.Dot(*this)) * Normal;
}

//==============================================================================

inline 
f32 v3_Dot( const vector3& V1, const vector3& V2 )
{
    FORCE_ALIGNED_16( &V1 );
    FORCE_ALIGNED_16( &V2 );

    return( (V1.GetX() * V2.GetX()) + (V1.GetY() * V2.GetY()) + (V1.GetZ() * V2.GetZ()) );
}

//==============================================================================

inline 
vector3 v3_Cross( const vector3& V1, const vector3& V2 )
{
    FORCE_ALIGNED_16( &V1 );
    FORCE_ALIGNED_16( &V2 );

    return( vector3( (V1.GetY() * V2.GetZ()) - (V1.GetZ() * V2.GetY()),
                     (V1.GetZ() * V2.GetX()) - (V1.GetX() * V2.GetZ()),
                     (V1.GetX() * V2.GetY()) - (V1.GetY() * V2.GetX()) ) );
}

//==============================================================================

inline 
radian vector3::GetPitch( void ) const
{
    FORCE_ALIGNED_16( this );

    f32 L = (f32)x_sqrt( GetX()*GetX() + GetZ()*GetZ() );
    return( -x_atan2( GetY(), L ) );
}

//==============================================================================

inline 
radian vector3::GetYaw( void ) const
{
    FORCE_ALIGNED_16( this );

    return( x_atan2( GetX(), GetZ() ) );
}

//==============================================================================

inline 
void vector3::GetPitchYaw( radian& Pitch, radian& Yaw ) const
{
    FORCE_ALIGNED_16( this );

    Pitch = GetPitch();
    Yaw   = GetYaw();
}

//==============================================================================
//           * Start
//           |
//           <--------------(* this )
//           | Return Vector
//           |
//           |
//           * End
//
// Such: 
//
// this.GetClosestVToLSeg(a,b).LengthSquared(); // Is the length square to the line segment
// this.GetClosestVToLSeg(a,b) + this;          // Is the closest point in to the line segment
// this.GetClosestPToLSegRatio(a,b)             // Is the ratio (0=a to 1=b) of the closest
//                                                 point in line segment.
//
//==============================================================================

inline 
vector3 vector3::GetClosestVToLSeg( const vector3& Start, const vector3& End ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &Start );
    FORCE_ALIGNED_16( &End );

    vector3 Diff = *this - Start;
    vector3 Dir  = End   - Start;
    f32     T    = Diff.Dot( Dir );

    if( T > 0.0f )
    {
        f32 SqrLen = Dir.Dot( Dir );

        if ( T >= SqrLen )
        {
            Diff -= Dir;
        }
        else
        {
            T    /= SqrLen;
            Diff -= T * Dir;
        }
    }

    return -Diff;
}

//==============================================================================

inline
f32 vector3::GetSqrtDistToLineSeg( const vector3& Start, const vector3& End ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &Start );
    FORCE_ALIGNED_16( &End );

    return GetClosestVToLSeg(Start,End).LengthSquared();
}

//==============================================================================

inline
vector3 vector3::GetClosestPToLSeg( const vector3& Start, const vector3& End ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &Start );
    FORCE_ALIGNED_16( &End );

    return GetClosestVToLSeg(Start,End) + *this;
}

//==============================================================================
// Given the line defined by the points A and B, returns the ratio (0->1) of the 
// closest point on the line to P. 
// 0 = A
// 1 = B
// >0 and <1 means somewhere inbetween A and B

inline
f32 vector3::GetClosestPToLSegRatio( const vector3& A, const vector3& B ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &A );
    FORCE_ALIGNED_16( &B );

    // Get direction vector and length squared of line
    vector3 AB = B - A ;
    f32     L  = AB.LengthSquared() ;
    
    // Is line a point?
    if (L == 0.0f)
        return 0.0f ;

    // Get vector from start of line to point
    vector3 AP = (*this) - A ;

    // Calculate: Cos(theta)*|AP|*|AB|
    f32 Dot = AP.Dot(AB) ;

    // Before start?
    if (Dot <= 0)
        return 0.0f ;

    // After end?
    if (Dot >= L)
        return 1.0f ;

    // Let distance along AB = X = (|AP| dot |AB|) / |AB|
    // Ratio T is X / |AB| = (|AP| dot |AB|) / (|AB| * |AB|)
    f32 T = Dot / L ;
    ASSERT(T >= 0.0f) ;
    ASSERT(T <= 1.0f) ;

    // Slerp between A and B
    return T ;
}

//==============================================================================

inline
vector3::operator const f32* ( void ) const
{
    return( (f32*)this );
}

//==============================================================================

inline 
vector3 vector3::operator - ( void ) const
{
    FORCE_ALIGNED_16( this );

    return( vector3( -GetX(), -GetY(), -GetZ() ) );
}

//==============================================================================

inline 
const vector3& vector3::operator = ( const vector4& V )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    GetX() = V.GetX();
    GetY() = V.GetY();
    GetZ() = V.GetZ();
    return( *this );
}

//==============================================================================

inline 
vector3& vector3::operator += ( const vector3& V )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    GetX() += V.GetX();
    GetY() += V.GetY();
    GetZ() += V.GetZ();
    return( *this );
}

//==============================================================================

inline 
vector3& vector3::operator -= ( const vector3& V )
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    GetX() -= V.GetX();
    GetY() -= V.GetY();
    GetZ() -= V.GetZ();
    return( *this );
}

//==============================================================================

inline 
vector3& vector3::operator *= ( const f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    GetX() *= Scalar; 
    GetY() *= Scalar; 
    GetZ() *= Scalar;
    return( *this );
}

//==============================================================================

inline 
vector3& vector3::operator /= ( const f32 Scalar )
{
    FORCE_ALIGNED_16( this );

    ASSERT( Scalar != 0.0f );
    f32 d = 1.0f / Scalar;

    GetX() *= d; 
    GetY() *= d; 
    GetZ() *= d;
    return( *this );
}

//==============================================================================

inline 
xbool vector3::operator == ( const vector3& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return( (GetX() == V.GetX()) && (GetY() == V.GetY()) && (GetZ() == V.GetZ()) );
}

//==============================================================================

inline 
xbool vector3::operator != ( const vector3& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return( (GetX() != V.GetX()) || (GetY() != V.GetY()) || (GetZ() != V.GetZ()) );
}

//==============================================================================

inline 
vector3 operator + ( const vector3& V1, const vector3& V2 )
{
    FORCE_ALIGNED_16( &V1 );
    FORCE_ALIGNED_16( &V2 );

    return( vector3( V1.GetX() + V2.GetX(), 
                     V1.GetY() + V2.GetY(), 
                     V1.GetZ() + V2.GetZ() ) );
}

//==============================================================================
inline
vector3 operator * ( const vector3& V1, const vector3& V2 )
{
    FORCE_ALIGNED_16( &V1 );
    FORCE_ALIGNED_16( &V2 );

    return( vector3( V1.GetX() * V2.GetX(),
                     V1.GetY() * V2.GetY(),
                     V1.GetZ() * V2.GetZ() ) );
}

//==============================================================================

inline 
vector3 operator - ( const vector3& V1, const vector3& V2 )
{
    FORCE_ALIGNED_16( &V1 );
    FORCE_ALIGNED_16( &V2 );

    return( vector3( V1.GetX() - V2.GetX(),
                     V1.GetY() - V2.GetY(),
                     V1.GetZ() - V2.GetZ() ) );
}

//==============================================================================

inline 
vector3 operator / ( const vector3& V, f32 S )
{
    FORCE_ALIGNED_16( &V );

    ASSERT( S != 0.0f );
    S = 1.0f / S;

    return( vector3( V.GetX() * S, 
                     V.GetY() * S, 
                     V.GetZ() * S ) );
}

//==============================================================================

inline 
vector3 operator * ( const vector3& V, f32 S )
{
    FORCE_ALIGNED_16( &V );

    return( vector3( V.GetX() * S, 
                     V.GetY() * S, 
                     V.GetZ() * S ) );
}

//==============================================================================

inline 
vector3 operator * ( f32 S, const vector3& V )
{
    FORCE_ALIGNED_16( &V );

    return( vector3( V.GetX() * S, 
                     V.GetY() * S, 
                     V.GetZ() * S ) );
}

//==============================================================================

inline
radian v3_AngleBetween ( const vector3& V1, const vector3& V2 )
{
    FORCE_ALIGNED_16( &V1 );
    FORCE_ALIGNED_16( &V2 );

    f32 D, Cos;
    
    D = V1.Length() * V2.Length();
    
    if( D == 0.0f )  return( R_0 );
    
    Cos = v3_Dot( V1, V2 ) / D;
    
    if     ( Cos >  1.0f )  Cos =  1.0f;
    else if( Cos < -1.0f )  Cos = -1.0f;
    
    return( x_acos( Cos ) );
}

//==============================================================================

inline
void  vector3::GetRotationTowards( const vector3& DestV,
                                         vector3& RotAxis, 
                                         radian&  RotAngle ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &DestV );
    FORCE_ALIGNED_16( &RotAxis );

    // Get lengths of vectors
    f32 D = this->Length() * DestV.Length();
    
    // vectors are zero length
    if( D == 0.0f )  
    {
        RotAxis = vector3(1,0,0);
        RotAngle = 0;
        return;
    }
    
    // Get unit dot product of the vectors
    f32 Dot = this->Dot(DestV) / D;
    
    if     ( Dot >  1.0f )  Dot =  1.0f;
    else if( Dot < -1.0f )  Dot = -1.0f;
    
    // Get axis to rotate about
    RotAxis = v3_Cross( *this, DestV );

    if( RotAxis.SafeNormalize() )
    {
        RotAngle = x_acos(Dot);
    }
    else
    {
        RotAxis = vector3(1,0,0);

        if( Dot > 0 )
            RotAngle = 0;       // Facing same direction
        else
            RotAngle = R_180;   // Facing opposite directions
    }
}

//==============================================================================

inline
f32 vector3::Difference( const vector3& V ) const
{
    FORCE_ALIGNED_16( this );
    FORCE_ALIGNED_16( &V );

    return (*this - V).LengthSquared();
}

//==============================================================================

inline
xbool vector3::InRange( f32 aMin, f32 aMax ) const
{
    FORCE_ALIGNED_16( this );

    return( (GetX()>=aMin) && (GetX()<=aMax) &&
            (GetY()>=aMin) && (GetY()<=aMax) &&
            (GetZ()>=aMin) && (GetZ()<=aMax) );
}

//==============================================================================

inline
xbool vector3::IsValid( void ) const
{
    FORCE_ALIGNED_16( this );

    return( x_isvalid(GetX()) && x_isvalid(GetY()) && x_isvalid(GetZ()) );
}

//==============================================================================

inline
vector3p::vector3p( void )
{
}

//==============================================================================

inline
vector3p::vector3p( f32 aX, f32 aY, f32 aZ )
{
    Set( aX, aY, aZ );
}

//==============================================================================

inline
vector3p::vector3p( const vector3& V )
{
    Set( V.GetX(), V.GetY(), V.GetZ() );
}

//==============================================================================

inline
void vector3p::Set( f32 aX, f32 aY, f32 aZ )
{
    X = aX;
    Y = aY;
    Z = aZ;
}

//==============================================================================

inline
const vector3p& vector3p::operator = ( const vector3& V )
{
    Set( V.GetX(), V.GetY(), V.GetZ() );
    return( *this );
}

//==============================================================================

inline
vector3p::operator const vector3( void ) const
{
    return vector3( X, Y, Z );
}

//==============================================================================
inline
f32 vector3::ClosestPointToRectangle( 
    const vector3& P0,                      // Origin from the edges. 
    const vector3& E0, 
    const vector3& E1, 
    vector3&       OutClosestPoint ) const
{
    vector3 kDiff    = P0 - *this;
    f32     fA00     = E0.LengthSquared();
    f32     fA11     = E1.LengthSquared();
    f32     fB0      = kDiff.Dot( E0 );
    f32     fB1      = kDiff.Dot( E1 );
    f32     fS       = -fB0;
    f32     fT       = -fB1;
    f32     fSqrDist = kDiff.LengthSquared();

    if( fS < 0.0f )
    {
        fS = 0.0f;
    }
    else if( fS <= fA00 )
    {
        fS /= fA00;
        fSqrDist += fB0*fS;
    }
    else
    {
        fS = 1.0f;
        fSqrDist += fA00 + 2.0f*fB0;
    }

    if( fT < 0.0f )
    {
        fT = 0.0f;
    }
    else if( fT <= fA11 )
    {
        fT /= fA11;
        fSqrDist += fB1*fT;
    }
    else
    {
        fT = 1.0f;
        fSqrDist += fA11 + 2.0f*fB1;
    }

    // Set the closest point
    OutClosestPoint = P0 + (E0 * fS) + (E1 * fT);

    return x_abs(fSqrDist);
}

