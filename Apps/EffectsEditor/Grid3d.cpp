
#include "StdAfx.h"
#include "Grid3d.hpp"
#include "Render/PrimitiveDebug.hpp"

//=========================================================================
// FUNCTIONS
//=========================================================================


//=========================================================================
grid3d::grid3d( void )
{
    m_Color.Set( 64, 64, 192, 128 );
    m_Color.Set( 255, 255, 255, 64 );
    m_SeparationX = 100;        // One meter separation
    m_SeparationZ = 100;
    m_SizeX       = 2500; //10000;
    m_SizeZ       = 2500; //10000;
    m_OffSetX     = 0;
    m_OffSetZ     = 0;
    m_OffSetY     = 0;
    m_Plane.Setup( vector3(0,0,0), vector3(0,1,0) );
}

//=========================================================================

void grid3d::SetTranslations ( vector3& Pos )
{
    m_OffSetX = Pos.GetX();
    m_OffSetY = Pos.GetY();
    m_OffSetZ = Pos.GetZ();
}

//=========================================================================

void grid3d::SetSeparation( f32 X, f32 Z )
{
    ASSERT( X >= 0 );
    ASSERT( Z >= 0 );
    m_SeparationX = X;
    m_SeparationZ = Z;
}

//=========================================================================

void grid3d::SetSize( f32 X, f32 Z )
{
    ASSERT( X >= 0 );
    ASSERT( Z >= 0 );
    m_SizeX = X;
    m_SizeZ = Z;
}

//=========================================================================

void grid3d::Render( void )
{
    ASSERT( eng_InBeginEnd() );
    if( (m_SeparationX <= 0.0f) || (m_SeparationZ <= 0.0f) )
        return;

    const vector3 Corner( m_OffSetX - m_SizeX, m_OffSetY, m_OffSetZ - m_SizeZ );
    const vector3 EdgeX( m_SizeX * 2.0f, 0.0f, 0.0f );
    const vector3 EdgeZ( 0.0f, 0.0f, m_SizeZ * 2.0f );
    const s32 XDivisions = MAX( 1, (s32)x_ceil( (m_SizeX * 2.0f) / m_SeparationX ) );
    const s32 ZDivisions = MAX( 1, (s32)x_ceil( (m_SizeZ * 2.0f) / m_SeparationZ ) );
    VERIFY( render::debug::Grid( Corner, EdgeX, EdgeZ, m_Color, XDivisions, ZDivisions,
                                 render::PRIMITIVE_DEPTH_DISABLED ) );

    const xcolor AxisColor( 255, 255, 255, 128 );
    VERIFY( render::debug::Line( vector3( m_OffSetX, m_OffSetY, m_OffSetZ - m_SizeZ ),
                                 vector3( m_OffSetX, m_OffSetY, m_OffSetZ + m_SizeZ ),
                                 AxisColor,
                                 render::PRIMITIVE_DEPTH_DISABLED ) );
    VERIFY( render::debug::Line( vector3( m_OffSetX - m_SizeX, m_OffSetY, m_OffSetZ ),
                                 vector3( m_OffSetX + m_SizeX, m_OffSetY, m_OffSetZ ),
                                 AxisColor,
                                 render::PRIMITIVE_DEPTH_DISABLED ) );
}
