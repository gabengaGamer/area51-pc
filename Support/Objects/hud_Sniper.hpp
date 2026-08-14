//==============================================================================
//
//  hud_Sniper.hpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

#ifndef HUD_SNIPER_HPP
#define HUD_SNIPER_HPP

//==============================================================================
// INCLUDES
//==============================================================================

#include "Obj_mgr/obj_mgr.hpp"
#include "Render/Texture.hpp"
#include "Objects/Player/Player.hpp"

#include "hud_Renderable.hpp"

//==============================================================================
// CLASS
//==============================================================================

class hud_sniper : public hud_renderable
{
public:
                    hud_sniper      ( void ) {}
    virtual        ~hud_sniper      ( void ) {}

    virtual void    OnRender        ( player*       pPlayer );
    virtual void    OnAdvanceSimulation  ( player*       pPlayer, f32 DeltaTime );
    virtual xbool   OnProperty      ( prop_query&   rPropQuery );
    virtual void    OnEnumProp      ( prop_enum&    List );

//------------------------------------------------------------------------------
// Public Storage
public:    
    static xcolor               m_SniperHudColor;
    static xcolor               m_SniperScanLineColor;
    static xcolor               m_SniperTrackerLineColor;
    static xcolor               m_SniperZoomTrackerColor;

    xwstring                    m_WeaponZoomLevel;
    xwstring                    m_WeaponZoomDistance;

    static rhandle<texture>     m_SniperHud;
    static rhandle<texture>     m_SniperStencilHud;
    static rhandle<texture>     m_SniperTrackerLine;
    static rhandle<texture>     m_SniperScanLine;
    static rhandle<texture>     m_SniperZoomPitchTracker;
};

#endif
