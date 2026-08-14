//==============================================================================
//
//  PlayerHud.hpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================
#include "hud_Player.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/GameMgr.hpp"
#include "NetworkMgr/Voice/VoiceMgr.hpp"
#include "../SaveData/SaveDataMgr.hpp"
#include "HudObject.hpp"
#include "StringMgr/StringMgr.hpp"
#include "GameLib/RenderContext.hpp"
#include "UI/ui_renderer.hpp"

#ifndef X_EDITOR
#include "NetworkMgr/MatchMgr.hpp"
#include "NetGhost.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#endif

#define ENEMY_ANGLE_LIMIT   R_10        // View range of enemy to see an icon
#define ENEMY_RANGE_LIMIT   1500.0f     // Range that forces enemy icon to always be on

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
player_hud::player_hud( void )
{
    m_LocalSlot  = -1;
    m_PlayerSlot = 0;
    m_XPos       = 0.0f;
    m_YPos       = 0.0f;
    m_Width      = 0.0f;
    m_Height     = 0.0f;
    m_CenterX    = 0.0f;
    m_CenterY    = 0.0f;
    m_ViewDimensions.Clear();
    m_Type       = 0;

    m_NotificationIcon   = HUD_NOTIFICATION_NONE;
    m_ReceivedFriendReq  = FALSE;
    m_ReceivedGameInvite = FALSE;
    m_AutoSaveing        = FALSE;
    m_InvitationAlpha    = 0;
    m_AutoSaveingAlpha   = 0;
    m_InvitationState    = 0;
    m_AutoSaveState      = 0;
    m_InvitationHoldTime = 0.0f;
    m_AutoSaveHoldTime   = 0.0f;

    m_HowTextTimer = 0.0f;
    m_HowLastMode = -1;
    m_HowTextAlpha = 0;

    m_HowGlobalBMP.SetName  ( PRELOAD_FILE( "hud_multiplayer_headset_global.xbmp"   ) );
    m_HowTeamBMP.SetName    ( PRELOAD_FILE( "hud_multiplayer_headset_team.xbmp"     ) );
    m_HowLocalBMP.SetName   ( PRELOAD_FILE( "hud_multiplayer_headset_local.xbmp"    ) );

    m_WhoBMP.SetName        ( PRELOAD_FILE( "hud_multiplayer_player_speakr.xbmp"    ) );

    m_FriendReqBMP.SetName  ( PRELOAD_PS2_FILE( "UI_PS2_Icon_Friend_Req_Rcvd.xbmp"      ) ); // TODO: CHANGE THIS ON PC
    m_GameInviteBMP.SetName ( PRELOAD_PS2_FILE( "UI_PS2_Icon_Invite_Rcvd.xbmp"          ) );
    m_AutoSave.SetName      ( PRELOAD_PS2_FILE( "UI_PS2_autosave.xbmp"                  ) );

    x_memset( m_HudComponents,0,sizeof(hud_renderable*)*HUD_ELEMENT_NUM_ELEMENTS );

};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void player_hud::OnRender( void )
{
    // Lookup owner
    player* pPlayer = (player*)g_ObjMgr.GetObjectBySlot( m_PlayerSlot );
    if( !pPlayer )
        return;
        
    if( pPlayer->IsAlive() && m_Active )
    {        
        //
        // Render each HUD element.
        //
        s32 i;
        for( i = 0; i < HUD_ELEMENT_NUM_ELEMENTS; i++ )
        {
            m_HudComponents[ i ]->OnRender( pPlayer );
        }


        //
        // Render the current speaker name, if any..
        //
#ifndef X_EDITOR
        if( g_NetworkMgr.IsOnline() && 
            g_VoiceMgr.IsVoiceEnabled() && 
            g_VoiceMgr.IsGameVoiceEnabled() && 
            !g_VoiceMgr.IsVoiceBanned() )
        {
            // Render the ( HOW )
            texture* pTexture = NULL;
            xwstring  HowString;
            
            switch( g_VoiceMgr.GetLocalTalkType() )
            {
            case TALK_GLOBAL:
                pTexture = m_HowGlobalBMP.GetPointer();
                HowString = g_StringTableMgr("ui","IDS_GLOBAL");
                break;
            case TALK_TEAM:
                pTexture = m_HowTeamBMP.GetPointer();
                HowString = g_StringTableMgr("ui","IDS_TEAM");
                break;
            case TALK_LOCAL:
                pTexture = m_HowLocalBMP.GetPointer();
                HowString = g_StringTableMgr("ui","IDS_LOCAL");
                break;
            default:
                pTexture = NULL;
                break;
            }

            // Restart the help text lifetime whenever the talk mode changes.
            if( g_VoiceMgr.GetLocalTalkType() != m_HowLastMode )
            {
                m_HowTextTimer = 3.0f;
                m_HowLastMode = g_VoiceMgr.GetLocalTalkType();
            }

            if( pTexture && g_VoiceMgr.IsHeadsetPresent() )
            {
                static f32 HOW_BMP_X =   8; 
                static f32 HOW_BMP_Y = 340; 
                const xbitmap& Bitmap = pTexture->m_bitmap;
                g_UIRenderer.DrawImage( *pTexture,
                                        vector2( m_XPos + HOW_BMP_X, m_YPos + HOW_BMP_Y ),
                                        vector2( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() ),
                                        vector2( 0.0f, 0.0f ),
                                        vector2( 1.0f, 1.0f ),
                                        g_HudColor,
                                        0.0f,
                                        UI_BLEND_ALPHA,
                                        UI_SAMPLER_LINEAR_CLAMP );

                // render name
                static s32 HOW_L = 40;
                static s32 HOW_T = 350;
                static s32 HOW_B = 1;
                static s32 HOW_R = 1;
                irect Position(
                    (s32)m_XPos + HOW_L,
                    (s32)m_YPos + HOW_T,
                    (s32)m_XPos + HOW_L + HOW_R,
                    (s32)m_YPos + HOW_T + HOW_B
                    );
                
                RenderLine( HowString, 
                    Position, 
                    m_HowTextAlpha, 
                    g_HudColor,
                    0,
                    ui_font::h_left|ui_font::v_center, 
                    TRUE ); 
            }        

#ifndef X_RETAIL
            if ( !eng_ScreenShotActive() )            
#endif // X_RETAIL
            {
                // Render the ( WHO )
                s32     VoiceOwner;
                const game_score& Score = GameMgr.GetScore();

                texture* pWHOTexture = NULL;

                VoiceOwner = g_VoiceMgr.GetLocalVoiceOwner();
                if( VoiceOwner != -1 )
                {
                    xwchar VoiceOwnerName[ 42 ];
                    switch( g_VoiceMgr.GetLocalVoiceTalkType() )
                    {
                    case TALK_NEW_GLOBAL:
                    case TALK_OLD_GLOBAL:
                        pWHOTexture = m_HowGlobalBMP.GetPointer();
                        break;
                    case TALK_NEW_TEAM:
                    case TALK_OLD_TEAM:
                        pWHOTexture = m_HowTeamBMP.GetPointer();
                        break;
                    case TALK_NEW_LOCAL:
                    case TALK_OLD_LOCAL:
                        pWHOTexture = m_HowLocalBMP.GetPointer();
                        break;
                    default:
                        pWHOTexture = NULL;
                        break;
                    }

                    if( pWHOTexture )
                    {
                        static f32 WHO_BMP_X = 148;
                        static f32 WHO_BMP_Y = 400-16;
                        const xbitmap& Bitmap = pWHOTexture->m_bitmap;
                        g_UIRenderer.DrawImage( *pWHOTexture,
                                                vector2( m_XPos + WHO_BMP_X, m_YPos + WHO_BMP_Y ),
                                                vector2( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() ),
                                                vector2( 0.0f, 0.0f ),
                                                vector2( 1.0f, 1.0f ),
                                                g_HudColor,
                                                0.0f,
                                                UI_BLEND_ALPHA,
                                                UI_SAMPLER_LINEAR_CLAMP );


                        // render name
                        static s32 WHO_L = 180;
                        static s32 WHO_T = 400-16;
                        static s32 WHO_B = 18;
                        static s32 WHO_R = 18;
                        irect Position(
                            (s32)m_XPos + WHO_L,
                            (s32)m_YPos + WHO_T,
                            (s32)m_XPos + WHO_L+ WHO_R,
                            (s32)m_YPos + WHO_T + WHO_B
                        );
                        
                        x_wstrcpy( VoiceOwnerName, Score.Player[ VoiceOwner ].Name );
                        RenderLine( VoiceOwnerName, 
                                    Position, 
                                    255, 
                                    g_HudColor,
                                    0,
                                    ui_font::h_left|ui_font::v_center, 
                                    TRUE ); 

                    }
                }
            }
        }
#endif
    }
    else if( m_Active )
    {
        m_HudComponents[ HUD_ELEMENT_TEXT_BOX ]->OnRender( pPlayer );
    }

    static s32 LORE_OFFSET = -102;

    {
        const s32 RCVD_X = (s32)m_Width - 36;
        static s32 RCVD_Y = 320-22;
        texture* pInviteTexture = NULL;

        // Decide what icon we need here..
        switch( m_NotificationIcon )
        {
            case HUD_NOTIFICATION_NONE          : break;
            case HUD_NOTIFICATION_FRIEND_REQ    : pInviteTexture = m_FriendReqBMP.GetPointer();     break;
            case HUD_NOTIFICATION_GAME_INVITE   : pInviteTexture = m_GameInviteBMP.GetPointer();    break;
            default                             : ASSERT( FALSE );
        }

        xbool CanDraw = FALSE;
#ifndef X_EDITOR
        s32 LocalPlayerCount = g_NetworkMgr.GetLocalPlayerCount();
        if( LocalPlayerCount == 1 )
            CanDraw = TRUE;
        else if( (g_RenderContext.LocalPlayerIndex==0) && (LocalPlayerCount==2) )
            CanDraw = TRUE;
        else if( (g_RenderContext.LocalPlayerIndex==1) && (LocalPlayerCount >2) )
            CanDraw = TRUE;
#else
        s32 LocalPlayerCount = 1;
#endif

        if( (pInviteTexture != NULL) && CanDraw )
        {
            // If the scanner is up then lets shift the NotificationIcon up above the scanner.
            s32 LoreScannerOffset = 0;
            slot_id ID = g_ObjMgr.GetFirst( object::TYPE_HUD_OBJECT );
            hud_object* pHudObject = (hud_object*)g_ObjMgr.GetObjectBySlot( ID );
            if( pHudObject )
            {
                if( pHudObject->m_PlayerHuds[0].m_Scanner.GetLoreDetected() )
                {
                    LoreScannerOffset += LORE_OFFSET;
                }
            }

            xcolor rcvdRenderColor = XCOLOR_WHITE;
            rcvdRenderColor.A = m_InvitationAlpha;
 
            vector3 Position(m_XPos + RCVD_X,m_YPos + RCVD_Y + LoreScannerOffset,0);
            if( LocalPlayerCount > 1 )
                Position.GetY() -= 240;
            const xbitmap& Bitmap = pInviteTexture->m_bitmap;
            g_UIRenderer.DrawImage( *pInviteTexture,
                                    vector2( Position.GetX(), Position.GetY() ),
                                    vector2( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() ),
                                    vector2( 0.0f, 0.0f ),
                                    vector2( 1.0f, 1.0f ),
                                    rcvdRenderColor,
                                    0.0f,
                                    UI_BLEND_ALPHA,
                                    UI_SAMPLER_LINEAR_CLAMP );
        }
    }

    if( m_AutoSaveing ) // Auto Save icon
    {
        const s32 AUTO_SAVE_X = (s32)m_Width - 36;
        static s32 AUTO_SAVE_Y = 320+36-20;
        texture* pAutoSaveTexture = m_AutoSave.GetPointer();
        if( pAutoSaveTexture )
        {
            // If the Scanner is up then lets move the autosave icon above it.
            s32 LoreScannerOffset = 0;
            slot_id ID = g_ObjMgr.GetFirst( object::TYPE_HUD_OBJECT );
            hud_object* pHudObject = (hud_object*)g_ObjMgr.GetObjectBySlot( ID );
            if( pHudObject )
            {
                if( pHudObject->m_PlayerHuds[0].m_Scanner.GetLoreDetected() )
                {
                    LoreScannerOffset += LORE_OFFSET;
                }
            }

            xcolor autoSaveRenderColor = XCOLOR_WHITE;
            autoSaveRenderColor.A = m_AutoSaveingAlpha;

            const xbitmap& Bitmap = pAutoSaveTexture->m_bitmap;
            g_UIRenderer.DrawImage( *pAutoSaveTexture,
                                    vector2( m_XPos + AUTO_SAVE_X, m_YPos + AUTO_SAVE_Y + LoreScannerOffset ),
                                    vector2( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() ),
                                    vector2( 0.0f, 0.0f ),
                                    vector2( 1.0f, 1.0f ),
                                    autoSaveRenderColor,
                                    0.0f,
                                    UI_BLEND_ALPHA,
                                    UI_SAMPLER_LINEAR_CLAMP );
        }
    }
}

