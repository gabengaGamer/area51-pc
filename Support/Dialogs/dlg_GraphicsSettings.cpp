//==============================================================================
//
//  dlg_GraphicsSettings.cpp
//
//==============================================================================

#include "Entropy.hpp"

#include "UI/ui_button.hpp"
#include "UI/ui_check.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_slider.hpp"
#include "UI/ui_text.hpp"

#include "dlg_GraphicsSettings.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================

enum graphics_controls
{
    IDC_GRAPHICS_FIELD_OF_VIEW_TEXT,
    IDC_GRAPHICS_DYNAMIC_SHADOWS_TEXT,
    IDC_GRAPHICS_SHADOW_FILTER_TEXT,
    IDC_GRAPHICS_FILM_GRAIN_TEXT,
    IDC_GRAPHICS_BACKGROUND_BLUR_TEXT,
    IDC_GRAPHICS_ANTI_ALIASING_TEXT,

    IDC_GRAPHICS_FIELD_OF_VIEW,
    IDC_GRAPHICS_DYNAMIC_SHADOWS,
    IDC_GRAPHICS_SHADOW_FILTER,
    IDC_GRAPHICS_FILM_GRAIN,
    IDC_GRAPHICS_BACKGROUND_BLUR,
    IDC_GRAPHICS_ANTI_ALIASING,
    IDC_GRAPHICS_BUTTON_APPLY,
};

//==============================================================================

ui_manager::control_tem GraphicsSettingsControls[] =
{
    { IDC_GRAPHICS_FIELD_OF_VIEW_TEXT,   "IDS_NULL",                   "text",    40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_DYNAMIC_SHADOWS_TEXT, "IDS_NULL",                   "text",    40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_SHADOW_FILTER_TEXT,   "IDS_NULL",                   "text",    40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_FILM_GRAIN_TEXT,      "IDS_NULL",                   "text",    40, 145, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_BACKGROUND_BLUR_TEXT, "IDS_NULL",                   "text",    40, 180, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ANTI_ALIASING_TEXT,   "IDS_NULL",                   "text",    40, 215, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_GRAPHICS_FIELD_OF_VIEW,        "IDS_NULL",                   "slider", 300,  40, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_DYNAMIC_SHADOWS,      "IDS_NULL",                   "check",  290,  75, 140, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_SHADOW_FILTER,        "IDS_NULL",                   "combo",  290, 110, 140, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_FILM_GRAIN,           "IDS_NULL",                   "slider", 300, 145, 120, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_BACKGROUND_BLUR,      "IDS_NULL",                   "check",  290, 180, 140, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ANTI_ALIASING,        "IDS_NULL",                   "combo",  290, 215, 140, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },

    { IDC_GRAPHICS_BUTTON_APPLY,         "IDS_PROFILE_OPTIONS_ACCEPT", "button",  40, 285, 220, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE },
};

//==============================================================================

ui_manager::dialog_tem GraphicsSettingsDialog =
{
    "IDS_OPTIONS_GRAPHICS_SETTINGS",
    1, 9,
    sizeof(GraphicsSettingsControls)/sizeof(ui_manager::control_tem),
    &GraphicsSettingsControls[0],
    0
};

//==============================================================================

void dlg_graphics_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "graphics settings", &GraphicsSettingsDialog, &dlg_graphics_settings_factory );
}

//==============================================================================

ui_win* dlg_graphics_settings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_graphics_settings* pDialog = new dlg_graphics_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return (ui_win*)pDialog;
}

//==============================================================================

dlg_graphics_settings::dlg_graphics_settings( void )
{
}

//==============================================================================

