//=========================================================================
//
//  PrimitiveMgr.cpp
//
//=========================================================================

//=========================================================================
// INCLUDES
//=========================================================================

#include "PrimitiveMgr.hpp"

#include "x_debug.hpp"

//=========================================================================
// GLOBAL INSTANCE
//=========================================================================

PrimitiveMgr g_PrimitiveMgr;

//=========================================================================
// IMPLEMENTATION
//=========================================================================

PrimitiveMgr::PrimitiveMgr( void )
    : m_vertexMgr(), m_vertexShader(), m_pixelShader(), m_pipelines(), m_drawUniformVertexSlot( 0xffffffffu ),
      m_drawUniformPixelSlot( 0xffffffffu ), m_textureSlot( 0xffffffffu ), m_sceneTextureSlot( 0xffffffffu ),
      m_pDistortionScene( NULL ), m_whiteTexture(), m_isInitialized( FALSE ), m_batchDesc(), m_batchVertices(),
      m_batchIndices(), m_isBatchOpen( FALSE ), m_queuedVertices(), m_queuedIndices(), m_queuedDraws(),
      m_areQueuedDrawsUploaded( FALSE )
{
}

//=========================================================================

void PrimitiveMgr::Init( void )
{
    if ( m_isInitialized )
    {
        return;
    }

    if ( !m_vertexMgr.Init( sizeof( Vertex ) ) || !LoadShaders() || !CreateSamplers() || !CreateWhiteTexture() ||
         !PrewarmPipelines() )
    {
        Kill();
        return;
    }

    m_isInitialized = TRUE;
}

//=========================================================================

void PrimitiveMgr::Kill( void )
{
    ClearBatch();
    m_batchVertices.Clear();
    m_batchIndices.Clear();
    ClearQueuedDraws();
    m_queuedVertices.Clear();
    m_queuedIndices.Clear();
    m_queuedDraws.Clear();

    DestroyPipelines();
    DestroySamplers();
    vram_DestroyTexture( m_whiteTexture );

    shader_Destroy( m_pixelShader );
    shader_Destroy( m_vertexShader );

    m_vertexMgr.Kill();

    m_drawUniformVertexSlot = 0xffffffffu;
    m_drawUniformPixelSlot = 0xffffffffu;
    m_textureSlot = 0xffffffffu;
    m_sceneTextureSlot = 0xffffffffu;
    m_pDistortionScene = NULL;
    m_isInitialized = FALSE;
}

//=========================================================================

void PrimitiveMgr::BeginRender( void )
{
    ASSERTS( !m_isBatchOpen, "Primitive batch survived the previous frame" );
    ASSERTS( !HasQueuedDraws(), "Primitive draw queue survived the previous frame" );

    ClearBatch();
    ClearQueuedDraws();
}

//=========================================================================

