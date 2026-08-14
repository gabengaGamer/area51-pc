//=========================================================================
//
//  ui_friendlist.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_listbox.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "ui_friendlist.hpp"

#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"

#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/GameMgr.hpp"

//=========================================================================
//  Defines
//=========================================================================

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

//=========================================================================
//  Factory function
//=========================================================================

ui_win* ui_friendlist_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_friendlist* pList = new ui_friendlist;
    pList->Create( UserID, pManager, Position, pParent, Flags );

    return (ui_win*)pList;
}

//=========================================================================
//  ui_listbox
//=========================================================================

ui_friendlist::ui_friendlist( void )
{
    for( s32 i = 0; i < NUM_PRESENCE_ICONS; i++ )
    {
        m_IconID[i] = -1;
    }
    m_IsFriendsList = TRUE;
}

//=========================================================================

ui_friendlist::~ui_friendlist( void )
{
}

//=========================================================================

xbool ui_friendlist::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool const Success = ui_listbox::Create( UserID, pManager, Position, pParent, Flags );

    m_IconID[ICON_FRIEND]          = pManager->FindBitmap( "icon_friend" );
    m_IconID[ICON_VOICE_ON]        = pManager->FindBitmap( "icon_voice_on" );
    m_IconID[ICON_VOICE_MUTED]     = pManager->FindBitmap( "icon_voice_muted" );
    m_IconID[ICON_VOICE_THRU_TV]   = pManager->FindBitmap( "icon_voice_thru_tv" );
    m_IconID[ICON_VOICE_SPEAKING]  = pManager->FindBitmap( "icon_voice_speaking" );
    m_IconID[ICON_FRIEND_REQ_SENT] = pManager->FindBitmap( "icon_friend_req_sent" );
    m_IconID[ICON_FRIEND_REQ_RCVD] = pManager->FindBitmap( "icon_friend_req_rcvd" );
    m_IconID[ICON_INVITE_SENT]     = pManager->FindBitmap( "icon_invite_sent" );
    m_IconID[ICON_INVITE_RCVD]     = pManager->FindBitmap( "icon_invite_rcvd" );

    return Success;
}

//=========================================================================

void ui_friendlist::RenderHeader( irect r )
{
    (void)r;
}

//=========================================================================

void ui_friendlist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const char* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_friendlist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const xwchar* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_friendlist::RenderItem( irect r, const item& Item, const xcolor& c1, const xcolor& c2 )
{
    r.Deflate( 4, 0 );
    r.Translate( 1, -2 );

    // render the item based on the flags
    if( Item.Flags & FLAG_ITEM_SEPARATOR )
    {
        // render separator
        RenderString( r, ui_font::h_center|ui_font::v_center, c1, c2, xstring("--------------------------------") );
    }
    else
    {
        // render current player information in the list
        buddy_info* pBuddy = (buddy_info*)Item.Data[0];

        // render presence icon
        r.l += 8;
        r.r = r.l + 26;  
        irect r2 = r;
        r2.t += 3;
        r2.b -= 3;

        // Is there any buddy information existing about this player?
        if( pBuddy )
        {
            xbool IsInviteAnswered = (pBuddy->Flags & USER_REJECTED_INVITE) ||
                                     (pBuddy->Flags & USER_ACCEPTED_INVITE);

            // Display the highest-priority presence state.
            if( pBuddy->Flags & USER_HAS_INVITE )
            {
                m_pManager->RenderBitmap( m_IconID[ICON_INVITE_RCVD], r2, c1 );
            }
            else if( pBuddy->Flags & USER_REQUEST_RECEIVED )
            {
                m_pManager->RenderBitmap( m_IconID[ICON_FRIEND_REQ_RCVD], r2, c1 );
            }
            else if( (pBuddy->Flags & USER_SENT_INVITE) && !IsInviteAnswered )
            {
                m_pManager->RenderBitmap( m_IconID[ICON_INVITE_SENT], r2, c1 );
            }
            else if( pBuddy->Flags & USER_REQUEST_SENT )
            {
                m_pManager->RenderBitmap( m_IconID[ICON_FRIEND_REQ_SENT], r2, c1 );
            }
            else if( pBuddy->Status == BUDDY_STATUS_ONLINE )
            {
                m_pManager->RenderBitmap( m_IconID[ICON_FRIEND], r2, c1 );
            }
            else if( pBuddy->Status == BUDDY_STATUS_INGAME )
            {
                m_pManager->RenderBitmap( m_IconID[ICON_FRIEND], r2, c1 );
            }
        }

        // render player name
        r.l += 40;
        r.r = r.l + 300;
        RenderString( r, ui_font::h_center|ui_font::v_center, c1, c2, Item.Label );

        // render voice icon
        r.l += 311;
        r.r = r.l + 26;
        r2 = r;
        r2.t += 3;
        r2.b -= 3;
        if( pBuddy )
        {
            if( m_IsFriendsList == TRUE )
            {
                // We are displaying the friends list so user will have valid voice
                // status in the Flags field.
                if( pBuddy->Flags & USER_VOICE_ENABLED )
                {
                    m_pManager->RenderBitmap( m_IconID[ ICON_VOICE_ON ], r2, c1 );
                }
            }
            else
            {
                if( pBuddy->Flags & FRIENDLIST_IS_MUTED )
                {
                    m_pManager->RenderBitmap( m_IconID[ ICON_VOICE_MUTED ], r2, c1 );
                }
                else
                {
                    if( pBuddy->Flags & FRIENDLIST_IS_VOICE_ALLOWED )
                    {
                        s32 Icon = (pBuddy->Flags & FRIENDLIST_IS_VOICE_CAPABLE) ? ICON_VOICE_ON : ICON_VOICE_THRU_TV;

                        if( pBuddy->Flags & FRIENDLIST_IS_TALKING )
                            Icon = ICON_VOICE_SPEAKING;

                        m_pManager->RenderBitmap( m_IconID[ Icon ], r2, c1 );
                    }
                }
            }
        }
    }
}

//=========================================================================

void ui_friendlist::Configure( xbool IsFriendsList )
{
    m_IsFriendsList = IsFriendsList;
}

//=========================================================================