dlg_graphics_settings::~dlg_graphics_settings( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_graphics_settings::Create( s32                       UserID,
                                     ui_manager*               pManager,
                                     ui_manager::dialog_tem*   pDialogTem,
                                     const irect&              Position,
                                     ui_win*                   pParent,
                                     s32                       Flags,
                                     void*                     pUserData )
{
    (void)pUserData;
    ASSERT( pManager );

    xbool const Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    m_pFieldOfView        = (ui_slider*)FindChildByID( IDC_GRAPHICS_FIELD_OF_VIEW );
    m_pDynamicShadows     = (ui_check*) FindChildByID( IDC_GRAPHICS_DYNAMIC_SHADOWS );
    m_pShadowFilter       = (ui_combo*) FindChildByID( IDC_GRAPHICS_SHADOW_FILTER );
    m_pFilmGrain          = (ui_slider*)FindChildByID( IDC_GRAPHICS_FILM_GRAIN );
    m_pBackgroundBlur     = (ui_check*) FindChildByID( IDC_GRAPHICS_BACKGROUND_BLUR );
    m_pAntiAliasing       = (ui_combo*) FindChildByID( IDC_GRAPHICS_ANTI_ALIASING );
    m_pButtonApply        = (ui_button*)FindChildByID( IDC_GRAPHICS_BUTTON_APPLY );

    m_pFieldOfViewText    = (ui_text*)FindChildByID( IDC_GRAPHICS_FIELD_OF_VIEW_TEXT );
    m_pDynamicShadowsText = (ui_text*)FindChildByID( IDC_GRAPHICS_DYNAMIC_SHADOWS_TEXT );
    m_pShadowFilterText   = (ui_text*)FindChildByID( IDC_GRAPHICS_SHADOW_FILTER_TEXT );
    m_pFilmGrainText      = (ui_text*)FindChildByID( IDC_GRAPHICS_FILM_GRAIN_TEXT );
    m_pBackgroundBlurText = (ui_text*)FindChildByID( IDC_GRAPHICS_BACKGROUND_BLUR_TEXT );
    m_pAntiAliasingText   = (ui_text*)FindChildByID( IDC_GRAPHICS_ANTI_ALIASING_TEXT );

    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    m_OriginalSettings = PendingSettings;

    m_pFieldOfView->SetRange( FIELD_OF_VIEW_MIN_DEGREES, FIELD_OF_VIEW_MAX_DEGREES );
    m_pFieldOfView->SetStep( 5 );
    m_pFieldOfView->SetValue( PendingSettings.GetFieldOfView() );
    m_pFieldOfView->UseDefaultSound( FALSE );

    m_pDynamicShadows->SetChecked( PendingSettings.GetDynamicShadowsEnabled() );

    m_pShadowFilter->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pShadowFilter->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_SHADOW_FILTER_EVSM" ),
                              static_cast<uaddr>( ShadowFilterType::Evsm ) );
    m_pShadowFilter->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_SHADOW_FILTER_HARD" ),
                              static_cast<uaddr>( ShadowFilterType::Hard ) );
    s32 ShadowFilterSelection =
        m_pShadowFilter->FindItemByData( static_cast<uaddr>( PendingSettings.GetShadowFilterType() ) );
    if( ShadowFilterSelection == -1 )
    {
        ShadowFilterSelection =
            m_pShadowFilter->FindItemByData( static_cast<uaddr>( SHADOW_FILTER_TYPE_DEFAULT ) );
    }
    m_pShadowFilter->SetSelection( ShadowFilterSelection );

    m_pFilmGrain->SetRange( FILM_GRAIN_MIN_STRENGTH, FILM_GRAIN_MAX_STRENGTH );
    m_pFilmGrain->SetStep( 5, 10 );
    m_pFilmGrain->SetValue( PendingSettings.GetFilmGrainStrength() );
    m_pFilmGrain->UseDefaultSound( FALSE );

    m_pBackgroundBlur->SetChecked( PendingSettings.GetBackgroundBlurEnabled() );

    m_pAntiAliasing->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pAntiAliasing->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_ANTI_ALIASING_NONE" ),
                              static_cast<uaddr>( AntiAliasingType::None ) );
    m_pAntiAliasing->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_ANTI_ALIASING_CMAA2" ),
                              static_cast<uaddr>( AntiAliasingType::Cmaa2 ) );
    s32 const AntiAliasingSelection =
        m_pAntiAliasing->FindItemByData(
            static_cast<uaddr>( ANTI_ALIASING_TYPE_DEFAULT ) );
    m_pAntiAliasing->SetSelection( AntiAliasingSelection );

    m_pFieldOfViewText   ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_FIELD_OF_VIEW" ) );
    m_pDynamicShadowsText->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_DYNAMIC_SHADOWS" ) );
    m_pShadowFilterText  ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_SHADOW_FILTER" ) );
    m_pFilmGrainText     ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_FILM_GRAIN" ) );
    m_pBackgroundBlurText->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_BACKGROUND_BLUR" ) );
    m_pAntiAliasingText  ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_ANTI_ALIASING" ) );

    m_pFieldOfViewText   ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pDynamicShadowsText->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pShadowFilterText  ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pFilmGrainText     ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pBackgroundBlurText->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pAntiAliasingText  ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pButtonApply       ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pAntiAliasing      ->SetFlag( ui_win::WF_DISABLED, TRUE );
    m_pAntiAliasingText  ->SetFlag( ui_win::WF_DISABLED, TRUE );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( (ui_control*)m_pFieldOfView );
        m_CurrentControl = IDC_GRAPHICS_FIELD_OF_VIEW;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    m_pFieldOfView       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pDynamicShadows    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pShadowFilter      ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFilmGrain         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pBackgroundBlur    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pAntiAliasing      ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pButtonApply       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFieldOfViewText   ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pDynamicShadowsText->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pShadowFilterText  ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFilmGrainText     ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pBackgroundBlurText->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pAntiAliasingText  ->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_CurrHL = 0;
    m_PopUp = NULL;
    m_PopUpResult = DLG_POPUP_IDLE;

    xwstring navText( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) );
    navText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
    navText += g_StringTableMgr( "ui", "IDS_NAV_RESTORE_DEFAULTS" );
    SetNavText( navText );

    InitScreenScaling( Position );
    m_bRenderBlackout = FALSE;
    m_State = DIALOG_STATE_ACTIVE;
    return Success;
}