//------------------------------------------------------------------------------

void player_hud::UpdateMPIcons( f32 DeltaTime, player* pPlayer )
{
    (void)DeltaTime;

#ifndef X_EDITOR
    // no MP icons in campaign mode!
    if( GameMgr.GetGameType() == GAME_CAMPAIGN )
    {
        return;
    }

    // Special-case for two-player split-screen. Since we know who the guy
    // sitting next to us is, don't bother displaying the name. For 
    // split-screen games where there are more than two players or for
    // a normal online game, we will allow icons to be displayed.
    if( g_NetworkMgr.GetLocalPlayerCount() == 2 )
    {
        return;
    }

    // Grab pointers to all net ghosts and players.
    actor* ppActor[32];
    x_memset( ppActor, 0, sizeof(actor*)*32 );
    slot_id SlotID;
    for( SlotID = g_ObjMgr.GetFirst( object::TYPE_NET_GHOST );
         SlotID != SLOT_NULL;
         SlotID = g_ObjMgr.GetNext( SlotID ) )
    {
        object*    pObj     = g_ObjMgr.GetObjectBySlot( SlotID );
        net_ghost& NetGhost = net_ghost::GetSafeType( *pObj );
        s32        NetSlot  = NetGhost.net_GetSlot();

        if( IN_RANGE( 0, NetSlot, 31 ) )
        {
            ppActor[NetSlot] = &NetGhost;
        }
    }

    for( SlotID = g_ObjMgr.GetFirst( object::TYPE_PLAYER );
         SlotID != SLOT_NULL;
         SlotID = g_ObjMgr.GetNext( SlotID ) )
    {
        object* pObj    = g_ObjMgr.GetObjectBySlot( SlotID );
        player& Player  = player::GetSafeType( *pObj );
        s32     NetSlot = Player.net_GetSlot();

        if( IN_RANGE( 0, NetSlot, 31 ) )
        {
            ppActor[NetSlot] = &Player;
        }
    }

    // grab out some useful variables to use for doing visibility tests
    // for the enemies
    vector3           V0, V1, ViewDirection;
    const game_score& Score        = GameMgr.GetScore();
    const view&       View         = pPlayer->GetSimulationView();
    vector3           ViewPosition = View.GetPosition();
    const matrix4&    V2W          = View.GetV2W();
    V2W.GetColumns( V0, V1, ViewDirection );

    // Now loop through all the actors and figure out if they should
    // have an icon or not.
    s32 NetSlot;
    for( NetSlot = 0; NetSlot < 32; NetSlot++ )
    {
        // if there is not an actor in this slot, or if the actor is use,
        // then don't allow an icon
        if( (ppActor[NetSlot] == NULL) ||
            (ppActor[NetSlot] == pPlayer) )
        {
            m_Icon.SetPlayerIconTarget( NetSlot, FALSE, FALSE );
        }
        else
        {
            // friend or foe?
            xbool bIsAlly = FALSE;
            if( !pPlayer->IsContagious() &&
                (ppActor[NetSlot]->net_GetTeamBits() & pPlayer->net_GetTeamBits()) )
            {
                bIsAlly = TRUE;
            }

            // Assume we will want an icon. We will do some tests to
            // verify that is the case.
            xbool bWantIcon = TRUE;

            // Is the actor dead?
            if( ppActor[NetSlot]->IsDead() )
            {
                bWantIcon = FALSE;
            }

            // is the head in view?
            vector3 HeadPos;
            vector3 RootPos;
            ppActor[NetSlot]->GetHeadAndRootPosition( HeadPos, RootPos );
            if( bWantIcon && !View.PointInView( HeadPos ) )
            {
                bWantIcon = FALSE;
            }

            // For an enemy, there are additional tests so that we can help avoid
            // giving away a player's location just by scanning around.
            vector3 HeadDirection = HeadPos - ViewPosition;
            radian  HeadAngle     = v3_AngleBetween( HeadDirection, ViewDirection );
            f32     HeadDistance  = HeadDirection.Length();
            if( (bIsAlly == FALSE) &&
                (HeadAngle > ENEMY_ANGLE_LIMIT) &&
                (HeadDistance > ENEMY_RANGE_LIMIT) )
            {
                bWantIcon = FALSE;
            }

            // Now, do an occlusion test with the world. This will avoid us seeing
            // enemies around the corner.
            if( bWantIcon )
            {
                // Use center of avatar for collision check because players can 
                // poke their real head position through walls.
                vector3 HeadCollPos = ppActor[NetSlot]->GetPosition();
                HeadCollPos.GetY() = HeadPos.GetY();
            
                // do a simple line of set check to make sure we can see the player's head
                g_CollisionMgr.LineOfSightSetup( pPlayer->GetGuid(), ViewPosition, HeadCollPos );
                g_CollisionMgr.SetMaxCollisions(1);
                g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES,
                                                object::ATTR_COLLIDABLE,
                                                (object::object_attr)(object::ATTR_COLLISION_PERMEABLE|object::ATTR_LIVING) );
                if( g_CollisionMgr.m_nCollisions )
                    bWantIcon = FALSE;
            }

            m_Icon.SetPlayerIconTarget( NetSlot, bWantIcon, bIsAlly );

            const xbool bIsSpeaking = (g_VoiceMgr.GetLocalVoiceOwner() == NetSlot);
            const xwchar* pCharName = Score.Player[NetSlot].Name;
            const vector3 RenderPos = HeadPos + vector3( 0.0f, 30.0f, 0.0f );
            m_Icon.SubmitIcon( bIsSpeaking ? ICON_SPEAKING : (bIsAlly ? ICON_ALLY : ICON_ENEMY),
                               HeadPos,
                               RenderPos,
                               TRUE,
                               GUTTER_NONE,
                               bIsAlly ? XCOLOR_GREEN : XCOLOR_RED,
                               pCharName,
                               FALSE,
                               FALSE,
                               1.0f,
                               -1.0f,
                               -1.0f,
                               NetSlot );
        }
    }
