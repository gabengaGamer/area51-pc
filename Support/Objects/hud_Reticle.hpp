//==============================================================================
//
//  hud_Reticle.hpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

#ifndef HUD_RETICLE_HPP
#define HUD_RETICLE_HPP

//==============================================================================
// INCLUDES
//==============================================================================

#include "Obj_mgr/obj_mgr.hpp"
#include "Objects/Player/Player.hpp"
#include "Render/Texture.hpp"

#include "hud_Renderable.hpp"

//==============================================================================
// CLASS
//==============================================================================

class hud_reticle : public hud_renderable
{
public:
                    hud_reticle     ( void );
    virtual        ~hud_reticle     ( void ) {};

    virtual void    OnRender        ( player*       pPlayer );
    virtual void    OnAdvanceSimulation( player* pPlayer, f32 DeltaTime );
    virtual xbool   OnProperty      ( prop_query&   rPropQuery );
    virtual void    OnEnumProp      ( prop_enum&    List );

private:
    guid                    m_WeaponGuid;
    f32                     m_AimDegradation;
    xbool                   m_ShouldRender;
    xbool                   m_IsZoomed;
    xbool                   m_UsesMutationReticle;
    xbool                   m_UsesCardinalEdges;
    xbool                   m_HasLiveEnemyTarget;
    rhandle<texture>        m_MutationReticle;

};

#endif
