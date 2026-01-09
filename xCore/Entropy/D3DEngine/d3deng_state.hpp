//==============================================================================
//  
//  d3deng_state.hpp
//  
//  Render state manager for D3DENG.
//
//==============================================================================

#ifndef D3DENG_STATE_HPP
#define D3DENG_STATE_HPP

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

//==============================================================================
//  ENUMS
//==============================================================================

enum state_type
{
    STATE_TYPE_BLEND = 0,
    STATE_TYPE_RASTERIZER,
    STATE_TYPE_DEPTH,
    STATE_TYPE_SAMPLER,
    STATE_TYPE_COUNT
};

//------------------------------------------------------------------------------

enum state_blend_mode
{
    STATE_BLEND_NONE = 0,
    STATE_BLEND_ALPHA,
    STATE_BLEND_ADD,
    STATE_BLEND_SUB,
    STATE_BLEND_PREMULT_ALPHA,
    STATE_BLEND_PREMULT_ADD,
    STATE_BLEND_PREMULT_SUB,
    STATE_BLEND_MULTIPLY,
    STATE_BLEND_INTENSITY,
    STATE_BLEND_COLOR_WRITE_DISABLE,    
    STATE_BLEND_COUNT,
    STATE_BLEND_INVALID = 0xFF
};

//------------------------------------------------------------------------------

enum state_raster_mode
{
    STATE_RASTER_SOLID = 0,
    STATE_RASTER_WIRE,
    STATE_RASTER_SOLID_NO_CULL,
    STATE_RASTER_WIRE_NO_CULL,
    STATE_RASTER_COUNT,
    STATE_RASTER_INVALID = 0xFF
};

//------------------------------------------------------------------------------

enum state_depth_mode
{
    STATE_DEPTH_NORMAL = 0,
    STATE_DEPTH_NO_WRITE,
    STATE_DEPTH_DISABLED,
    STATE_DEPTH_DISABLED_NO_WRITE,
    STATE_DEPTH_COUNT,
    STATE_DEPTH_INVALID = 0xFF
};

//------------------------------------------------------------------------------

enum state_sampler_mode
{
    STATE_SAMPLER_LINEAR_WRAP = 0,
    STATE_SAMPLER_LINEAR_CLAMP,
    STATE_SAMPLER_POINT_WRAP,
    STATE_SAMPLER_POINT_CLAMP,
    STATE_SAMPLER_ANISOTROPIC_WRAP,
    STATE_SAMPLER_ANISOTROPIC_CLAMP,	
    STATE_SAMPLER_COUNT,
    STATE_SAMPLER_INVALID = 0xFF
};

//------------------------------------------------------------------------------

enum state_sampler_stage
{
    STATE_SAMPLER_STAGE_PS  = (1 << 0),
    STATE_SAMPLER_STAGE_VS  = (1 << 1),
    STATE_SAMPLER_STAGE_GS  = (1 << 2),
    STATE_SAMPLER_STAGE_CS  = (1 << 3),
    STATE_SAMPLER_STAGE_ALL = (STATE_SAMPLER_STAGE_PS |
                               STATE_SAMPLER_STAGE_VS |
                               STATE_SAMPLER_STAGE_GS |
                               STATE_SAMPLER_STAGE_CS)
};

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

// Initialize state management system
void                state_Init                 ( void );
void                state_Kill                 ( void );

//==============================================================================
//  STATE MANAGEMENT FUNCTIONS
//==============================================================================

// State setters
xbool               state_SetBlend             ( state_blend_mode Mode );
xbool               state_SetRasterizer        ( state_raster_mode Mode );
xbool               state_SetDepth             ( state_depth_mode Mode );
xbool               state_SetSampler           ( state_sampler_mode Mode, u32 Slot = 0, u32 StageMask = STATE_SAMPLER_STAGE_PS );

// Legacy state setter
xbool               state_SetState             ( state_type Type, s32 Mode, u32 Slot = 0, u32 StageMask = STATE_SAMPLER_STAGE_PS );

//==============================================================================
//  UTILITY FUNCTIONS
//==============================================================================

// Force all states to be reapplied
void                state_FlushCache           ( void );

// State query functions
s32                 state_GetState             ( state_type Type );
xbool               state_IsState              ( state_type Type, s32 Mode );

//==============================================================================
#endif // D3DENG_STATE_HPP
//==============================================================================
