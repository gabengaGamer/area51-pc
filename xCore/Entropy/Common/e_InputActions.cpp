//==============================================================================
//
//  e_InputActions.cpp
//
//==============================================================================

// GS: NOTE: Poor monkey Poopy Joe ( Monkey testing ) was deleted, 
//           Because it was tied to input logic from Support. 
//           In the future, it should be implemented at the Entropy level. So... TODO:

//==============================================================================
//
// Fixed timestep input handling was based on
// https://jakubtomsu.github.io/posts/input_in_fixed_timestep/
//
// Thanks to Jakub Tomsu for the great article.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "..\e_InputActions.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

static const f32 s_ActionActuationThreshold = 0.25f;
static const f32 s_ActionTapTimeThreshold   = 0.3f;

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
xbool IsActionActuated( f32 Value )
{
    return( x_abs( Value ) > s_ActionActuationThreshold );
}

//==============================================================================

static
xbool IsActionMapListValid( input_action_map** ppMaps, s32 MapCount )
{
    return( (MapCount >= 0) && (ppMaps || (MapCount == 0)) );
}

//==============================================================================
//  INPUT ACTION SOURCE
//==============================================================================

input_action_source::input_action_source( void )
{
    Clear();
}

//==============================================================================

void input_action_source::Clear( void )
{
    GadgetID         = INPUT_UNDEFINED;
    DeviceID         = -1;
    Info.Platform    = INPUT_PLATFORM_NONE;
    Info.Device      = INPUT_DEVICE_NONE;
    Info.ValueKind   = INPUT_VALUE_NONE;
    Info.ControlKind = INPUT_CONTROL_NONE;
    Info.Flags       = INPUT_GADGET_FLAG_NONE;
}

//==============================================================================

void input_action_source::Set( input_gadget InGadgetID, s32 InDeviceID )
{
    GadgetID = InGadgetID;
    DeviceID = InDeviceID;
    Info     = input_system::GetGadgetInfo( InGadgetID );
}

//==============================================================================
//  INPUT ACTION STATE
//==============================================================================

input_action_state::input_action_state( void )
{
    m_ActionName[0] = 0;
    Clear();
}

//==============================================================================

void input_action_state::Clear( void )
{
    m_IsValue        = 0.0f;
    m_WasValue       = 0.0f;
    m_CurrentValue   = 0.0f;
    m_FrameValue     = 0.0f;
    m_FrameWasValue  = 0.0f;
    m_FixedValue     = 0.0f;
    m_FixedStepValue = 0.0f;
    m_FixedWasValue  = 0.0f;
    m_FixedStepCount = 0;
    m_TimePressed    = 0.0f;
    m_IsSource.Clear();
    m_WasSource.Clear();
    m_CurrentSource.Clear();
    m_FrameSource.Clear();
    m_FrameWasSource.Clear();
    m_FixedSource.Clear();
    m_FixedStepSource.Clear();
    m_FixedWasSource.Clear();
}

//==============================================================================

void input_action_state::ClearFrameSamples( void )
{
    m_CurrentValue  = 0.0f;
    m_FrameValue    = 0.0f;
    m_FrameWasValue = 0.0f;
    m_CurrentSource.Clear();
    m_FrameSource.Clear();
    m_FrameWasSource.Clear();
}

//==============================================================================

void input_action_state::ClearFixedSamples( void )
{
    m_FixedValue     = 0.0f;
    m_FixedStepValue = 0.0f;
    m_FixedWasValue  = 0.0f;
    m_FixedStepCount = 0;
    m_FixedSource.Clear();
    m_FixedStepSource.Clear();
    m_FixedWasSource.Clear();
}

//==============================================================================

void input_action_state::PrepareFixedSamples( s32 StepCount )
{
    ASSERT( StepCount >= 0 );

    if( StepCount < 0 )
        return;

    m_FixedStepCount = StepCount;
}

//==============================================================================

