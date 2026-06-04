//==============================================================================
//  
//  e_Input.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "..\e_Input.hpp"

//==============================================================================
//  LOCAL TABLE TYPES
//==============================================================================

struct input_gadget_lookup
{
    input_gadget    Gadget;
    const char*     pName;
};

//------------------------------------------------------------------------------

struct input_gadget_range
{
    input_gadget        First;
    input_gadget        Last;
    input_gadget_info   Info;
};

//==============================================================================
//  GADGET NAME TABLE
//==============================================================================

#define BEGIN_GADGETS                               static const input_gadget_lookup s_GadgetLookupTable[] = {
#define DEFINE_GADGET(__gadget__)                   { __gadget__, #__gadget__ },
#define DEFINE_GADGET_VALUE(__gadget__, __value__)  { __gadget__, #__gadget__ },
#define DEFINE_GADGET_RANGE(__first__, __last__, __platform__, __device__, __value_kind__, __control_kind__, __flags__)
#define END_GADGETS                                 };

#include "e_input_gadget_defines.hpp"

//==============================================================================
//  GADGET METADATA TABLE
//==============================================================================

#define BEGIN_GADGETS
#define DEFINE_GADGET(__gadget__)
#define DEFINE_GADGET_VALUE(__gadget__, __value__)
#define DEFINE_GADGET_RANGE(__first__, __last__, __platform__, __device__, __value_kind__, __control_kind__, __flags__) \
    { __first__, __last__, { __platform__, __device__, __value_kind__, __control_kind__, __flags__ } },
#define END_GADGETS

static
const input_gadget_range s_GadgetInfoRanges[] =
{
#include "e_input_gadget_defines.hpp"
};

//==============================================================================
//  CONSTANTS
//==============================================================================

static
const input_gadget_info s_InvalidGadgetInfo =
{
    INPUT_PLATFORM_NONE,
    INPUT_DEVICE_NONE,
    INPUT_VALUE_NONE,
    INPUT_CONTROL_NONE,
    INPUT_GADGET_FLAG_NONE
};

static const f32 s_InputActivityThreshold = 0.25f;

//------------------------------------------------------------------------------

