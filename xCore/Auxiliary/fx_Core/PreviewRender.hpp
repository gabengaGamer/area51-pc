//=============================================================================
//
//  PreviewRender.hpp
//
//  Explicit primitive materials and indexed mesh submission for FX previews.
//
//=============================================================================

#ifndef FX_CORE_PREVIEW_RENDER_HPP
#define FX_CORE_PREVIEW_RENDER_HPP

#include "Render/PrimitiveBatch.hpp"

namespace fx_core
{
enum preview_mesh_topology
{
    PREVIEW_MESH_TRIANGLE_LIST = 0,
    PREVIEW_MESH_TRIANGLE_STRIP
};

render::primitive_draw_desc CreatePreviewMaterial(
    const texture* pTexture,
    s32 combineMode,
    xbool readDepth,
    xbool clampU = FALSE,
    xbool clampV = FALSE,
    render::primitive_raster_mode raster = render::PRIMITIVE_RASTER_SOLID_NO_CULL );

xbool SubmitPreviewMesh(
    const render::primitive_draw_desc& material,
    const matrix4& localToWorld,
    const vector3* pPositions,
    const vector2* pUVs,
    const xcolor* pColors,
    s32 vertexCount,
    const s16* pIndices,
    s32 indexCount,
    preview_mesh_topology topology );
}

#endif