void input_action_state::CommitFrame( void )
{
    if( m_FrameValue != 0.0f )
    {
        m_IsValue  = m_FrameValue;
        m_IsSource = m_FrameSource;
    }
    else
    {
        m_IsValue  = m_CurrentValue;
        m_IsSource = m_CurrentSource;
    }

    m_WasValue  = m_FrameWasValue;
    m_WasSource = m_FrameWasSource;
}

//==============================================================================

void input_action_state::CommitFixed( void )
{
    f32 StepValue = 0.0f;

    if( m_FixedStepValue != 0.0f )
    {
        if( m_FixedStepCount > 0 )
        {
            StepValue = m_FixedStepValue / (f32)m_FixedStepCount;
            m_FixedStepValue -= StepValue;
            m_FixedStepCount--;
        }
        else
        {
            StepValue        = m_FixedStepValue;
            m_FixedStepValue = 0.0f;
        }
    }

    m_IsValue   = m_CurrentValue;
    m_IsSource  = m_CurrentSource;
    m_WasValue  = m_FixedWasValue;
    m_WasSource = m_FixedWasSource;

    if( m_FixedValue != 0.0f )
    {
        m_IsValue = m_FixedValue;
        m_IsSource = m_FixedSource;
    }

    if( StepValue != 0.0f )
    {
        m_IsValue = StepValue;
        m_IsSource = m_FixedStepSource;
    }

    m_FixedValue    = 0.0f;
    m_FixedWasValue = 0.0f;
    m_FixedSource.Clear();
    m_FixedWasSource.Clear();

    if( m_FixedStepValue == 0.0f )
        m_FixedStepSource.Clear();
}

//==============================================================================
//  INPUT ACTION BINDING
//==============================================================================

input_action_map::binding::binding( void )
{
    IsTap           = FALSE;
    IsHold          = FALSE;
    GadgetID        = INPUT_UNDEFINED;
    Scale           = 1.0f;
    LastAnalogValue = 0.0f;
    ActionID        = -1;
    ContextMask     = 0;
    ValueMode       = INPUT_ACTION_VALUE_AUTO;
}

//==============================================================================
//  INPUT ACTION MAP SETUP
//==============================================================================

input_action_map::input_action_map( void )
{
    for( s32 i = 0; i < INPUT_PLATFORM_COUNT; i++ )
    {
        m_Bindings[i].SetGrowAmount( 16 );
    }

    m_Actions.SetGrowAmount( 64 );
    m_ActiveContext = 0;
    m_DeviceID      = -1;
}

//==============================================================================

input_action_state& input_action_map::GetActionState( s32 ActionID )
{
    ASSERT( ActionID >= 0 );
    ASSERT( ActionID < m_Actions.GetCount() );

    return m_Actions[ActionID];
}

//==============================================================================

const input_action_state& input_action_map::GetActionState( s32 ActionID ) const
{
    ASSERT( ActionID >= 0 );
    ASSERT( ActionID < m_Actions.GetCount() );

    return m_Actions[ActionID];
}

//==============================================================================

void input_action_map::SetActionCount( s32 Count )
{
    ASSERT( Count >= 0 );

    if( Count < 0 )
        return;

    m_Actions.SetCount( Count );
    ClearAllActions();
}

//==============================================================================

void input_action_map::SetActionName( s32 ActionID, const char* pName )
{
    ASSERT( ActionID >= 0 );
    ASSERT( ActionID <  m_Actions.GetCount() );
    ASSERT( pName );

    if( (ActionID < 0) || (ActionID >= m_Actions.GetCount()) || !pName )
        return;

    x_strncpy( m_Actions[ ActionID ].m_ActionName, pName, sizeof( m_Actions[ ActionID ].m_ActionName ) );
    m_Actions[ ActionID ].m_ActionName[ sizeof( m_Actions[ ActionID ].m_ActionName ) - 1 ] = 0;
}

//==============================================================================

