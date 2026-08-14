//=============================================================================
//
//  PrimitiveRenderer.cpp
//
//=============================================================================

#include "PrimitiveRenderer.hpp"

#include "x_debug.hpp"

//=============================================================================

PrimitiveRenderer g_PrimitiveRenderer;

//=============================================================================

PrimitiveRenderer::PrimitiveRenderer( void ) : m_isSubmissionActive( FALSE ), m_pPrimitives( NULL )
{
}

//=============================================================================

void PrimitiveRenderer::Init( PrimitiveMgr& primitives )
{
    ASSERT( !m_pPrimitives );
    m_pPrimitives = &primitives;
    ResetState();
}

//=============================================================================

void PrimitiveRenderer::Kill( void )
{
    ASSERTS( !m_isSubmissionActive, "Primitive submission survived renderer shutdown" );
    ResetState();
    m_pPrimitives = NULL;
}

//=============================================================================

void PrimitiveRenderer::ResetState( void )
{
    m_isSubmissionActive = FALSE;
}

//=============================================================================

xbool PrimitiveRenderer::BeginSubmission( void )
{
    ASSERTS( !m_isSubmissionActive, "Primitive submission is already active" );

    if ( m_isSubmissionActive )
    {
        return FALSE;
    }

    m_isSubmissionActive = TRUE;
    return TRUE;
}

//=============================================================================

xbool PrimitiveRenderer::EndSubmission( void )
{
    ASSERTS( m_isSubmissionActive, "Primitive submission is not active" );

    if ( !m_isSubmissionActive )
    {
        return FALSE;
    }

    ResetState();
    return TRUE;
}

//=============================================================================

void PrimitiveRenderer::DiscardSubmission( void )
{
    ResetState();
}

//=============================================================================

