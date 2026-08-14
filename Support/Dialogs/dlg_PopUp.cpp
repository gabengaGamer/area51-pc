//=========================================================================
//
//  dlg_PopUp.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_text.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"

#include "dlg_PopUp.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"

#include "NetworkMgr/Voice/VoiceMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"

//=========================================================================
//  Popup Dialog
//=========================================================================
enum popup_controls
{
    IDC_QUICK_MATCH_NAV_TEXT,
    IDC_NAV_TEXT_BUTTON_0,
    IDC_NAV_TEXT_BUTTON_1,
    IDC_NAV_TEXT_BUTTON_2,
    IDC_NAV_TEXT_BUTTON_3,
    IDC_POPUP_ACTION_0,
    IDC_POPUP_ACTION_1,
    IDC_POPUP_ACTION_2,
    IDC_POPUP_ACTION_3,
    IDC_POPUP_ACTION_4,
};

ui_manager::control_tem PopUpControls[] = 
{
    { IDC_NAV_TEXT_BUTTON_0,     "IDS_NULL", "text", -28, 175, 240, 25, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_NAV_TEXT_BUTTON_1,     "IDS_NULL", "text",  46, 175, 240, 25, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_NAV_TEXT_BUTTON_2,     "IDS_NULL", "text",   0, 175, 240, 25, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_NAV_TEXT_BUTTON_3,     "IDS_NULL", "text",   0, 175, 240, 25, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_QUICK_MATCH_NAV_TEXT,  "IDS_NULL", "text",   0, 175, 240, 25, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_POPUP_ACTION_0,        "IDS_NULL", "button", 0,   0,  80, 24, 0, 0, 1, 1, ui_win::WF_BORDER },
    { IDC_POPUP_ACTION_1,        "IDS_NULL", "button", 0,   0,  80, 24, 1, 0, 1, 1, ui_win::WF_BORDER },
    { IDC_POPUP_ACTION_2,        "IDS_NULL", "button", 0,   0,  80, 24, 2, 0, 1, 1, ui_win::WF_BORDER },
    { IDC_POPUP_ACTION_3,        "IDS_NULL", "button", 0,   0,  80, 24, 3, 0, 1, 1, ui_win::WF_BORDER },
    { IDC_POPUP_ACTION_4,        "IDS_NULL", "button", 0,   0,  80, 24, 4, 0, 1, 1, ui_win::WF_BORDER },
};

static char* s_pRecordButtonText[4] =
{
    "IDS_NAV_RECORD",
    "IDS_NAV_PLAY",
    "IDS_NAV_BACK",
    "IDS_NAV_SEND",
};

static char* s_pPlayButtonText[2] =
{
    "IDS_NAV_PLAY_PLAY",
    "IDS_NAV_BACK",
};

ui_manager::dialog_tem PopUpDialog =
{
    "IDS_NULL",
    5, 1,
    sizeof(PopUpControls)/sizeof(ui_manager::control_tem),
    &PopUpControls[0],
    0
};

//=========================================================================
//  Defines
//=========================================================================

static s32 const ACTION_BUTTON_MIN_WIDTH = 72;
static s32 const ACTION_BUTTON_HEIGHT    = 24;
static s32 const ACTION_BUTTON_GAP       = 8;
static s32 const ACTION_BUTTON_MARGIN    = 10;
static s32 const ACTION_BUTTON_BOTTOM    = 7;

//=========================================================================

static xbool IsPromptWhitespace( xwchar Character )
{
    return (Character == ' ')  ||
           (Character == '\t') ||
           (Character == '\r') ||
           (Character == '\n');
}

//=========================================================================

static void TrimPromptText( xwstring& Text )
{
    while( Text.GetLength() && IsPromptWhitespace( Text[0] ) )
    {
        Text.Delete( 0 );
    }

    while( Text.GetLength() && IsPromptWhitespace( Text[Text.GetLength()-1] ) )
    {
        Text.Delete( Text.GetLength()-1 );
    }
}

//=========================================================================

static xwstring GetPromptLabel( const xwchar* pPrompt )
{
    xwstring Label;
    if( pPrompt )
    {
        Label = pPrompt;
    }

    const s32 GlyphEnd = Label.Find( (xwchar)0x00BB );

    if( GlyphEnd != -1 )
    {
        Label = Label.Mid( GlyphEnd + 1, Label.GetLength() - GlyphEnd - 1 );
    }

    TrimPromptText( Label );
    return Label;
}

//=========================================================================

static s32 SplitActionPrompts( const xwchar* pNavText, xwstring* pPrompts, s32 MaxPrompts )
{
    if( !pNavText || !pNavText[0] || !pPrompts || (MaxPrompts <= 0) )
    {
        return 0;
    }

    xwstring Text( pNavText );
    s32      Count = 0;
    s32      Start = Text.Find( (xwchar)0x00AB );

    if( Start == -1 )
    {
        Text = GetPromptLabel( Text );
        if( Text.GetLength() )
        {
            pPrompts[Count++] = Text;
        }
        return Count;
    }

    while( (Start != -1) && (Count < MaxPrompts) )
    {
        const s32 Next = Text.Find( (xwchar)0x00AB, Start + 1 );
        const s32 End  = (Next == -1) ? Text.GetLength() : Next;

        pPrompts[Count] = Text.Mid( Start, End - Start );
        TrimPromptText( pPrompts[Count] );
        Count++;
        Start = Next;
    }

    return Count;
}

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

//=========================================================================
//  Registration function
//=========================================================================

void dlg_popup_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "popup", &PopUpDialog, &dlg_popup_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_popup_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_popup* pDialog = new dlg_popup;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_popup
//=========================================================================

dlg_popup::dlg_popup( void )
{
    m_AskMode = BUDDY_MODE_ADD;
    m_PlayDialogState = PLAY_STATE_NONE;
    m_RecordDialogState = RECORD_STATE_NONE;
    m_PasswordDialogActive = FALSE;
    m_PasswordIndex = 0;
    m_IsVoiceMailPopUp = FALSE;
    m_ActionCount = 0;
    m_ActionRowCount = 0;
    m_LastInputDevice = ui_input_device::None;
    m_HasNavText = FALSE;
    m_NavTextIsActionPrompt = FALSE;
    x_memset( m_pActionButtons, 0, sizeof( m_pActionButtons ) );
    x_memset( m_pButtonText, 0, sizeof( m_pButtonText ) );
#ifdef TARGET_XBOX
    m_bIsPopup = 1;
    m_XBOXNotificationOffsetX = 0;
#endif // TARGET_XBOX
}

//=========================================================================

dlg_popup::~dlg_popup( void )
{
    #ifdef TARGET_XBOX
    if( m_IsVoiceMailPopUp == TRUE )
    {
        // Kill Voice Buffer Recording.
        headset& Headset = g_VoiceMgr.GetHeadset();

        // We MUST forcefully stop any recording!
        if( Headset.GetVoiceIsRecording() == TRUE )
            Headset.StopVoiceRecording( TRUE );

        // We MUST forcefully stop any playing!
        if( Headset.GetVoiceIsPlaying() == TRUE )
            Headset.StopVoicePlaying( TRUE );

        if( Headset.GetVoiceMessageRec() != NULL )
            Headset.KillVoiceRecording();

        // Free internal memory used by the voice message
        g_MatchMgr.FreeMessageDownload();
    }
    #endif
}

//=========================================================================

void dlg_popup::Configure( const f32 Timeout, const xwchar* Title, const xwchar* Message, const xwchar* Message2, const xwchar* Message3 )
{
    SetLabel( Title );

    m_Message = Message;

    if( Message2 )
    {
        m_Message2 = Message2;
    }
    else
    {
        m_Message2 = xwstring( "" );
    }

    if( Message3 )
    {
        m_Message3 = Message3;
    }
    else
    {
        m_Message3 = xwstring("");
    }

    // initialize
    m_State       = DIALOG_STATE_ACTIVE;
    m_Timeout     = Timeout;     
    m_bDoTimeout  = TRUE;
    m_bDoBlackout = FALSE;

    ClearActions();
}

//=========================================================================

void dlg_popup::Configure( irect& Position, const f32 Timeout, const xwchar* Title, const xwchar* Message, const xwchar* Message2, const xwchar* Message3 )
{
    SetLabel( Title );

    m_Message = Message;

    if( Message2 )
    {
        m_Message2 = Message2;
    }
    else
    {
        m_Message2 = xwstring( "" );
    }

    if( Message3 )
    {
        m_Message3 = Message3;
    }
    else
    {
        m_Message3 = xwstring("");
    }

    // initialize
    m_State       = DIALOG_STATE_ACTIVE;
    if( m_Timeout >= 0.0f )
    {
        m_Timeout     = Timeout;     
        m_bDoTimeout  = TRUE;
    }
    else
    {
        m_bDoTimeout  = FALSE;
    }
    m_bDoBlackout = FALSE;

    ClearActions();

    // Set Size of window
    irect Bounds = g_UiMgr->GetUserBounds( g_UiUserID );

    // center in screen
    Position.Translate( Bounds.l + (Bounds.GetWidth()-Position.GetWidth())/2 - Position.l,
                        Bounds.t + (Bounds.GetHeight()-Position.GetHeight())/2 - Position.t );
    SetPosition( Position );

    // reposition nav text
    irect NavPos = m_pNavText->GetPosition();
    NavPos.b = Position.b - Position.t;
    NavPos.t = NavPos.b - 25;
    NavPos.l = 0;
    NavPos.r = Position.r - Position.l;
    m_pNavText->SetPosition( NavPos );
}