void input_action_map::AddBinding( s32          ActionID,
                                   input_gadget GadgetID,
                                   f32          Scale,
                                   u32          ContextMask,
                                   input_action_value_mode ValueMode )
{
    ASSERT( ActionID >= 0 );
    ASSERT( ActionID <  m_Actions.GetCount() );
    ASSERT( ContextMask != 0 );

    if( (ActionID < 0) || (ActionID >= m_Actions.GetCount()) || (ContextMask == 0) )
        return;

    if( GadgetID == INPUT_UNDEFINED )
        return;

    const input_platform Platform = input_system::GetGadgetPlatform( GadgetID );

    ASSERT( Platform >= 0 );
    ASSERT( Platform <  INPUT_PLATFORM_COUNT );

    if( (Platform < 0) || (Platform >= INPUT_PLATFORM_COUNT) )
        return;

    binding& Binding = m_Bindings[Platform].Append();
    Binding.GadgetID    = GadgetID;
    Binding.Scale       = Scale;
    Binding.ActionID    = ActionID;
    Binding.ContextMask = ContextMask;
    Binding.ValueMode   = ValueMode;
}

//==============================================================================
//  DEVICE CONTEXT
//==============================================================================

void input_action_map::SetActiveContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );

    if( ContextMask == 0 )
        return;

    m_ActiveContext = ContextMask;
}

//==============================================================================

void input_action_map::SetDeviceID( s32 DeviceID )
{
    ASSERT( DeviceID >= -1 );

    if( DeviceID < -1 )
        return;

    m_DeviceID = DeviceID;
}

//==============================================================================

s32 input_action_map::GetResolvedDeviceID( s32 FallbackDeviceID ) const
{
    ASSERT( FallbackDeviceID >= 0 );

    if( FallbackDeviceID < 0 )
        FallbackDeviceID = 0;

    return HasDeviceID() ? m_DeviceID : FallbackDeviceID;
}

//==============================================================================
//  ACTION SAMPLE STATE
//==============================================================================

void input_action_map::ClearAllActions( void )
{
    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].Clear();
    }
}

//==============================================================================

void input_action_map::ClearFrameSamples( void )
{
    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].ClearFrameSamples();
    }
}

//==============================================================================

void input_action_map::ClearCurrentSamples( void )
{
    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].m_CurrentValue = 0.0f;
        m_Actions[i].m_CurrentSource.Clear();
    }
}

//==============================================================================

void input_action_map::ClearFixedSamples( void )
{
    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].ClearFixedSamples();
    }
}

//==============================================================================

void input_action_map::PrepareFixedSamples( s32 StepCount )
{
    ASSERT( StepCount >= 0 );

    if( StepCount < 0 )
        return;

    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].PrepareFixedSamples( StepCount );
    }
}

//==============================================================================
//  ACTION MAP SAMPLNG
//==============================================================================

void input_action_map::SampleFrame( f32 DeltaTime )
{
    SampleDevice( DeltaTime, GetResolvedDeviceID( 0 ) );
}

//==============================================================================

void input_action_map::SampleDevice( f32 DeltaTime, s32 DeviceID )
{
    ASSERT( DeviceID >= 0 );

    if( DeviceID < 0 )
        DeviceID = 0;

    for( s32 iPlatform = 0; iPlatform < INPUT_PLATFORM_COUNT; iPlatform++ )
    {
        for( s32 i = 0; i < m_Bindings[iPlatform].GetCount(); i++ )
        {
            SampleBinding( m_Bindings[iPlatform][i], DeviceID, DeltaTime );
        }
    }
}

//==============================================================================

void input_action_map::CommitFrame( void )
{
    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].CommitFrame();
    }
}

//==============================================================================

void input_action_map::CommitFixed( void )
{
    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].CommitFixed();
    }
}

//==============================================================================
//  BINDING SAMPLING
//==============================================================================

