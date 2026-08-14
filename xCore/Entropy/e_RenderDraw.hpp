//==============================================================================
//
//  e_RenderDraw.hpp
//
//  Explicit GPU draw API for PSO render backends.
//
//==============================================================================

#ifndef E_RENDERDRAW_HPP
#define E_RENDERDRAW_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

#include "e_RenderBuffer.hpp"

//==============================================================================
//  RENDER DRAW STRUCTURES
//==============================================================================

struct rdraw_viewport
{
    f32 TopLeftX;
    f32 TopLeftY;
    f32 Width;
    f32 Height;
    f32 MinDepth;
    f32 MaxDepth;

    rdraw_viewport( void ) :
        TopLeftX( 0.0f ),
        TopLeftY( 0.0f ),
        Width   ( 0.0f ),
        Height  ( 0.0f ),
        MinDepth( 0.0f ),
        MaxDepth( 1.0f )
    {
    }
};

//------------------------------------------------------------------------------

struct rdraw_scissor
{
    s32 X;
    s32 Y;
    s32 Width;
    s32 Height;

    rdraw_scissor( void ) :
        X     ( 0 ),
        Y     ( 0 ),
        Width ( 0 ),
        Height( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct rdraw_indexed_indirect_command
{
    u32 IndexCount;
    u32 InstanceCount;
    u32 FirstIndex;
    s32 BaseVertex;
    u32 FirstInstance;

    rdraw_indexed_indirect_command( void ) :
        IndexCount   ( 0 ),
        InstanceCount( 0 ),
        FirstIndex   ( 0 ),
        BaseVertex   ( 0 ),
        FirstInstance( 0 )
    {
    }
};

static_assert( sizeof(rdraw_indexed_indirect_command) == 20,
               "Indexed indirect command layout must match the GPU backend" );

//==============================================================================
//  RENDER DRAW FUNCTIONS
//==============================================================================

xbool                   rdraw_SetViewport           ( const rdraw_viewport& Viewport );
xbool                   rdraw_SetScissor            ( const rdraw_scissor& Scissor );
xbool                   rdraw_Draw                  ( s32 VertexCount,
                                                      s32 StartVertex = 0 );
xbool                   rdraw_DrawInstanced         ( s32 VertexCount,
                                                      s32 InstanceCount,
                                                      s32 StartVertex = 0,
                                                      s32 StartInstance = 0 );
xbool                   rdraw_DrawIndexed           ( s32 IndexCount,
                                                      s32 StartIndex = 0,
                                                      s32 BaseVertex = 0 );
xbool                   rdraw_DrawIndexedInstanced  ( s32 IndexCount,
                                                      s32 InstanceCount,
                                                      s32 StartIndex = 0,
                                                      s32 BaseVertex = 0,
                                                      s32 StartInstance = 0 );
xbool                   rdraw_DrawIndexedIndirect   ( const rbuffer& Buffer,
                                                      u32 Offset,
                                                      u32 DrawCount );

//==============================================================================
#endif // E_RENDERDRAW_HPP
//==============================================================================