xbool PrimitiveRenderer::BlendPresetFromMode( render::primitive_blend_mode blend, rstate_blend_preset& preset )
{
    switch ( blend )
    {
        case render::PRIMITIVE_BLEND_OPAQUE:
        {
            preset = RSTATE_BLEND_PRESET_NONE;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_BLEND_ALPHA:
        {
            preset = RSTATE_BLEND_PRESET_ALPHA;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_BLEND_ADDITIVE:
        {
            preset = RSTATE_BLEND_PRESET_ADD;
            return TRUE;
        }
        break;
        
        case render::PRIMITIVE_BLEND_SUBTRACTIVE:
        {
            preset = RSTATE_BLEND_PRESET_SUB;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_BLEND_INTENSITY:
        {
            preset = RSTATE_BLEND_PRESET_INTENSITY;
            return TRUE;
        }
        break;

        default:
        {
            return FALSE;
        }
    }
}

//=============================================================================

xbool PrimitiveRenderer::TopologyFromMode( render::primitive_topology topology, shader_topology& result )
{
    switch ( topology )
    {
        case render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        {
            result = SHADER_TOPOLOGY_TRIANGLE_LIST;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_TOPOLOGY_LINE_LIST:
        {
            result = SHADER_TOPOLOGY_LINE_LIST;
            return TRUE;
        }
        break;

        default:
        {
            return FALSE;
        }
    }
}

//=============================================================================

xbool PrimitiveRenderer::DepthPresetFromMode( render::primitive_depth_mode depth, rstate_depth_preset& preset )
{
    switch ( depth )
    {
        case render::PRIMITIVE_DEPTH_DISABLED:
        {
            preset = RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_DEPTH_READ_ONLY:
        {
            preset = RSTATE_DEPTH_PRESET_NO_WRITE;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_DEPTH_READ_WRITE:
        {
            preset = RSTATE_DEPTH_PRESET_NORMAL;
            return TRUE;
        }
        break;

        default:
        {
            return FALSE;
        }
    }
}

//=============================================================================

xbool PrimitiveRenderer::RasterPresetFromMode( render::primitive_raster_mode raster, rstate_raster_preset& preset )
{
    switch ( raster )
    {
        case render::PRIMITIVE_RASTER_SOLID:
        {
            preset = RSTATE_RASTER_PRESET_SOLID;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_RASTER_SOLID_NO_CULL:
        {
            preset = RSTATE_RASTER_PRESET_SOLID_NO_CULL;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_RASTER_WIREFRAME:
        {
            preset = RSTATE_RASTER_PRESET_WIRE;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_RASTER_WIREFRAME_NO_CULL:
        {
            preset = RSTATE_RASTER_PRESET_WIRE_NO_CULL;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_RASTER_COLLISION_BIASED:
        {
            preset = RSTATE_RASTER_PRESET_COLLISION_BIASED;
            return TRUE;
        }
        break;

        default:
        {
            return FALSE;
        }
    }
}

//=============================================================================

xbool PrimitiveRenderer::SamplerPresetFromMode( render::primitive_sampler_mode sampler, rstate_sampler_preset& preset )
{
    switch ( sampler )
    {
        case render::PRIMITIVE_SAMPLER_LINEAR_WRAP:
        {
            preset = RSTATE_SAMPLER_PRESET_LINEAR_WRAP;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_SAMPLER_LINEAR_CLAMP:
        {
            preset = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_SAMPLER_ANISOTROPIC_WRAP:
        {
            preset = RSTATE_SAMPLER_PRESET_ANISOTROPIC_WRAP;
            return TRUE;
        }
        break;

        case render::PRIMITIVE_SAMPLER_ANISOTROPIC_CLAMP:
        {
            preset = RSTATE_SAMPLER_PRESET_ANISOTROPIC_CLAMP;
            return TRUE;
        }
        break;

        default:
        {
            return FALSE;
        }
    }
}

//=============================================================================

xbool PrimitiveRenderer::BuildBatchDesc( PrimitiveMgr::BatchDesc& out, render::primitive_draw_desc const& desc,
                                         matrix4 const& localToWorld ) const
{
    if ( !m_isSubmissionActive || ( desc.Topology < render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ) ||
         ( desc.Topology >= render::PRIMITIVE_TOPOLOGY_COUNT ) || ( desc.Blend < render::PRIMITIVE_BLEND_OPAQUE ) ||
         ( desc.Blend >= render::PRIMITIVE_BLEND_COUNT ) || ( desc.Depth < render::PRIMITIVE_DEPTH_DISABLED ) ||
         ( desc.Depth >= render::PRIMITIVE_DEPTH_COUNT ) || ( desc.Raster < render::PRIMITIVE_RASTER_SOLID ) ||
         ( desc.Raster >= render::PRIMITIVE_RASTER_COUNT ) ||
         ( desc.Sampler < render::PRIMITIVE_SAMPLER_LINEAR_WRAP ) ||
         ( desc.Sampler >= render::PRIMITIVE_SAMPLER_COUNT ) ||
         ( desc.Output < render::PRIMITIVE_OUTPUT_COLOR ) || ( desc.Output >= render::PRIMITIVE_OUTPUT_COUNT ) ||
         ( desc.Layer < render::PRIMITIVE_LAYER_SURFACE ) || ( desc.Layer >= render::PRIMITIVE_LAYER_COUNT ) ||
         ( ( desc.Output == render::PRIMITIVE_OUTPUT_DISTORTION ) !=
           ( desc.Layer == render::PRIMITIVE_LAYER_DISTORTION ) ) ||
         !x_isvalid( desc.DistortionScale ) || ( desc.DistortionScale < 0.0f ) )
    {
        return FALSE;
    }

    shader_resource const* pTexture = NULL;
    if ( desc.pTexture )
    {
        pTexture = desc.pTexture->GetShaderResource();
        if ( !pTexture || !*pTexture )
        {
            return FALSE;
        }
    }

    shader_topology       topology;
    rstate_blend_preset   blend;
    rstate_depth_preset   depth;
    rstate_raster_preset  raster;
    rstate_sampler_preset sampler;
    if ( !TopologyFromMode( desc.Topology, topology ) || !BlendPresetFromMode( desc.Blend, blend ) ||
         !DepthPresetFromMode( desc.Depth, depth ) || !RasterPresetFromMode( desc.Raster, raster ) ||
         !SamplerPresetFromMode( desc.Sampler, sampler ) )
    {
        return FALSE;
    }

    out                           = PrimitiveMgr::BatchDesc();
    out.Pipeline.Topology         = topology;
    out.Pipeline.Blend            = blend;
    out.Pipeline.Depth            = depth;
    out.Pipeline.Raster           = raster;
    out.Pipeline.Sampler          = sampler;
    out.Pipeline.pDebugName       = "PrimitivePipeline";
    out.pTexture                  = pTexture;
    out.Output                    = desc.Output;
    out.Layer                     = desc.Layer;
    out.Constants.Output          = static_cast<u32>( desc.Output );
    out.Constants.DistortionScale = desc.DistortionScale;

    return PrimitiveMgr::BuildDrawConstants( out.Constants, localToWorld );
}

//=============================================================================

xbool PrimitiveRenderer::ComputeSortDepth( f32& outDepth, matrix4 const& localToWorld,
                                           render::primitive_vertex const* pVertices, s32 nVertices )
{
    view const* pView = eng_GetView();
    if ( !pView || !pVertices || ( nVertices <= 0 ) )
    {
        return FALSE;
    }

    bbox bounds( pVertices[0].Position );
    for ( s32 i = 1; i < nVertices; ++i )
    {
        bounds += pVertices[i].Position;
    }

    vector3 const worldCenter = localToWorld * bounds.GetCenter();
    vector3 const viewCenter = pView->GetW2V() * worldCenter;
    outDepth = viewCenter.GetZ();
    return x_isvalid( outDepth );
}

//=============================================================================

xbool PrimitiveRenderer::SubmitPrimitives( render::primitive_draw_desc const& desc, matrix4 const& localToWorld,
                                           render::primitive_vertex const* pVertices, s32 nVertices,
                                           u16 const* pIndices, s32 nIndices )
{
    ASSERTS( m_isSubmissionActive, "Primitive geometry submitted outside primitive mode" );
    ASSERT( m_pPrimitives );

    s32 const indicesPerPrimitive = ( desc.Topology == render::PRIMITIVE_TOPOLOGY_LINE_LIST ) ? 2 : 3;
    if ( !m_isSubmissionActive || !m_pPrimitives || !pVertices || !pIndices || ( nVertices <= 0 ) ||
         ( nVertices > PrimitiveMgr::MAX_BATCH_VERTICES ) || ( nIndices <= 0 ) ||
         ( ( nIndices % indicesPerPrimitive ) != 0 ) )
    {
        return FALSE;
    }

    PrimitiveMgr::BatchDesc batchDesc;
    if ( !BuildBatchDesc( batchDesc, desc, localToWorld ) ||
         !ComputeSortDepth( batchDesc.SortDepth, localToWorld, pVertices, nVertices ) )
    {
        return FALSE;
    }

    if ( !m_pPrimitives->BeginBatch( batchDesc ) )
    {
        return FALSE;
    }

    xbool const submitted = m_pPrimitives->SubmitBatch( pVertices, nVertices, pIndices, nIndices );
    xbool const closed = m_pPrimitives->EndBatch();
    return submitted && closed;
}

//=============================================================================

xbool PrimitiveRenderer::SubmitDepthRect( irect const& rect, f32 depth )
{
    ASSERTS( m_isSubmissionActive, "Depth rectangle submitted outside primitive mode" );
    ASSERT( m_pPrimitives );

    view const* pView = eng_GetView();
    if ( !m_isSubmissionActive || !m_pPrimitives || !pView || !x_isvalid( depth ) || ( depth < 0.0f ) ||
         ( depth > 1.0f ) || ( rect.r <= rect.l ) || ( rect.b <= rect.t ) )
    {
        return FALSE;
    }

    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;
    pView->GetViewport( x0, y0, x1, y1 );
    f32 const width = static_cast<f32>( x1 - x0 );
    f32 const height = static_cast<f32>( y1 - y0 );
    if ( ( width <= 0.0f ) || ( height <= 0.0f ) )
    {
        return FALSE;
    }

    f32 const left = ( static_cast<f32>( rect.l - x0 ) / width ) * 2.0f - 1.0f;
    f32 const right = ( static_cast<f32>( rect.r - x0 ) / width ) * 2.0f - 1.0f;
    f32 const top = 1.0f - ( static_cast<f32>( rect.t - y0 ) / height ) * 2.0f;
    f32 const bottom = 1.0f - ( static_cast<f32>( rect.b - y0 ) / height ) * 2.0f;
    render::primitive_vertex const vertices[4] = {
        render::primitive_vertex( vector3( left, top, depth ), vector2( 0.0f, 0.0f ), XCOLOR_WHITE ),
        render::primitive_vertex( vector3( right, top, depth ), vector2( 1.0f, 0.0f ), XCOLOR_WHITE ),
        render::primitive_vertex( vector3( right, bottom, depth ), vector2( 1.0f, 1.0f ), XCOLOR_WHITE ),
        render::primitive_vertex( vector3( left, bottom, depth ), vector2( 0.0f, 1.0f ), XCOLOR_WHITE ) };
    u16 const indices[6] = { 0, 1, 2, 2, 3, 0 };

    PrimitiveMgr::BatchDesc batchDesc;
    batchDesc.Pipeline.Topology = SHADER_TOPOLOGY_TRIANGLE_LIST;
    batchDesc.Pipeline.Blend = RSTATE_BLEND_PRESET_COLOR_WRITE_DISABLE;
    batchDesc.Pipeline.Depth = RSTATE_DEPTH_PRESET_WRITE_ALWAYS;
    batchDesc.Pipeline.Raster = RSTATE_RASTER_PRESET_SOLID_NO_CULL;
    batchDesc.Pipeline.Sampler = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
    batchDesc.Pipeline.pDebugName = "PrimitiveDepthRectPipeline";
    batchDesc.Output = render::PRIMITIVE_OUTPUT_COLOR;
    batchDesc.Layer = render::PRIMITIVE_LAYER_SURFACE;
    batchDesc.Constants.Output = render::PRIMITIVE_OUTPUT_COLOR;
    batchDesc.Constants.LocalToClip.Identity();
    batchDesc.SortDepth = 0.0f;

    if ( !m_pPrimitives->BeginBatch( batchDesc ) )
    {
        return FALSE;
    }

    xbool const submitted =
        m_pPrimitives->SubmitBatch( vertices, ARRAYSIZE( vertices ), indices, ARRAYSIZE( indices ) );
    xbool const closed = m_pPrimitives->EndBatch();
    return submitted && closed;
}

//=============================================================================
