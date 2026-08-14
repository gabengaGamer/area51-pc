//=========================================================================
//
//  ui_joinlist.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_listbox.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "ui_joinlist.hpp"

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

ui_win* ui_joinlist_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_joinlist* pList = new ui_joinlist;
    pList->Create( UserID, pManager, Position, pParent, Flags );

    return (ui_win*)pList;
}

//=========================================================================
//  ui_listbox
//=========================================================================

ui_joinlist::ui_joinlist( void )
{
    for( s32 i = 0; i < NUM_MUTATION_ICONS; i++ )
    {
        MutationIcon[i] = -1;
    }
}

//=========================================================================

ui_joinlist::~ui_joinlist( void )
{
    if( m_pManager )
    {
        m_pManager->UnloadBitmap( "MutantIcon" );
        m_pManager->UnloadBitmap( "HumanIcon" );
        m_pManager->UnloadBitmap( "VersusIcon" );
        m_pManager->UnloadBitmap( "CycleIcon" );
    }
}

//=========================================================================

xbool ui_joinlist::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool const Success = ui_listbox::Create( UserID, pManager, Position, pParent, Flags );

    MutationIcon[ICON_MUTANT] = pManager->LoadBitmap( "MutantIcon", "UI_MutantIcon.xbmp" );
    MutationIcon[ICON_HUMAN]  = pManager->LoadBitmap( "HumanIcon", "UI_HumanIcon.xbmp" );
    MutationIcon[ICON_VS]     = pManager->LoadBitmap( "VersusIcon", "UI_VersusIcon.xbmp" );
    MutationIcon[ICON_CYCLE]  = pManager->LoadBitmap( "CycleIcon", "UI_CycleIcon.xbmp" );

    return Success;
}

//=========================================================================

void ui_joinlist::RenderHeader( irect r )
{
    r.l += 8;
    RenderTitle( r, ui_font::h_left | ui_font::v_center, xwstring( xstring( (char)0x88 ) ) );
    r.l += 14;
    RenderTitle( r, ui_font::h_left | ui_font::v_center, xwstring( xstring( (char)0x85 ) ) );
    r.l += 14;
    RenderTitle( r, ui_font::h_left | ui_font::v_center, xwstring( xstring( (char)0x16 ) ) );
    r.l += 16;
    RenderTitle( r, ui_font::h_left | ui_font::v_center, g_StringTableMgr( "ui", "IDS_HEADER_SERVER" ) );

    irect PlayerCountRect = r;
    PlayerCountRect.l += 91;
    PlayerCountRect.r = PlayerCountRect.l + 84;
    RenderTitle( PlayerCountRect, ui_font::h_right | ui_font::v_center, g_StringTableMgr( "ui", "IDS_HEADER_PLAYERS" ) );

    r.l += 182;
    RenderTitle( r, ui_font::h_left | ui_font::v_center, g_StringTableMgr( "ui", "IDS_HEADER_MUTANT_MODE" ) );
    r.l += 66;
    RenderTitle( r, ui_font::h_left | ui_font::v_center, g_StringTableMgr( "ui", "IDS_HEADER_MAPNAME" ) );
}

//=========================================================================

void ui_joinlist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const char* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_joinlist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const xwchar* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_joinlist::RenderTitle( irect r, u32 Flags, const xwchar* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, XCOLOR_BLACK, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, m_HeaderColor, pString );
}

//=========================================================================

