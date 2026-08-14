//==============================================================================
//
//  StickBone.cpp
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================
#include "StickBone.hpp"
#include "Entropy.hpp"


//==============================================================================
// CLASSES
//==============================================================================

stick_bone::stick_bone()
{
    m_pName = NULL ;
    m_Color = XCOLOR_BLACK ;
    m_L2W.Identity() ;
    m_Start.Zero() ;
    m_End.Zero() ;
}

//==============================================================================

void stick_bone::Init( const char* pName, xcolor Color )
{
    m_pName = pName ;
    m_Color = Color ;
}

//==============================================================================

#ifdef X_DEBUG

void stick_bone::Render( void )
{
    // Lookup matrix values
    vector3& AxisX = *(vector3*)(&m_L2W(0,0)) ;
    vector3& AxisY = *(vector3*)(&m_L2W(1,0)) ;
    vector3& AxisZ = *(vector3*)(&m_L2W(2,0)) ;

    // Draw bone
    render::debug::Line( m_Start, m_End, m_Color, render::PRIMITIVE_DEPTH_DISABLED );

    // Draw an axis at the center
    vector3 P = (m_Start + m_End) / 2 ;
    render::debug::Line( P, P + (AxisX * 2.5f), XCOLOR_RED, render::PRIMITIVE_DEPTH_DISABLED );
    render::debug::Line( P, P + (AxisY * 2.5f), XCOLOR_GREEN, render::PRIMITIVE_DEPTH_DISABLED );
    render::debug::Line( P, P + (AxisZ * 2.5f), XCOLOR_BLUE, render::PRIMITIVE_DEPTH_DISABLED );
}

#endif

//==============================================================================