//=========================================================================

void dlg_popup::Configure( const xwchar* Title, const xbool Yes, const xbool No, const xbool Maybe, const xwchar* Message, const xwchar* pNavText, s32* pResult )
{
    Configure(Title, Yes, No, Maybe, FALSE, Message, pNavText, pResult);
}

//=========================================================================

void dlg_popup::Configure( irect& Position, const xwchar* Title, const xbool Yes, const xbool No, const xbool Maybe, const xwchar* Message, const xwchar* pNavText, s32* pResult )
{
    Configure(Position, Title, Yes, No, Maybe, FALSE, Message, pNavText, pResult);
}

//=========================================================================

void dlg_popup::Configure( const xwchar* Title, const xbool Yes, const xbool No, const xbool Maybe, const xbool Help, const xwchar* Message, const xwchar* pNavText, s32* pResult )
{
    Configure( Title, Yes, No, Maybe, Help, FALSE, Message, pNavText, pResult );
}

//=========================================================================

void dlg_popup::Configure( const xwchar* Title, const xbool Yes, const xbool No, const xbool Maybe, const xbool Help, const xbool Other, const xwchar* Message, const xwchar* pNavText, s32* pResult )
{
    SetLabel( Title );

    m_Message       = Message;
    m_Message2      = xwstring( "" );
    m_Message3      = xwstring( "" );

    ConfigureStandardActions( Yes, No, Maybe, Help, Other, pNavText );
    ApplyInputLayout( m_pManager->GetInputDevice( m_UserID ) );

    m_pResult       = pResult;
    m_bDoTimeout    = FALSE;
    m_bDoBlackout   = TRUE;

    // initialize result
    if( m_pResult )
    {
        *m_pResult = DLG_POPUP_IDLE;
    }
}

//=========================================================================

void dlg_popup::Configure( irect& Position, const xwchar* Title, const xbool Yes, const xbool No, const xbool Maybe, const xbool Help, const xwchar* Message, const xwchar* pNavText, s32* pResult )
{
    Configure( Position, Title, Yes, No, Maybe, Help, FALSE, Message, pNavText, pResult );
}

//=========================================================================

void dlg_popup::Configure( irect& Position, const xwchar* Title, const xbool Yes, const xbool No, const xbool Maybe, const xbool Help, const xbool Other, const xwchar* Message, const xwchar* pNavText, s32* pResult )
{
    SetLabel( Title );

    m_Message       = Message;
    m_Message2      = xwstring( "" );
    m_Message3      = xwstring( "" );

    ConfigureStandardActions( Yes, No, Maybe, Help, Other, pNavText );

    m_pResult       = pResult;
    m_bDoTimeout    = FALSE;
    m_bDoBlackout   = TRUE;

    // Set Size of window
    irect Bounds = g_UiMgr->GetUserBounds( g_UiUserID );

    // center in screen
    Position.Translate( Bounds.l + (Bounds.GetWidth()-Position.GetWidth())/2 - Position.l,
                        Bounds.t + (Bounds.GetHeight()-Position.GetHeight())/2 - Position.t );
    SetPosition( Position );


    // reposition nav text
    irect NavPos = m_pNavText->GetPosition();
    NavPos.b = Position.b - Position.t;
    s32 Height = m_HasNavText ? g_UiMgr->TextHeight( 1, pNavText, 256 ) : 0;
    if( Height > 22 )
    {
        NavPos.t = NavPos.b - 40;
    }
    else
    {
        NavPos.t = NavPos.b - 25;
    }
    NavPos.l = 0;
    NavPos.r = Position.r - Position.l;
    m_pNavText->SetPosition( NavPos );
    ApplyInputLayout( m_pManager->GetInputDevice( m_UserID ) );

    // initialize result
    if( m_pResult )
    {
        *m_pResult = DLG_POPUP_IDLE;
    }
}

//=========================================================================

void dlg_popup::ConfigureAutosaveDialog( irect& Position, const xwchar* Title, const xwchar* Message, const xwchar* pNavText, s32* pResult /* = NULL  */)
{
    SetLabel( Title );

    m_Message       = Message;
    m_Message2      = xwstring( "" );
    m_Message3      = xwstring( "" );

    ConfigureStandardActions( TRUE, TRUE, FALSE, FALSE, FALSE, pNavText );

    m_pResult       = pResult;
    m_bDoTimeout    = FALSE;
    m_bDoBlackout   = TRUE;

    // Set Size of window
    irect Bounds = g_UiMgr->GetUserBounds( g_UiUserID );

    // center in screen
    Position.Translate( Bounds.l + (Bounds.GetWidth()-Position.GetWidth())/2 - Position.l,
                        Bounds.t + (Bounds.GetHeight()-Position.GetHeight())/2 - Position.t );
    SetPosition( Position );

    // reposition nav text
    irect NavPos = m_pNavText->GetPosition();
    NavPos.b = Position.b - Position.t;
    if( x_GetLocale() == XL_LANG_ENGLISH )
    {
        NavPos.t = NavPos.b - 25;
    }
    else
    {
        NavPos.t = NavPos.b - 50;
    }
    NavPos.l = 0;
    NavPos.r = Position.r - Position.l;
    m_pNavText->SetPosition( NavPos );
    ApplyInputLayout( m_pManager->GetInputDevice( m_UserID ) );

    // initialize result
    if( m_pResult )
    {
        *m_pResult = DLG_POPUP_IDLE;
    }
}

//=========================================================================

void dlg_popup::ConfigureRecordDialog( irect& Position, const xwchar* Title, const xwchar* Message, s32* pResult /* = NULL  */)
{
    SetLabel( Title );

    m_IsVoiceMailPopUp = TRUE;

    ClearActions();
    m_RecordDialogState = RECORD_STATE_INIT;
    m_AskMode = BUDDY_MODE_ADD;

    m_ProgressValue = 0.0f;

    m_Message       = Message;
    m_Message2      = xwstring( "" );
    m_Message3      = xwstring( "" );

    m_pResult       = pResult;
    m_bDoTimeout    = FALSE;
    m_bDoBlackout   = TRUE;
    m_EnableRecord  = FALSE;
    m_EnablePlay    = FALSE;
    m_EnableSend    = FALSE;
    m_ShowStop      = FALSE;

    AddAction( POPUP_ACTION_ACCEPT,    g_StringTableMgr( "ui", s_pRecordButtonText[0] ), NULL, FALSE );
    AddAction( POPUP_ACTION_DELETE,    g_StringTableMgr( "ui", s_pRecordButtonText[1] ), NULL, FALSE );
    AddAction( POPUP_ACTION_CANCEL,    g_StringTableMgr( "ui", s_pRecordButtonText[2] ), NULL, TRUE  );
    AddAction( POPUP_ACTION_ALTERNATE, g_StringTableMgr( "ui", s_pRecordButtonText[3] ), NULL, FALSE );

    irect Bounds = g_UiMgr->GetUserBounds( g_UiUserID );

    // center in screen
    Position.Translate( Bounds.l + (Bounds.GetWidth()-Position.GetWidth())/2 - Position.l,
                        Bounds.t + (Bounds.GetHeight()-Position.GetHeight())/2 - Position.t );
    SetPosition( Position );

    s32 iFont            = g_UiMgr->FindFont( "small" );
    s32 TotalStringWidth = 0;
    s32 Widths [4];
    s32 Offsets[4];

    // English gets wider spacing between strings
    s32 Spacing = (x_GetLocale() == XL_LANG_ENGLISH) ? 25 : 15;

    // Determine the maximum number of pixels needed horizontally for the 4 strings
    for( s32 j=0; j<4; j++ )
    {
        xwstring NavText( g_StringTableMgr( "ui", s_pRecordButtonText[j] ) );

        s32 StringWidth = g_UiMgr->TextWidth( iFont, NavText );
        Widths [j] = StringWidth;
        Offsets[j] = TotalStringWidth;

        StringWidth += Spacing;

        TotalStringWidth += StringWidth;
    }

    s32 Left = -(TotalStringWidth / 2);

    for( s32 i=0; i < 4; i++ )
    {
        xwstring NavText( g_StringTableMgr( "ui", s_pRecordButtonText[i] ) );

        irect NavPos = m_pButtonText[i]->GetPosition();

        NavPos.l = Left + Offsets[i] + (Widths[i] / 2);
        NavPos.b = Position.b - Position.t;
        NavPos.t = NavPos.b - 25;
        NavPos.r = NavPos.l + (Position.r - Position.l);

        m_pButtonText[i]->SetPosition( NavPos  );
        m_pButtonText[i]->SetLabel   ( NavText );
        m_pButtonText[i]->SetFlag    ( ui_win::WF_VISIBLE, TRUE );
    }

    ApplyInputLayout( m_pManager->GetInputDevice( m_UserID ) );

    // initialize result
    if( m_pResult )
    {
        *m_pResult = DLG_POPUP_IDLE;
    }
}

