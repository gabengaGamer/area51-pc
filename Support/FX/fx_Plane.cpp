//==============================================================================
//
//  fx_Plane.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "fx_Plane.hpp"
#include "fx_Render.hpp"
#include "x_profile.hpp"

//==============================================================================
//  STATIC-LIBRARY REGISTRATION ANCHOR
//==============================================================================

s32 fx_Plane;

//==============================================================================
//  FUNCTIONS
//==============================================================================

void fx_plane::AdvanceLogic( const fx_effect_base* pEffect, f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "fx_plane::AdvanceLogic" );

    (void)DeltaTime;

    // All we need to do here is update the bounding box.

    matrix4 L2W;

    L2W.Setup( m_Scale, m_Rotate, m_Translate );
    L2W = pEffect->GetL2W() * L2W;

    m_BBox.Set( vector3(0,0,0), 0.5f );
    m_BBox.Transform( L2W );
}

//==============================================================================

void fx_plane::SubmitRender( const fx_effect_base* pEffect ) const
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "fx_plane::SubmitRender" );

    if( pEffect->IsSuspended() )
        return;

    const fx_edef_plane& PlaneDef = (fx_edef_plane&)(*m_pElementDef);

    matrix4 L2W;

    L2W.Setup( m_Scale, m_Rotate, m_Translate );
    L2W = pEffect->GetL2W() * L2W;

    vector3 v3TL( -0.5f,  0.5f,  0.0f );  // Top left
    vector3 v3TR(  0.5f,  0.5f,  0.0f );  // Top right
    vector3 v3BR(  0.5f, -0.5f,  0.0f );  // Bottom right
    vector3 v3BL( -0.5f, -0.5f,  0.0f );  // Bottom left

    f32 TU = PlaneDef.TileU;
    f32 TV = PlaneDef.TileV;
    f32 SU = x_fmod( (PlaneDef.ScrollU * pEffect->GetAge()), TU );
    f32 SV = x_fmod( (PlaneDef.ScrollV * pEffect->GetAge()), TV );

    vector2 uvTL( 0.0f + SU, 0.0f + SV );  // Top left 
    vector2 uvTR(  TU  + SU, 0.0f + SV );  // Top right
    vector2 uvBR(  TU  + SU,  TV  + SV );  // Bottom right
    vector2 uvBL( 0.0f + SU,  TV  + SV );  // Bottom left    

    const texture* pDiffuse = pEffect->GetDiffuseTexture( PlaneDef.BitmapIndex );

    if( pDiffuse )
    {
        const vector3 Positions[4] = { v3TL, v3TR, v3BR, v3BL };
        const vector2 UVs[4]       = { uvTL, uvTR, uvBR, uvBL };
        const xcolor  Colors[4]    = { m_Color, m_Color, m_Color, m_Color };
        const s16     Indices[6]   = { 0, 3, 2, 2, 1, 0 };

        const render::primitive_draw_desc Material =
            fx_CreateMaterial( *pDiffuse, PlaneDef.CombineMode, PlaneDef.ReadZ,
                               render::PRIMITIVE_SAMPLER_LINEAR_WRAP );

        VERIFY( fx_SubmitMesh( Material,
                               L2W,
                               Positions,
                               UVs,
                               Colors,
                               4,
                               Indices,
                               6,
                               FX_MESH_TRIANGLE_LIST ) );
    }

#ifndef X_RETAIL
    fx_element::SubmitRender( pEffect );
#endif // X_RETAIL
}

//==============================================================================

#undef new
REGISTER_FX_ELEMENT_CLASS( fx_plane, "PLANE", DefaultElementMemoryFn );

//==============================================================================
