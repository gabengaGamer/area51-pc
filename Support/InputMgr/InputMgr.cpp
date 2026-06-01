//=========================================================================
//
//  InputMgr.cpp
//
//=========================================================================

//=========================================================================
//
// Fixed timestep input handling was based on
// https://jakubtomsu.github.io/posts/input_in_fixed_timestep/
//
// Thanks to Jakub Tomsu for the great article.
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "InputMgr.hpp"
#include "GamePad.hpp"
#include "Monkey.hpp"

#include "../../Apps/GameApp/Config.hpp"

//=========================================================================
//  STORAGE
//=========================================================================

input_mgr    g_InputMgr;
input_pad*   input_mgr::s_pHead = NULL;

static const f32 s_LogicalPressThreshold = 0.25f;

#if CONFIG_IS_DEMO && !defined( X_EDITOR )
extern xtimer g_DemoIdleTimer;
#endif

//=========================================================================
// LOCAL HELPER FUNCTIONS
//=========================================================================

static
xbool WasKeyboardPressedThisFrame( void )
{
    for( s32 i = INPUT_KBD__BEGIN + 1; i < INPUT_KBD__END; i++ )
    {
        if( input_WasPressed( (input_gadget)i ) )
            return TRUE;
    }

    return FALSE;
}

//=========================================================================
// INPUT_PAD LOGICAL FUNCTIONS
//=========================================================================

input_pad::logical::logical( void )
{
    ActionName[0] = 0;
    Clear();
}

//==============================================================================

void input_pad::logical::Clear( void )
{
    IsValue        = 0.0f;
    WasValue       = 0.0f;
    CurrentValue   = 0.0f;
    FrameValue     = 0.0f;
    FrameWasValue  = 0.0f;
    FixedValue     = 0.0f;
    FixedStepValue = 0.0f;
    FixedWasValue  = 0.0f;
    FixedStepCount = 0;
    TimePressed    = 0.0f;
}

//==============================================================================

void input_pad::logical::ClearFrame( void )
{
    CurrentValue  = 0.0f;
    FrameValue    = 0.0f;
    FrameWasValue = 0.0f;
}

//==============================================================================

void input_pad::logical::ClearFixed( void )
{
    FixedValue     = 0.0f;
    FixedStepValue = 0.0f;
    FixedWasValue  = 0.0f;
    FixedStepCount = 0;
}

//==============================================================================

void input_pad::logical::PrepareFixed( s32 StepCount )
{
    ASSERT( StepCount >= 0 );

    FixedStepCount = StepCount;
}

//==============================================================================

void input_pad::logical::CommitLocal( void )
{
    IsValue  = (FrameValue != 0.0f) ? FrameValue : CurrentValue;
    WasValue = FrameWasValue;
}

//==============================================================================

void input_pad::logical::CommitFixed( void )
{
    f32 StepValue = 0.0f;

    if( FixedStepValue != 0.0f )
    {
        if( FixedStepCount > 0 )
        {
            StepValue = FixedStepValue / (f32)FixedStepCount;
            FixedStepValue -= StepValue;
            FixedStepCount--;
        }
        else
        {
            StepValue      = FixedStepValue;
            FixedStepValue = 0.0f;
        }
    }

    IsValue  = CurrentValue;
    WasValue = FixedWasValue;

    if( FixedValue != 0.0f )
        IsValue = FixedValue;

    if( StepValue != 0.0f )
        IsValue = StepValue;

    FixedValue    = 0.0f;
    FixedWasValue = 0.0f;
}

//==============================================================================
// INPUT_PAD MAPPING FUNCTIONS
//==============================================================================

input_pad::mapping::mapping( void )
{
    bIsTap     = FALSE;
    bIsHold    = FALSE;
    GadgetID    = INPUT_UNDEFINED;
    Scale       = 1.0f;
    LastAnalogValue = 0.0f;
    LogicalID   = -1;
    ContextMask = 0;
}

//==============================================================================
// INPUT_PAD SETUP FUNCTIONS
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

