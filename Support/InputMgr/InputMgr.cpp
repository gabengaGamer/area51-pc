//=========================================================================
//
//  InputMgr.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "InputMgr.hpp"
#include "Monkey.hpp"
#include "StateMgr\StateMgr.hpp"

#include "../../Apps/GameApp/Config.hpp"

//==============================================================================
//  STORAGE
//==============================================================================

input_mgr    g_InputMgr;
input_pad*   input_mgr::s_pHead = NULL;

#if CONFIG_IS_DEMO && !defined( X_EDITOR )
extern xtimer g_DemoIdleTimer;
#endif

//=========================================================================
// INPUT_PAD HELPER FUNCTIONS
//=========================================================================

static 
void AccumulateButtonValue( input_pad::logical& Log, f32 Value )
{
    if( Value != Log.IsValue )
    {
        if( Value > Log.MapsWasValue )
            Log.MapsWasValue = Value;
    }

    if( Value > Log.MapsIsValue )
        Log.MapsIsValue = Value;
}

//==============================================================================

static 
void AccumulateAnalogValue( input_pad::logical& Log, f32 Value )
{
    if( x_abs( Value ) > x_abs( Log.MapsIsValue ) )
        Log.MapsIsValue = Value;

    if( x_abs( Log.IsValue ) > x_abs( Log.MapsWasValue ) )
        Log.MapsWasValue = Log.IsValue;
}

//=========================================================================
// INPUT_PAD FUNCTIONS
//=========================================================================

input_pad::logical::logical( void )
{
    ActionName[0] = 0;
    Clear();
}

//==============================================================================

void input_pad::logical::Clear( void )
{
    IsValue      = 0.0f;
    WasValue     = 0.0f;
    MapsIsValue  = 0.0f;
    MapsWasValue = 0.0f;
    TimePressed  = 0.0f;
}

//==============================================================================

void input_pad::logical::Commit( void )
{
    IsValue      = MapsIsValue;
    WasValue     = MapsWasValue;
    MapsIsValue  = 0.0f;
    MapsWasValue = 0.0f;
}

//==============================================================================

input_pad::mapping::mapping( void )
{
    bButton     = FALSE;
    bIsTap      = FALSE;
    bIsHold     = FALSE;
    GadgetID    = INPUT_UNDEFINED;
    Scale       = 1.0f;
    LogicalID   = -1;
    ContextMask = 0;
}

//==============================================================================

input_pad::input_pad( void )
{
    for( s32 i = 0; i < MAX_INPUT_PLATFORMS; i++ )
    {
        m_Mappings[i].SetGrowAmount( 16 );
    }

    m_Logicals.SetGrowAmount( 64 );
    m_pNext          = NULL;
    m_ControllerID  = -1;
    m_ActiveContext = INGAME_CONTEXT;
}

//==============================================================================

void input_pad::OnInitialize( void )
{
}

//==============================================================================

input_pad::logical& input_pad::GetLogical( s32 I )
{
    ASSERT( I >= 0 );
    ASSERT( I < m_Logicals.GetCount() );

    return m_Logicals[I];
}

//==============================================================================

const input_pad::logical& input_pad::GetLogical( s32 I ) const
{
    ASSERT( I >= 0 );
    ASSERT( I < m_Logicals.GetCount() );

    return m_Logicals[I];
}

//==============================================================================

void input_pad::SetLogicalCount( s32 Count )
{
    ASSERT( Count > 0 );

    m_Logicals.SetCount( Count );
    ClearAllLogical();
}

//==============================================================================

void input_pad::SetLogicalName( s32 ID, const char* pName )
{
    ASSERT( ID >= 0 );
    ASSERT( ID <  m_Logicals.GetCount() );
    ASSERT( pName );

    x_strncpy( m_Logicals[ ID ].ActionName, pName, sizeof( m_Logicals[ ID ].ActionName ) );
    m_Logicals[ ID ].ActionName[ sizeof( m_Logicals[ ID ].ActionName ) - 1 ] = 0;
    m_Logicals[ ID ].Clear();
}

//==============================================================================

void input_pad::ClearLogicalForGadget( s32 iPlatform, input_gadget GadgetID )
{
    ASSERT( iPlatform >= 0 );
    ASSERT( iPlatform <  MAX_INPUT_PLATFORMS );

    for( s32 i = 0; i < m_Mappings[iPlatform].GetCount(); i++ )
    {
        if( m_Mappings[iPlatform][ i ].GadgetID == GadgetID )
        {
            const s32 LogicalID = m_Mappings[iPlatform][ i ].LogicalID;
            m_Logicals[ LogicalID ].Clear();
        }
    }
}

