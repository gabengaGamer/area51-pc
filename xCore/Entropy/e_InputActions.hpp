//==============================================================================
//
//  e_InputActions.hpp
//
//==============================================================================

#ifndef E_INPUT_ACTIONS_HPP
#define E_INPUT_ACTIONS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_Input.hpp"
#include "x_array.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum input_action_value_mode
{
    INPUT_ACTION_VALUE_AUTO,
    INPUT_ACTION_VALUE_SIGNED_AXIS,
    INPUT_ACTION_VALUE_POSITIVE_AXIS
};

//------------------------------------------------------------------------------

struct input_action_source
{
    input_action_source    ( void );

    void               Clear           ( void );
    void               Set             ( input_gadget GadgetID, s32 DeviceID );

    xbool              IsValid         ( void ) const { return( GadgetID != INPUT_UNDEFINED ); }
    input_platform     GetPlatform     ( void ) const { return Info.Platform; }
    input_device       GetDevice       ( void ) const { return Info.Device; }
    input_value_kind   GetValueKind    ( void ) const { return Info.ValueKind; }
    input_control_kind GetControlKind  ( void ) const { return Info.ControlKind; }

    input_gadget       GadgetID;
    s32                DeviceID;
    input_gadget_info  Info;
};

//==============================================================================
//  INPUT ACTION STATE CLASS
//==============================================================================

class input_action_state
{
public:
    input_action_state    ( void );

    //--------------------------------------------------------------------------
    // Queries
    //--------------------------------------------------------------------------

    const char*                GetActionName     ( void ) const { return m_ActionName; }
    f32                        GetIsValue        ( void ) const { return m_IsValue; }
    f32                        GetWasValue       ( void ) const { return m_WasValue; }
    f32                        GetCurrentValue   ( void ) const { return m_CurrentValue; }
    f32                        GetFrameValue     ( void ) const { return m_FrameValue; }
    f32                        GetFrameWasValue  ( void ) const { return m_FrameWasValue; }
    f32                        GetFixedValue     ( void ) const { return m_FixedValue; }
    f32                        GetFixedStepValue ( void ) const { return m_FixedStepValue; }
    f32                        GetFixedWasValue  ( void ) const { return m_FixedWasValue; }
    s32                        GetFixedStepCount ( void ) const { return m_FixedStepCount; }
    f32                        GetTimePressed    ( void ) const { return m_TimePressed; }
    const input_action_source& GetIsSource       ( void ) const { return m_IsSource; }
    const input_action_source& GetWasSource      ( void ) const { return m_WasSource; }
    const input_action_source& GetCurrentSource  ( void ) const { return m_CurrentSource; }
    const input_action_source& GetFrameSource    ( void ) const { return m_FrameSource; }

private:
    friend class input_action_map;

    //--------------------------------------------------------------------------
    // Sample Lifetime
    //--------------------------------------------------------------------------

    void            Clear               ( void );
    void            ClearFrameSamples   ( void );
    void            ClearFixedSamples   ( void );
    void            PrepareFixedSamples ( s32 StepCount );
    void            CommitFrame         ( void );
    void            CommitFixed         ( void );

    //--------------------------------------------------------------------------
    // State
    //--------------------------------------------------------------------------

    char                m_ActionName[48];
    f32                 m_IsValue;
    f32                 m_WasValue;
    f32                 m_CurrentValue;
    f32                 m_FrameValue;
    f32                 m_FrameWasValue;
    f32                 m_FixedValue;
    f32                 m_FixedStepValue;
    f32                 m_FixedWasValue;
    s32                 m_FixedStepCount;
    f32                 m_TimePressed;
    input_action_source m_IsSource;
    input_action_source m_WasSource;
    input_action_source m_CurrentSource;
    input_action_source m_FrameSource;
    input_action_source m_FrameWasSource;
    input_action_source m_FixedSource;
    input_action_source m_FixedStepSource;
    input_action_source m_FixedWasSource;
};

//==============================================================================
//  INPUT ACTION MAP CLASS
//==============================================================================

class input_action_map
{
public:
    input_action_map    ( void );

    //--------------------------------------------------------------------------
    // Action Setup And Queries
    //--------------------------------------------------------------------------

    input_action_state&       GetActionState      ( s32 ActionID );
    const input_action_state& GetActionState      ( s32 ActionID ) const;
    s32                       GetActionCount      ( void ) const { return m_Actions.GetCount(); }

    void            SetActionCount      ( s32 Count );
    void            SetActionName       ( s32 ActionID, const char* pName );

    //--------------------------------------------------------------------------
    // Bindings
    //--------------------------------------------------------------------------