void input_pad::AddMapping( s32             iPlatform,
                            s32             ID,
                            input_gadget    GadgetID,
                            f32             Scale,
                            u32             ContextMask )
{
    ASSERT( iPlatform >= 0 );
    ASSERT( iPlatform <  MAX_INPUT_PLATFORMS );
    ASSERT( ID >= 0 );
    ASSERT( ID <  m_Logicals.GetCount() );
    ASSERT( ContextMask != 0 );

    mapping& Mapping = m_Mappings[iPlatform].Append();
    Mapping.GadgetID    = GadgetID;
    Mapping.Scale       = Scale;
    Mapping.LogicalID   = ID;
    Mapping.ContextMask = ContextMask;
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

void input_pad::ClearFrameValues( void )
{
    for( s32 i=0; i<m_Logicals.GetCount(); i++ )
    {
        m_Logicals[ i ].ClearFrame();
    }
}

//==============================================================================

void input_pad::ClearCurrentValues( void )
{
    for( s32 i=0; i<m_Logicals.GetCount(); i++ )
    {
        m_Logicals[ i ].CurrentValue = 0.0f;
    }
}

//==============================================================================

void input_pad::ClearFixedValues( void )
{
    for( s32 i=0; i<m_Logicals.GetCount(); i++ )
    {
        m_Logicals[ i ].ClearFixed();
    }
}

//==============================================================================

void input_pad::PrepareFixedValues( s32 StepCount )
{
    ASSERT( StepCount >= 0 );

    for( s32 i=0; i<m_Logicals.GetCount(); i++ )
    {
        m_Logicals[ i ].PrepareFixed( StepCount );
    }
}

//==============================================================================

void input_pad::SetActiveContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );
    m_ActiveContext = ContextMask;
}

//==============================================================================

