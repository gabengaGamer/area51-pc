#include "StdAfx.h"
#include "Axis3d.hpp"
#include "Render/PrimitiveDebug.hpp"

//=========================================================================
// FUNCTIONS
//=========================================================================

//=========================================================================

axis3d::axis3d( void )
{
    L2W.Identity();
}

//=========================================================================

void axis3d::SetPosition( vector3& Pos )
{   
    L2W.SetTranslation( Pos );
}


//=========================================================================

void axis3d::Render( void )
{
    ASSERT( eng_InBeginEnd() );
    VERIFY( render::debug::Axis( L2W, 65.0f, render::PRIMITIVE_DEPTH_READ_ONLY ) );
}