void input_action_map::SampleBinding( input_action_map::binding& Binding, s32 DeviceID, f32 DeltaTime )
{
    if( Binding.GadgetID == INPUT_UNDEFINED )
        return;

    if( (Binding.ActionID < 0) || (Binding.ActionID >= m_Actions.GetCount()) )
        return;

    const input_gadget_info& Info = input_system::GetGadgetInfo( Binding.GadgetID );

    if( (Info.ValueKind == INPUT_VALUE_NONE) || (Info.ValueKind == INPUT_VALUE_QUERY) )
        return;

    if( (Binding.ContextMask & m_ActiveContext) == 0 )
    {
        if( (Info.ValueKind == INPUT_VALUE_ABSOLUTE_AXIS) &&
            (Binding.ValueMode == INPUT_ACTION_VALUE_POSITIVE_AXIS) )
        {
            Binding.LastAnalogValue = g_Input.GetValue( Binding.GadgetID, DeviceID ) * Binding.Scale;
        }

        return;
    }

    if( Info.ValueKind == INPUT_VALUE_DIGITAL )
    {
        SampleDigitalBinding( Binding, DeviceID, DeltaTime );
        return;
    }

    SampleAnalogBinding( Binding, DeviceID );
}

//==============================================================================

void input_action_map::SampleDigitalBinding( const input_action_map::binding& Binding,
                                             s32                         DeviceID,
                                             f32                         DeltaTime )
{
    input_action_state& Action  = m_Actions[ Binding.ActionID ];
    const xbool WasInputPressed = g_Input.WasPressed( Binding.GadgetID, DeviceID );
    f32         Value           = g_Input.IsPressed( Binding.GadgetID, DeviceID ) ? Binding.Scale : 0.0f;

    if( WasInputPressed )
        OnInputActivity( Binding.GadgetID, 1.0f );

    const xbool bValueOverridden = OverrideActionValue( Binding.ActionID, Value );
    const xbool bInputActuated = IsActionActuated( Value );

    if( Binding.IsTap || Binding.IsHold )
    {
        xbool ClearTime = !bInputActuated;

        if( bInputActuated )
        {
            Action.m_TimePressed += DeltaTime;
        }

        if( Binding.IsTap )
        {
            if(   !bInputActuated
               && (Action.m_TimePressed > 0.0f)
               && (Action.m_TimePressed < s_ActionTapTimeThreshold) )
            {
                Value = 1.0f;
            }
            else if( bInputActuated )
            {
                Value = 0.0f;
            }
        }

        if( Binding.IsHold )
        {
            if( bInputActuated && (Action.m_TimePressed < s_ActionTapTimeThreshold) )
            {
                Value = 0.0f;
            }
            else if( !bInputActuated )
            {
                ClearTime = TRUE;
            }
        }

        if( ClearTime )
        {
            Action.m_TimePressed = 0.0f;
        }
    }

    OnActionValue( Binding.ActionID, Value );

    const xbool WasActionDownAtCommit = IsActionActuated( Action.m_IsValue );
    const xbool IsSampledActionDown   = IsActionActuated( Value );
    const xbool IsTriggerBinding      = (Binding.IsTap || Binding.IsHold);

    if( x_abs( Value ) > x_abs( Action.m_CurrentValue ) )
    {
        Action.m_CurrentValue = Value;
        Action.m_CurrentSource.Set( Binding.GadgetID, DeviceID );
    }

    if( IsSampledActionDown )
    {
        if( x_abs( Value ) > x_abs( Action.m_FrameValue ) )
        {
            Action.m_FrameValue = Value;
            Action.m_FrameSource.Set( Binding.GadgetID, DeviceID );
        }

        if( x_abs( Value ) > x_abs( Action.m_FixedValue ) )
        {
            Action.m_FixedValue = Value;
            Action.m_FixedSource.Set( Binding.GadgetID, DeviceID );
        }
    }

    if( IsSampledActionDown && !WasActionDownAtCommit && (IsTriggerBinding || WasInputPressed || bValueOverridden) )
    {
        const f32 AbsValue = x_abs( Value );

        if( AbsValue > Action.m_FrameWasValue )
        {
            Action.m_FrameWasValue = AbsValue;
            Action.m_FrameWasSource.Set( Binding.GadgetID, DeviceID );
        }

        if( AbsValue > Action.m_FixedWasValue )
        {
            Action.m_FixedWasValue = AbsValue;
            Action.m_FixedWasSource.Set( Binding.GadgetID, DeviceID );
        }
    }
}

