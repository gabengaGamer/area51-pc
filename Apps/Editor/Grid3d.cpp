
#include "BaseStdAfx.h"
#include "Grid3d.hpp"
#include "Render/PrimitiveDebug.hpp"

//=========================================================================
// FUNCTIONS
//=========================================================================


//=========================================================================
grid3d::grid3d( void )
{
    m_Color.Set( 0, 0, 128, 128);
    m_SeparationX = 100;        // One meter separation
    m_SeparationZ = 100;
    m_SizeX       = 10000;
    m_SizeZ       = 10000;
    m_OffSetX     = 0;
    m_OffSetZ     = 0;
    m_OffSetY     = 0;
}

//=========================================================================

void grid3d::SetColor( xcolor GridColor )
{
    m_Color = GridColor;
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
                                 render::PRIMITIVE_DEPTH_READ_ONLY ) );
}