//==============================================================================

void dlg_graphics_settings::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_graphics_settings::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap   = 9;
    static s32 width = 4;
    irect rb;

    if( m_bRenderBlackout )
    {
        rb = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderGouraudRect( rb,
                                    xcolor(0,0,0,180), xcolor(0,0,0,180),
                                    xcolor(0,0,0,180), xcolor(0,0,0,180), FALSE );
    }

    rb.l = m_CurrPos.l + 22;
    rb.t = m_CurrPos.t;
    rb.r = m_CurrPos.r - 23;
    rb.b = m_CurrPos.b;
    g_UiMgr->RenderGouraudRect( rb,
                                xcolor(56,115,58,64), xcolor(56,115,58,64),
                                xcolor(56,115,58,64), xcolor(56,115,58,64), FALSE );

    for( s32 y = rb.t + offset; y < rb.b; y += gap )
    {
        irect bar( rb.l, y, rb.r, MIN( y + width, rb.b ) );
        g_UiMgr->RenderGouraudRect( bar,
                                    xcolor(56,115,58,30), xcolor(56,115,58,30),
                                    xcolor(56,115,58,30), xcolor(56,115,58,30), FALSE );
    }

    ui_dialog::Render( ox, oy );
    g_UiMgr->RenderGlowBar();
}

//==============================================================================

void dlg_graphics_settings::OnAccept( ui_win* pWin )
{
    if( (m_State != DIALOG_STATE_ACTIVE) || (pWin != (ui_win*)m_pButtonApply) )
    {
        return;
    }

    global_settings Settings = m_OriginalSettings;
    ApplySettings( Settings );
    if( !Settings.HasChanged() )
    {
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_BACK;
        return;
    }

    g_StateMgr.GetPendingSettings() = Settings;
    g_AudioMgr.Play( "Select_Norm" );
    OpenSavePopup();
}

//==============================================================================

void dlg_graphics_settings::OnCancel( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        global_settings Settings = m_OriginalSettings;
        ApplySettings( Settings );
        if( !Settings.HasChanged() )
        {
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
            return;
        }

        g_StateMgr.GetPendingSettings() = Settings;
        OpenSavePopup();
    }
}

//==============================================================================

void dlg_graphics_settings::ApplySettings( global_settings& Settings )
{
    Settings.SetFieldOfView( m_pFieldOfView->GetValue() );
    Settings.SetDynamicShadowsEnabled( m_pDynamicShadows->IsChecked() );
    if( m_pShadowFilter->GetSelection() != -1 )
    {
        Settings.SetShadowFilterType(
            static_cast<ShadowFilterType>( m_pShadowFilter->GetSelectedItemData() ) );
    }
    Settings.SetFilmGrainStrength( m_pFilmGrain->GetValue() );
    Settings.SetBackgroundBlurEnabled( m_pBackgroundBlur->IsChecked() );
    Settings.SetAntiAliasingType( ANTI_ALIASING_TYPE_DEFAULT );
}

//==============================================================================

void dlg_graphics_settings::BeginSave( void )
{
    g_SaveDataMgr.SaveSettings( this, &dlg_graphics_settings::OnSaveSettingsCB );
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_graphics_settings::OpenSavePopup( void )
{
    irect const Bounds = g_UiMgr->GetUserBounds( g_UiUserID );
    m_PopUp = static_cast<dlg_popup*>( g_UiMgr->OpenDialog( m_UserID,
                                                            "popup",
                                                            Bounds,
                                                            NULL,
                                                            ui_win::WF_VISIBLE | ui_win::WF_BORDER |
                                                            ui_win::WF_DLG_CENTER | ui_win::WF_INPUTMODAL ) );
    xwstring PopupNavText( g_StringTableMgr( "ui", "IDS_NAV_YES" ) );
    PopupNavText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
    SetNavTextVisible( FALSE );
    m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_SETTINGS_EDIT" ),
                        TRUE,
                        TRUE,
                        FALSE,
                        g_StringTableMgr( "ui", "IDS_SETTINGS_CHANGED_MSG" ),
                        PopupNavText,
                        &m_PopUpResult );
    m_State = DIALOG_STATE_POPUP;
}

