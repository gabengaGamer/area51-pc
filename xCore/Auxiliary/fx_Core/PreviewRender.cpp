//=============================================================================
//
//  PreviewRender.cpp
//
//=============================================================================

#include "PreviewRender.hpp"

#include "base.hpp"

namespace
{
static xbool AppendTriangle( xarray<u16>& output, s16 index0, s16 index1, s16 index2, s32 vertexCount )
{
    if( (index0 < 0) || (index0 >= vertexCount) ||
        (index1 < 0) || (index1 >= vertexCount) ||
        (index2 < 0) || (index2 >= vertexCount) )
    {
        return FALSE;
    }

    output.Append( (u16)index0 );
    output.Append( (u16)index1 );
    output.Append( (u16)index2 );
    return TRUE;
}

static xbool ConvertTriangleStrip( xarray<u16>& output, const s16* pIndices, s32 indexCount, s32 vertexCount )
{
    s16 window[2] = { -1, -1 };
    s32 stripVertex = 0;
    for( s32 i = 0; i < indexCount; ++i )
    {
        const s16 index = pIndices[i];
        if( index < 0 )
        {
            stripVertex = 0;
            window[0] = window[1] = -1;
            continue;
        }

        if( index >= vertexCount )
            return FALSE;

        if( stripVertex >= 2 )
        {
            const xbool odd = (stripVertex & 1) != 0;
            if( !AppendTriangle( output, odd ? window[1] : window[0], odd ? window[0] : window[1], index,
                                 vertexCount ) )
            {
                return FALSE;
            }
        }

        window[0] = window[1];
        window[1] = index;
        ++stripVertex;
    }

    return TRUE;
}
}

render::primitive_draw_desc fx_core::CreatePreviewMaterial(
    const texture* pTexture,
    s32 combineMode,
    xbool readDepth,
    xbool clampU,
    xbool clampV,
    render::primitive_raster_mode raster )
{
    render::primitive_blend_mode blend = render::PRIMITIVE_BLEND_ALPHA;
    render::primitive_output_mode output = render::PRIMITIVE_OUTPUT_COLOR;

    switch( combineMode )
    {
        case base::COMBINEMODE_ADDITIVE:
            blend = render::PRIMITIVE_BLEND_ADDITIVE;
            break;
        case base::COMBINEMODE_SUBTRACTIVE:
            blend = render::PRIMITIVE_BLEND_SUBTRACTIVE;
            break;
        case base::COMBINEMODE_GLOW_ALPHA:
            output = render::PRIMITIVE_OUTPUT_GLOW;
            break;
        case base::COMBINEMODE_GLOW_ADD:
            blend = render::PRIMITIVE_BLEND_ADDITIVE;
            output = render::PRIMITIVE_OUTPUT_GLOW;
            break;
        case base::COMBINEMODE_GLOW_SUB:
            blend = render::PRIMITIVE_BLEND_SUBTRACTIVE;
            output = render::PRIMITIVE_OUTPUT_GLOW;
            break;
        case base::COMBINEMODE_DISTORT:
            output = render::PRIMITIVE_OUTPUT_DISTORTION;
            break;
        default:
            break;
    }

    render::primitive_sampler_mode sampler = render::PRIMITIVE_SAMPLER_LINEAR_WRAP;
    if( clampU && clampV )
        sampler = render::PRIMITIVE_SAMPLER_LINEAR_CLAMP;
    else if( clampU )
        sampler = render::PRIMITIVE_SAMPLER_LINEAR_CLAMP_U_WRAP_V;
    else if( clampV )
        sampler = render::PRIMITIVE_SAMPLER_LINEAR_WRAP_U_CLAMP_V;

    const render::primitive_render_layer layer =
        (output == render::PRIMITIVE_OUTPUT_DISTORTION)
        ? render::PRIMITIVE_LAYER_DISTORTION
        : ((blend == render::PRIMITIVE_BLEND_ALPHA)
           ? render::PRIMITIVE_LAYER_TRANSPARENT
           : render::PRIMITIVE_LAYER_ADDITIVE);

    return render::primitive_draw_desc( pTexture,
                                        render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                        blend,
                                        readDepth ? render::PRIMITIVE_DEPTH_READ_ONLY
                                                  : render::PRIMITIVE_DEPTH_DISABLED,
                                        raster,
                                        sampler,
                                        layer,
                                        output );
}

xbool fx_core::SubmitPreviewMesh(
    const render::primitive_draw_desc& material,
    const matrix4& localToWorld,
    const vector3* pPositions,
    const vector2* pUVs,
    const xcolor* pColors,
    s32 vertexCount,
    const s16* pIndices,
    s32 indexCount,
    preview_mesh_topology topology )
{
    if( !pPositions || !pUVs || !pColors || !pIndices ||
        (vertexCount <= 0) || (vertexCount > render::MAX_PRIMITIVE_VERTICES) || (indexCount <= 0) )
    {
        return FALSE;
    }

    xarray<render::primitive_vertex> vertices;
    vertices.SetCount( vertexCount );
    for( s32 i = 0; i < vertexCount; ++i )
        vertices[i] = render::primitive_vertex( pPositions[i], pUVs[i], pColors[i] );

    xarray<u16> indices;
    if( topology == PREVIEW_MESH_TRIANGLE_LIST )
    {
        if( (indexCount % 3) != 0 )
            return FALSE;
        indices.SetCapacity( indexCount );
        for( s32 i = 0; i < indexCount; i += 3 )
        {
            if( !AppendTriangle( indices, pIndices[i], pIndices[i + 1], pIndices[i + 2], vertexCount ) )
                return FALSE;
        }
    }
    else if( topology == PREVIEW_MESH_TRIANGLE_STRIP )
    {
        indices.SetCapacity( indexCount * 3 );
        if( !ConvertTriangleStrip( indices, pIndices, indexCount, vertexCount ) )
            return FALSE;
    }
    else
    {
        return FALSE;
    }

    if( indices.GetCount() == 0 )
        return TRUE;

    return render::SubmitPrimitives( material, localToWorld, vertices.GetPtr(), vertices.GetCount(),
                                     indices.GetPtr(), indices.GetCount() );
}