    void            AddBinding          ( s32 ActionID,
                                          input_gadget GadgetID,
                                          f32 Scale,
                                          u32 ContextMask,
                                          input_action_value_mode ValueMode = INPUT_ACTION_VALUE_AUTO );

    //--------------------------------------------------------------------------
    // Context And Device
    //--------------------------------------------------------------------------

    void            SetActiveContext    ( u32 ContextMask );
    u32             GetActiveContext    ( void ) const { return m_ActiveContext; }

    void            SetDeviceID         ( s32 DeviceID );
    s32             GetDeviceID         ( void ) const { return m_DeviceID; }
    xbool           HasDeviceID         ( void ) const { return( m_DeviceID != -1 ); }
    s32             GetResolvedDeviceID ( s32 FallbackDeviceID ) const;

    //--------------------------------------------------------------------------
    // Action Sample State
    //--------------------------------------------------------------------------

    void            ClearAllActions     ( void );
    void            ClearFrameSamples   ( void );
    void            ClearCurrentSamples ( void );
    void            ClearFixedSamples   ( void );
    void            PrepareFixedSamples ( s32 StepCount );

    //--------------------------------------------------------------------------
    // Sampling
    //--------------------------------------------------------------------------

    virtual void    SampleFrame         ( f32 DeltaTime );
    void            SampleDevice        ( f32 DeltaTime, s32 DeviceID );
    void            CommitFrame         ( void );
    void            CommitFixed         ( void );

protected:
    //--------------------------------------------------------------------------
    // Extension Hooks
    //--------------------------------------------------------------------------

    virtual void    OnInputActivity     ( input_gadget GadgetID, f32 Value );
    virtual xbool   OverrideActionValue ( s32 ActionID, f32& Value );
    virtual void    OnActionValue       ( s32 ActionID, f32 Value );

private:
    //--------------------------------------------------------------------------
    // Binding State
    //--------------------------------------------------------------------------

    struct binding
    {
        binding                     ( void );

        xbool                       IsTap;
        xbool                       IsHold;
        input_gadget                GadgetID;
        f32                         Scale;
        f32                         LastAnalogValue;
        s32                         ActionID;
        u32                         ContextMask;
        input_action_value_mode     ValueMode;
    };

    //--------------------------------------------------------------------------
    // Binding Sampling
    //--------------------------------------------------------------------------

    void            SampleBinding       ( binding& Binding, s32 DeviceID, f32 DeltaTime );
    void            SampleDigitalBinding( const binding& Binding, s32 DeviceID, f32 DeltaTime );
    void            SampleAnalogBinding ( binding& Binding, s32 DeviceID );

private:
    //--------------------------------------------------------------------------
    // Runtime State
    //--------------------------------------------------------------------------

    xarray<input_action_state>      m_Actions;
    xarray<binding>                 m_Bindings[INPUT_PLATFORM_COUNT];
    u32                             m_ActiveContext;
    s32                             m_DeviceID;
};

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

template< class T, int MapCount >
inline input_action_map** input_system::GetActionMaps( T (&Maps)[MapCount] ) const
{
    static input_action_map* s_Maps[MapCount];

    for( s32 i = 0; i < MapCount; i++ )
    {
        s_Maps[i] = &Maps[i];
    }

    return s_Maps;
}

//==============================================================================

template< class T, int MapCount >
inline xbool input_system::SampleActionMaps( T (&Maps)[MapCount], f32 DeltaTime )
{
    return SampleActionMaps( GetActionMaps( Maps ), MapCount, DeltaTime );
}

//==============================================================================

template< class T, int MapCount >
inline xbool input_system::SampleActionMaps( T (&Maps)[MapCount], f32 DeltaTime, u32 ContextMask )
{
    return SampleActionMaps( GetActionMaps( Maps ), MapCount, DeltaTime, ContextMask );
}

//==============================================================================

template< class T, int MapCount >
inline void input_system::CommitActionMapsFrame( T (&Maps)[MapCount] )
{
    CommitActionMapsFrame( GetActionMaps( Maps ), MapCount );
}

//==============================================================================

template< class T, int MapCount >
inline void input_system::PrepareActionMapsFixed( T (&Maps)[MapCount], s32 StepCount )
{
    PrepareActionMapsFixed( GetActionMaps( Maps ), MapCount, StepCount );
}

//==============================================================================

template< class T, int MapCount >
inline void input_system::CommitActionMapsFixed( T (&Maps)[MapCount] )
{
    CommitActionMapsFixed( GetActionMaps( Maps ), MapCount );
}

//==============================================================================

template< class T, int MapCount >
inline void input_system::ClearActionMapsFixed( T (&Maps)[MapCount] )
{
    ClearActionMapsFixed( GetActionMaps( Maps ), MapCount );
}

//==============================================================================
#endif // E_INPUT_ACTIONS_HPP
//==============================================================================
