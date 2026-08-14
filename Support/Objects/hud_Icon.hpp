//==============================================================================
//
//  hud_Icon.hpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

#ifndef HUD_ICON_HPP
#define HUD_ICON_HPP

//==============================================================================
// INCLUDES
//==============================================================================

#include "Obj_mgr/obj_mgr.hpp"
#include "Render/Texture.hpp"
#include "Objects/Player/Player.hpp"

#include "hud_Renderable.hpp"

#define NUM_ICONS 64

//==============================================================================
// CLASS
//==============================================================================

enum icon_type
{
    ICON_ALLY,
    ICON_ENEMY,
    ICON_BLANK1,
    ICON_WAYPOINT,

    ICON_CNH_OUTER,
    ICON_CNH_INNER,
    ICON_SPEAKING,
    ICON_BASE,

    ICON_FLAG_INNER,
    ICON_FLAG_OUTER,
};

enum gutter_type
{
    GUTTER_NONE,
    GUTTER_ELLIPSE,
    GUTTER_RECTANGLE,
};

struct icon_inf 
{
    icon_type       IconType; 
    vector3         FocusPosition;
    vector3         RenderPosition;
    xbool           bAlignToBottom;
    gutter_type     GutterType; 
    xcolor          Color; 
    xbool           Pulsing; 
    xbool           Distance; 
    xwchar          Label[32];
    s32             PlayerNum;
    f32             Opacity;
    f32             IconFadeDist;
    f32             TextFadeDist;
};

class hud_icon : public hud_renderable
{
public:
    hud_icon      ( void );
    virtual        ~hud_icon      ( void ) {};
    void    Init            ( void );
    void    Kill            ( void );

    virtual void    OnRender        ( player*   pPlayer );
    virtual xbool   OnProperty      ( prop_query& rPropQuery );
    virtual void    OnEnumProp      ( prop_enum&  List );

    void    BeginSimulationSnapshot( void );
    void    SubmitIcon      (       icon_type     IconType,
        const vector3&      FocusPosition,
        const vector3&      RenderPosition,
        xbool         bAlignToBottom,
        gutter_type   GutterType, 
        xcolor        Color,
        const xwchar*       pCharName,
        xbool         Pulsing,
        xbool         Distance, 
        f32           Opacity,
        f32           IconFadeDist = -1.0f,
        f32           TextFadeDist = -1.0f,
        s32           PlayerNum = -1
        );
    void    CommitSimulationSnapshot( void );
    void    SetPlayerIconTarget( s32 PlayerNum, xbool Visible, xbool IsAlly );

    void    RenderIcon      ( player* pPlayer, const icon_inf& Icon );
    f32     GetOpacity      ( f32 DistFromCenter, f32 Opacity, f32 FadeDist );

    //------------------------------------------------------------------------------
    // Public Storage
public:
    s32                     m_NumActiveIcons;
    icon_inf                m_Icons[ NUM_ICONS ];
    s32                     m_NumStagingIcons;
    icon_inf                m_StagingIcons[ NUM_ICONS ];
    xbool                   m_PlayerIconVisible[32];
    xbool                   m_PlayerIconIsAlly[32];
    f32                     m_PlayerIconOpacity[32];
    f32                     m_PlayerIconSightDelay[32];

    xbool                   m_Active;
    rhandle<texture>        m_ScreenEdgeBmp;
    rhandle<texture>        m_ScreenCenterBmp;
    xcolor                  m_ScreenEdgeColor;
    xcolor                  m_ScreenCenterColor;
};

#endif


