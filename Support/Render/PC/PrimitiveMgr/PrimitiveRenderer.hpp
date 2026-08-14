//=============================================================================
//
//  PrimitiveRenderer.hpp
//
//=============================================================================

#ifndef PRIMITIVE_RENDERER_HPP
#define PRIMITIVE_RENDERER_HPP

//=============================================================================
//  INCLUDES
//=============================================================================

#include "PrimitiveMgr.hpp"
#include "../../PrimitiveBatch.hpp"

//=============================================================================

class PrimitiveRenderer
{
public:
    PrimitiveRenderer ( void );
    
    void Init        ( PrimitiveMgr& primitives );
    void Kill        ( void );
    
    xbool BeginSubmission    ( void );
    xbool EndSubmission      ( void );
    void  DiscardSubmission  ( void );
    xbool IsSubmissionActive ( void ) const;
    xbool SubmitPrimitives   ( render::primitive_draw_desc const& desc, matrix4 const& localToWorld,
                               render::primitive_vertex const* pVertices, s32 nVertices, u16 const* pIndices,
                               s32 nIndices );
    xbool SubmitDepthRect    ( irect const& rect, f32 depth );

protected:
    static xbool ComputeSortDepth      ( f32& outDepth, matrix4 const& localToWorld,
                                         render::primitive_vertex const* pVertices, s32 nVertices );
    static xbool TopologyFromMode      ( render::primitive_topology topology, shader_topology& result );
    static xbool BlendPresetFromMode   ( render::primitive_blend_mode blend, rstate_blend_preset& preset );
    static xbool DepthPresetFromMode   ( render::primitive_depth_mode depth, rstate_depth_preset& preset );
    static xbool RasterPresetFromMode  ( render::primitive_raster_mode raster, rstate_raster_preset& preset );
    static xbool SamplerPresetFromMode ( render::primitive_sampler_mode sampler, rstate_sampler_preset& preset );
    
    void  ResetState     ( void );
    xbool BuildBatchDesc ( PrimitiveMgr::BatchDesc& out, render::primitive_draw_desc const& desc,
                           matrix4 const& localToWorld ) const;
    
    xbool         m_isSubmissionActive;
    PrimitiveMgr* m_pPrimitives;
};

//=============================================================================

extern PrimitiveRenderer g_PrimitiveRenderer;

//=============================================================================
//  INLINE FUNCTIONS
//=============================================================================

inline xbool PrimitiveRenderer::IsSubmissionActive( void ) const
{
    return m_isSubmissionActive;
}

//=============================================================================
#endif // PRIMITIVE_RENDERER_HPP
//=============================================================================