#endif // !defined(X_EDITOR)
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void player_hud::UpdateTextWidth( void )
{
    s32 TextWidth = 0;

    if( m_Width > 32.0f )
    {
        TextWidth = (s32)(m_Width - m_InfoBox.GetWidth() - 4);
        if( TextWidth <= 32 )
            TextWidth = 0;
    }

    m_Text.SetMaxWidth( TextWidth );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void player_hud::OnAdvanceSimulation( f32 DeltaTime )
{
    if( !m_Active )
    {
        return;
    }

    // Lookup owner
    player* pPlayer = (player*)g_ObjMgr.GetObjectBySlot( m_PlayerSlot );
    if( !pPlayer )
        return;

    // Keep the text width in sync with the current layout before processing
    // queued messages.
    UpdateTextWidth();

    for( s32 i = 0; i < HUD_ELEMENT_NUM_ELEMENTS; i++ )
    {
        m_HudComponents[ i ]->OnAdvanceSimulation( pPlayer, DeltaTime );
    }

    // Update the (how) text lifecycle.
    if( m_HowTextTimer > 0.0f )
    {
        m_HowTextAlpha = 255;
        m_HowTextTimer -= DeltaTime;
        if( m_HowTextTimer < 0.0f )
            m_HowTextTimer = 0.0f;
    }
    else
    {
        s32 Alpha = (s32)m_HowTextAlpha - 8;
        m_HowTextAlpha = (u8)MAX( Alpha, 0 );
    }

    // Invitations are persistent external state, but the HUD presentation is
    // a finite state machine.  Without advancing it, the icon stays latched.
    xbool NewReceivedFriendReq  = FALSE;
    xbool NewReceivedGameInvite = FALSE;

#ifndef X_EDITOR
    for( s32 i = 0; i < g_MatchMgr.GetBuddyCount(); i++ )
    {
        buddy_info& Buddy = g_MatchMgr.GetBuddy( i );
        if( Buddy.Flags & USER_REQUEST_RECEIVED )
            NewReceivedFriendReq = TRUE;
        if( Buddy.Flags & USER_HAS_INVITE )
            NewReceivedGameInvite = TRUE;
    }
#endif

    if( m_ReceivedFriendReq != NewReceivedFriendReq )
    {
        m_ReceivedFriendReq = NewReceivedFriendReq;
        if( m_ReceivedFriendReq )
        {
            m_NotificationIcon = HUD_NOTIFICATION_FRIEND_REQ;
            m_InvitationState  = 1;
        }
    }

    if( m_ReceivedGameInvite != NewReceivedGameInvite )
    {
        m_ReceivedGameInvite = NewReceivedGameInvite;
        if( m_ReceivedGameInvite )
        {
            m_NotificationIcon = HUD_NOTIFICATION_GAME_INVITE;
            m_InvitationState  = 1;
        }
    }

    switch( m_InvitationState )
    {
        case 0:
            m_InvitationAlpha = 0;
            break;

        case 1:
            m_InvitationAlpha = MIN( m_InvitationAlpha + 8, 255 );
            if( m_InvitationAlpha >= 255 )
            {
                m_InvitationState   = 2;
                m_InvitationHoldTime = 0.0f;
            }
            break;

        case 2:
            m_InvitationHoldTime += DeltaTime;
            if( m_InvitationHoldTime >= 6.0f )
                m_InvitationState = 3;
            break;

        case 3:
            m_InvitationAlpha = MAX( m_InvitationAlpha - 8, 0 );
            if( m_InvitationAlpha <= 0 )
                m_InvitationState = 4;
            break;

        case 4:
            m_InvitationState  = 0;
            m_NotificationIcon = HUD_NOTIFICATION_NONE;
            break;

        default:
            m_InvitationState  = 0;
            m_InvitationAlpha  = 0;
            m_NotificationIcon = HUD_NOTIFICATION_NONE;
            break;
    }

    // Visibility, LOS, and snapshot submission remain simulation-update work.
    UpdateMPIcons( DeltaTime, pPlayer );
}

//------------------------------------------------------------------------------