//=========================================================================

void dlg_popup::ConfigurePlayDialog( irect& Position, const xwchar* Title, const xwchar* Message, const xwchar* pNavText, s32* pResult /* = NULL  */)
{
    SetLabel( Title );

    m_IsVoiceMailPopUp = TRUE;

    ClearActions();
    m_PlayDialogState = PLAY_STATE_START_DOWNLOAD;

    m_ProgressValue = 0.0f;

    m_Message       = Message;
    m_Message2      = xwstring( "" );
    m_Message3      = xwstring( "" );

    m_pResult       = pResult;
    m_bDoTimeout    = FALSE;
    m_bDoBlackout   = TRUE;
    m_EnablePlay    = FALSE;
    m_ShowStop      = FALSE;

    AddAction( POPUP_ACTION_ACCEPT, g_StringTableMgr( "ui", s_pPlayButtonText[0] ), NULL, FALSE );
    AddAction( POPUP_ACTION_CANCEL, g_StringTableMgr( "ui", s_pPlayButtonText[1] ), NULL, TRUE  );

    irect Bounds = g_UiMgr->GetUserBounds( g_UiUserID );

    // center in screen
    Position.Translate( Bounds.l + (Bounds.GetWidth()-Position.GetWidth())/2 - Position.l,
                        Bounds.t + (Bounds.GetHeight()-Position.GetHeight())/2 - Position.t );
    SetPosition( Position );

    if( pNavText )
    {
        m_HasNavText = pNavText[0] != 0;
        m_NavTextIsActionPrompt = FALSE;
        m_pNavText->SetLabelFlags( ui_font::h_center|ui_font::v_top );
        m_pNavText->SetLabel( pNavText );
        m_pNavText->SetFlag (ui_win::WF_VISIBLE, TRUE);
    }

    // reposition "From: Player" text
    irect FromPos = m_pNavText->GetPosition();
    FromPos.b = (Position.b - Position.t) / 2;
    FromPos.t = FromPos.b - 25;
    FromPos.r = (Position.r - Position.l);
    m_pNavText->SetPosition( FromPos );

    for( s32 i=0; i < 2; i++ )
    {
        xwstring NavText( g_StringTableMgr( "ui", s_pPlayButtonText[i] ) );

        irect NavPos = m_pButtonText[i]->GetPosition();
        NavPos.b = Position.b - Position.t;
        NavPos.t = NavPos.b - 25;
        NavPos.r = NavPos.l + (Position.r - Position.l);

        m_pButtonText[i]->SetPosition( NavPos  );
        m_pButtonText[i]->SetLabel   ( NavText );
        m_pButtonText[i]->SetFlag    ( ui_win::WF_VISIBLE, TRUE );
    }

    ApplyInputLayout( m_pManager->GetInputDevice( m_UserID ) );

    // initialize result
    if( m_pResult )
    {
        *m_pResult = DLG_POPUP_IDLE;
    }
}

//=========================================================================

void dlg_popup::SetBuddy( const buddy_info& Buddy )
{
    m_Buddy = Buddy;
}

//=========================================================================

buddy_info& dlg_popup::GetBuddy( void )
{
    return( m_Buddy );
}

//=========================================================================

void dlg_popup::SetBuddyMode( s32 mode )
{
    m_AskMode = mode;
}

//=========================================================================

void dlg_popup::Close( void )
{
    ui_win* pParent = GetParent();

    if( pParent ) 
    {
        delete this;
    }
    else
    {
        g_UiMgr->EndDialog(m_UserID,TRUE);
    }
}

//=========================================================================


xbool dlg_popup::Create( s32                        UserID,
                         ui_manager*                pManager,
                         ui_manager::dialog_tem*    pDialogTem,
                         const irect&               Position,
                         ui_win*                    pParent,
                         s32                        Flags,
                         void*                      pUserData )
{
    xbool   Success = FALSE;

    (void)pUserData;

    ASSERT( pManager );

    // Set Size of window
    irect r( 0, 0, 240, 200 );

    // center in screen
    r.Translate( Position.l + (Position.GetWidth()-r.GetWidth())/2,
                 Position.t + (Position.GetHeight()-r.GetHeight())/2 );

    // Do dialog creation
    Success = ui_dialog::Create( UserID, pManager, pDialogTem, r, pParent, Flags );
   
    // Initialize nav text
    m_pNavText = (ui_text*)FindChildByID( IDC_QUICK_MATCH_NAV_TEXT );
    m_pNavText->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pNavText->SetLabelFlags( ui_font::h_center|ui_font::v_top|ui_font::input_glyphs );
    m_pNavText->UseSmallText(TRUE);
    m_pNavText->SetLabelColor( xcolor(255,252,204,255) );

    for( s32 i=0; i < 4; i++ )
    {
        m_pButtonText[i] = (ui_text*)FindChildByID( IDC_NAV_TEXT_BUTTON_0 + i );

        m_pButtonText[i]->SetFlag      ( ui_win::WF_VISIBLE, FALSE );
        m_pButtonText[i]->SetLabelFlags( ui_font::h_center|ui_font::v_top|ui_font::input_glyphs );
        m_pButtonText[i]->UseSmallText ( TRUE );
    }

    for( s32 i=0; i<MAX_POPUP_ACTIONS; i++ )
    {
        m_pActionButtons[i] = (ui_button*)FindChildByID( IDC_POPUP_ACTION_0 + i );
        m_pActionButtons[i]->UseSmallText  ( TRUE );
        m_pActionButtons[i]->SetButtonStyle( ui_button::BUTTON_STYLE_FLAT );
        m_pActionButtons[i]->SetFlag       ( ui_win::WF_VISIBLE, FALSE );
    }

    // Initialize Data
    m_iElement = m_pManager->FindElement( "frame2" );
    ASSERT( m_iElement != -1 );

    m_BackgroundColor   = xcolor (19,59,14,255); 
    m_Timeout           = -1.0f;     
    m_bDoTimeout        = FALSE;
    m_bDoBlackout       = TRUE;

    // Clear pointer to result code
    m_pResult = NULL;

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // play create sound
    g_AudioMgr.Play( "Dialog" );

    // Return success code
    return Success;
}

//=========================================================================

void dlg_popup::ApplyInputLayout( ui_input_device Device )
{
    const xbool UseDesktopButtons = (m_ActionCount > 0) &&
                                    !m_PasswordDialogActive &&
                                   ((Device == ui_input_device::Mouse) ||
                                    (Device == ui_input_device::Keyboard));

    if( UseDesktopButtons )
    {
        LayoutActionButtons();
    }
    else
    {
        m_ActionRowCount = 0;
    }

    for( s32 i=0; i<MAX_POPUP_ACTIONS; i++ )
    {
        const xbool IsAction = i < m_ActionCount;
        m_pActionButtons[i]->SetFlag( ui_win::WF_VISIBLE,  UseDesktopButtons && IsAction );
        m_pActionButtons[i]->SetFlag( ui_win::WF_DISABLED, IsAction && !m_Actions[i].Enabled );
    }

    if( m_NavTextIsActionPrompt )
    {
        m_pNavText->SetFlag( ui_win::WF_VISIBLE, m_HasNavText && !UseDesktopButtons );
    }
    else
    {
        m_pNavText->SetFlag( ui_win::WF_VISIBLE, m_HasNavText );
    }

    const xbool IsSpecialDialog = (m_RecordDialogState != RECORD_STATE_NONE) ||
                                  (m_PlayDialogState   != PLAY_STATE_NONE);
    for( s32 i=0; i<4; i++ )
    {
        const xbool IsAction = i < m_ActionCount;
        m_pButtonText[i]->SetFlag( ui_win::WF_VISIBLE,
                                  IsSpecialDialog && IsAction && !UseDesktopButtons );
        m_pButtonText[i]->SetFlag( ui_win::WF_DISABLED,
                                  IsAction && !m_Actions[i].Enabled );
    }

    ui_win* pFocus = m_pManager->GetFocusedWindow( m_UserID );
    const s32 FocusedAction = FindAction( pFocus );

    if( (Device == ui_input_device::Keyboard) && UseDesktopButtons )
    {
        if( (FocusedAction == -1) || !m_Actions[FocusedAction].Enabled )
        {
            for( s32 i=0; i<m_ActionCount; i++ )
            {
                if( m_Actions[i].Enabled )
                {
                    m_pManager->SetFocusWindow( m_UserID, m_pActionButtons[i] );
                    break;
                }
            }
        }
    }
    else if( (Device != m_LastInputDevice) && (FocusedAction != -1) )
    {
        m_pManager->SetFocusWindow( m_UserID, this );
    }

    m_LastInputDevice = Device;
}

