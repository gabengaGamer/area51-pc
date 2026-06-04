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

#include "e_input_gadget_defines.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum input_platform
{
    INPUT_PLATFORM_NONE = -1,
    INPUT_PLATFORM_PS2,
    INPUT_PLATFORM_XBOX,
    INPUT_PLATFORM_PC,
    INPUT_PLATFORM_COUNT
};

//------------------------------------------------------------------------------

enum input_device
{
    INPUT_DEVICE_NONE,
    INPUT_DEVICE_KEYBOARD,
    INPUT_DEVICE_MOUSE,
    INPUT_DEVICE_GAMEPAD,
    INPUT_DEVICE_MESSAGE
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

//==============================================================================
//  INPUT SYSTEM CLASSES
//==============================================================================

enum
{
    INPUT_DEVICE_ID_ANY = -1,
    INPUT_MAX_DEVICES   = 8
};

//------------------------------------------------------------------------------

class input_action_map;
class input_system
{
public:
                    input_system                ( void );

    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------

    void            Init                        ( void );
    void            Kill                        ( void );

    //--------------------------------------------------------------------------
    // Sampled Input Queries
    //--------------------------------------------------------------------------

    xbool               IsPressed               ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    xbool               WasPressed              ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    f32                 GetValue                ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    xbool               IsPresent               ( input_gadget GadgetID, s32 DeviceID = INPUT_DEVICE_ID_ANY ) const;
    static input_gadget LookupGadget            ( const char* pName );
#ifdef TARGET_PC
    s32                 GetPadCount             ( void ) const;
#endif

    //--------------------------------------------------------------------------
    // Gadget Metadata
    //--------------------------------------------------------------------------

    static const input_gadget_info& GetGadgetInfo        ( input_gadget GadgetID );
    static input_platform           GetGadgetPlatform    ( input_gadget GadgetID );
    static input_device             GetGadgetDevice      ( input_gadget GadgetID );
    static input_value_kind         GetGadgetValueKind   ( input_gadget GadgetID );
    static input_control_kind       GetGadgetControlKind ( input_gadget GadgetID );
    static xbool                    IsGadgetValid        ( input_gadget GadgetID );

    //--------------------------------------------------------------------------
    // Frame Input State
    //--------------------------------------------------------------------------

    void            ClearFrameInput             ( void );
    void            SampleFrameInput            ( void );
    void            RecordGadgetActivity        ( input_gadget GadgetID, f32 Value );
    xbool           WasDeviceButtonPressed      ( input_device Device ) const;
    input_device    GetCurrentInputDevice       ( void ) const;
    input_platform  GetCurrentInputPlatform     ( void ) const;
    s32             GetMouseDeltaX              ( s32 DeviceID = 0 ) const;
    s32             GetMouseDeltaY              ( s32 DeviceID = 0 ) const;

    //--------------------------------------------------------------------------
    // Action Map Coordination
    //--------------------------------------------------------------------------

    xbool           SampleActionMaps            ( input_action_map** ppMaps, s32 MapCount, f32 DeltaTime );
    xbool           SampleActionMaps            ( input_action_map** ppMaps, s32 MapCount, f32 DeltaTime, u32 ContextMask );
    void            SetActionMapsContext        ( input_action_map** ppMaps, s32 MapCount, u32 ContextMask );
    void            CommitActionMapsFrame       ( input_action_map** ppMaps, s32 MapCount );
    void            PrepareActionMapsFixed      ( input_action_map** ppMaps, s32 MapCount, s32 StepCount );
    void            CommitActionMapsFixed       ( input_action_map** ppMaps, s32 MapCount );
    void            ClearActionMapsFixed        ( input_action_map** ppMaps, s32 MapCount );

    template< class T, int MapCount >
    xbool           SampleActionMaps            ( T (&Maps)[MapCount], f32 DeltaTime );

    template< class T, int MapCount >
    xbool           SampleActionMaps            ( T (&Maps)[MapCount], f32 DeltaTime, u32 ContextMask );

    template< class T, int MapCount >
    void            CommitActionMapsFrame       ( T (&Maps)[MapCount] );

    template< class T, int MapCount >
    void            PrepareActionMapsFixed      ( T (&Maps)[MapCount], s32 StepCount );

    template< class T, int MapCount >
    void            CommitActionMapsFixed       ( T (&Maps)[MapCount] );

    template< class T, int MapCount >
    void            ClearActionMapsFixed        ( T (&Maps)[MapCount] );

    //--------------------------------------------------------------------------
    // Hardware Pump And Feedback
    //--------------------------------------------------------------------------

    xbool           PollHardwareState           ( void );
    void            Feedback                    ( f32 Duration, f32 Intensity, s32 DeviceID = 0 );
    void            Feedback                    ( s32 Count, feedback_envelope* pEnvelope, s32 DeviceID = 0 );
    void            EnableFeedback              ( xbool state, s32 DeviceID = 0 );
    void            SuppressFeedback            ( xbool Suppress );
    void            ClearFeedback               ( void );

private:
    //--------------------------------------------------------------------------
    // Frame Gadget State
    //--------------------------------------------------------------------------

    struct frame_gadget_state
    {
        f32     Value;
        xbool   IsDown;
        xbool   WasPressed;
        xbool   IsPresent;
    };

    //--------------------------------------------------------------------------
    // Action Map Helpers
    //--------------------------------------------------------------------------

    template< class T, int MapCount >
    input_action_map** GetActionMaps            ( T (&Maps)[MapCount] ) const;

    //--------------------------------------------------------------------------
    // Platform Backend
    //--------------------------------------------------------------------------

    xbool           IsRawGadgetDown             ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    xbool           WasRawGadgetPressed         ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    f32             GetRawGadgetValue           ( input_gadget GadgetID, s32 DeviceID = 0 ) const;
    xbool           IsRawGadgetPresent          ( input_gadget GadgetID, s32 DeviceID ) const;

    //--------------------------------------------------------------------------
    // Frame Sampling Internals
    //--------------------------------------------------------------------------

    xbool                     IsFrameGadgetValid       ( input_gadget GadgetID ) const;
    xbool                     IsInputDeviceValid       ( s32 DeviceID ) const;
    void                      ClearFrameActivity       ( void );
    void                      RecordGadgetActivity     ( const input_gadget_info& Info, f32 Value );
    void                      RecordGadgetActivity     ( const input_gadget_info& Info, f32 Value, s32 Priority );
    void                      ResolveFrameActivity     ( void );
    void                      AccumulateFrameValue     ( frame_gadget_state& State, const input_gadget_info& Info, f32 Value ) const;
    void                      SampleFrameGadget        ( input_gadget GadgetID, s32 DeviceID );
    const frame_gadget_state& GetFrameGadgetState      ( input_gadget GadgetID, s32 DeviceID ) const;

private:
    //--------------------------------------------------------------------------
    // Runtime State
    //--------------------------------------------------------------------------

    input_device        m_CurrentDevice;
    input_platform      m_CurrentPlatform;
    input_device        m_FrameActivityDevice;
    input_platform      m_FrameActivityPlatform;
    s32                 m_FrameActivityPriority;
    xbool               m_FrameActivityConflict;
    frame_gadget_state  m_FrameGadgets[INPUT_GADGET_COUNT][INPUT_MAX_DEVICES];
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern input_system g_Input;

//==============================================================================
#endif // E_INPUT_HPP
//==============================================================================
