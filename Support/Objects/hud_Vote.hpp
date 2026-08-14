//==============================================================================
//
//  hud_Vote.hpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

#ifndef HUD_VOTE_HPP
#define HUD_VOTE_HPP

//==============================================================================
// INCLUDES
//==============================================================================

#include "hud_Renderable.hpp"
#include "Render/Texture.hpp"
#include "ResourceMgr/ResourceMgr.hpp"

//==============================================================================
// CLASS
//==============================================================================

class hud_vote : public hud_renderable
{
public:
                    hud_vote        ( void );
    virtual        ~hud_vote        ( void ) {};

    virtual void    OnRender        ( player*       pPlayer );
    virtual void    OnAdvanceSimulation  ( player*       pPlayer, f32 DeltaTime );

    xbool           IsActive        ( void );

protected:
    rhandle<texture>        m_ControllerBmp;

    xbool                   m_bKeyOn;
    f32                     m_KeyOpacity;

    xbool                   m_bTallyOn;
    xbool                   m_CanVote;
    xbool                   m_VoteInProgress;
    f32                     m_TallyOpacity;

    f32                     m_PercentOnScreen;
    f32                     m_PercentOpen;

    f32                     m_YesPercentage;
    f32                     m_NoPercentage;

    s32                     m_YesVotes;
    s32                     m_NoVotes;
    s32                     m_MissingVotes;
    f32                     m_PercentNeeded;
    const xwchar*           m_pVoteSub;
    const xwchar*           m_pVoteType;

    f32                     m_TargetPercentage;
    f32                     m_LeftMargin;
    f32                     m_RightMargin;
    f32                     m_TopMargin;
    f32                     m_BottomMargin;
};

//==============================================================================
#endif
//==============================================================================