//=========================================================================

void dlg_popup::ClearActions( void )
{
    m_ActionCount            = 0;
    m_ActionRowCount         = 0;
    m_HasNavText             = FALSE;
    m_NavTextIsActionPrompt  = FALSE;
    m_LastInputDevice        = ui_input_device::None;

    if( m_pNavText )
    {
        m_pNavText->SetFlag( ui_win::WF_VISIBLE, FALSE );
        m_pNavText->SetLabel( xwstring( "" ) );
    }

    for( s32 i=0; i<MAX_POPUP_ACTIONS; i++ )
    {
        m_Actions[i] = popup_action();

        if( m_pActionButtons[i] )
        {
            m_pActionButtons[i]->SetLabel( xwstring( "" ) );
            m_pActionButtons[i]->SetFlag( ui_win::WF_VISIBLE,  FALSE );
            m_pActionButtons[i]->SetFlag( ui_win::WF_DISABLED, FALSE );
        }
    }

    for( s32 i=0; i<4; i++ )
    {
        if( m_pButtonText[i] )
        {
            m_pButtonText[i]->SetFlag( ui_win::WF_VISIBLE,  FALSE );
            m_pButtonText[i]->SetFlag( ui_win::WF_DISABLED, FALSE );
        }
    }
}

//=========================================================================

void dlg_popup::ConfigureStandardActions( xbool Yes,
                                          xbool No,
                                          xbool Maybe,
                                          xbool Help,
                                          xbool Other,
                                          const xwchar* pNavText )
{
    xwstring Prompts[MAX_POPUP_ACTIONS];
    const s32 PromptCount = SplitActionPrompts( pNavText, Prompts, MAX_POPUP_ACTIONS );
    s32       PromptIndex = 0;
    s32       YesPrompt   = -1;
    s32       MaybePrompt = -1;
    s32       NoPrompt    = -1;
    s32       HelpPrompt  = -1;
    s32       OtherPrompt = -1;

    ClearActions();

    m_HasNavText            = pNavText && pNavText[0];
    m_NavTextIsActionPrompt = TRUE;
    m_pNavText->SetLabelFlags( ui_font::h_center|ui_font::v_top|ui_font::input_glyphs );
    if( m_HasNavText )
    {
        m_pNavText->SetLabel( pNavText );
    }

    if( Yes   && (PromptIndex < PromptCount) ) YesPrompt   = PromptIndex++;
    if( Maybe && (PromptIndex < PromptCount) ) MaybePrompt = PromptIndex++;
    if( No    && (PromptIndex < PromptCount) ) NoPrompt    = PromptIndex++;
    if( Help  && (PromptIndex < PromptCount) ) HelpPrompt  = PromptIndex++;
    if( Other && (PromptIndex < PromptCount) ) OtherPrompt = PromptIndex++;

    if( Yes )
    {
        AddAction( POPUP_ACTION_ACCEPT,
                   (YesPrompt != -1) ? (const xwchar*)Prompts[YesPrompt] : NULL,
                   "IDS_YES" );
    }

    if( Maybe )
    {
        AddAction( POPUP_ACTION_DELETE,
                   (MaybePrompt != -1) ? (const xwchar*)Prompts[MaybePrompt] : NULL,
                   NULL );
    }

    if( Help )
    {
        AddAction( POPUP_ACTION_HELP,
                   (HelpPrompt != -1) ? (const xwchar*)Prompts[HelpPrompt] : NULL,
                   NULL );
    }

    if( Other )
    {
        AddAction( POPUP_ACTION_ALTERNATE,
                   (OtherPrompt != -1) ? (const xwchar*)Prompts[OtherPrompt] : NULL,
                   NULL );
    }

    // Keep the safe/back action at the right edge of the desktop group.
    if( No )
    {
        AddAction( POPUP_ACTION_CANCEL,
                   (NoPrompt != -1) ? (const xwchar*)Prompts[NoPrompt] : NULL,
                   "IDS_NO" );
    }
}

//=========================================================================

void dlg_popup::AddAction( popup_action_type Type,
                           const xwchar*      Prompt,
                           const char*        pFallbackString,
                           xbool              Enabled )
{
    ASSERT( m_ActionCount < MAX_POPUP_ACTIONS );
    if( m_ActionCount >= MAX_POPUP_ACTIONS )
    {
        return;
    }

    popup_action& Action = m_Actions[m_ActionCount];
    Action.Type    = Type;
    Action.Prompt  = xwstring( "" );
    if( Prompt )
    {
        Action.Prompt = Prompt;
    }
    Action.Label   = GetPromptLabel( Prompt );
    Action.Enabled = Enabled;

    if( Action.Label.IsEmpty() && pFallbackString )
    {
        Action.Label = g_StringTableMgr( "ui", pFallbackString );
        TrimPromptText( Action.Label );
    }

    m_pActionButtons[m_ActionCount]->SetLabel( Action.Label );
    m_pActionButtons[m_ActionCount]->SetFlag( ui_win::WF_DISABLED, !Enabled );
    m_ActionCount++;
}

//=========================================================================

void dlg_popup::SetAction( popup_action_type Type, const xwchar* Prompt, xbool Enabled )
{
    const s32 Index = FindAction( Type );
    if( Index == -1 )
    {
        return;
    }

    popup_action& Action = m_Actions[Index];
    if( Prompt )
    {
        Action.Prompt = Prompt;
        Action.Label  = GetPromptLabel( Prompt );
        m_pActionButtons[Index]->SetLabel( Action.Label );
    }

    Action.Enabled = Enabled;
    m_pActionButtons[Index]->SetFlag( ui_win::WF_DISABLED, !Enabled );
}

//=========================================================================

s32 dlg_popup::FindAction( popup_action_type Type ) const
{
    for( s32 i=0; i<m_ActionCount; i++ )
    {
        if( m_Actions[i].Type == Type )
        {
            return i;
        }
    }

    return -1;
}

//=========================================================================

s32 dlg_popup::FindAction( const ui_win* pWin ) const
{
    for( s32 i=0; i<m_ActionCount; i++ )
    {
        if( m_pActionButtons[i] == pWin )
        {
            return i;
        }
    }

    return -1;
}

//=========================================================================

xbool dlg_popup::IsActionEnabled( popup_action_type Type ) const
{
    const s32 Index = FindAction( Type );
    return (Index != -1) && m_Actions[Index].Enabled;
}

//=========================================================================

void dlg_popup::LayoutActionButtons( void )
{
    if( m_ActionCount == 0 )
    {
        m_ActionRowCount = 0;
        return;
    }

    const s32 AvailableWidth = m_Position.GetWidth() - (ACTION_BUTTON_MARGIN * 2);
    const s32 FontID         = m_pManager->FindFont( "small" );
    s32       DesiredWidth   = ACTION_BUTTON_MIN_WIDTH;

    for( s32 i=0; i<m_ActionCount; i++ )
    {
        const s32 LabelWidth = m_pManager->TextWidth( FontID, m_Actions[i].Label ) + 20;
        if( LabelWidth > DesiredWidth )
        {
            DesiredWidth = LabelWidth;
        }
    }

    if( DesiredWidth > AvailableWidth )
    {
        DesiredWidth = AvailableWidth;
    }

    s32 Columns = (AvailableWidth + ACTION_BUTTON_GAP) /
                  (DesiredWidth + ACTION_BUTTON_GAP);
    if( Columns < 1 )
    {
        Columns = 1;
    }
    if( Columns > m_ActionCount )
    {
        Columns = m_ActionCount;
    }

    const s32 ButtonWidth = (AvailableWidth - ((Columns - 1) * ACTION_BUTTON_GAP)) / Columns;
    m_ActionRowCount      = (m_ActionCount + Columns - 1) / Columns;

    const s32 GroupTop = m_Position.GetHeight() -
                         ACTION_BUTTON_BOTTOM -
                         (m_ActionRowCount * ACTION_BUTTON_HEIGHT) -
                         ((m_ActionRowCount - 1) * ACTION_BUTTON_GAP);

    s32 ActionIndex = 0;
    for( s32 Row=0; Row<m_ActionRowCount; Row++ )
    {
        s32 RowCount = m_ActionCount - ActionIndex;
        if( RowCount > Columns )
        {
            RowCount = Columns;
        }

        const s32 RowWidth = (RowCount * ButtonWidth) +
                             ((RowCount - 1) * ACTION_BUTTON_GAP);
        const s32 RowLeft  = ACTION_BUTTON_MARGIN +
                             ((AvailableWidth - RowWidth) / 2);
        const s32 Top      = GroupTop + Row * (ACTION_BUTTON_HEIGHT + ACTION_BUTTON_GAP);

        for( s32 Column=0; Column<RowCount; Column++, ActionIndex++ )
        {
            const s32 Left = RowLeft + Column * (ButtonWidth + ACTION_BUTTON_GAP);

            m_pActionButtons[ActionIndex]->SetPosition(
                irect( Left,
                       Top,
                       Left + ButtonWidth,
                       Top + ACTION_BUTTON_HEIGHT ) );
        }
    }
}