xbool input_pad::ShouldPollInput( void ) const
{
    if( m_ControllerID == -1 )
    {
        u32 UnassignedContexts = FRONTEND_CONTEXT;
#if defined( ENABLE_DEBUG_MENU )
        UnassignedContexts |= DEBUG_MENU_CONTEXT;
#endif

        if( (m_ActiveContext & UnassignedContexts) == 0 )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================

s32 input_pad::GetPollControllerID( void ) const
{
    return (m_ControllerID == -1) ? 0 : m_ControllerID;
}

//==============================================================================

void input_pad::SampleButtonMapping( const mapping& Mapping, s32 ControllerID, f32 DeltaTime )
{
    logical& Log = m_Logicals[ Mapping.LogicalID ];
    const xbool RawWasPressed = input_WasPressed( Mapping.GadgetID, ControllerID );
    f32         Value         = input_IsPressed( Mapping.GadgetID, ControllerID ) ? 1.0f : 0.0f;

    if( RawWasPressed )
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
        xbool ClearTime = (Value <= s_LogicalPressThreshold);

        if( Value > s_LogicalPressThreshold )
        {
            Log.TimePressed += DeltaTime;
        }

        const f32 TapTime = 0.3f;

        if( Mapping.bIsTap )
        {
            if(   (Value <= s_LogicalPressThreshold)
               && (Log.TimePressed > 0.0f)
               && (Log.TimePressed < TapTime) )
            {
                Value = 1.0f;
            }
            else if( Value > s_LogicalPressThreshold )
            {
                Value = 0.0f;
            }
        }

        if( Mapping.bIsHold )
        {
            if( (Value > s_LogicalPressThreshold) && (Log.TimePressed < TapTime) )
            {
                Value = 0.0f;
            }
            else if( Value <= s_LogicalPressThreshold )
            {
                ClearTime = TRUE;
            }
        }

        if( ClearTime )
        {
            Log.TimePressed = 0.0f;
        }
    }

    const xbool WasDown = (Log.IsValue > s_LogicalPressThreshold);
    const xbool IsDown  = (Value       > s_LogicalPressThreshold);
    const xbool IsProcessedButton = (Mapping.bIsTap || Mapping.bIsHold);

    if( Value > Log.CurrentValue )
        Log.CurrentValue = Value;

    if( IsDown )
    {
        if( Value > Log.FrameValue )
            Log.FrameValue = Value;

        if( Value > Log.FixedValue )
            Log.FixedValue = Value;
    }

    if( IsDown && !WasDown && (IsProcessedButton || RawWasPressed) )
    {
        Log.FrameWasValue = x_max( Log.FrameWasValue, Value );
        Log.FixedWasValue = x_max( Log.FixedWasValue, Value );
    }
}

//==============================================================================

void input_pad::SampleAnalogMapping( mapping& Mapping, s32 ControllerID )
{
    logical& Log   = m_Logicals[ Mapping.LogicalID ];
    f32      Value = 0.0f;
    const ingame_pad::logical_id LogicalID = (ingame_pad::logical_id)Mapping.LogicalID;

    if( Mapping.GadgetID != INPUT_UNDEFINED )
    {
        Value = input_GetValue( Mapping.GadgetID, ControllerID );
        g_InputMgr.NotifyInputActivity( Mapping.GadgetID, Value );
    }

    if( g_MonkeyOptions.Enabled )
        Value = g_Monkey.GetValue( Mapping.LogicalID );

    Value *= Mapping.Scale;

#if CONFIG_IS_DEMO && !defined( X_EDITOR )
    if( x_abs( Value ) > 0.2f )
    {
        g_DemoIdleTimer.Trip();
    }
#endif

    if( IsMouseDeltaGadget( Mapping.GadgetID ) )
    {
        Log.FrameValue     += Value;
        Log.FixedStepValue += Value;
        return;
    }

    if( IsMouseWheelGadget( Mapping.GadgetID ) )
    {
        ASSERT( IsCycleLogical( LogicalID ) );

        if( Value <= 0.0f )
            return;

        Log.FrameValue    += Value;
        Log.FrameWasValue += Value;
        Log.FixedValue    += Value;
        Log.FixedWasValue += Value;
        return;
    }

    if( IsPositiveAxisLogical( LogicalID ) )
    {
        const xbool WasDown = (Mapping.LastAnalogValue > s_LogicalPressThreshold);
        const xbool IsDown  = (Value                   > s_LogicalPressThreshold);

        Mapping.LastAnalogValue = Value;

        if( Value > Log.CurrentValue )
            Log.CurrentValue = Value;

        if( !IsDown || WasDown )
            return;

        Log.FrameWasValue = x_max( Log.FrameWasValue, Value );
        Log.FixedWasValue = x_max( Log.FixedWasValue, Value );
        return;
    }

    ASSERT( IsLookLogical( LogicalID ) || IsStickAxisGadget( Mapping.GadgetID ) );

    if( x_abs( Value ) > x_abs( Log.CurrentValue ) )
        Log.CurrentValue = Value;
}

//==============================================================================

void input_pad::SampleMapping( mapping& Mapping, s32 ControllerID, f32 DeltaTime )
{
    if( Mapping.GadgetID == INPUT_UNDEFINED )
        return;

    const ingame_pad::logical_id LogicalID = (ingame_pad::logical_id)Mapping.LogicalID;
    const xbool IsAnalogMapping = IsMouseDeltaGadget( Mapping.GadgetID ) ||
                                  IsMouseWheelGadget( Mapping.GadgetID ) ||
                                  IsStickAxisGadget( Mapping.GadgetID );

    if( (Mapping.ContextMask & m_ActiveContext) == 0 )
    {
        if( IsAnalogMapping && IsPositiveAxisLogical( LogicalID ) )
        {
            Mapping.LastAnalogValue = input_GetValue( Mapping.GadgetID, ControllerID ) * Mapping.Scale;
        }

        return;
    }

    if( IsAnalogMapping )
    {
        ASSERT( IsLookLogical( LogicalID ) ||
                IsPositiveAxisLogical( LogicalID ) ||
                IsCycleLogical( LogicalID ) );

        SampleAnalogMapping( Mapping, ControllerID );
        return;
    }

    ASSERT( IsButtonGadget( Mapping.GadgetID ) );
    ASSERT( IsButtonLogical( LogicalID ) ||
            IsPositiveAxisLogical( LogicalID ) );

    SampleButtonMapping( Mapping, ControllerID, DeltaTime );
}

//==============================================================================

void input_pad::OnBeginFrame( f32 DeltaTime )
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
            SampleMapping( m_Mappings[iPlatform][i], ControllerID, DeltaTime );
        }
    }
}

//==============================================================================

void input_pad::OnUpdateLocal( f32 DeltaTime )
{
    (void)DeltaTime;

    for( s32 i = 0; i < m_Logicals.GetCount(); i++ )
    {
        m_Logicals[i].CommitLocal();
    }
}

//==============================================================================

void input_pad::OnUpdateFixed( f32 DeltaTime )
{
    (void)DeltaTime;

    for( s32 i = 0; i < m_Logicals.GetCount(); i++ )
    {
        m_Logicals[i].CommitFixed();
    }
}