//==============================================================================

void input_pad::ClearAllLogical( void )
{
    for( s32 i=0; i<m_Logicals.GetCount(); i++ )
    {
        m_Logicals[ i ].Clear();
    }
}

//==============================================================================

void input_pad::SetContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );
    m_ActiveContext = ContextMask;
}

//==============================================================================

void input_pad::EnableContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );
    m_ActiveContext |= ContextMask;
}

//==============================================================================

void input_pad::DisableContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );
    m_ActiveContext &= ~ContextMask;

    if( m_ActiveContext == 0 )
        m_ActiveContext = INGAME_CONTEXT;
}

//==============================================================================

void input_pad::AddMapping( s32             iPlatform,
                            s32             ID,
                            input_gadget    GadgetID,
                            xbool           IsButton,
                            f32             Scale,
                            u32             ContextMask )
{
    ASSERT( iPlatform >= 0 );
    ASSERT( iPlatform <  MAX_INPUT_PLATFORMS );
    ASSERT( ID >= 0 );
    ASSERT( ID <  m_Logicals.GetCount() );
    ASSERT( ContextMask != 0 );

    mapping& Mapping = m_Mappings[iPlatform].Append();
    Mapping.bButton     = IsButton;
    Mapping.bIsTap      = FALSE;
    Mapping.bIsHold     = FALSE;
    Mapping.GadgetID    = GadgetID;
    Mapping.Scale       = Scale;
    Mapping.LogicalID   = ID;
    Mapping.ContextMask = ContextMask;
}

//==============================================================================