//=========================================================================

void dlg_popup::UpdateSpecialActions( void )
{
    if( m_RecordDialogState != RECORD_STATE_NONE )
    {
        SetAction( POPUP_ACTION_ACCEPT,
                   g_StringTableMgr( "ui", "IDS_NAV_RECORD" ),
                   m_EnableRecord );
        SetAction( POPUP_ACTION_DELETE,
                   g_StringTableMgr( "ui", "IDS_NAV_PLAY" ),
                   m_EnablePlay );
        SetAction( POPUP_ACTION_CANCEL,
                   g_StringTableMgr( "ui", m_ShowStop ? "IDS_NAV_STOP" : "IDS_NAV_BACK" ),
                   TRUE );
        SetAction( POPUP_ACTION_ALTERNATE,
                   g_StringTableMgr( "ui", "IDS_NAV_SEND" ),
                   m_EnableSend );
    }
    else if( m_PlayDialogState != PLAY_STATE_NONE )
    {
        if( m_PlayDialogState == PLAY_STATE_INVALID_MESSAGE )
        {
            SetAction( POPUP_ACTION_ACCEPT,
                       g_StringTableMgr( "ui", "IDS_NAV_OK" ),
                       TRUE );
        }
        else
        {
            SetAction( POPUP_ACTION_ACCEPT,
                       g_StringTableMgr( "ui", "IDS_NAV_PLAY_PLAY" ),
                       m_EnablePlay );
            SetAction( POPUP_ACTION_CANCEL,
                       g_StringTableMgr( "ui", m_ShowStop ? "IDS_NAV_STOP" : "IDS_NAV_BACK" ),
                       TRUE );
        }
    }

    for( s32 i=0; i<m_ActionCount && i<4; i++ )
    {
        m_pButtonText[i]->SetLabel( m_Actions[i].Prompt );
    }
}

//=========================================================================

void dlg_popup::CompleteAction( s32 Result, xbool TrackController )
{
    if( m_pResult )
    {
        *m_pResult = Result;
    }

    if( TrackController )
    {
        g_StateMgr.SetControllerRequested( g_UiMgr->GetActiveController(), TRUE );
    }

    g_AudioMgr.Play( "Select_Norm" );
    g_UiMgr->EndDialog( m_UserID, TRUE );
}

//=========================================================================

void dlg_popup::Destroy( void )
{
    ui_dialog::Destroy();
}

//=========================================================================

void dlg_popup::Render( s32 ox, s32 oy )
{
    // dim the background dialog
    if( m_bDoBlackout )
    {
        irect rb = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderGouraudRect(rb, xcolor(0,0,0,180),
                                       xcolor(0,0,0,180),
                                       xcolor(0,0,0,180),
                                       xcolor(0,0,0,180),FALSE);
    }

    // Get window rectangle
    irect   r = m_Position;

    // Render background color
    if( m_BackgroundColor.A > 0 )
    {
        irect   rb = r;
        rb.Deflate( 1, 1 );
        m_pManager->RenderRect( rb, m_BackgroundColor, FALSE );
    }

    // Render Title
    irect rect = r;
    rect.Deflate( 1, 1 );
    rect.SetHeight( 22 );
    xcolor c1 = xcolor (25,77,18,255); 
    xcolor c2 = xcolor (25,77,18,255); 
    m_pManager->RenderGouraudRect( rect, c1, c1, c2, c2, FALSE );

    rect.Deflate( 8, 0 );
    rect.Translate( 1, -1 );
    m_pManager->RenderText( 0, rect, ui_font::h_center|ui_font::v_center, xcolor(XCOLOR_BLACK), m_Label );
    rect.Translate( -1, -1 );
    m_pManager->RenderText( 0, rect, ui_font::h_center|ui_font::v_center, xcolor(255,252,204,255), m_Label );

    // Render frame
    m_pManager->RenderElement( m_iElement, r, 0 );
    

    // Render message
    rect = r;
    rect.Deflate(5, 30);
    if( m_ActionRowCount > 0 )
    {
        const s32 ActionAreaHeight = (m_ActionRowCount * ACTION_BUTTON_HEIGHT) +
                                     ((m_ActionRowCount - 1) * ACTION_BUTTON_GAP) +
                                     ACTION_BUTTON_BOTTOM + 5;
        const s32 MessageBottom = r.b - ActionAreaHeight;
        if( rect.b > MessageBottom )
        {
            rect.b = MessageBottom;
        }
    }
    rect.Translate( 1, -1 );
    g_UiMgr->RenderText_Wrap( 1, rect, ui_font::h_center|ui_font::v_top, xcolor(XCOLOR_BLACK), m_Message );
    rect.Translate( -1, -1 );
    g_UiMgr->RenderText_Wrap( 1, rect, ui_font::h_center|ui_font::v_top, xcolor(255,252,204,255), m_Message );

    if (m_Message2)
    {
        rect = r;
        rect.Deflate(5, 70);
        rect.Translate( 1, -1 );
        g_UiMgr->RenderText_Wrap( 1, rect, ui_font::h_center|ui_font::v_top, xcolor(XCOLOR_BLACK), m_Message2 );
        rect.Translate( -1, -1 );
        g_UiMgr->RenderText_Wrap( 1, rect, ui_font::h_center|ui_font::v_top, xcolor(255,252,204,255), m_Message2 );
    }
    
    if (m_Message3)
    {
        rect = r;
        rect.Deflate(5, 110);
        rect.Translate( 1, -1 );
        g_UiMgr->RenderText_Wrap( 1, rect, ui_font::h_center|ui_font::v_top, xcolor(XCOLOR_BLACK), m_Message3 );
        rect.Translate( -1, -1 );
        g_UiMgr->RenderText_Wrap( 1, rect, ui_font::h_center|ui_font::v_top, xcolor(255,252,204,255), m_Message3 );
    }

    if( m_RecordDialogState != RECORD_STATE_NONE || m_PlayDialogState != PLAY_STATE_NONE )
    {
        if( m_PlayDialogState != PLAY_STATE_INVALID_MESSAGE )
        {        
            // render a rect inside a rect ...
            irect rBar;
            rBar = r;
            rBar.Deflate( 20, 1 );
            rBar.SetHeight( 22 );
            rBar.Translate(0,140);
            g_UiMgr->RenderRect(rBar,xcolor (25,77,18,255),FALSE);

            // ? Render Progress Bar
            if( m_RecordDialogState == RECORD_STATE_RECORD || 
                m_RecordDialogState == RECORD_STATE_SENDING ||
                m_RecordDialogState == RECORD_STATE_PLAY ||
                m_PlayDialogState == PLAY_STATE_DOWNLOADING ||
                m_PlayDialogState == PLAY_STATE_PLAY
                )
            {
                rBar.Deflate( 5, 5 );
                s32 progress = (s32)(rBar.GetWidth() * m_ProgressValue);
                rBar.r = rBar.l + progress;
                g_UiMgr->RenderRect(rBar,xcolor (25,180,18,255),FALSE);
            }
        }
    }

    // If in Password mode then lets render out the passcodes
    if( m_PasswordDialogActive )
    {
        //
        irect rBar;
        rBar = r;
        rBar.Deflate( 20, 1 );
        rBar.SetHeight( 22 );
        rBar.Translate(0,140);
        g_UiMgr->RenderRect(rBar,xcolor (25,77,18,255),FALSE);
        rBar.Deflate( 5, 5 );

        xwstring navText;
        if( m_Password[0] != 0 )
            navText =  g_StringTableMgr( "ui", "IDS_PASSWORD_HIDDEN_KEY" );
        if( m_Password[1] != 0 )
            navText += g_StringTableMgr( "ui", "IDS_PASSWORD_HIDDEN_KEY" );
        if( m_Password[2] != 0 )
            navText += g_StringTableMgr( "ui", "IDS_PASSWORD_HIDDEN_KEY" );
        if( m_Password[3] != 0 )
            navText += g_StringTableMgr( "ui", "IDS_PASSWORD_HIDDEN_KEY" );

        g_UiMgr->RenderText_Wrap( 1, rBar, ui_font::h_center|ui_font::v_top, xcolor(XCOLOR_BLACK), navText );
        rBar.Translate( -1, -1 );
        g_UiMgr->RenderText_Wrap( 1, rBar, ui_font::h_center|ui_font::v_top, xcolor(255,252,204,255), navText );
    }

    // Render children
    for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
    {
        m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
    }
}