void ui_joinlist::RenderItem( irect r, const item& Item, const xcolor& c1, const xcolor& c2 )
{
    if (m_ShowFrame)
        r.Deflate( 1, 1 );

    r.Translate( 0, -2 );

    irect rIcons   = r;
    irect rName    = r;
    irect rPlayers = r;
    irect rMode    = r;
    irect rMap     = r;

    rIcons.l   = r.l +   5;
    rIcons.r   = r.l +  47;
    rName.l    = r.l +  49;
    rName.r    = r.l + 172;
    rPlayers.l = r.l + 172;
    rPlayers.r = r.l + 225;
    rMode.l    = r.l + 230;
    rMode.r    = rMode.l + 16;
    rMap.l     = r.l + 297;
    rMap.r     = r.r - 4;

    const server_info* pServerInfo = g_MatchMgr.GetServerInfo(Item.Data[0]);

    ASSERT( pServerInfo );
    {
        xstring Name;
        xstring Players;
        xwchar MapName[128];

        Name     = pServerInfo->Name;
        Players  = xfs( "%d/%d", pServerInfo->Players, pServerInfo->MaxPlayers );
        x_wstrncpy( MapName, pServerInfo->MissionName, ( sizeof( MapName ) / sizeof( xwchar ) ) - 1 );
        MapName[( sizeof( MapName ) / sizeof( xwchar ) ) - 1] = 0;

        if( pServerInfo->Flags & SERVER_VOICE_ENABLED )
        {
            RenderString( rIcons  , ui_font::h_left  |ui_font::v_center,                        c1, c2, xstring( (char)0x88 ) );
        }

        rIcons.l += 14;
        if( pServerInfo->Flags & SERVER_HAS_BUDDY )
        {
            RenderString( rIcons  , ui_font::h_left  |ui_font::v_center,                        c1, c2, xstring( (char)0x85 ) );
        }

        rIcons.l += 14;
        if( pServerInfo->Flags & SERVER_HAS_PASSWORD )
        {
            RenderString( rIcons  , ui_font::h_left  |ui_font::v_center,                        c1, c2, xstring( (char)0x16 ) );
        }


        RenderString( rName   , ui_font::h_left  |ui_font::v_center|ui_font::clip_ellipsis, c1, c2, (const char*)Name );
        RenderString( rPlayers, ui_font::h_center|ui_font::v_center|ui_font::clip_ellipsis, c1, c2, (const char*)Players );
        
        // set mutant icon color
        xcolor iconColor;
        if( c1 == XCOLOR_GREY )
        {
            iconColor = XCOLOR_GREY;
        }
        else if( c1 == xcolor(0,0,0,255) )
        {
            iconColor = XCOLOR_GREY;
        }
        else
        {
            iconColor = XCOLOR_WHITE;
        }

        //render icons based on mutation mode
        switch( pServerInfo->MutationMode )
        {
            case MUTATE_CHANGE_AT_WILL:
                m_pManager->RenderBitmap( MutationIcon[ICON_HUMAN],  rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_CYCLE],  rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_MUTANT], rMode, iconColor );
                break;

            case MUTATE_HUMAN_LOCK:
                m_pManager->RenderBitmap( MutationIcon[ICON_HUMAN],  rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_HUMAN],  rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_HUMAN],  rMode, iconColor );
                break;

            case MUTATE_MUTANT_LOCK:
                m_pManager->RenderBitmap( MutationIcon[ICON_MUTANT], rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_MUTANT], rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_MUTANT], rMode, iconColor );
                break;

            case MUTATE_HUMAN_VS_MUTANT:
                m_pManager->RenderBitmap( MutationIcon[ICON_HUMAN],  rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_VS],     rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_MUTANT], rMode, iconColor );
                break;

            case MUTATE_MAN_HUNT:
                m_pManager->RenderBitmap( MutationIcon[ICON_HUMAN],  rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_MUTANT], rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_MUTANT], rMode, iconColor );
                break;

            case MUTATE_MUTANT_HUNT:
                m_pManager->RenderBitmap( MutationIcon[ICON_MUTANT], rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_HUMAN],  rMode, iconColor );
                rMode.Translate( 16, 0 );
                m_pManager->RenderBitmap( MutationIcon[ICON_HUMAN],  rMode, iconColor );
                break;
        }
        
        RenderString( rMap    , ui_font::h_left  |ui_font::v_center|ui_font::clip_ellipsis, c1, c2, MapName );
    }
}

//=========================================================================
