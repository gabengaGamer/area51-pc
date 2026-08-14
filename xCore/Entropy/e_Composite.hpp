//==============================================================================
//
//  e_Composite.hpp
//
//==============================================================================

#ifndef E_COMPOSITE_HPP
#define E_COMPOSITE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

#include "e_RenderState.hpp"
#include "e_RenderTarget.hpp"

struct rtarget;
struct shader;

//==============================================================================
//  TYPES
//==============================================================================

enum composite_blend_mode
{
    COMPOSITE_BLEND_ALPHA,
    COMPOSITE_BLEND_ADDITIVE,
    COMPOSITE_BLEND_MULTIPLY,
    COMPOSITE_BLEND_OVERLAY,
    COMPOSITE_BLEND_COPY,

    COMPOSITE_BLEND_COUNT
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

void    composite_Init          ( void );
void    composite_Kill          ( void );

xbool   composite_PrewarmPipeline( composite_blend_mode BlendMode,
                                   rtarget_format       ColorFormat,
                                   rtarget_format       DepthFormat = RTARGET_FORMAT_COUNT,
                                   u32                  SampleCount = 1,
                                   const shader*        pCustomShader = NULL );

void    composite_Blit          ( const rtarget&         Source,
                                  composite_blend_mode   BlendMode = COMPOSITE_BLEND_ALPHA,
                                  f32                    Alpha = 1.0f,
                                  const shader*          pCustomShader = NULL,
                                  rstate_sampler_preset  SamplerMode = RSTATE_SAMPLER_PRESET_POINT_CLAMP,
                                  const char*            pSourceBindingName = NULL );

//==============================================================================
#endif // E_COMPOSITE_HPP
//==============================================================================