//=========================================================================

void dlg_popup::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    if( m_RecordDialogState != RECORD_STATE_NONE )
        OnUpdateRecordDialog( DeltaTime );
    else if( m_PlayDialogState != PLAY_STATE_NONE )
        OnUpdatePlayDialog( DeltaTime );

    if(m_PasswordDialogActive)
        OnUpdatePasswordDialog( DeltaTime );

    UpdateSpecialActions();
    ApplyInputLayout( m_pManager->GetInputDevice( m_UserID ) );

    // check if the popup has a timeout
    if( m_bDoTimeout )
    {
        // update the timeout
        if( m_Timeout > DeltaTime )
        {
            m_Timeout -= DeltaTime;
        }
        else
        {
            ui_win* pParent = GetParent();
    
            if( pParent ) 
            {
                m_Timeout = 0.0f;
                m_State = DIALOG_STATE_TIMEOUT;
            }
            else
            {
                // Close dialog
                m_pManager->EndDialog( m_UserID, TRUE );
            }
        }
    }
}

//=========================================================================

void dlg_popup::OnAccept( ui_win* pWin )
{
    // no early outs for timed popups
    if( m_bDoTimeout )
        return;

    // No input if scaling
    if( g_UiMgr->IsScreenScaling() )
    {
        return;
    }

    const s32 ButtonAction = FindAction( pWin );
    if( ButtonAction != -1 )
    {
        if( !m_Actions[ButtonAction].Enabled )
        {
            return;
        }

        switch( m_Actions[ButtonAction].Type )
        {
            case POPUP_ACTION_CANCEL:    OnCancel   ( NULL ); return;
            case POPUP_ACTION_DELETE:    OnDelete   ( NULL ); return;
            case POPUP_ACTION_HELP:      OnHelp     ( NULL ); return;
            case POPUP_ACTION_ALTERNATE: OnAlternate( NULL ); return;
            default: break;
        }
    }

    // record dialog override
    if( m_RecordDialogState != RECORD_STATE_NONE )
    {
        if( IsActionEnabled( POPUP_ACTION_ACCEPT ) )
        {
            OnPadRecord();
        }
        return;
    }
    else if ( m_PlayDialogState != PLAY_STATE_NONE )
    {
        if( IsActionEnabled( POPUP_ACTION_ACCEPT ) )
        {
            OnPadRecord();
        }
        return;
    }

    if( m_PasswordDialogActive )
    {
        if( 
            m_Password[0] == m_CheckPassword[0] &&  
            m_Password[1] == m_CheckPassword[1] &&  
            m_Password[2] == m_CheckPassword[2] &&  
            m_Password[3] == m_CheckPassword[3] 
            )
        {
            // check input flag

            // Set result and close dialog
            if( m_pResult )
                *m_pResult = DLG_POPUP_YES;

            g_AudioMgr.Play( "Select_Norm" );
            g_UiMgr->EndDialog( m_UserID, TRUE );
            return;
        }
        else
        {
            // Play ERROR SOUND
            g_AudioMgr.Play( "Select_Norm" );
            g_UiMgr->EndDialog( m_UserID, TRUE );
            return;
        }
    }

    if( IsActionEnabled( POPUP_ACTION_ACCEPT ) )
    {
        CompleteAction( DLG_POPUP_YES, TRUE );
    }
}

//=========================================================================

void dlg_popup::OnCancel( ui_win* pWin )
{
    (void)pWin;

    // no early outs for timed popups
    if( m_bDoTimeout )
        return;

    // No input if scaling
    if( g_UiMgr->IsScreenScaling() )
    {
        return;
    }
    // record dialog override
    if( m_RecordDialogState != RECORD_STATE_NONE || m_PlayDialogState != PLAY_STATE_NONE )
    {
        if( IsActionEnabled( POPUP_ACTION_CANCEL ) )
        {
            OnPadStop();
        }
        return;
    }

    if( m_PasswordDialogActive )
    {
        // Remove pass key and if its the last then back out of the popup.
        if( RemovePassKey() )
        {
            CompleteAction( DLG_POPUP_NO );
            return;
        }
        else
        {
            return;
        }
    }
    if( IsActionEnabled( POPUP_ACTION_CANCEL ) )
    {
        CompleteAction( DLG_POPUP_NO );
    }
}

//=========================================================================

void dlg_popup::OnDelete( ui_win* pWin )
{
    (void)pWin;

    // no early outs for timed popups
    if( m_bDoTimeout )
        return;

    // No input if scaling
    if( g_UiMgr->IsScreenScaling() )
    {
        return;
    }
    // record dialog override
    if( m_RecordDialogState != RECORD_STATE_NONE )
    {
        if( IsActionEnabled( POPUP_ACTION_DELETE ) )
        {
            OnPadPlay();
        }
        return;
    }
    else if ( m_PlayDialogState != PLAY_STATE_NONE )
    {
        if( IsActionEnabled( POPUP_ACTION_DELETE ) )
        {
            OnPadPlay();
        }
        return;
    }

    if( m_PasswordDialogActive )
    {
        // Add X to Password List.
#ifdef TARGET_XBOX
        AddPassKey(XONLINE_PASSCODE_GAMEPAD_X);
#endif
        return;
    }

    if( IsActionEnabled( POPUP_ACTION_DELETE ) )
    {
        CompleteAction( DLG_POPUP_MAYBE );
    }
}

//=========================================================================

void dlg_popup::OnHelp( ui_win* pWin )
{
    (void)pWin;

    // no early outs for timed popups
    if( m_bDoTimeout )
        return;

    // No input if scaling
    if( g_UiMgr->IsScreenScaling() )
    {
        return;
    }
    if( IsActionEnabled( POPUP_ACTION_HELP ) )
    {
        CompleteAction( DLG_POPUP_HELP );
    }
}

//=========================================================================

void dlg_popup::OnAlternate( ui_win* pWin )
{
    (void)pWin;
    
    // no early outs for timed popups
    if( m_bDoTimeout )
        return;

    // No input if scaling
    if( g_UiMgr->IsScreenScaling() )
    {
        return;
    }
    // record dialog override
    if( m_RecordDialogState != RECORD_STATE_NONE )
    {
        if( IsActionEnabled( POPUP_ACTION_ALTERNATE ) )
        {
            OnPadSend();
        }
        return;
    }
    else if( m_PlayDialogState != PLAY_STATE_NONE )
    {
        if( IsActionEnabled( POPUP_ACTION_ALTERNATE ) )
        {
            OnPadSend();
        }
        return;
    }
    
    if( m_PasswordDialogActive )
    {
        // Add X to Password List.
#ifdef TARGET_XBOX
        AddPassKey(XONLINE_PASSCODE_GAMEPAD_Y);
#endif
        return;
    }

    if( IsActionEnabled( POPUP_ACTION_ALTERNATE ) )
    {
        CompleteAction( DLG_POPUP_OTHER );
    }
}

//========================================================================

void dlg_popup::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX /* = FALSE  */, xbool WrapY /* = FALSE */)
{
    const ui_input_device Device = m_pManager->GetInputDevice( m_UserID );
    const xbool UseDesktopButtons = (m_ActionCount > 0) &&
                                    !m_PasswordDialogActive &&
                                   ((Device == ui_input_device::Mouse) ||
                                    (Device == ui_input_device::Keyboard));
    if( UseDesktopButtons && (Device == ui_input_device::Keyboard) )
    {
        (void)Presses;
        (void)Repeats;
        (void)WrapX;
        (void)WrapY;

        s32 Current = FindAction( pWin );
        if( Current == -1 )
        {
            Current = FindAction( m_pManager->GetFocusedWindow( m_UserID ) );
        }

        if( Current == -1 )
        {
            for( s32 i=0; i<m_ActionCount; i++ )
            {
                if( m_Actions[i].Enabled )
                {
                    m_pManager->SetFocusWindow( m_UserID, m_pActionButtons[i] );
                    break;
                }
            }
            return;
        }

        const irect& CurrentPos = m_pActionButtons[Current]->GetPosition();
        const s32 CurrentX = CurrentPos.l + CurrentPos.GetWidth () / 2;
        const s32 CurrentY = CurrentPos.t + CurrentPos.GetHeight() / 2;
        s32 BestIndex = -1;
        s32 BestScore = 0x7fffffff;

        for( s32 i=0; i<m_ActionCount; i++ )
        {
            if( (i == Current) || !m_Actions[i].Enabled )
            {
                continue;
            }

            const irect& CandidatePos = m_pActionButtons[i]->GetPosition();
            const s32 CandidateX = CandidatePos.l + CandidatePos.GetWidth () / 2;
            const s32 CandidateY = CandidatePos.t + CandidatePos.GetHeight() / 2;
            const s32 DeltaX = CandidateX - CurrentX;
            const s32 DeltaY = CandidateY - CurrentY;

            xbool IsCandidate = FALSE;
            s32   Primary     = 0;
            s32   Secondary   = 0;

            switch( Code )
            {
                case ui_navigation::Left:
                    IsCandidate = DeltaX < 0;
                    Primary     = -DeltaX;
                    Secondary   = (DeltaY < 0) ? -DeltaY : DeltaY;
                    break;

                case ui_navigation::Right:
                    IsCandidate = DeltaX > 0;
                    Primary     = DeltaX;
                    Secondary   = (DeltaY < 0) ? -DeltaY : DeltaY;
                    break;

                case ui_navigation::Up:
                    IsCandidate = DeltaY < 0;
                    Primary     = -DeltaY;
                    Secondary   = (DeltaX < 0) ? -DeltaX : DeltaX;
                    break;

                case ui_navigation::Down:
                    IsCandidate = DeltaY > 0;
                    Primary     = DeltaY;
                    Secondary   = (DeltaX < 0) ? -DeltaX : DeltaX;
                    break;

                default:
                    return;
            }

            const s32 Score = Primary * 1024 + Secondary;
            if( IsCandidate && (Score < BestScore) )
            {
                BestIndex = i;
                BestScore = Score;
            }
        }

        if( BestIndex != -1 )
        {
            m_pManager->SetFocusWindow( m_UserID, m_pActionButtons[BestIndex] );
        }
        return;
    }

#ifdef TARGET_XBOX
    if( !m_PasswordDialogActive )
    {
        return;
    }

    switch( Code )
    {
        case ui_navigation::Left:  AddPassKey(XONLINE_PASSCODE_DPAD_LEFT);  break;
        case ui_navigation::Right: AddPassKey(XONLINE_PASSCODE_DPAD_RIGHT); break;
        case ui_navigation::Up:    AddPassKey(XONLINE_PASSCODE_DPAD_UP);    break;
        case ui_navigation::Down:  AddPassKey(XONLINE_PASSCODE_DPAD_DOWN);  break;
    }
#endif
}