//==============================================================================

xbool input_pad::IsPausePressed( void ) const
{
    return FALSE;
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
    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        if( pPad == &Pad )
            return;
    }

    Pad.m_pNext = s_pHead;
    s_pHead = &Pad;

    Pad.OnInitialize();
}

//==============================================================================

void input_mgr::ApplyPadContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );

    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->SetActiveContext( ContextMask );
    }
}

//==============================================================================

xbool input_mgr::BeginFrame( f32 DeltaTime, u32 ContextMask )
{
    xbool ExitRequested = FALSE;

    ASSERT( ContextMask != 0 );

    m_MouseDeltaX = 0;
    m_MouseDeltaY = 0;

    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->ClearFrameValues();
    }

    if( g_MonkeyOptions.Enabled )
    {
        g_Monkey.Update( DeltaTime );
        input_SuppressFeedback( TRUE );
    }

    ApplyPadContext( ContextMask );

    while( input_UpdateState() )
    {
        if( input_IsPressed( INPUT_MSG_EXIT ) )
            ExitRequested = TRUE;

        for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
        {
            pPad->ClearCurrentValues();
        }

        SampleMouseKeyboardState();

        for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
        {
            pPad->OnBeginFrame( DeltaTime );
        }
    }

    if( input_IsPressed( INPUT_MSG_EXIT ) )
        ExitRequested = TRUE;

    return ExitRequested;
}

//==============================================================================

void input_mgr::UpdateLocal( f32 DeltaTime )
{
    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->OnUpdateLocal( DeltaTime );
    }
}

//==============================================================================

void input_mgr::PrepareFixedInput( s32 StepCount )
{
    ASSERT( StepCount >= 0 );

    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->PrepareFixedValues( StepCount );
    }
}

//==============================================================================

void input_mgr::UpdateFixed( f32 DeltaTime )
{
    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->OnUpdateFixed( DeltaTime );
    }
}

//==============================================================================

void input_mgr::ClearFixedInput( void )
{
    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        pPad->ClearFixedValues();
    }
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

    input_device Device = GetInputGadgetDevice( GadgetID );
    SetActiveDevice( Device );

    if( Device != INPUT_DEVICE_NONE )
        m_ActivePlatform = GetInputGadgetPlatform( GadgetID );
}

//==============================================================================

void input_mgr::SampleMouseKeyboardState( void )
{
    s32 MouseDeltaX = (s32)input_GetValue( INPUT_MOUSE_X_REL );
    s32 MouseDeltaY = (s32)input_GetValue( INPUT_MOUSE_Y_REL );

    m_MouseDeltaX += MouseDeltaX;
    m_MouseDeltaY += MouseDeltaY;

    m_MouseButtons[INPUT_MOUSE_BUTTON_LEFT]   = input_IsPressed( INPUT_MOUSE_BTN_L );
    m_MouseButtons[INPUT_MOUSE_BUTTON_MIDDLE] = input_IsPressed( INPUT_MOUSE_BTN_C );
    m_MouseButtons[INPUT_MOUSE_BUTTON_RIGHT]  = input_IsPressed( INPUT_MOUSE_BTN_R );

    if(   MouseDeltaX
       || MouseDeltaY
       || x_abs( input_GetValue( INPUT_MOUSE_WHEEL_REL ) ) > 0.25f
       || input_WasPressed( INPUT_MOUSE_BTN_L )
       || input_WasPressed( INPUT_MOUSE_BTN_C )
       || input_WasPressed( INPUT_MOUSE_BTN_R ) )
    {
        SetActiveDevice( INPUT_DEVICE_MOUSE );
        m_ActivePlatform = INPUT_PLATFORM_PC;
    }

    if( WasKeyboardPressedThisFrame() )
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

s32 input_mgr::WasPausePressed( xbool IsPaused )
{
    for( input_pad* pPad = s_pHead; pPad != NULL; pPad = pPad->m_pNext )
    {
        s32 ControllerID = pPad->m_ControllerID;

        if( ControllerID == -1 )
            continue;
#if !defined(X_EDITOR) && !defined(CONFIG_RETAIL)            
        if( g_MonkeyOptions.Enabled )
        {
            if( !IsPaused )
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
