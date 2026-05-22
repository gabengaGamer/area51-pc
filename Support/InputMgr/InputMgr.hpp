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

//=========================================================================
// DEFINES
//=========================================================================

enum input_platform
{
    INPUT_PLATFORM_PS2,
    INPUT_PLATFORM_XBOX,
    INPUT_PLATFORM_PC,
    MAX_INPUT_PLATFORMS
};

//-------------------------------------------------------------------------

enum input_context
{
    INGAME_CONTEXT   = (1 << 0),
    FRONTEND_CONTEXT = (1 << 1),
    ALL_CONTEXTS     = INGAME_CONTEXT | FRONTEND_CONTEXT
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
        void            Commit          ( void );

        char            ActionName[48];
        f32             IsValue;
        f32             WasValue;
        f32             MapsIsValue;
        f32             MapsWasValue;
        f32             TimePressed;
    };

protected:
    struct mapping
    {
                        mapping         ( void );

        u32             bButton : 1;
        u32             bIsTap : 1;
        u32             bIsHold : 1;
        input_gadget    GadgetID;
        f32             Scale;
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
    void                ClearLogicalForGadget( s32 iPlatform, input_gadget GadgetID );
    void                AddMapping      ( s32 iPlatform, s32 ID, input_gadget GadgetID, xbool IsButton, f32 Scale = 1, u32 ContextMask = INGAME_CONTEXT );

    void                SetControllerID ( s32 ControllerID ){ m_ControllerID = ControllerID; }
    s32                 GetControllerID ( void ) const      { return m_ControllerID; }

    void                SetContext      ( u32 ContextMask );
    void                EnableContext   ( u32 ContextMask );
    void                DisableContext  ( u32 ContextMask );
    u32                 GetContext      ( void ) const      { return m_ActiveContext; }

    void                ClearAllLogical ( void );

protected:

    virtual void        OnUpdate        ( f32 DeltaTime );
    virtual void        OnInitialize    ( void );
    virtual xbool       IsPausePressed  ( void ) const;

    xbool               ShouldPollInput ( void ) const;
    s32                 GetPollControllerID( void ) const;
    void                UpdateMapping   ( const mapping& Mapping, s32 ControllerID, f32 DeltaTime );
    void                UpdateButtonMapping( const mapping& Mapping, s32 ControllerID, f32 DeltaTime );
    void                UpdateAnalogMapping( const mapping& Mapping, s32 ControllerID );
    void                CommitLogicalValues( void );

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
    xbool               Update          ( f32 DeltaTime );
    void                RegisterPad     ( input_pad& Pad );
    s32                 WasPausePressed ( void );
    input_device        GetActiveDevice ( void ) const      { return m_ActiveDevice; }
    input_platform      GetActivePlatform( void ) const     { return m_ActivePlatform; }
    xbool               IsGamepadActive ( void ) const      { return( m_ActiveDevice == INPUT_DEVICE_GAMEPAD ); }
    s32                 GetMouseDeltaX  ( void ) const      { return m_MouseDeltaX; }
    s32                 GetMouseDeltaY  ( void ) const      { return m_MouseDeltaY; }
    xbool               IsMouseButtonDown( input_mouse_button Button ) const;
    void                SetContext      ( u32 ContextMask );
    void                EnableContext   ( u32 ContextMask );
    void                DisableContext  ( u32 ContextMask );

    void                NotifyInputActivity( input_gadget GadgetID, f32 Value );

protected:

    static input_pad* s_pHead;    
    void                UpdateDeviceState( void );
    void                SetActiveDevice ( input_device Device );

    input_device        m_ActiveDevice;
    input_platform      m_ActivePlatform;
    s32                 m_MouseDeltaX;
    s32                 m_MouseDeltaY;
    xbool               m_MouseButtons[INPUT_MOUSE_BUTTON_COUNT];

};

//=========================================================================
// GLOBALS 
//=========================================================================

extern input_mgr g_InputMgr;

//=========================================================================
#endif // INPUT_MGR_HPP
//=========================================================================