xbool input_pad::ShouldPollInput( void ) const
{
    if( m_ControllerID == -1 )
    {
        if( (m_ActiveContext & FRONTEND_CONTEXT) == 0 )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

s32 input_pad::GetPollControllerID( void ) const
{
    return (m_ControllerID == -1) ? 0 : m_ControllerID;
}

//==============================================================================

void input_pad::UpdateButtonMapping( const mapping& Mapping, s32 ControllerID, f32 DeltaTime )
{
    logical& Log = m_Logicals[ Mapping.LogicalID ];
    f32      Value = (f32)input_IsPressed( Mapping.GadgetID, ControllerID );

    if( input_WasPressed( Mapping.GadgetID, ControllerID ) )
        g_InputMgr.NotifyInputActivity( Mapping.GadgetID, 1.0f );

#if CONFIG_IS_DEMO && !defined( X_EDITOR )
    if( Value )
    {
        g_DemoIdleTimer.Trip();
    }
#endif

    if( g_MonkeyOptions.Enabled )
        Value = g_Monkey.GetValue( Mapping.LogicalID );

    if( Mapping.bIsTap || Mapping.bIsHold )
    {
        xbool ClearTime = !Value;

        if( Value )
        {
            Log.TimePressed += DeltaTime;
        }

        const f32 TapTime = 0.3f;

        if( Mapping.bIsTap )
        {
            if(   !Value
               && (Log.TimePressed > 0.0f)
               && (Log.TimePressed < TapTime) )
            {
                Value = 1.0f;
            }
            else if( Value )
            {
                Value = 0.0f;
            }
        }
        else
        {
            ASSERT( Mapping.bIsHold );

            if( Value && (Log.TimePressed < TapTime) )
            {
                Value = 0.0f;
            }
            else if( !Value )
            {
                ClearTime = TRUE;
            }
        }

        if( ClearTime )
        {
            Log.TimePressed = 0.0f;
        }
    }

    AccumulateButtonValue( Log, Value );
}

//==============================================================================

void input_pad::UpdateAnalogMapping( const mapping& Mapping, s32 ControllerID )
{
    logical& Log   = m_Logicals[ Mapping.LogicalID ];
    f32      Value = 0.0f;

    if( Mapping.GadgetID != INPUT_UNDEFINED )
        Value = input_GetValue( Mapping.GadgetID, ControllerID );

    g_InputMgr.NotifyInputActivity( Mapping.GadgetID, Value );

    if( g_MonkeyOptions.Enabled )
        Value = g_Monkey.GetValue( Mapping.LogicalID );

    Value *= Mapping.Scale;
    AccumulateAnalogValue( Log, Value );

#if CONFIG_IS_DEMO && !defined( X_EDITOR )
    if( x_abs( Value ) > 0.2f )
    {
        g_DemoIdleTimer.Trip();
    }
#endif
}

//==============================================================================

void input_pad::UpdateMapping( const mapping& Mapping, s32 ControllerID, f32 DeltaTime )
{
    if( (Mapping.ContextMask & m_ActiveContext) == 0 )
        return;

    if( Mapping.GadgetID == INPUT_UNDEFINED )
        return;

    if( Mapping.bButton )
        UpdateButtonMapping( Mapping, ControllerID, DeltaTime );
    else
        UpdateAnalogMapping( Mapping, ControllerID );
}

//==============================================================================

void input_pad::CommitLogicalValues( void )
{
    for( s32 i = 0; i < m_Logicals.GetCount(); i++ )
    {
        m_Logicals[i].Commit();
    }
}

//==============================================================================

void input_pad::OnUpdate( f32 DeltaTime )
{
    if( !ShouldPollInput() )
    {
        ClearAllLogical();
        return;
    }

    const s32 ControllerID = GetPollControllerID();

    for( s32 iPlatform = 0; iPlatform < MAX_INPUT_PLATFORMS; iPlatform++ )
    {
        for( s32 i = 0; i < m_Mappings[iPlatform].GetCount(); i++ )
        {
            UpdateMapping( m_Mappings[iPlatform][i], ControllerID, DeltaTime );
        }
    }

    CommitLogicalValues();
}

//==============================================================================

xbool input_pad::IsPausePressed( void ) const
{
    return FALSE;
}

//==============================================================================
// INPUT_MGR HELPER FUNCTIONS
//==============================================================================

static
xbool IsGadgetInRange( input_gadget GadgetID, input_gadget Begin, input_gadget End )
{
    return( (GadgetID > Begin) && (GadgetID < End) );
}

//==============================================================================

static
input_device GetInputDeviceForGadget( input_gadget GadgetID )
{
    if( IsGadgetInRange( GadgetID, INPUT_MOUSE__BEGIN, INPUT_MOUSE__END ) )
        return INPUT_DEVICE_MOUSE;

    if( IsGadgetInRange( GadgetID, INPUT_KBD__BEGIN, INPUT_KBD__END ) )
        return INPUT_DEVICE_KEYBOARD;

    if( IsGadgetInRange( GadgetID, INPUT_PS2__BEGIN,  INPUT_PS2__END  ) ||
        IsGadgetInRange( GadgetID, INPUT_XBOX__BEGIN, INPUT_XBOX__END ) ||
        IsGadgetInRange( GadgetID, INPUT_PC__BEGIN,   INPUT_PC__END   ) )
    {
        return INPUT_DEVICE_GAMEPAD;
    }

    return INPUT_DEVICE_NONE;
}

//==============================================================================

static
input_platform GetInputPlatformForGadget( input_gadget GadgetID )
{
    if( IsGadgetInRange( GadgetID, INPUT_PS2__BEGIN, INPUT_PS2__END ) )
        return INPUT_PLATFORM_PS2;

    if( IsGadgetInRange( GadgetID, INPUT_XBOX__BEGIN, INPUT_XBOX__END ) )
        return INPUT_PLATFORM_XBOX;

    return INPUT_PLATFORM_PC;
}

//==============================================================================

static
xbool WasAnyKeyboardPressed( void )
{
    for( s32 i = INPUT_KBD__BEGIN + 1; i < INPUT_KBD__END; i++ )
    {
        if( input_WasPressed( (input_gadget)i ) )
            return TRUE;
    }

    return FALSE;
}

//==============================================================================

static 
u32 GetStateMgrInputContext( void )
{
    if( g_StateMgr.IsPaused() || g_StateMgr.InSystemError() )
        return FRONTEND_CONTEXT;

    if( g_StateMgr.GetState() == SM_PLAYING_GAME )
        return INGAME_CONTEXT;

    return FRONTEND_CONTEXT;
}

//==============================================================================
// INPUT_MGR FUNCTIONS
//==============================================================================

input_mgr::input_mgr( void )
{
    m_ActiveDevice   = INPUT_DEVICE_KEYBOARD;
    m_ActivePlatform = INPUT_PLATFORM_PC;
    m_MouseDeltaX    = 0;
    m_MouseDeltaY    = 0;

    for( s32 i = 0; i < INPUT_MOUSE_BUTTON_COUNT; i++ )
    {
        m_MouseButtons[i] = FALSE;
    }
}

//==============================================================================

void input_mgr::RegisterPad( input_pad& Pad )
{
    Pad.m_pNext = s_pHead;
    s_pHead = &Pad;

    Pad.OnInitialize();
}

//==============================================================================

void input_mgr::SetContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );

    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->SetContext( ContextMask );
    }
}