xbool PrimitiveMgr::LoadShaders( void )
{
    shader_LoadFromEcs( m_vertexShader, "primitive_basic_vs.vs.ecs" );
    shader_LoadFromEcs( m_pixelShader, "primitive_basic_ps.ps.ecs" );

    if ( !m_vertexShader || !m_pixelShader )
    {
        x_DebugMsg( "PrimitiveMgr: failed to load primitive shaders\n" );
        return FALSE;
    }

    if ( !shader_FindUniformSlot( m_vertexShader, "cbPrimitiveDraw", m_drawUniformVertexSlot ) )
    {
        x_DebugMsg( "PrimitiveMgr: vertex draw constants binding not found\n" );
        return FALSE;
    }

    if ( !shader_FindUniformSlot( m_pixelShader, "cbPrimitiveDraw", m_drawUniformPixelSlot ) )
    {
        x_DebugMsg( "PrimitiveMgr: pixel draw constants binding not found\n" );
        return FALSE;
    }

    if ( !shader_FindSampledTextureSlot( m_pixelShader, "txPrimitive", m_textureSlot ) )
    {
        x_DebugMsg( "PrimitiveMgr: primitive texture binding not found\n" );
        return FALSE;
    }

    if ( !shader_FindSampledTextureSlot( m_pixelShader, "txPrimitiveScene", m_sceneTextureSlot ) )
    {
        x_DebugMsg( "PrimitiveMgr: primitive scene texture binding not found\n" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PrimitiveMgr::CreateSamplers( void )
{
    for ( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        if ( !rstate_CreateSampler( m_samplers[i], static_cast<rstate_sampler_preset>( i ), "PrimitiveSampler" ) )
        {
            DestroySamplers();
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void PrimitiveMgr::DestroySamplers( void )
{
    for ( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        rstate_DestroySampler( m_samplers[i] );
    }
}

//=========================================================================

xbool PrimitiveMgr::CreateWhiteTexture( void )
{
    vram_texture_desc desc;
    desc.Width = 1;
    desc.Height = 1;
    desc.Format = VRAM_TEXTURE_FORMAT_RGBA8;
    desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    desc.pDebugName = "PrimitiveWhiteTexture";

    if ( !vram_CreateTexture( m_whiteTexture, desc ) )
    {
        return FALSE;
    }

    u32 const whitePixel = 0xffffffffu;
    vram_texture_upload_desc upload;
    upload.Region.Width = 1;
    upload.Region.Height = 1;
    upload.Region.Depth = 1;
    upload.pData = &whitePixel;
    upload.Size = sizeof( whitePixel );
    upload.RowPitch = sizeof( whitePixel );
    upload.SlicePitch = sizeof( whitePixel );
    return vram_UploadTexture( m_whiteTexture, upload );
}

//=========================================================================

void PrimitiveMgr::DestroyPipelines( void )
{
    m_pipelines.Reset();
}

//=========================================================================

xbool PrimitiveMgr::PrewarmPipelines( void )
{
    static rstate_blend_preset const blendPresets[] = { RSTATE_BLEND_PRESET_ADD, RSTATE_BLEND_PRESET_SUB,
                                                        RSTATE_BLEND_PRESET_ALPHA, RSTATE_BLEND_PRESET_INTENSITY };

    PipelineDesc desc;
    desc.Pass.ColorFormat = RTARGET_FORMAT_RGBA8;
    desc.Pass.SampleCount = 1;
    desc.Topology         = SHADER_TOPOLOGY_TRIANGLE_LIST;
    desc.Raster           = RSTATE_RASTER_PRESET_SOLID_NO_CULL;
    desc.Sampler          = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
    desc.pDebugName       = "ForwardPrimitivePipeline";

    u32 pipelineCount = 0;
    for ( u32 blendIndex = 0; blendIndex < ARRAYSIZE( blendPresets ); ++blendIndex )
    {
        desc.Blend = blendPresets[blendIndex];

        desc.Pass.ColorFormat = RTARGET_FORMAT_RGBA8;
        desc.Pass.DepthFormat = RTARGET_FORMAT_DEPTH_STENCIL;
        desc.Depth = RSTATE_DEPTH_PRESET_NO_WRITE;
        if ( !GetOrCreatePipeline( desc, TRUE ) )
        {
            return FALSE;
        }
        pipelineCount++;

        desc.Depth = RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE;
        if ( !GetOrCreatePipeline( desc, TRUE ) )
        {
            return FALSE;
        }
        pipelineCount++;

        // Target overrides without a depth attachment are valid for primitives.
        desc.Pass.DepthFormat = RTARGET_FORMAT_COUNT;
        if ( !GetOrCreatePipeline( desc, TRUE ) )
        {
            return FALSE;
        }
        pipelineCount++;

        // Glow primitives are replayed by ForwardRenderMgr into the HDR glow
        // target instead of the RGBA8 scene target.
        desc.Pass.ColorFormat = RTARGET_FORMAT_RGBA16F;
        desc.Pass.DepthFormat = RTARGET_FORMAT_DEPTH_STENCIL;
        desc.Depth = RSTATE_DEPTH_PRESET_NO_WRITE;
        if ( !GetOrCreatePipeline( desc, TRUE ) )
        {
            return FALSE;
        }
        pipelineCount++;

        desc.Depth = RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE;
        if ( !GetOrCreatePipeline( desc, TRUE ) )
        {
            return FALSE;
        }
        pipelineCount++;
    }

    x_DebugMsg( "PrimitiveMgr: prewarmed %u graphics pipeline variants\n", pipelineCount );
    return TRUE;
}

//=========================================================================

u64 PrimitiveMgr::MakePipelineKey( PipelineDesc const& desc )
{
    return static_cast<u64>( static_cast<u8>( desc.Pass.ColorFormat ) ) |
           ( static_cast<u64>( static_cast<u8>( desc.Pass.DepthFormat ) ) << 8 ) |
           ( static_cast<u64>( static_cast<u8>( desc.Pass.SampleCount ) ) << 16 ) |
           ( static_cast<u64>( static_cast<u8>( desc.Topology ) ) << 24 ) |
           ( static_cast<u64>( static_cast<u8>( desc.Blend ) ) << 32 ) |
           ( static_cast<u64>( static_cast<u8>( desc.Depth ) ) << 40 ) |
           ( static_cast<u64>( static_cast<u8>( desc.Raster ) ) << 48 );
}

//=========================================================================

xbool PrimitiveMgr::ValidatePipelineDesc( PipelineDesc const& desc )
{
    if ( !ValidatePipelineState( desc ) )
    {
        return FALSE;
    }

    if ( ( static_cast<u32>( desc.Pass.ColorFormat ) >= static_cast<u32>( RTARGET_FORMAT_COUNT ) ) ||
         ( static_cast<u32>( desc.Pass.DepthFormat ) > static_cast<u32>( RTARGET_FORMAT_COUNT ) ) ||
         ( ( desc.Pass.SampleCount != 1 ) && ( desc.Pass.SampleCount != 2 ) && ( desc.Pass.SampleCount != 4 ) &&
           ( desc.Pass.SampleCount != 8 ) ) )
    {
        return FALSE;
    }

    if ( ( desc.Pass.ColorFormat == RTARGET_FORMAT_DEPTH_STENCIL ) ||
         ( desc.Pass.ColorFormat == RTARGET_FORMAT_DEPTH32F ) )
    {
        return FALSE;
    }

    if ( ( desc.Pass.DepthFormat != RTARGET_FORMAT_COUNT ) &&
         ( desc.Pass.DepthFormat != RTARGET_FORMAT_DEPTH_STENCIL ) &&
         ( desc.Pass.DepthFormat != RTARGET_FORMAT_DEPTH32F ) )
    {
        return FALSE;
    }

    if ( ( ( desc.Depth == RSTATE_DEPTH_PRESET_NORMAL ) || ( desc.Depth == RSTATE_DEPTH_PRESET_NO_WRITE ) ||
           ( desc.Depth == RSTATE_DEPTH_PRESET_WRITE_ALWAYS ) ) &&
         ( desc.Pass.DepthFormat == RTARGET_FORMAT_COUNT ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PrimitiveMgr::ValidatePipelineState( PipelineDesc const& desc )
{
    if ( ( desc.Topology <= SHADER_TOPOLOGY_UNDEFINED ) || ( desc.Topology > SHADER_TOPOLOGY_TRIANGLE_STRIP ) ||
         ( desc.Blend < 0 ) || ( desc.Blend >= RSTATE_BLEND_PRESET_COUNT ) || ( desc.Depth < 0 ) ||
         ( desc.Depth >= RSTATE_DEPTH_PRESET_COUNT ) || ( desc.Raster < 0 ) ||
         ( desc.Raster >= RSTATE_RASTER_PRESET_COUNT ) || ( desc.Sampler < 0 ) ||
         ( desc.Sampler >= RSTATE_SAMPLER_PRESET_COUNT ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PrimitiveMgr::BuildPipelineDesc( render_pipeline_desc& pipelineDesc, PipelineDesc const& desc ) const
{
    if ( !ValidatePipelineDesc( desc ) )
    {
        return FALSE;
    }

    static shader_vertex_buffer_desc const vertexBuffer = []()
    {
        shader_vertex_buffer_desc result;
        result.Slot = 0;
        result.Stride = sizeof( Vertex );
        return result;
    }();

    static shader_vertex_element const layout[] = {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( Vertex, Position ) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_UBYTE4N_BGRA, offsetof( Vertex, Color ) ),
        shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, offsetof( Vertex, UV ) ) };

    pipelineDesc                          = render_pipeline_desc();
    pipelineDesc.Shader.pVertexShader     = &m_vertexShader;
    pipelineDesc.Shader.pPixelShader      = &m_pixelShader;
    pipelineDesc.Shader.pVertexBuffers    = &vertexBuffer;
    pipelineDesc.Shader.VertexBufferCount = 1;
    pipelineDesc.Shader.pInputElements    = layout;
    pipelineDesc.Shader.InputElementCount = ARRAYSIZE( layout );
    pipelineDesc.Shader.Topology          = desc.Topology;
    pipelineDesc.Depth                    = rstate_GetDepthDesc( desc.Depth );
    pipelineDesc.Raster                   = rstate_GetRasterDesc( desc.Raster );
    pipelineDesc.ColorCount               = 1;
    pipelineDesc.DepthFormat              = desc.Pass.DepthFormat;
    pipelineDesc.SampleCount              = desc.Pass.SampleCount;
    pipelineDesc.pDebugName               = desc.pDebugName ? desc.pDebugName : "PrimitivePipeline";

    pipelineDesc.ColorTargets[0].Format = desc.Pass.ColorFormat;
    pipelineDesc.ColorTargets[0].Blend  = rstate_GetBlendDesc( desc.Blend );

    return TRUE;
}

//=========================================================================

render_pipeline* PrimitiveMgr::GetOrCreatePipeline( PipelineDesc const& desc, xbool isPrewarm )
{
    render_pipeline_desc pipelineDesc;
    if ( !BuildPipelineDesc( pipelineDesc, desc ) )
    {
        return NULL;
    }

    u64 const key = MakePipelineKey( desc );
    return isPrewarm ? m_pipelines.Prewarm( key, pipelineDesc ) : m_pipelines.GetOrCreate( key, pipelineDesc );
}

//=========================================================================

xbool PrimitiveMgr::BindPipeline( PipelineDesc const& desc )
{
    render_pipeline* pPipeline = GetOrCreatePipeline( desc );
    return pPipeline && render_BindPipeline( *pPipeline );
}

//=========================================================================

xbool PrimitiveMgr::BindResources( BatchDesc const& desc, s32 drawIndex )
{
    if ( !shader_PushUniformData( SHADER_STAGE_VERTEX, m_drawUniformVertexSlot, &desc.Constants,
                                  sizeof( desc.Constants ) ) )
    {
        x_DebugMsg( "PrimitiveMgr: failed to push vertex constants packet=%d output=%d\n",
                    drawIndex, static_cast<s32>( desc.Output ) );
        return FALSE;
    }

    if ( !shader_PushUniformData( SHADER_STAGE_PIXEL, m_drawUniformPixelSlot, &desc.Constants,
                                  sizeof( desc.Constants ) ) )
    {
        x_DebugMsg( "PrimitiveMgr: failed to push pixel constants packet=%d output=%d\n",
                    drawIndex, static_cast<s32>( desc.Output ) );
        return FALSE;
    }

    shader_resource const* pTexture = desc.pTexture;
    if ( !pTexture || !*pTexture )
    {
        pTexture = vram_GetShaderResource( m_whiteTexture );
        if ( !pTexture || !*pTexture )
        {
            x_DebugMsg( "PrimitiveMgr: missing texture and white fallback packet=%d output=%d\n",
                        drawIndex, static_cast<s32>( desc.Output ) );
            return FALSE;
        }
    }

    rstate_sampler_preset sampler = desc.Pipeline.Sampler;
    if ( ( sampler < 0 ) || ( sampler >= RSTATE_SAMPLER_PRESET_COUNT ) )
    {
        x_DebugMsg( "PrimitiveMgr: invalid sampler packet=%d sampler=%d output=%d\n",
                    drawIndex, static_cast<s32>( sampler ), static_cast<s32>( desc.Output ) );
        return FALSE;
    }

    if ( !m_samplers[sampler] )
    {
        x_DebugMsg( "PrimitiveMgr: unavailable sampler packet=%d sampler=%d output=%d\n",
                    drawIndex, static_cast<s32>( sampler ), static_cast<s32>( desc.Output ) );
        return FALSE;
    }

    shader_resource const* pScene = pTexture;
    if ( desc.Output == render::PRIMITIVE_OUTPUT_DISTORTION )
    {
        pScene = m_pDistortionScene;
        if ( !pScene || !*pScene )
        {
            x_DebugMsg( "PrimitiveMgr: missing distortion scene packet=%d\n", drawIndex );
            return FALSE;
        }
    }

    if ( !shader_BindSampler(
             shader_sampler_binding( SHADER_STAGE_PIXEL, m_textureSlot, pTexture, &m_samplers[sampler] ) ) )
    {
        x_DebugMsg( "PrimitiveMgr: failed to bind texture sampler packet=%d slot=%u output=%d\n",
                    drawIndex, m_textureSlot, static_cast<s32>( desc.Output ) );
        return FALSE;
    }
    if ( !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_sceneTextureSlot, pScene,
                                                      &m_samplers[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP] ) ) )
    {
        x_DebugMsg( "PrimitiveMgr: failed to bind scene sampler packet=%d slot=%u output=%d\n",
                    drawIndex, m_sceneTextureSlot, static_cast<s32>( desc.Output ) );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PrimitiveMgr::BeginBatch( BatchDesc const& desc )
{
    if ( !m_isInitialized || m_areQueuedDrawsUploaded ||
         ( desc.Output < render::PRIMITIVE_OUTPUT_COLOR ) || ( desc.Output >= render::PRIMITIVE_OUTPUT_COUNT ) ||
         ( desc.Layer < render::PRIMITIVE_LAYER_SURFACE ) || ( desc.Layer >= render::PRIMITIVE_LAYER_COUNT ) ||
         ( ( desc.Output == render::PRIMITIVE_OUTPUT_DISTORTION ) !=
           ( desc.Layer == render::PRIMITIVE_LAYER_DISTORTION ) ) ||
         ( desc.Constants.Output != static_cast<u32>( desc.Output ) ) || !x_isvalid( desc.SortDepth ) ||
         !ValidatePipelineState( desc.Pipeline ) )
    {
        return FALSE;
    }

    if ( m_isBatchOpen )
    {
        return FALSE;
    }

    m_batchDesc = desc;

    m_batchVertices.SetCount( 0 );
    m_batchIndices.SetCount( 0 );
    m_isBatchOpen = TRUE;
    return TRUE;
}

//=========================================================================

xbool PrimitiveMgr::SubmitBatch( Vertex const* pVertices, s32 nVertices, u16 const* pIndices, s32 nIndices )
{
    if ( !m_isBatchOpen )
    {
        return FALSE;
    }

    if ( !pVertices || !pIndices )
    {
        return FALSE;
    }

    if ( ( nVertices <= 0 ) || ( nIndices <= 0 ) )
    {
        return FALSE;
    }

    if ( nVertices > MAX_BATCH_VERTICES )
    {
        return FALSE;
    }

    for ( s32 i = 0; i < nIndices; i++ )
    {
        if ( pIndices[i] >= nVertices )
        {
            return FALSE;
        }
    }

    if ( ( m_batchVertices.GetCount() + nVertices ) > MAX_BATCH_VERTICES )
    {
        BatchDesc desc = m_batchDesc;
        if ( !EndBatch() )
        {
            return FALSE;
        }

        if ( !BeginBatch( desc ) )
        {
            return FALSE;
        }
    }

    s32 const baseVertex = m_batchVertices.GetCount();

    for ( s32 i = 0; i < nVertices; i++ )
    {
        m_batchVertices.Append() = pVertices[i];
    }

    for ( s32 i = 0; i < nIndices; i++ )
    {
        m_batchIndices.Append() = static_cast<u16>( baseVertex + pIndices[i] );
    }

    return TRUE;
}

//=========================================================================

xbool PrimitiveMgr::EndBatch( void )
{
    if ( !m_isBatchOpen )
    {
        return TRUE;
    }

    if ( ( m_batchVertices.GetCount() == 0 ) || ( m_batchIndices.GetCount() == 0 ) )
    {
        ClearBatch();
        return TRUE;
    }

    QueuedDraw& draw = m_queuedDraws.Append();
    draw.Desc        = m_batchDesc;
    draw.StartIndex  = m_queuedIndices.GetCount();
    draw.BaseVertex  = m_queuedVertices.GetCount();
    draw.IndexCount  = m_batchIndices.GetCount();

    m_queuedVertices.Append( m_batchVertices );
    m_queuedIndices.Append( m_batchIndices );

    m_areQueuedDrawsUploaded = FALSE;
    ClearBatch();
    return TRUE;
}

//=========================================================================

xbool PrimitiveMgr::UploadQueuedDraws( void )
{
    if ( m_areQueuedDrawsUploaded )
    {
        return FALSE;
    }

    if ( m_isBatchOpen )
    {
        return FALSE;
    }

    if ( !HasQueuedDraws() )
    {
        return TRUE;
    }

    RuntimeVertexMgr::primitive primitive;
    primitive.pVertex   = m_queuedVertices.GetPtr();
    primitive.pIndex    = m_queuedIndices.GetPtr();
    primitive.nVertices = m_queuedVertices.GetCount();
    primitive.nIndices  = m_queuedIndices.GetCount();

    m_areQueuedDrawsUploaded = m_vertexMgr.UploadPrimitive( primitive );
    return m_areQueuedDrawsUploaded;
}

//=========================================================================

render::primitive_render_layer PrimitiveMgr::GetQueuedDrawLayer( s32 drawIndex ) const
{
    ASSERT( ( drawIndex >= 0 ) && ( drawIndex < m_queuedDraws.GetCount() ) );
    return m_queuedDraws[drawIndex].Desc.Layer;
}

//=========================================================================

render::primitive_output_mode PrimitiveMgr::GetQueuedDrawOutput( s32 drawIndex ) const
{
    ASSERT( ( drawIndex >= 0 ) && ( drawIndex < m_queuedDraws.GetCount() ) );
    return m_queuedDraws[drawIndex].Desc.Output;
}

//=========================================================================

f32 PrimitiveMgr::GetQueuedDrawSortDepth( s32 drawIndex ) const
{
    ASSERT( ( drawIndex >= 0 ) && ( drawIndex < m_queuedDraws.GetCount() ) );
    return m_queuedDraws[drawIndex].Desc.SortDepth;
}

//=========================================================================

xbool PrimitiveMgr::DrawQueuedDraw( s32 drawIndex, PassDesc const& pass )
{
    if ( !m_areQueuedDrawsUploaded || ( drawIndex < 0 ) || ( drawIndex >= m_queuedDraws.GetCount() ) )
    {
        x_DebugMsg( "PrimitiveMgr: invalid queued draw packet=%d count=%d uploaded=%d pass=%s\n",
                    drawIndex, m_queuedDraws.GetCount(), m_areQueuedDrawsUploaded,
                    ( pass.IsGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }

    QueuedDraw const& draw = m_queuedDraws[drawIndex];
    BatchDesc         desc = draw.Desc;
    desc.Pipeline.Pass = pass;
    if ( pass.IsGlowPass )
    {
        desc.Pipeline.Blend = RSTATE_BLEND_PRESET_ADD;
    }
    if ( pass.DepthFormat == RTARGET_FORMAT_COUNT )
    {
        if ( desc.Pipeline.Depth == RSTATE_DEPTH_PRESET_WRITE_ALWAYS )
        {
            x_DebugMsg( "PrimitiveMgr: depth-writing packet has no depth target packet=%d output=%d pass=%s\n",
                        drawIndex, static_cast<s32>( desc.Output ), ( pass.IsGlowPass ? "glow" : "scene" ) );
            return FALSE;
        }

        desc.Pipeline.Depth = RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE;
    }

    if ( !m_vertexMgr.BindBuffers() )
    {
        x_DebugMsg( "PrimitiveMgr: failed to bind vertex/index buffers packet=%d output=%d pass=%s\n",
                    drawIndex, static_cast<s32>( desc.Output ), ( pass.IsGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !BindPipeline( desc.Pipeline ) )
    {
        x_DebugMsg(
            "PrimitiveMgr: failed to bind pipeline packet=%d output=%d color=%d depth=%d samples=%u blend=%d pass=%s\n",
            drawIndex, static_cast<s32>( desc.Output ), static_cast<s32>( desc.Pipeline.Pass.ColorFormat ),
            static_cast<s32>( desc.Pipeline.Pass.DepthFormat ), desc.Pipeline.Pass.SampleCount,
            static_cast<s32>( desc.Pipeline.Blend ), ( pass.IsGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !BindResources( desc, drawIndex ) )
    {
        return FALSE;
    }
    if ( !m_vertexMgr.DrawIndexed( draw.IndexCount, draw.StartIndex, draw.BaseVertex ) )
    {
        x_DebugMsg( "PrimitiveMgr: indexed draw failed packet=%d output=%d indices=%d start=%d base=%d pass=%s\n",
                    drawIndex, static_cast<s32>( desc.Output ), draw.IndexCount, draw.StartIndex, draw.BaseVertex,
                    ( pass.IsGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void PrimitiveMgr::SetDistortionScene( shader_resource const* pScene )
{
    m_pDistortionScene = pScene;
}

//=========================================================================

void PrimitiveMgr::DiscardQueuedDraws( void )
{
    ClearBatch();
    ClearQueuedDraws();
}

//=========================================================================

void PrimitiveMgr::ClearBatch( void )
{
    m_batchDesc = BatchDesc();
    m_batchVertices.SetCount( 0 );
    m_batchIndices.SetCount( 0 );
    m_isBatchOpen = FALSE;
}

//=========================================================================

void PrimitiveMgr::ClearQueuedDraws( void )
{
    m_queuedVertices.SetCount( 0 );
    m_queuedIndices.SetCount( 0 );
    m_queuedDraws.SetCount( 0 );
    m_pDistortionScene = NULL;
    m_areQueuedDrawsUploaded = FALSE;
}

//=========================================================================

xbool PrimitiveMgr::BuildDrawConstants( DrawConstants& out, matrix4 const& localToWorld )
{
    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    out.LocalToClip = pView->GetW2C() * localToWorld;
    return TRUE;
}

//=========================================================================
