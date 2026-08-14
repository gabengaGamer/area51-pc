//=============================================================================
//
//  SurfaceColor.cpp
//
//=============================================================================

#include "SurfaceColor.hpp"

//=============================================================================

xbool render::SampleSurfaceColor( object::detail_tri const& triangle, vector3 const& position, xcolor& color )
{
    vector3 const& position0 = triangle.Vertex[0];
    vector3 const& position1 = triangle.Vertex[1];
    vector3 const& position2 = triangle.Vertex[2];

    vector3 scaledNormal = v3_Cross( position1 - position0, position2 - position0 );
    f32 const lengthSquared = scaledNormal.LengthSquared();
    if ( lengthSquared <= 1e-8f )
    {
        return FALSE;
    }
    scaledNormal *= 1.0f / lengthSquared;

    vector3 const barycentric(
        v3_Cross( position2 - position1, position - position1 ).Dot( scaledNormal ),
        v3_Cross( position0 - position2, position - position2 ).Dot( scaledNormal ),
        v3_Cross( position1 - position0, position - position0 ).Dot( scaledNormal ) );

    xcolor const& color0 = triangle.Color[0];
    xcolor const& color1 = triangle.Color[1];
    xcolor const& color2 = triangle.Color[2];
    vector3 sampledColor( barycentric.GetX() * color0.R + barycentric.GetY() * color1.R +
                              barycentric.GetZ() * color2.R,
                          barycentric.GetX() * color0.G + barycentric.GetY() * color1.G +
                              barycentric.GetZ() * color2.G,
                          barycentric.GetX() * color0.B + barycentric.GetY() * color1.B +
                              barycentric.GetZ() * color2.B );
    sampledColor.Min( 255.0f );
    sampledColor.Max( 0.0f );

    color.Set( static_cast<u8>( sampledColor.GetX() ), static_cast<u8>( sampledColor.GetY() ),
               static_cast<u8>( sampledColor.GetZ() ), 255 );
    return TRUE;
}

//=============================================================================
