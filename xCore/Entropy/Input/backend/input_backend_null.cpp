//==============================================================================
//
//  input_backend_null.cpp
//
//  Desktop input placeholder until the SDL3 input backend is available.
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#if !defined( TARGET_LINUX )
    #error This file should only be compiled for Linux platform.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "input_backend.hpp"

//==============================================================================

class null_input_backend : public input_backend
{
public:
    xbool Init( const input_init_desc& Desc ) override
    {
        (void)Desc;
        return TRUE;
    }

    void Kill( void ) override
    {
    }

    xbool CaptureFrameInput( input_event_buffer& Events ) override
    {
        Events.BeginCapture( 0 );
        Events.EndCapture( 0 );
        return FALSE;
    }

    xbool IsGadgetDown( input_gadget GadgetID, s32 DeviceID ) const override
    {
        (void)GadgetID;
        (void)DeviceID;
        return FALSE;
    }

    f32 GetGadgetValue( input_gadget GadgetID, s32 DeviceID ) const override
    {
        (void)GadgetID;
        (void)DeviceID;
        return 0.0f;
    }

    xbool IsGadgetPresent( input_gadget GadgetID, s32 DeviceID ) const override
    {
        (void)GadgetID;
        (void)DeviceID;
        return FALSE;
    }

    s32 GetPadCount( void ) const override
    {
        return 0;
    }

    void Feedback( f32 Duration, f32 Intensity, s32 DeviceID ) override
    {
        (void)Duration;
        (void)Intensity;
        (void)DeviceID;
    }

    void Feedback( s32 Count, feedback_envelope* pEnvelope, s32 DeviceID ) override
    {
        (void)Count;
        (void)pEnvelope;
        (void)DeviceID;
    }

    void EnableFeedback( xbool State, s32 DeviceID ) override
    {
        (void)State;
        (void)DeviceID;
    }

    void SuppressFeedback( xbool Suppress ) override
    {
        (void)Suppress;
    }

    void ClearFeedback( void ) override
    {
    }
};

//==============================================================================

input_backend* input_CreateDefaultBackend( void )
{
    return new null_input_backend;
}

//==============================================================================

void input_DestroyDefaultBackend( input_backend* pBackend )
{
    delete pBackend;
}