//========================================================================

void dlg_popup::OnPage( ui_win* pWin, s32 Direction )
{
    (void)pWin;
    (void)Direction;
#ifdef TARGET_XBOX
    if( !m_PasswordDialogActive )
    {
        return;
    }

    switch( Direction )
    {
        case -1: // LEFT?
            AddPassKey(XONLINE_PASSCODE_GAMEPAD_LEFT_TRIGGER);
        break;
        case 1: // RIGHT?
            AddPassKey(XONLINE_PASSCODE_GAMEPAD_RIGHT_TRIGGER);
        break;
    }
#endif
}

//=========================================================================

void dlg_popup::OnUpdateRecordDialog( f32 DeltaTime )
{
    (void)DeltaTime;

#ifdef TARGET_XBOX

    headset& Headset = g_VoiceMgr.GetHeadset();

    // Dynamically enable and disable dialog control buttons
    {
        xbool OperationInProgress = Headset.GetVoiceIsPlaying() ||
                                    Headset.GetVoiceIsRecording();

        m_ShowStop     =  OperationInProgress;
        m_EnableSend   = !OperationInProgress;
        m_EnableRecord = !OperationInProgress && (Headset.IsHardwarePresent() == TRUE);
        m_EnablePlay   = !OperationInProgress && (Headset.GetVoiceNumBytesRec() > 0);
    }

    switch( m_RecordDialogState )
    {
        case RECORD_STATE_NONE:

        break;

        case RECORD_STATE_INIT:

            Headset.InitVoiceRecording();
            m_RecordDialogState = RECORD_STATE_IDLE;

        break;

        case RECORD_STATE_RECORD_INIT:
        {
            m_ProgressValue = 0.0f;

            if( Headset.IsHardwarePresent() == TRUE )
            {
                Headset.StartVoiceRecording();
                m_RecordDialogState = RECORD_STATE_RECORD;
            }
            else
            {
                m_RecordDialogState = RECORD_STATE_IDLE;
            }
        }
        break;

        case RECORD_STATE_RECORD:

            if( Headset.GetVoiceIsRecording() == FALSE )
            {
                // Done Recording
                m_RecordDialogState = RECORD_STATE_STOP;
                m_ProgressValue     = 0.0f;
            }
            else
            {        
                // Show Progress bar.
                m_ProgressValue = Headset.GetVoiceRecordingProgress();
            }

        break;

        case RECORD_STATE_PLAY_INIT:  
        {
            s32 NumBytes   = Headset.GetVoiceNumBytesRec();
            s32 DurationMS = Headset.GetVoiceDurationMS();

            m_ProgressValue = 0.0f;

            if( NumBytes > 0 )
            {
                byte*   pVoiceMessage = Headset.GetVoiceMessageRec();
                ASSERT( pVoiceMessage != NULL );
                Headset.StartVoicePlaying( pVoiceMessage, DurationMS, NumBytes );
                m_RecordDialogState = RECORD_STATE_PLAY;
            }
            else
            {
                m_RecordDialogState = RECORD_STATE_IDLE;
            }
        }
        break;

        case RECORD_STATE_PLAY:

            if( Headset.GetVoiceIsPlaying() == FALSE )
            {
                // Finished with the voice message so destroy it
                m_RecordDialogState = RECORD_STATE_STOP_PLAYBACK;
                m_ProgressValue     = 0.0f;
            }
            else
            {
                // Print Play Progress
                m_ProgressValue = Headset.GetVoicePlayingProgress();
            }   

        break;

        case RECORD_STATE_STOP:
        {                        
            Headset.StopVoiceRecording();
            m_RecordDialogState = RECORD_STATE_IDLE;
        }
        break;

        case RECORD_STATE_STOP_PLAYBACK:
        {
            g_VoiceMgr.GetHeadset().StopVoicePlaying();
            m_RecordDialogState = RECORD_STATE_IDLE;
        }
        break;
        
        case RECORD_STATE_SEND:
        {            
            // Only allow voice mail to be sent if there is any data
            if( Headset.GetVoiceNumBytesRec() > 0 )
            {
                // Set voice attachment to be sent
                f32 Duration = Headset.GetVoiceDurationMS() / 1000.0f;
                g_MatchMgr.SetVoiceMessage( Headset.GetVoiceMessageRec(), Headset.GetVoiceNumBytesRec(), Duration );
            }

            if( m_AskMode == BUDDY_MODE_ADD )
            {
                g_MatchMgr.AddBuddy( m_Buddy );
                m_RecordDialogState = RECORD_STATE_SENDING;
            }
            else
            {
                g_MatchMgr.InviteBuddy( m_Buddy );
                m_RecordDialogState = RECORD_STATE_SENDING;
            }

            m_ProgressValue = 0.0f;
        }
        break;
        
        case RECORD_STATE_SENDING:

            if( g_MatchMgr.IsSendingMessage() == TRUE )
            {
                m_ProgressValue = g_MatchMgr.GetMessageSendProgress();
            }
            else
            {
                m_ProgressValue = 0.0f;
                g_MatchMgr.ClearVoiceMessage();

                // Done..
                if( m_pResult )
                    *m_pResult = DLG_POPUP_OTHER;
            }

        break;

        case RECORD_STATE_EXIT:
            
            if( m_pResult )
                *m_pResult = DLG_POPUP_NO;

        break;

        case RECORD_STATE_IDLE:
            m_ProgressValue = 0.0f;
        break;

        default:
        break;
    }

    if( m_pResult )
    {
        if( *m_pResult != DLG_POPUP_IDLE )
        {
            if( g_UiMgr->GetTopmostDialog( m_UserID ) == this )
                g_UiMgr->EndDialog( m_UserID, TRUE );
        }
    }

#endif
}

//=========================================================================

