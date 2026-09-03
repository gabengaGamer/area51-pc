//==============================================================================
//  
//  e_Input.hpp
//
//==============================================================================

#ifndef E_INPUT_HPP
#define E_INPUT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define BEGIN_GADGETS                               enum input_gadget {
#define DEFINE_GADGET(__gadget__)                   __gadget__ ,
#define DEFINE_GADGET_VALUE(__gadget__, __value__)  __gadget__ = __value__ ,
#define DEFINE_GADGET_RANGE(__first__, __last__, __platform__, __device__, __value_kind__, __control_kind__, __flags__)
#define END_GADGETS                                 };

#include "Input/e_Input_Gadget_Defines.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum input_platform
{
    INPUT_PLATFORM_NONE = -1,
    INPUT_PLATFORM_PS2,
    INPUT_PLATFORM_XBOX,
    INPUT_PLATFORM_DESKTOP, // TODO: GS: Mobile ?
    INPUT_PLATFORM_COUNT
};

//------------------------------------------------------------------------------

enum input_device
{
    INPUT_DEVICE_NONE,
    INPUT_DEVICE_KEYBOARD,
    INPUT_DEVICE_MOUSE,
    INPUT_DEVICE_GAMEPAD,
    INPUT_DEVICE_MESSAGE,
    INPUT_DEVICE_COUNT
};

//------------------------------------------------------------------------------

enum input_value_kind
{
    INPUT_VALUE_NONE,
    INPUT_VALUE_DIGITAL,
    INPUT_VALUE_ABSOLUTE_AXIS,
    INPUT_VALUE_RELATIVE_AXIS,
    INPUT_VALUE_PULSE,
    INPUT_VALUE_QUERY
};

//------------------------------------------------------------------------------

enum input_control_kind
{
    INPUT_CONTROL_NONE,
    INPUT_CONTROL_KEY,
    INPUT_CONTROL_BUTTON,
    INPUT_CONTROL_AXIS,
    INPUT_CONTROL_STICK_AXIS,
    INPUT_CONTROL_TRIGGER,
    INPUT_CONTROL_WHEEL,
    INPUT_CONTROL_MESSAGE
};

//------------------------------------------------------------------------------

enum input_gadget_flags
{
    INPUT_GADGET_FLAG_NONE    = 0,
    INPUT_GADGET_FLAG_MESSAGE = (1 << 0),
    INPUT_GADGET_FLAG_QUERY   = (1 << 1)
};

//------------------------------------------------------------------------------

struct input_gadget_info
{
    input_platform         Platform;
    input_device           Device;
    input_value_kind       ValueKind;
    input_control_kind     ControlKind;
    u32                    Flags;
};

//------------------------------------------------------------------------------

struct feedback_envelope
{
    f32     Intensity;
    f32     Duration;
    s32     Mode;
};

//------------------------------------------------------------------------------

struct input_init_desc
{
    void*   pWindow;
};

//------------------------------------------------------------------------------

enum
{
    INPUT_DEVICE_ID_ANY = -1,
    INPUT_MAX_DEVICES   = 8
};

//------------------------------------------------------------------------------

enum input_event_type
{
    INPUT_EVENT_PRESSED,
    INPUT_EVENT_RELEASED,
    INPUT_EVENT_ABSOLUTE,
    INPUT_EVENT_RELATIVE,
    INPUT_EVENT_EXIT
};

//------------------------------------------------------------------------------

struct input_event
{
    input_gadget       GadgetID;
    s32                DeviceID;
    input_event_type   Type;
    f32                Value;
    u32                TimeStamp;
    u32                Sequence;
};

//------------------------------------------------------------------------------

struct input_debug_stats
{
    u32                 CapturedMouseEvents      = 0;
    s32                 CapturedMouseDeltaX      = 0;
    s32                 CapturedMouseDeltaY      = 0;
    f32                 CaptureMilliseconds     = 0.0f;
};

//------------------------------------------------------------------------------

enum
{
    INPUT_EVENT_CAPACITY = 4096
};

//------------------------------------------------------------------------------

class input_event_buffer
{
public:
                        input_event_buffer      ( void );

    void                BeginCapture            ( u32 StartTimeStamp );
    xbool               Append                  ( input_gadget GadgetID,
                                                  s32 DeviceID,
                                                  input_event_type Type,
                                                  f32 Value,
                                                  u32 TimeStamp );
    void                MarkOverflow            ( void );
    void                EndCapture              ( u32 EndTimeStamp );
    void                Sort                    ( void );

    s32                 GetCount                ( void ) const;
    input_event&        GetEvent                ( s32 Index );
    input_event const&  GetEvent                ( s32 Index ) const;
    u32                 GetStartTimeStamp       ( void ) const;
    u32                 GetEndTimeStamp         ( void ) const;
    xbool               HasOverflowed           ( void ) const;

private:

    input_event         m_Events[INPUT_EVENT_CAPACITY];
    s32                 m_Count;
    u32                 m_StartTimeStamp;
    u32                 m_EndTimeStamp;
    u32                 m_NextSequence;
    xbool               m_HasOverflowed;
};

//------------------------------------------------------------------------------

struct input_gadget_sample
{
    f32     Value;
    xbool   IsDown;
    xbool   WasPressed;
    xbool   WasReleased;
    xbool   IsPresent;
};

//------------------------------------------------------------------------------

class input_snapshot
{
public:

    xbool                       IsPressed       ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    xbool                       WasPressed      ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    xbool                       WasReleased     ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    f32                         GetValue        ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    xbool                       IsPresent       ( input_gadget GadgetID, s32 DeviceID = INPUT_DEVICE_ID_ANY ) const;
    input_gadget_sample const&  GetSample      ( input_gadget GadgetID, s32 DeviceID = 0 ) const;

private:
    friend class input_system;
    friend class input_snapshot_builder;

    void                        Clear           ( void );

private:

    input_gadget_sample         m_Gadgets[INPUT_GADGET_COUNT][INPUT_MAX_DEVICES];
};

//------------------------------------------------------------------------------

// Explicit construction API for deterministic replays and headless tests.
// Runtime capture continues to write snapshots only through input_system.
class input_snapshot_builder
{
public:

                                input_snapshot_builder  ( void );

    void                        Clear                   ( void );
    void                        SetSample               ( input_gadget GadgetID,
                                                          s32 DeviceID,
                                                          const input_gadget_sample& Sample );
    input_snapshot const&       GetSnapshot             ( void ) const;

private:

    input_snapshot              m_Snapshot;
};

//==============================================================================
//  INPUT SYSTEM CLASSES
//==============================================================================

class input_action_map;
class input_backend;
class input_system
{
public:
                    input_system                ( void );

    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------

    xbool           Init                        ( const input_init_desc& Desc );
    void            Init                        ( void );
    void            Kill                        ( void );

    //--------------------------------------------------------------------------
    // Frame Input
    //--------------------------------------------------------------------------

    xbool               CaptureFrameInput       ( void );

    input_snapshot const& GetFrameSnapshot       ( void ) const;
    input_debug_stats const& GetDebugStats       ( void ) const;

    static input_gadget LookupGadget            ( const char* pName );
#ifdef TARGET_DESKTOP
    s32                 GetPadCount             ( void ) const;
#endif

    xbool               IsDevicePresent         ( input_device Device,
                                                  s32          DeviceID ) const;
    xbool               FindAvailableDevice     ( input_device& Device,
                                                  s32&          DeviceID ) const;

    //--------------------------------------------------------------------------
    // Gadget Metadata
    //--------------------------------------------------------------------------

    static const input_gadget_info& GetGadgetInfo        ( input_gadget GadgetID );
    static input_platform           GetGadgetPlatform    ( input_gadget GadgetID );
    static input_device             GetGadgetDevice      ( input_gadget GadgetID );
    static input_value_kind         GetGadgetValueKind   ( input_gadget GadgetID );
    static input_control_kind       GetGadgetControlKind ( input_gadget GadgetID );
    static xbool                    IsGadgetValid        ( input_gadget GadgetID );
    static xbool                    IsPromptDeviceActivity( input_gadget GadgetID,
                                                            f32 Value,
                                                            xbool WasPressed );

    //--------------------------------------------------------------------------
    // Frame Input State
    //--------------------------------------------------------------------------

    xbool           WasFrameDeviceButtonPressed ( input_device Device ) const;
    input_device    GetCurrentInputDevice       ( void ) const;
    input_platform  GetCurrentInputPlatform     ( void ) const;
    s32             GetFrameMouseDeltaX         ( s32 DeviceID = 0 ) const;
    s32             GetFrameMouseDeltaY         ( s32 DeviceID = 0 ) const;

    //--------------------------------------------------------------------------
    // Hardware Pump And Feedback
    //--------------------------------------------------------------------------

    void            Feedback                    ( f32 Duration, f32 Intensity, s32 DeviceID = 0 );
    void            Feedback                    ( s32 Count, feedback_envelope* pEnvelope, s32 DeviceID = 0 );
    void            EnableFeedback              ( xbool state, s32 DeviceID = 0 );
    void            SuppressFeedback            ( xbool Suppress );
    void            ClearFeedback               ( void );

private:
    //--------------------------------------------------------------------------
    // Platform Backend
    //--------------------------------------------------------------------------

    xbool           IsRawGadgetDown             ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    f32             GetRawGadgetValue           ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    xbool           IsRawGadgetPresent          ( input_gadget GadgetID, s32 DeviceID ) const;

    //--------------------------------------------------------------------------
    // Frame Sampling Internals
    //--------------------------------------------------------------------------

    xbool                    IsInputDeviceValid       ( s32 DeviceID ) const;
    void                     ClearFrameActivity       ( void );
    void                     RecordGadgetActivity     ( input_gadget_info const& Info, f32 Value, s32 Priority );
    void                     ResolveFrameActivity     ( void );
    void                     BuildFrameSnapshot       ( void );
    void                     InitializeSnapshotState ( input_snapshot& Snapshot ) const;
    void                     ApplyEvent               ( input_snapshot& Snapshot, input_event const& Event );

private:
    //--------------------------------------------------------------------------
    // Runtime State
    //--------------------------------------------------------------------------

    input_device        m_CurrentDevice;
    input_platform      m_CurrentPlatform;
    input_init_desc     m_InitDesc;
    input_backend*      m_pBackend;
    input_event_buffer  m_CapturedEvents;
    input_snapshot      m_FrameSnapshot;
    input_debug_stats   m_DebugStats;
    input_device        m_FrameActivityDevice;
    input_platform      m_FrameActivityPlatform;
    s32                 m_FrameActivityPriority;
    xbool               m_FrameActivityConflict;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern input_system g_Input;

//==============================================================================
#endif // E_INPUT_HPP
//==============================================================================