//==============================================================================

void input_mgr::EnableContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );

    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->EnableContext( ContextMask );
    }
}

//==============================================================================

void input_mgr::DisableContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );

    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->DisableContext( ContextMask );
    }
}

//==============================================================================

xbool input_mgr::Update( f32 DeltaTime )
{
    while( input_UpdateState() )
    {
        if( input_IsPressed( INPUT_MSG_EXIT ) )
            return TRUE;
    }

    UpdateDeviceState();
    
    if( g_MonkeyOptions.Enabled )
    {
        g_Monkey.Update( DeltaTime );
        input_SuppressFeedback( TRUE );
    }

    SetContext( GetStateMgrInputContext() );

    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->OnUpdate( DeltaTime );
    }

    return FALSE;
}

//==============================================================================

void input_mgr::SetActiveDevice( input_device Device )
{
    if( Device != INPUT_DEVICE_NONE )
        m_ActiveDevice = Device;
}

//==============================================================================

void input_mgr::NotifyInputActivity( input_gadget GadgetID, f32 Value )
{
    if( x_abs( Value ) <= 0.25f )
        return;

    input_device Device = GetInputDeviceForGadget( GadgetID );
    SetActiveDevice( Device );

    if( Device != INPUT_DEVICE_NONE )
        m_ActivePlatform = GetInputPlatformForGadget( GadgetID );
}

//==============================================================================

void input_mgr::UpdateDeviceState( void )
{
    m_MouseDeltaX = (s32)input_GetValue( INPUT_MOUSE_X_REL );
    m_MouseDeltaY = (s32)input_GetValue( INPUT_MOUSE_Y_REL );

    m_MouseButtons[INPUT_MOUSE_BUTTON_LEFT]   = input_IsPressed( INPUT_MOUSE_BTN_L );
    m_MouseButtons[INPUT_MOUSE_BUTTON_MIDDLE] = input_IsPressed( INPUT_MOUSE_BTN_C );
    m_MouseButtons[INPUT_MOUSE_BUTTON_RIGHT]  = input_IsPressed( INPUT_MOUSE_BTN_R );

    if(   m_MouseDeltaX
       || m_MouseDeltaY
       || x_abs( input_GetValue( INPUT_MOUSE_WHEEL_REL ) ) > 0.25f
       || input_WasPressed( INPUT_MOUSE_BTN_L )
       || input_WasPressed( INPUT_MOUSE_BTN_C )
       || input_WasPressed( INPUT_MOUSE_BTN_R ) )
    {
        SetActiveDevice( INPUT_DEVICE_MOUSE );
        m_ActivePlatform = INPUT_PLATFORM_PC;
    }

    if( WasAnyKeyboardPressed() )
    {
        SetActiveDevice( INPUT_DEVICE_KEYBOARD );
        m_ActivePlatform = INPUT_PLATFORM_PC;
    }
}

//==============================================================================

xbool input_mgr::IsMouseButtonDown( input_mouse_button Button ) const
{
    ASSERT( Button >= 0 );
    ASSERT( Button < INPUT_MOUSE_BUTTON_COUNT );

    if( (Button < 0) || (Button >= INPUT_MOUSE_BUTTON_COUNT) )
        return FALSE;

    return m_MouseButtons[Button];
}

//==============================================================================

s32 input_mgr::WasPausePressed( void )
{
    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        s32 ControllerID = pPad->m_ControllerID;

        if( ControllerID == -1 )
            continue;

#if !defined(X_EDITOR) && !defined(CONFIG_RETAIL)            
        if( g_MonkeyOptions.Enabled )
        {
            if( !g_StateMgr.IsPaused() )
            {
                if( g_Monkey.ShouldPause() )
                    return ControllerID;
            }
            else
            {
                if( g_Monkey.ShouldUnpause() )
                    return ControllerID;
            }
        }
#endif

        if( pPad->IsPausePressed() )
            return ControllerID;
    }

    return -1;
}
