//==============================================================================
//
//  LeastSquares.hpp
//
//  Copyright (c) 2004 Inevitable Entertainment Inc. All rights reserved.
//
//  This class is used to create a least-squares polynomial approximation for
//  a given set of data.
//
//==============================================================================

//------------------------------------------------------------------------------
// WARNING WARNING WARNING WARNING WARNING
// THIS CODE USES 64-BIT PRECISION FLOATS. DO NOT USE THIS CODE DURING
// GAMEPLAY. SETUP OR LOAD TIME ONLY!!!!!!
//------------------------------------------------------------------------------

#ifndef __LEASTSQUARES_HPP__
#define __LEASTSQUARES_HPP__

#include "x_types.hpp"

//==============================================================================

class LeastSquares
{
  public:
    LeastSquares( void );
    ~LeastSquares( void );

    enum
    {
        MAX_POLYNOMIAL_DEGREE = 3
    };

    void  Setup( s32 polynomialDegree = MAX_POLYNOMIAL_DEGREE );
    void  AddSample( f32 x, f32 y );
    xbool Solve( void );
    f32   GetCoeff( s32 index );
    void  SetCoeff( s32 index, f32 value );
    f32   Evaluate( f32 x );

  protected:
    s32   m_nSamples;
    s32   m_degree;
    xbool m_isSolved;
    f64   m_matrix[MAX_POLYNOMIAL_DEGREE + 1][MAX_POLYNOMIAL_DEGREE + 1];
    f64   m_vector[MAX_POLYNOMIAL_DEGREE + 1];
    f64   m_coeffs[MAX_POLYNOMIAL_DEGREE + 1];
};

#endif // __LEASTSQUARES_HPP__