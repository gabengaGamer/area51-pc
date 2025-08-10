//==============================================================================
//  
//  d3deng_composite.hpp
//  
//  Composite manager for D3DENG
//
//==============================================================================

#ifndef D3DENG_COMPOSITE_HPP
#define D3DENG_COMPOSITE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "d3deng_private.hpp"
#include "d3deng_rtarget.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum composite_blend_mode
{
    COMPOSITE_BLEND_ALPHA,      // Standard alpha blending
    COMPOSITE_BLEND_ADDITIVE,   // Additive blending
    COMPOSITE_BLEND_MULTIPLY,   // Multiply blending
    COMPOSITE_BLEND_OVERLAY,    // Overlay blending
    
    COMPOSITE_BLEND_COUNT
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

void    composite_Init          ( void );
void    composite_Kill          ( void );

// Composite source target onto currently bound render target
void    composite_Blit          ( const rtarget&         Source,
                                  composite_blend_mode   BlendMode = COMPOSITE_BLEND_ALPHA,
                                  f32                    Alpha = 1.0f,
                                  ID3D11PixelShader*     pCustomShader = NULL );

#endif // D3DENG_COMPOSITE_HPP