void dlg_popup::OnUpdatePlayDialog( f32 DeltaTime )
{
    (void)DeltaTime;
#ifdef TARGET_XBOX

    headset& Headset = g_VoiceMgr.GetHeadset();

    // Disable Play button if we are downloading or playing message
    m_EnablePlay = (m_PlayDialogState != PLAY_STATE_DOWNLOADING) &&
                   (Headset.GetVoiceIsPlaying() == FALSE);
    m_ShowStop   =  Headset.GetVoiceIsPlaying();

    switch( m_PlayDialogState )
    {
        case PLAY_STATE_NONE:

        break;

        case PLAY_STATE_IDLE:
            m_ProgressValue = 0.0f;
        break;

        case PLAY_STATE_START_DOWNLOAD:

            // Attempt to start the voice attachment download.
            if( g_MatchMgr.StartMessageDownload( m_Buddy ) == TRUE )
            {
                m_PlayDialogState = PLAY_STATE_DOWNLOADING;
            }
            else
            {
                // TODO: We need a dialog that says "This message is no longer available".
                m_PlayDialogState = PLAY_STATE_INVALID_MESSAGE;

                // Change the popup message.
                m_Message = g_StringTableMgr( "ui", "IDS_INVALID_MESSAGE" );

                ClearActions();
                AddAction( POPUP_ACTION_ACCEPT,
                           g_StringTableMgr( "ui", "IDS_NAV_OK" ),
                           NULL,
                           TRUE );

                irect NavTextPos = m_pButtonText[0]->GetPosition();
                NavTextPos.l = m_pNavText->GetPosition().l;
                NavTextPos.r = m_pNavText->GetPosition().r;
                m_pButtonText[0]->SetPosition(NavTextPos);
            }

        break;

        case PLAY_STATE_INVALID_MESSAGE:
        {
            // wait for OK input.
        }
        break;

        case PLAY_STATE_DOWNLOADING:

            if( g_MatchMgr.IsDownloadingMessage() == TRUE )
            {
                m_ProgressValue = g_MatchMgr.GetMessageRecProgress();
            }
            else
            {
                m_ProgressValue   = 0.0f;
                m_PlayDialogState = PLAY_STATE_IDLE;
            }

        break;

        case PLAY_STATE_START_PLAY:
        {
            byte* pVoiceMessage = NULL;
            s32   NumBytes;
            f32   Duration;

            g_MatchMgr.GetVoiceMessage( &pVoiceMessage, &NumBytes, &Duration );
            ASSERT( pVoiceMessage != NULL );

            if( NumBytes > 0 )
            {
                Headset.StartVoicePlaying( pVoiceMessage, s32(Duration * 1000.0f), NumBytes );
                m_PlayDialogState = PLAY_STATE_PLAY;
            }
        }
        break;

        case PLAY_STATE_PLAY:

            if( Headset.GetVoiceIsPlaying() == FALSE )
            {
                // Finished with the voice message so destroy it
                m_PlayDialogState = PLAY_STATE_STOP;
                m_ProgressValue   = 0.0f;
            }
            else
            {
                // Print Play Progress
                m_ProgressValue = Headset.GetVoicePlayingProgress();
            }

        break;

        case PLAY_STATE_STOP:
        {
            Headset.StopVoicePlaying();
            m_PlayDialogState = PLAY_STATE_IDLE;
        }
        break;

        case PLAY_STATE_EXIT:
        {
            if( m_pResult )
                *m_pResult = DLG_POPUP_OTHER;
        }
        break;

        case PLAY_STATE_INVALID_MESSAGE_EXIT:
        {
            if( m_pResult )
                *m_pResult = DLG_POPUP_NO;
        }
        break;
    }

    if( m_pResult )
    {
        if( *m_pResult != DLG_POPUP_IDLE )
        {
            if( g_UiMgr->GetTopmostDialog( m_UserID ) == this )
                g_UiMgr->EndDialog( m_UserID, TRUE );
        }
    }

#endif
}


//=========================================================================

void dlg_popup::OnPadRecord( void )
{
    // OnAccept

    if( m_RecordDialogState != RECORD_STATE_NONE )
    {
        
        if( m_RecordDialogState == RECORD_STATE_INIT )
            return;

        if( 
            m_RecordDialogState != RECORD_STATE_RECORD_INIT && 
            m_RecordDialogState != RECORD_STATE_RECORD &&
            m_RecordDialogState != RECORD_STATE_PLAY &&
            m_RecordDialogState != RECORD_STATE_PLAY_INIT
        )
            m_RecordDialogState = RECORD_STATE_RECORD_INIT;
    }
    else
    {
        // ? Invalid Message
        if( m_PlayDialogState == PLAY_STATE_INVALID_MESSAGE )
        {
            // then we need to exit now.
            m_PlayDialogState = PLAY_STATE_INVALID_MESSAGE_EXIT;
            return;
        }

        // Play attachment
        if( 
            m_PlayDialogState != PLAY_STATE_PLAY &&
            m_PlayDialogState != PLAY_STATE_START_DOWNLOAD &&
            m_PlayDialogState != PLAY_STATE_DOWNLOADING
            )
        {
            m_PlayDialogState = PLAY_STATE_START_PLAY;
        }
    }
    return;
}

//=========================================================================

void dlg_popup::OnPadStop( void )
{
    // OnCancel

    if( m_RecordDialogState != RECORD_STATE_NONE )
    {
        if( m_RecordDialogState == RECORD_STATE_RECORD )
        {
            // Stop Record
            m_RecordDialogState = RECORD_STATE_STOP;
        }
        else if( m_RecordDialogState == RECORD_STATE_PLAY )
        {
            // Stop Playback.
            m_RecordDialogState = RECORD_STATE_STOP_PLAYBACK;
        }
        else
        {
            // Back out.
            m_RecordDialogState = RECORD_STATE_EXIT;
        }
    }
    else
    {
        // Stop playback
        if( m_PlayDialogState == PLAY_STATE_PLAY )
            m_PlayDialogState = PLAY_STATE_STOP;

        if( m_PlayDialogState == PLAY_STATE_IDLE )
            m_PlayDialogState = PLAY_STATE_EXIT;
    }
}

//=========================================================================

void dlg_popup::OnPadPlay( void )
{
    // OnDelete
    if( m_RecordDialogState != RECORD_STATE_NONE )
    {
        if( 
            m_RecordDialogState != RECORD_STATE_RECORD_INIT &&
            m_RecordDialogState != RECORD_STATE_RECORD && 
            m_RecordDialogState != RECORD_STATE_PLAY 
            )
        {
            m_RecordDialogState = RECORD_STATE_PLAY_INIT;
        }
    }
}

//=========================================================================

void dlg_popup::OnPadSend( void )
{
    // OnAlternate
    if( m_RecordDialogState != RECORD_STATE_NONE )
    {
   
        if( m_RecordDialogState != RECORD_STATE_PLAY &&
            m_RecordDialogState != RECORD_STATE_RECORD &&
            m_RecordDialogState != RECORD_STATE_RECORD_INIT &&
            m_RecordDialogState != RECORD_STATE_STOP &&
            m_RecordDialogState != RECORD_STATE_SEND &&
            m_RecordDialogState != RECORD_STATE_SENDING

            )
        {
            // SEND
            m_RecordDialogState = RECORD_STATE_SEND;
        }
    }
}

//========================================================================

void dlg_popup::ConfigurePassword( irect& Position, const xwchar* Title, const xwchar* Message, const xwchar* pNavText, s32* pResult /* = NULL  */)
{
    SetLabel( Title );

    ClearActions();
    m_PasswordDialogActive = TRUE;

    m_PasswordIndex = -1;
    m_Password[0] = 0;
    m_Password[1] = 0;
    m_Password[2] = 0;
    m_Password[3] = 0;

    m_ProgressValue = 0.0f;

    m_Message       = Message;
    m_Message2      = xwstring( "" );
    m_Message3      = xwstring( "" );

    // set up nav text
    if( pNavText )
    {
        m_HasNavText = pNavText[0] != 0;
        m_NavTextIsActionPrompt = TRUE;
        m_pNavText->SetLabelFlags( ui_font::h_center|ui_font::v_top|ui_font::input_glyphs );
        m_pNavText->SetLabel( pNavText );
        m_pNavText->SetFlag(ui_win::WF_VISIBLE, TRUE);
    }

    m_pResult       = pResult;
    m_bDoTimeout    = FALSE;
    m_bDoBlackout   = TRUE;

    irect Bounds = g_UiMgr->GetUserBounds( g_UiUserID );

    // center in screen
    Position.Translate( Bounds.l + (Bounds.GetWidth()-Position.GetWidth())/2 - Position.l,
                        Bounds.t + (Bounds.GetHeight()-Position.GetHeight())/2 - Position.t );
    SetPosition( Position );

    // reposition nav text
    irect NavPos = m_pNavText->GetPosition();
    NavPos.b = Position.b - Position.t;
    NavPos.t = NavPos.b - 25;
    NavPos.l = 0;
    NavPos.r = Position.r - Position.l;
    m_pNavText->SetPosition( NavPos );

    // initialize result
    if( m_pResult )
    {
        *m_pResult = DLG_POPUP_IDLE;
    }
}

//========================================================================

void dlg_popup::OnUpdatePasswordDialog( f32 DelatTime )
{
    (void)DelatTime;   
}

//========================================================================

void dlg_popup::AddPassKey( s32 key )
{
    if( m_PasswordIndex == 3 )
        return;

    m_PasswordIndex++;
    m_Password[m_PasswordIndex] = key;
}

//========================================================================

xbool dlg_popup::RemovePassKey( void )
{
    if( m_PasswordIndex > -1 )
    {
        m_Password[0] = 0;
        m_Password[1] = 0;
        m_Password[2] = 0;
        m_Password[3] = 0;
        m_PasswordIndex = -1;
    }
    else
    {
        return TRUE;
    }
    return FALSE;
}

//========================================================================

void dlg_popup::SetPassword( u8* password )
{
    m_CheckPassword[0] = password[0];
    m_CheckPassword[1] = password[1];
    m_CheckPassword[2] = password[2];
    m_CheckPassword[3] = password[3];
}

//========================================================================