//==============================================================================

void input_action_map::SampleAnalogBinding( input_action_map::binding& Binding, s32 DeviceID )
{
    input_action_state& Action = m_Actions[ Binding.ActionID ];
    const input_gadget_info& Info = input_system::GetGadgetInfo( Binding.GadgetID );
    f32 Value = 0.0f;

    if( Binding.GadgetID != INPUT_UNDEFINED )
    {
        Value = g_Input.GetValue( Binding.GadgetID, DeviceID );
        OnInputActivity( Binding.GadgetID, Value );
    }

    (void)OverrideActionValue( Binding.ActionID, Value );

    Value *= Binding.Scale;
    OnActionValue( Binding.ActionID, Value );

    if( Info.ValueKind == INPUT_VALUE_RELATIVE_AXIS )
    {
        Action.m_FrameValue     += Value;
        Action.m_FixedStepValue += Value;
        Action.m_FrameSource.Set( Binding.GadgetID, DeviceID );
        Action.m_FixedStepSource.Set( Binding.GadgetID, DeviceID );
        return;
    }

    if( Info.ValueKind == INPUT_VALUE_PULSE )
    {
        if( Value <= 0.0f )
            return;

        Action.m_FrameValue    += Value;
        Action.m_FrameWasValue += Value;
        Action.m_FixedValue    += Value;
        Action.m_FixedWasValue += Value;
        Action.m_FrameSource.Set( Binding.GadgetID, DeviceID );
        Action.m_FrameWasSource.Set( Binding.GadgetID, DeviceID );
        Action.m_FixedSource.Set( Binding.GadgetID, DeviceID );
        Action.m_FixedWasSource.Set( Binding.GadgetID, DeviceID );
        return;
    }

    ASSERT( Info.ValueKind == INPUT_VALUE_ABSOLUTE_AXIS );

    if( Binding.ValueMode == INPUT_ACTION_VALUE_POSITIVE_AXIS )
    {
        const xbool WasAxisActuated = (Binding.LastAnalogValue > s_ActionActuationThreshold);
        const xbool IsAxisActuated  = (Value                   > s_ActionActuationThreshold);

        Binding.LastAnalogValue = Value;

        if( Value > Action.m_CurrentValue )
        {
            Action.m_CurrentValue = Value;
            Action.m_CurrentSource.Set( Binding.GadgetID, DeviceID );
        }

        if( IsAxisActuated && !WasAxisActuated )
        {
            if( Value > Action.m_FrameWasValue )
            {
                Action.m_FrameWasValue = Value;
                Action.m_FrameWasSource.Set( Binding.GadgetID, DeviceID );
            }

            if( Value > Action.m_FixedWasValue )
            {
                Action.m_FixedWasValue = Value;
                Action.m_FixedWasSource.Set( Binding.GadgetID, DeviceID );
            }
        }

        return;
    }

    // Signed absolute axes are held state, not frame deltas. Commit uses
    // CurrentValue as the fallback; Frame/Fixed values are for accumulated input.
    if( x_abs( Value ) > x_abs( Action.m_CurrentValue ) )
    {
        Action.m_CurrentValue = Value;
        Action.m_CurrentSource.Set( Binding.GadgetID, DeviceID );
    }
}

//==============================================================================
//  EXTENSION HOOKS
//==============================================================================

void input_action_map::OnInputActivity( input_gadget GadgetID, f32 Value )
{
    g_Input.RecordGadgetActivity( GadgetID, Value );
}

//==============================================================================

xbool input_action_map::OverrideActionValue( s32 ActionID, f32& Value )
{
    (void)ActionID;
    (void)Value;
    return FALSE;
}

//==============================================================================

void input_action_map::OnActionValue( s32 ActionID, f32 Value )
{
    (void)ActionID;
    (void)Value;
}

//==============================================================================
//  INPUT SYSTEM ACTION MAP COORDINATION
//==============================================================================

