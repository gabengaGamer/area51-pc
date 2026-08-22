//==============================================================================
//
//  fx_Sprite.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "fx_Sprite.hpp"
#include "fx_Render.hpp"
#include "x_profile.hpp"
#include "e_Engine.hpp"

//==============================================================================
//  STATIC-LIBRARY REGISTRATION ANCHOR
//==============================================================================

s32 fx_Sprite;

//==============================================================================
//  FUNCTIONS
//==============================================================================

void fx_sprite::AdvanceLogic( const fx_effect_base* pEffect, f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "fx_sprite::AdvanceLogic" );

    (void)DeltaTime;

    // All we need to do here is update the bounding box.

    f32     Scale    = pEffect->GetUniformScale() * MAX( m_Scale.X, m_Scale.Y );
    vector3 Position = pEffect->GetL2W()          * m_Translate;

    m_BBox.Set( Position, Scale );
}

//==============================================================================

void fx_sprite::SubmitRender( const fx_effect_base* pEffect ) const
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "fx_sprite::SubmitRender" );

    if( pEffect->IsSuspended() )
        return;

    const fx_edef_sprite& SpriteDef = (fx_edef_sprite&)(*m_pElementDef);

    vector3  Position = pEffect->GetL2W() * m_Translate;
    f32      UniScale = pEffect->GetUniformScale();
    vector2  WH( UniScale * m_Scale.X, UniScale * m_Scale.Y );
    vector2  UV0( 0.0f, 0.0f );
    vector2  UV1( 1.0f, 1.0f );

    if( SpriteDef.ZBias != 0.0f )
    {
        const view* pView = eng_GetView();
        if( !pView )
            return;

        Position += pView->GetViewZ() * SpriteDef.ZBias;
    }

    const texture* pDiffuse = pEffect->GetDiffuseTexture( SpriteDef.BitmapIndex );

    if( pDiffuse )
    {
        const render::primitive_draw_desc Material =
            fx_CreateMaterial( *pDiffuse, SpriteDef.CombineMode, SpriteDef.ReadZ,
                               render::PRIMITIVE_SAMPLER_LINEAR_CLAMP );

        VERIFY( render::SubmitPrimitiveSprite( Material,
                                               Position,
                                               WH,
                                               UV0,
                                               UV1,
                                               m_Color,
                                               m_Rotate.Roll ) );
    }

#ifndef X_RETAIL
    fx_element::SubmitRender( pEffect );
#endif // X_RETAIL
}

//==============================================================================

#undef new
REGISTER_FX_ELEMENT_CLASS( fx_sprite, "SPRITE", DefaultElementMemoryFn );

//==============================================================================
