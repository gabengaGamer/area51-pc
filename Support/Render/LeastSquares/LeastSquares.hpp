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

class least_squares
{
public:
    least_squares( void );
    ~least_squares( void );

    enum    { MAX_POLYNOMIAL_DEGREE = 3 };

    void    Setup       ( s32 PolynomialDegree = MAX_POLYNOMIAL_DEGREE );
    void    AddSample   ( f32 X, f32 Y );
    xbool   Solve       ( void );
    f32     GetCoeff    ( s32 Index );
    void    SetCoeff    ( s32 Index, f32 Value );
    f32     Evaluate    ( f32 X );

protected:
    s32             m_nSamples;
    s32             m_Degree;
    xbool           m_bSolved;
    f64             m_Matrix[MAX_POLYNOMIAL_DEGREE+1][MAX_POLYNOMIAL_DEGREE+1];
    f64             m_Vector[MAX_POLYNOMIAL_DEGREE+1];
    f64             m_Coeffs[MAX_POLYNOMIAL_DEGREE+1];
};

#endif // __LEASTSQUARES_HPP__