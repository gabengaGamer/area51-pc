//==============================================================================
//
//  input_backend.hpp
//
//==============================================================================

#ifndef INPUT_BACKEND_HPP
#define INPUT_BACKEND_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_Input.hpp"

//==============================================================================
//  INPUT BACKEND INTERFACE
//==============================================================================

class input_backend
{
public:
    virtual                    ~input_backend             ( void ) {}

    virtual xbool               Init                      ( const input_init_desc& Desc ) = 0;
    virtual void                Kill                      ( void ) = 0;

    virtual xbool               CaptureFrameInput         ( input_event_buffer& Events ) = 0;
    virtual xbool               IsGadgetDown              ( input_gadget GadgetID, s32 DeviceID ) const = 0;
    virtual f32                 GetGadgetValue            ( input_gadget GadgetID, s32 DeviceID ) const = 0;
    virtual xbool               IsGadgetPresent           ( input_gadget GadgetID, s32 DeviceID ) const = 0;
    virtual xbool               IsDevicePresent           ( input_device Device, s32 DeviceID ) const = 0;
    virtual s32                 GetPadCount               ( void ) const = 0;

    virtual void                Feedback                  ( f32 Duration, f32 Intensity, s32 DeviceID ) = 0;
    virtual void                Feedback                  ( s32 Count, feedback_envelope* pEnvelope, s32 DeviceID ) = 0;
    virtual void                EnableFeedback            ( xbool State, s32 DeviceID ) = 0;
    virtual void                SuppressFeedback          ( xbool Suppress ) = 0;
    virtual void                ClearFeedback             ( void ) = 0;
};

//==============================================================================
//  DEFAULT BACKEND FACTORY
//==============================================================================

input_backend*  input_CreateDefaultBackend       ( void );
void            input_DestroyDefaultBackend      ( input_backend* pBackend );

//==============================================================================
#endif // INPUT_BACKEND_HPP
//==============================================================================