//==============================================================================

void dlg_graphics_settings::RestoreSettings( void )
{
    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    PendingSettings = m_OriginalSettings;
    PendingSettings.CommitGraphics();
}

//==============================================================================

void dlg_graphics_settings::OnSaveSettingsCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        g_StateMgr.ActivatePendingSettings();
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_BACK;
    }
    else
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_SAVE_DATA_ERROR;
    }
}

//==============================================================================

void dlg_graphics_settings::OnDelete( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play( "Select_Norm" );
        global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
        PendingSettings.Reset( RESET_GRAPHICS );

        m_pFieldOfView->SetValue( PendingSettings.GetFieldOfView() );
        m_pDynamicShadows->SetChecked( PendingSettings.GetDynamicShadowsEnabled() );
        s32 const ShadowFilterSelection =
            m_pShadowFilter->FindItemByData(
                static_cast<uaddr>( PendingSettings.GetShadowFilterType() ) );
        m_pShadowFilter->SetSelection( ShadowFilterSelection );
        m_pFilmGrain->SetValue( PendingSettings.GetFilmGrainStrength() );
        m_pBackgroundBlur->SetChecked( PendingSettings.GetBackgroundBlurEnabled() );
        s32 const AntiAliasingSelection =
            m_pAntiAliasing->FindItemByData(
                static_cast<uaddr>( ANTI_ALIASING_TYPE_DEFAULT ) );
        m_pAntiAliasing->SetSelection( AntiAliasingSelection );
    }
}

//==============================================================================

void dlg_graphics_settings::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    s32 highLight = -1;

    if( m_PopUp && (m_PopUpResult != DLG_POPUP_IDLE) )
    {
        s32 const Result = m_PopUpResult;
        m_PopUp = NULL;
        m_PopUpResult = DLG_POPUP_IDLE;
        SetNavTextVisible( TRUE );

        if( Result == DLG_POPUP_YES )
        {
            BeginSave();
        }
        else
        {
            RestoreSettings();
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
        }
    }

    if( g_UiMgr->IsScreenScaling() && (UpdateScreenScaling( DeltaTime ) == FALSE) )
    {
        m_pFieldOfView       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pDynamicShadows    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pShadowFilter      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pFilmGrain         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pBackgroundBlur    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pAntiAliasing      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pButtonApply       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pFieldOfViewText   ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pDynamicShadowsText->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pShadowFilterText  ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pFilmGrainText     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pBackgroundBlurText->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pAntiAliasingText  ->SetFlag( ui_win::WF_VISIBLE, TRUE );

        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( (ui_control*)m_pFieldOfView );
            m_CurrentControl = IDC_GRAPHICS_FIELD_OF_VIEW;
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
        g_UiMgr->SetScreenHighlight( m_pFieldOfViewText->GetPosition() );
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    xbool const ShadowFilterDisabled = !m_pDynamicShadows->IsChecked();
    m_pShadowFilter->SetFlag( ui_win::WF_DISABLED, ShadowFilterDisabled );
    m_pShadowFilterText->SetFlag( ui_win::WF_DISABLED, ShadowFilterDisabled );

    if( m_pFieldOfView->IsFocused() )
    {
        highLight = 0;
        m_pFieldOfViewText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pFieldOfViewText->GetPosition() );
    }
    else
    {
        m_pFieldOfViewText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pDynamicShadows->IsFocused() )
    {
        highLight = 1;
        m_pDynamicShadowsText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pDynamicShadowsText->GetPosition() );
    }
    else
    {
        m_pDynamicShadowsText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pShadowFilter->IsFocused() )
    {
        highLight = 2;
        m_pShadowFilterText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pShadowFilterText->GetPosition() );
    }
    else
    {
        m_pShadowFilterText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pFilmGrain->IsFocused() )
    {
        highLight = 3;
        m_pFilmGrainText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pFilmGrainText->GetPosition() );
    }
    else
    {
        m_pFilmGrainText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pBackgroundBlur->IsFocused() )
    {
        highLight = 4;
        m_pBackgroundBlurText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pBackgroundBlurText->GetPosition() );
    }
    else
    {
        m_pBackgroundBlurText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pAntiAliasing->IsFocused() )
    {
        highLight = 5;
        m_pAntiAliasingText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pAntiAliasingText->GetPosition() );
    }
    else
    {
        m_pAntiAliasingText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pButtonApply->IsFocused() )
    {
        highLight = 6;
        g_UiMgr->SetScreenHighlight( m_pButtonApply->GetPosition() );
    }

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
        {
            g_AudioMgr.Play( "Cusor_Norm" );
        }
        m_CurrHL = highLight;
    }
}

//==============================================================================