enum input_activity_priority
{
    INPUT_ACTIVITY_NONE      = 0,
    INPUT_ACTIVITY_AXIS      = 1,
    INPUT_ACTIVITY_MOMENTARY = 2
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

input_system g_Input;

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
xbool IsGadgetInRange( input_gadget GadgetID, input_gadget First, input_gadget Last )
{
    return( (GadgetID >= First) && (GadgetID <= Last) );
}

//==============================================================================

static
s32 GetSampleActivityPriority( const input_gadget_info& Info, f32 Value, xbool WasPressed )
{
    switch( Info.ValueKind )
    {
    case INPUT_VALUE_DIGITAL:
        return WasPressed ? INPUT_ACTIVITY_MOMENTARY : INPUT_ACTIVITY_NONE;

    case INPUT_VALUE_RELATIVE_AXIS:
    case INPUT_VALUE_PULSE:
        return( Value != 0.0f ) ? INPUT_ACTIVITY_MOMENTARY : INPUT_ACTIVITY_NONE;

    case INPUT_VALUE_ABSOLUTE_AXIS:
        return( x_abs( Value ) > s_InputActivityThreshold ) ? INPUT_ACTIVITY_AXIS : INPUT_ACTIVITY_NONE;

    default:
        return INPUT_ACTIVITY_NONE;
    }
}

//==============================================================================

static
s32 GetExplicitActivityPriority( const input_gadget_info& Info )
{
    switch( Info.ValueKind )
    {
    case INPUT_VALUE_DIGITAL:
    case INPUT_VALUE_RELATIVE_AXIS:
    case INPUT_VALUE_PULSE:
        return INPUT_ACTIVITY_MOMENTARY;

    case INPUT_VALUE_ABSOLUTE_AXIS:
        return INPUT_ACTIVITY_AXIS;

    default:
        return INPUT_ACTIVITY_NONE;
    }
}

//==============================================================================
//  INPUT SYSTEM LIFETIME
//==============================================================================

input_system::input_system( void )
{
    m_CurrentDevice   = INPUT_DEVICE_KEYBOARD;
    m_CurrentPlatform = INPUT_PLATFORM_PC;
    ClearFrameActivity();
    ClearFrameInput();
}

//==============================================================================
//  FRAME SAMPLING INTERNALS
//==============================================================================

xbool input_system::IsFrameGadgetValid( input_gadget GadgetID ) const
{
    return( (GadgetID >= 0) && (GadgetID < INPUT_GADGET_COUNT) );
}

//==============================================================================

xbool input_system::IsInputDeviceValid( s32 DeviceID ) const
{
    return( (DeviceID >= 0) && (DeviceID < INPUT_MAX_DEVICES) );
}

//==============================================================================
//  INPUT ACTIVITY INTERNALS
//==============================================================================

void input_system::ClearFrameActivity( void )
{
    m_FrameActivityDevice   = INPUT_DEVICE_NONE;
    m_FrameActivityPlatform = INPUT_PLATFORM_NONE;
    m_FrameActivityPriority = INPUT_ACTIVITY_NONE;
    m_FrameActivityConflict = FALSE;
}

//==============================================================================

void input_system::RecordGadgetActivity( const input_gadget_info& Info, f32 Value )
{
    RecordGadgetActivity( Info, Value, GetExplicitActivityPriority( Info ) );
}

//==============================================================================

void input_system::RecordGadgetActivity( const input_gadget_info& Info, f32 Value, s32 Priority )
{
    if( x_abs( Value ) <= s_InputActivityThreshold )
        return;

    if( (Info.ValueKind == INPUT_VALUE_NONE) || (Info.ValueKind == INPUT_VALUE_QUERY) )
        return;

    if( Info.Device == INPUT_DEVICE_NONE )
        return;

    if( Priority <= INPUT_ACTIVITY_NONE )
        return;

    if( Priority > m_FrameActivityPriority )
    {
        m_FrameActivityDevice   = Info.Device;
        m_FrameActivityPlatform = Info.Platform;
        m_FrameActivityPriority = Priority;
        m_FrameActivityConflict = FALSE;
        return;
    }

    if( Priority == m_FrameActivityPriority )
    {
        if(   (m_FrameActivityDevice   != Info.Device)
           || (m_FrameActivityPlatform != Info.Platform) )
        {
            m_FrameActivityConflict = TRUE;
        }
    }
}

//==============================================================================

void input_system::ResolveFrameActivity( void )
{
    if( m_FrameActivityPriority == INPUT_ACTIVITY_NONE )
        return;

    if( !m_FrameActivityConflict )
    {
        m_CurrentDevice   = m_FrameActivityDevice;
        m_CurrentPlatform = m_FrameActivityPlatform;
    }

    ClearFrameActivity();
}

//==============================================================================
//  FRAME VALUE ACCUMULATION
//==============================================================================

void input_system::AccumulateFrameValue( frame_gadget_state& State, const input_gadget_info& Info, f32 Value ) const
{
    if( (Info.ValueKind == INPUT_VALUE_RELATIVE_AXIS) || (Info.ValueKind == INPUT_VALUE_PULSE) )
    {
        State.Value += Value;
        return;
    }

    if( x_abs( Value ) > x_abs( State.Value ) )
        State.Value = Value;
}

//==============================================================================

void input_system::SampleFrameGadget( input_gadget GadgetID, s32 DeviceID )
{
    if( !IsFrameGadgetValid( GadgetID ) || !IsInputDeviceValid( DeviceID ) )
        return;

    const input_gadget_info& Info = GetGadgetInfo( GadgetID );
    frame_gadget_state& State = m_FrameGadgets[ GadgetID ][ DeviceID ];

    if( Info.ValueKind == INPUT_VALUE_NONE )
        return;

    State.IsPresent |= IsRawGadgetPresent( GadgetID, DeviceID );

    if( Info.ValueKind == INPUT_VALUE_QUERY )
        return;

    const f32   Value       = GetRawGadgetValue( GadgetID, DeviceID );
    const xbool IsDown     = IsRawGadgetDown( GadgetID, DeviceID );
    const xbool WasPressed = WasRawGadgetPressed( GadgetID, DeviceID );

    AccumulateFrameValue( State, Info, Value );

    State.IsDown     |= IsDown;
    State.WasPressed |= WasPressed;

    const s32 ActivityPriority = GetSampleActivityPriority( Info, Value, WasPressed );

    if( ActivityPriority != INPUT_ACTIVITY_NONE )
        RecordGadgetActivity( Info, WasPressed ? 1.0f : Value, ActivityPriority );
}

//==============================================================================

const input_system::frame_gadget_state& input_system::GetFrameGadgetState( input_gadget GadgetID, s32 DeviceID ) const
{
    static const frame_gadget_state s_EmptyState = { 0.0f, FALSE, FALSE, FALSE };

    if( !IsFrameGadgetValid( GadgetID ) || !IsInputDeviceValid( DeviceID ) )
        return s_EmptyState;

    return m_FrameGadgets[ GadgetID ][ DeviceID ];
}

//==============================================================================
//  GADGET METADATA
//==============================================================================

const input_gadget_info& input_system::GetGadgetInfo( input_gadget GadgetID )
{
    if( GadgetID == INPUT_UNDEFINED )
        return s_InvalidGadgetInfo;

    for( s32 i = 0; i < (s32)(sizeof(s_GadgetInfoRanges) / sizeof(s_GadgetInfoRanges[0])); i++ )
    {
        if( IsGadgetInRange( GadgetID, s_GadgetInfoRanges[i].First, s_GadgetInfoRanges[i].Last ) )
            return s_GadgetInfoRanges[i].Info;
    }

    return s_InvalidGadgetInfo;
}

//==============================================================================

input_platform input_system::GetGadgetPlatform( input_gadget GadgetID )
{
    return GetGadgetInfo( GadgetID ).Platform;
}

//==============================================================================

input_device input_system::GetGadgetDevice( input_gadget GadgetID )
{
    return GetGadgetInfo( GadgetID ).Device;
}

//==============================================================================

input_value_kind input_system::GetGadgetValueKind( input_gadget GadgetID )
{
    return GetGadgetInfo( GadgetID ).ValueKind;
}

//==============================================================================

input_control_kind input_system::GetGadgetControlKind( input_gadget GadgetID )
{
    return GetGadgetInfo( GadgetID ).ControlKind;
}

//==============================================================================

xbool input_system::IsGadgetValid( input_gadget GadgetID )
{
    return( GetGadgetInfo( GadgetID ).ValueKind != INPUT_VALUE_NONE );
}

//==============================================================================
//  SAMPLED INPUT QUERIES
//==============================================================================

xbool input_system::IsPressed( input_gadget GadgetID, s32 DeviceID ) const
{
    return GetFrameGadgetState( GadgetID, DeviceID ).IsDown;
}

//==============================================================================

xbool input_system::WasPressed( input_gadget GadgetID, s32 DeviceID ) const
{
    return GetFrameGadgetState( GadgetID, DeviceID ).WasPressed;
}

//==============================================================================

f32 input_system::GetValue( input_gadget GadgetID, s32 DeviceID ) const
{
    return GetFrameGadgetState( GadgetID, DeviceID ).Value;
}

//==============================================================================

xbool input_system::IsPresent( input_gadget GadgetID, s32 DeviceID ) const
{
    if( DeviceID == INPUT_DEVICE_ID_ANY )
    {
        for( s32 iDevice = 0; iDevice < INPUT_MAX_DEVICES; iDevice++ )
        {
            if( GetFrameGadgetState( GadgetID, iDevice ).IsPresent )
                return TRUE;
        }

        return FALSE;
    }

    return GetFrameGadgetState( GadgetID, DeviceID ).IsPresent;
}

//==============================================================================

xbool input_system::WasDeviceButtonPressed( input_device Device ) const
{
    if( Device == INPUT_DEVICE_NONE )
        return FALSE;

    for( s32 iRange = 0; iRange < (s32)(sizeof(s_GadgetInfoRanges) / sizeof(s_GadgetInfoRanges[0])); iRange++ )
    {
        const input_gadget_range& Range = s_GadgetInfoRanges[iRange];
        const input_gadget_info&  Info  = Range.Info;

        if( (Info.Device != Device) || (Info.ValueKind != INPUT_VALUE_DIGITAL) )
            continue;

        s32 First = (s32)Range.First;
        s32 Last  = (s32)Range.Last;

        if( First < 0 )
            First = 0;

        if( Last >= INPUT_GADGET_COUNT )
            Last = INPUT_GADGET_COUNT - 1;

        for( s32 GadgetID = First; GadgetID <= Last; GadgetID++ )
        {
            for( s32 DeviceID = 0; DeviceID < INPUT_MAX_DEVICES; DeviceID++ )
            {
                if( m_FrameGadgets[ GadgetID ][ DeviceID ].WasPressed )
                    return TRUE;
            }
        }
    }

    return FALSE;
}

//==============================================================================
//  FRAME INPUT STATE
//==============================================================================

void input_system::ClearFrameInput( void )
{
    ClearFrameActivity();

    for( s32 GadgetID = 0; GadgetID < INPUT_GADGET_COUNT; GadgetID++ )
    {
        for( s32 DeviceID = 0; DeviceID < INPUT_MAX_DEVICES; DeviceID++ )
        {
            m_FrameGadgets[GadgetID][DeviceID].Value       = 0.0f;
            m_FrameGadgets[GadgetID][DeviceID].IsDown     = FALSE;
            m_FrameGadgets[GadgetID][DeviceID].WasPressed = FALSE;
            m_FrameGadgets[GadgetID][DeviceID].IsPresent  = FALSE;
        }
    }
}

//==============================================================================

void input_system::SampleFrameInput( void )
{
    for( s32 DeviceID = 0; DeviceID < INPUT_MAX_DEVICES; DeviceID++ )
    {
        for( s32 i = 0; i < (s32)(sizeof(s_GadgetLookupTable) / sizeof(s_GadgetLookupTable[0])); i++ )
        {
            SampleFrameGadget( s_GadgetLookupTable[i].Gadget, DeviceID );
        }
    }

    ResolveFrameActivity();
}

//==============================================================================
//  INPUT ACTIVITY STATE
//==============================================================================

void input_system::RecordGadgetActivity( input_gadget GadgetID, f32 Value )
{
    const input_gadget_info& Info = GetGadgetInfo( GadgetID );
    RecordGadgetActivity( Info, Value );
}

//==============================================================================

input_device input_system::GetCurrentInputDevice( void ) const
{
    return m_CurrentDevice;
}

//==============================================================================

input_platform input_system::GetCurrentInputPlatform( void ) const
{
    return m_CurrentPlatform;
}

//==============================================================================
//  MOUSE DELTA QUERIES
//==============================================================================

s32 input_system::GetMouseDeltaX( s32 DeviceID ) const
{
    return (s32)GetFrameGadgetState( INPUT_MOUSE_X_REL, DeviceID ).Value;
}

//==============================================================================

s32 input_system::GetMouseDeltaY( s32 DeviceID ) const
{
    return (s32)GetFrameGadgetState( INPUT_MOUSE_Y_REL, DeviceID ).Value;
}

//==============================================================================
//  GADGET NAME LOOKUP
//==============================================================================

input_gadget input_system::LookupGadget( const char* pName )
{
    ASSERT( pName );

    if( !pName )
        return( INPUT_UNDEFINED );

    for( s32 i = 0; i < (s32)(sizeof(s_GadgetLookupTable) / sizeof(s_GadgetLookupTable[0])); i++ )
    {
        if( x_strcmp( s_GadgetLookupTable[i].pName, pName ) == 0 )
            return( s_GadgetLookupTable[i].Gadget );
    }

    return( INPUT_UNDEFINED );
}
