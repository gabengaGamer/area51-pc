//=========================================================================
//
//  InputMgr.hpp
//
//=========================================================================

#ifndef INPUT_MGR_HPP
#define INPUT_MGR_HPP

//=========================================================================
// INCLUDES
//=========================================================================

#include "Entropy.hpp"
#include "x_array.hpp"
#ifdef CONFIG_VIEWER
#include "../../Apps/ArtistViewer/Config.hpp"
#else
#include "../../Apps/GameApp/Config.hpp"
#endif

#include "../Menu/DebugMenuDefine.hpp"

//=========================================================================
// DEFINES
//=========================================================================

enum input_platform
{
    INPUT_PLATFORM_NONE = -1,
    INPUT_PLATFORM_PS2,
    INPUT_PLATFORM_XBOX,
    INPUT_PLATFORM_PC,
    MAX_INPUT_PLATFORMS
};

//-------------------------------------------------------------------------

enum input_context
{
    INGAME_CONTEXT     = (1 << 0),
    FRONTEND_CONTEXT   = (1 << 1),
#if defined( ENABLE_DEBUG_MENU )
    DEBUG_MENU_CONTEXT = (1 << 2),
#endif
    ALL_CONTEXTS       = INGAME_CONTEXT | FRONTEND_CONTEXT
#if defined( ENABLE_DEBUG_MENU )
                       | DEBUG_MENU_CONTEXT
#endif
};

//-------------------------------------------------------------------------

enum input_device
{
    INPUT_DEVICE_NONE,
    INPUT_DEVICE_KEYBOARD,
    INPUT_DEVICE_MOUSE,
    INPUT_DEVICE_GAMEPAD
};

//-------------------------------------------------------------------------

enum input_mouse_button
{
    INPUT_MOUSE_BUTTON_LEFT,
    INPUT_MOUSE_BUTTON_MIDDLE,
    INPUT_MOUSE_BUTTON_RIGHT,
    INPUT_MOUSE_BUTTON_COUNT
};

//=========================================================================
// CLASSES
//=========================================================================

class input_pad
{
public:

    struct logical
    {
                        logical         ( void );

        void            Clear           ( void );
        void            ClearFrame      ( void );
        void            ClearFixed      ( void );
        void            PrepareFixed    ( s32 StepCount );
        void            CommitLocal     ( void );
        void            CommitFixed     ( void );

        char            ActionName[48];
        f32             IsValue;
        f32             WasValue;
        f32             CurrentValue;
        f32             FrameValue;
        f32             FrameWasValue;
        f32             FixedValue;
        f32             FixedStepValue;
        f32             FixedWasValue;
        s32             FixedStepCount;
        f32             TimePressed;
    };

protected:

    struct mapping
    {
                        mapping         ( void );

        u32             bIsTap : 1;
        u32             bIsHold : 1;
        input_gadget    GadgetID;
        f32             Scale;
        f32             LastAnalogValue;
        s32             LogicalID;
        u32             ContextMask;
    };

public:
                        input_pad       ( void );

    logical&            GetLogical      ( s32 I );
    const logical&      GetLogical      ( s32 I ) const;
    s32                 GetLogicalCount ( void ) const      { return m_Logicals.GetCount(); }

    void                SetLogicalCount ( s32 Count );
    void                SetLogicalName  ( s32 ID, const char* pName );
    void                AddMapping      ( s32 iPlatform, s32 ID, input_gadget GadgetID, f32 Scale = 1, u32 ContextMask = INGAME_CONTEXT );

    void                SetControllerID ( s32 ControllerID ){ m_ControllerID = ControllerID; }
    s32                 GetControllerID ( void ) const      { return m_ControllerID; }

    void                ClearAllLogical ( void );

protected:

    virtual void        OnBeginFrame    ( f32 DeltaTime );
    virtual void        OnUpdateLocal   ( f32 DeltaTime );
    virtual void        OnUpdateFixed   ( f32 DeltaTime );
    virtual void        OnInitialize    ( void );
    virtual xbool       IsPausePressed  ( void ) const;

    xbool               ShouldPollInput ( void ) const;
    s32                 GetPollControllerID( void ) const;
    void                SetActiveContext( u32 ContextMask );
    void                ClearFrameValues( void );
    void                ClearCurrentValues( void );
    void                ClearFixedValues( void );
    void                PrepareFixedValues( s32 StepCount );
    void                SampleMapping   ( mapping& Mapping, s32 ControllerID, f32 DeltaTime );
    void                SampleButtonMapping( const mapping& Mapping, s32 ControllerID, f32 DeltaTime );
    void                SampleAnalogMapping( mapping& Mapping, s32 ControllerID );
protected:

    xarray<logical>     m_Logicals;
    xarray<mapping>     m_Mappings[MAX_INPUT_PLATFORMS];
    input_pad*          m_pNext;
    s32                 m_ControllerID;
    u32                 m_ActiveContext;

protected:

    friend class input_mgr;
};

//=========================================================================

class input_mgr
{
public:

                        input_mgr       ( void );
    xbool               BeginFrame      ( f32 DeltaTime, u32 ContextMask );
    void                UpdateLocal     ( f32 DeltaTime );
    void                PrepareFixedInput( s32 StepCount );
    void                UpdateFixed     ( f32 DeltaTime );
    void                ClearFixedInput ( void );
    static void         RegisterPad     ( input_pad& Pad );
    s32                 WasPausePressed ( xbool IsPaused );
    input_device        GetActiveDevice ( void ) const      { return m_ActiveDevice; }
    input_platform      GetActivePlatform( void ) const     { return m_ActivePlatform; }
    xbool               IsGamepadActive ( void ) const      { return( m_ActiveDevice == INPUT_DEVICE_GAMEPAD ); }
    s32                 GetMouseDeltaX  ( void ) const      { return m_MouseDeltaX; }
    s32                 GetMouseDeltaY  ( void ) const      { return m_MouseDeltaY; }
    xbool               IsMouseButtonDown( input_mouse_button Button ) const;

protected:

    friend class input_pad;

    static input_pad* s_pHead;
    void                ApplyPadContext ( u32 ContextMask );
    void                SampleMouseKeyboardState( void );
    void                SetActiveDevice ( input_device Device );
    void                NotifyInputActivity( input_gadget GadgetID, f32 Value );

    input_device        m_ActiveDevice;
    input_platform      m_ActivePlatform;
    s32                 m_MouseDeltaX;
    s32                 m_MouseDeltaY;
    xbool               m_MouseButtons[INPUT_MOUSE_BUTTON_COUNT];
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
xbool IsInputGadgetInRange( input_gadget GadgetID, input_gadget Begin, input_gadget End )
{
    return( (GadgetID > Begin) && (GadgetID < End) );
}

//=========================================================================

inline
xbool IsKeyboardGadget( input_gadget GadgetID )
{
    return IsInputGadgetInRange( GadgetID, INPUT_KBD__BEGIN, INPUT_KBD__END );
}

//=========================================================================

inline
xbool IsMouseGadget( input_gadget GadgetID )
{
    return IsInputGadgetInRange( GadgetID, INPUT_MOUSE__BEGIN, INPUT_MOUSE__END );
}

//=========================================================================

inline
xbool IsPS2Gadget( input_gadget GadgetID )
{
    return IsInputGadgetInRange( GadgetID, INPUT_PS2__BEGIN, INPUT_PS2__END );
}

//=========================================================================

inline
xbool IsXboxGadget( input_gadget GadgetID )
{
    return IsInputGadgetInRange( GadgetID, INPUT_XBOX__BEGIN, INPUT_XBOX__END );
}

//=========================================================================

inline
xbool IsPCGadget( input_gadget GadgetID )
{
    return IsInputGadgetInRange( GadgetID, INPUT_PC__BEGIN, INPUT_PC__END );
}

//=========================================================================

inline
xbool IsGamepadGadget( input_gadget GadgetID )
{
    return( IsPS2Gadget( GadgetID ) || IsXboxGadget( GadgetID ) || IsPCGadget( GadgetID ) );
}

//=========================================================================

inline
xbool IsMouseDeltaGadget( input_gadget GadgetID )
{
    return( (GadgetID == INPUT_MOUSE_X_REL) || (GadgetID == INPUT_MOUSE_Y_REL) );
}

//=========================================================================

inline
xbool IsMouseWheelGadget( input_gadget GadgetID )
{
    return( GadgetID == INPUT_MOUSE_WHEEL_REL );
}

//=========================================================================

inline
xbool IsStickAxisGadget( input_gadget GadgetID )
{
    return( ((GadgetID >= INPUT_PS2_STICK_LEFT_X) && (GadgetID <= INPUT_PS2_STICK_RIGHT_Y)) ||
            IsInputGadgetInRange( GadgetID, INPUT_XBOX__STICKS_BEGIN, INPUT_XBOX__STICKS_END ) ||
            IsInputGadgetInRange( GadgetID, INPUT_PC__ANALOG, INPUT_PC__END ) );
}

//=========================================================================

inline
xbool IsButtonGadget( input_gadget GadgetID )
{
    return( IsKeyboardGadget( GadgetID ) ||
            IsInputGadgetInRange( GadgetID, INPUT_MOUSE__DIGITAL, INPUT_MOUSE__ANALOG ) ||
            ((GadgetID > INPUT_PS2__BEGIN) && (GadgetID < INPUT_PS2_STICK_LEFT_X)) ||
            IsInputGadgetInRange( GadgetID, INPUT_XBOX__DIGITAL_BUTTONS_BEGIN, INPUT_XBOX__DIGITAL_BUTTONS_END ) ||
            IsInputGadgetInRange( GadgetID, INPUT_XBOX__ANALOG_BUTTONS_BEGIN, INPUT_XBOX__ANALOG_BUTTONS_END ) ||
            IsInputGadgetInRange( GadgetID, INPUT_PC__DIGITAL, INPUT_PC__ANALOG ) );
}

//=========================================================================

inline
input_device GetInputGadgetDevice( input_gadget GadgetID )
{
    if( IsMouseGadget( GadgetID ) )
        return INPUT_DEVICE_MOUSE;

    if( IsKeyboardGadget( GadgetID ) )
        return INPUT_DEVICE_KEYBOARD;

    if( IsGamepadGadget( GadgetID ) )
        return INPUT_DEVICE_GAMEPAD;

    return INPUT_DEVICE_NONE;
}

//=========================================================================

inline
input_platform GetInputGadgetPlatform( input_gadget GadgetID )
{
    if( IsPS2Gadget( GadgetID ) )
        return INPUT_PLATFORM_PS2;

    if( IsXboxGadget( GadgetID ) )
        return INPUT_PLATFORM_XBOX;

    if( IsKeyboardGadget( GadgetID ) || IsMouseGadget( GadgetID ) || IsPCGadget( GadgetID ) )
        return INPUT_PLATFORM_PC;

    return INPUT_PLATFORM_NONE;
}

//=========================================================================
// GLOBALS
//=========================================================================

extern input_mgr g_InputMgr;

//=========================================================================
#endif // INPUT_MGR_HPP
//=========================================================================