xbool input_system::SampleActionMaps( input_action_map** ppMaps, s32 MapCount, f32 DeltaTime )
{
    xbool ExitRequested = FALSE;

    ASSERT( MapCount >= 0 );
    ASSERT( ppMaps || (MapCount == 0) );

    if( !IsActionMapListValid( ppMaps, MapCount ) )
        return FALSE;

    ClearFrameInput();

    for( s32 i = 0; i < MapCount; i++ )
    {
        if( ppMaps[i] )
            ppMaps[i]->ClearFrameSamples();
    }

    while( PollHardwareState() )
    {
        SampleFrameInput();
    }

    if( IsPressed( INPUT_MSG_EXIT ) )
        ExitRequested = TRUE;

    for( s32 i = 0; i < MapCount; i++ )
    {
        if( ppMaps[i] )
            ppMaps[i]->ClearCurrentSamples();
    }

    for( s32 i = 0; i < MapCount; i++ )
    {
        if( ppMaps[i] )
            ppMaps[i]->SampleFrame( DeltaTime );
    }

    // SampleFrameInput resolves raw device activity. Action maps can record
    // their own activity while sampling, so resolve again after the maps run.
    ResolveFrameActivity();

    return ExitRequested;
}

//==============================================================================

void input_system::SetActionMapsContext( input_action_map** ppMaps, s32 MapCount, u32 ContextMask )
{
    ASSERT( ContextMask != 0 );
    ASSERT( MapCount >= 0 );
    ASSERT( ppMaps || (MapCount == 0) );

    if( !ContextMask || !IsActionMapListValid( ppMaps, MapCount ) )
        return;

    for( s32 i = 0; i < MapCount; i++ )
    {
        if( ppMaps[i] )
            ppMaps[i]->SetActiveContext( ContextMask );
    }
}

//==============================================================================

xbool input_system::SampleActionMaps( input_action_map** ppMaps, s32 MapCount, f32 DeltaTime, u32 ContextMask )
{
    SetActionMapsContext( ppMaps, MapCount, ContextMask );
    return SampleActionMaps( ppMaps, MapCount, DeltaTime );
}

//==============================================================================

void input_system::CommitActionMapsFrame( input_action_map** ppMaps, s32 MapCount )
{
    ASSERT( MapCount >= 0 );
    ASSERT( ppMaps || (MapCount == 0) );

    if( !IsActionMapListValid( ppMaps, MapCount ) )
        return;

    for( s32 i = 0; i < MapCount; i++ )
    {
        if( ppMaps[i] )
            ppMaps[i]->CommitFrame();
    }
}

//==============================================================================

void input_system::PrepareActionMapsFixed( input_action_map** ppMaps, s32 MapCount, s32 StepCount )
{
    ASSERT( StepCount >= 0 );
    ASSERT( MapCount >= 0 );
    ASSERT( ppMaps || (MapCount == 0) );

    if( !IsActionMapListValid( ppMaps, MapCount ) || (StepCount < 0) )
        return;

    for( s32 i = 0; i < MapCount; i++ )
    {
        if( ppMaps[i] )
            ppMaps[i]->PrepareFixedSamples( StepCount );
    }
}

//==============================================================================

void input_system::CommitActionMapsFixed( input_action_map** ppMaps, s32 MapCount )
{
    ASSERT( MapCount >= 0 );
    ASSERT( ppMaps || (MapCount == 0) );

    if( !IsActionMapListValid( ppMaps, MapCount ) )
        return;

    for( s32 i = 0; i < MapCount; i++ )
    {
        if( ppMaps[i] )
            ppMaps[i]->CommitFixed();
    }
}

//==============================================================================

void input_system::ClearActionMapsFixed( input_action_map** ppMaps, s32 MapCount )
{
    ASSERT( MapCount >= 0 );
    ASSERT( ppMaps || (MapCount == 0) );

    if( !IsActionMapListValid( ppMaps, MapCount ) )
        return;

    for( s32 i = 0; i < MapCount; i++ )
    {
        if( ppMaps[i] )
            ppMaps[i]->ClearFixedSamples();
    }
}
