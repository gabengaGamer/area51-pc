//=========================================================================
//
//  PrimitiveMgr.hpp
//
//=========================================================================

#ifndef PRIMITIVE_MANAGER_HPP
#define PRIMITIVE_MANAGER_HPP

//=========================================================================
// INCLUDES
//=========================================================================

#include "x_array.hpp"
#include "../../RenderPipelineCache.hpp"

#include <stddef.h>

#include "e_Engine.hpp"
#include "e_RenderState.hpp"
#include "e_VRAM.hpp"

#include "../../PrimitiveBatch.hpp"
#include "../FrameRenderContext.hpp"
#include "../RuntimeVertexMgr.hpp"

//=========================================================================
// CLASS
//=========================================================================

class PrimitiveMgr
{
public:
    enum
    {
        MAX_BATCH_VERTICES = render::MAX_PRIMITIVE_VERTICES
    };
    
    using Vertex = render::primitive_vertex;
    
    struct DrawConstants
    {
        matrix4 LocalToClip;
        u32     Output;
        f32     DistortionScale;
        f32     Padding[2];
    
        DrawConstants( void )
        {
            LocalToClip.Identity();
            Output = render::PRIMITIVE_OUTPUT_COLOR;
            DistortionScale = 0.0f;
            Padding[0] = 0.0f;
            Padding[1] = 0.0f;
        }
    };
    
    static_assert( sizeof( Vertex ) == 24, "Primitive Vertex layout must match HLSL" );
    static_assert( offsetof( Vertex, Position ) == 0, "Primitive position offset must match PSO" );
    static_assert( offsetof( Vertex, Color ) == 12, "Primitive color offset must match PSO" );
    static_assert( offsetof( Vertex, UV ) == 16, "Primitive UV offset must match PSO" );
    static_assert( sizeof( DrawConstants ) == 80, "Primitive constants must match HLSL" );
    
    struct PassDesc
    {
        rtarget_format ColorFormat;
        rtarget_format DepthFormat;
        u32            SampleCount;
        xbool          IsGlowPass;

        PassDesc( void )
            : ColorFormat( RTARGET_FORMAT_COUNT ), DepthFormat( RTARGET_FORMAT_COUNT ), SampleCount( 1 ),
              IsGlowPass( FALSE )
        {
        }
    };
    
    struct PipelineDesc
    {
        PassDesc              Pass;
        shader_topology       Topology;
        rstate_blend_preset   Blend;
        rstate_depth_preset   Depth;
        rstate_raster_preset  Raster;
        rstate_sampler_preset Sampler;
        char const*           pDebugName;
    
        PipelineDesc( void )
            : Pass(), Topology( SHADER_TOPOLOGY_TRIANGLE_LIST ), Blend( RSTATE_BLEND_PRESET_ALPHA ),
              Depth( RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE ), Raster( RSTATE_RASTER_PRESET_SOLID_NO_CULL ),
              Sampler( RSTATE_SAMPLER_PRESET_LINEAR_CLAMP ), pDebugName( NULL )
        {
        }
    };
    
    struct BatchDesc
    {
        PipelineDesc                   Pipeline;
        DrawConstants                  Constants;
        shader_resource const*         pTexture;
        render::primitive_output_mode  Output;
        render::primitive_render_layer Layer;
        f32                            SortDepth;
    
        BatchDesc( void )
            : Pipeline(), Constants(), pTexture( NULL ), Output( render::PRIMITIVE_OUTPUT_COLOR ),
              Layer( render::PRIMITIVE_LAYER_TRANSPARENT ), SortDepth( 0.0f )
        {
        }
    };

protected:
    struct QueuedDraw
    {
        BatchDesc Desc;
        s32       StartIndex;
        s32       BaseVertex;
        s32       IndexCount;
    
        QueuedDraw( void ) : Desc(), StartIndex( 0 ), BaseVertex( 0 ), IndexCount( 0 )
        {
        }
    };

public:
    PrimitiveMgr ( void );
    
    void Init        ( void );
    void Kill        ( void );
    void BeginRender ( void );
    
    xbool                          BeginBatch             ( BatchDesc const& desc );
    xbool                          SubmitBatch            ( Vertex const* pVertices, s32 nVertices, u16 const* pIndices, s32 nIndices );
    xbool                          EndBatch               ( void );
    xbool                          HasQueuedDraws         ( void ) const;
    xbool                          UploadQueuedDraws      ( void );
    s32                            GetQueuedDrawCount     ( void ) const;
    render::primitive_render_layer GetQueuedDrawLayer     ( s32 drawIndex ) const;
    render::primitive_output_mode  GetQueuedDrawOutput    ( s32 drawIndex ) const;
    f32                            GetQueuedDrawSortDepth ( s32 drawIndex ) const;
    xbool                          DrawQueuedDraw         ( s32 drawIndex, PassDesc const& pass );
    void                           SetDistortionScene     ( shader_resource const* pScene );
    void                           DiscardQueuedDraws     ( void );
    
    static xbool BuildDrawConstants ( DrawConstants& out, matrix4 const& localToWorld );

protected:
    xbool LoadShaders      ( void );
    xbool PrewarmPipelines ( void );
    void  DestroyPipelines ( void );
    xbool CreateSamplers   ( void );
    void  DestroySamplers  ( void );
    xbool CreateWhiteTexture( void );
    
    xbool            BindPipeline          ( PipelineDesc const& desc );
    render_pipeline* GetOrCreatePipeline   ( PipelineDesc const& desc, xbool isPrewarm = FALSE );
    xbool            BuildPipelineDesc     ( render_pipeline_desc& out, PipelineDesc const& desc ) const;
    xbool            BindResources         ( BatchDesc const& desc, s32 drawIndex );
    static xbool     ValidatePipelineState ( PipelineDesc const& desc );
    static xbool     ValidatePipelineDesc  ( PipelineDesc const& desc );
    
    static u64 MakePipelineKey  ( PipelineDesc const& desc );
    void       ClearBatch       ( void );
    void       ClearQueuedDraws ( void );

protected:
    RuntimeVertexMgr       m_vertexMgr;
    shader                 m_vertexShader;
    shader                 m_pixelShader;
    RenderPipelineCache    m_pipelines;
    rstate_sampler         m_samplers[RSTATE_SAMPLER_PRESET_COUNT];
    u32                    m_drawUniformVertexSlot;
    u32                    m_drawUniformPixelSlot;
    u32                    m_textureSlot;
    u32                    m_sceneTextureSlot;
    shader_resource const* m_pDistortionScene;
    vram_texture           m_whiteTexture;
    xbool                  m_isInitialized;
    
    BatchDesc      m_batchDesc;
    xarray<Vertex> m_batchVertices;
    xarray<u16>    m_batchIndices;
    xbool          m_isBatchOpen;
    
    xarray<Vertex>     m_queuedVertices;
    xarray<u16>        m_queuedIndices;
    // Ordering is owned by ForwardRenderMgr; this array only retains the
    // immutable upload ranges and draw state produced by each submission.
    xarray<QueuedDraw> m_queuedDraws;
    xbool              m_areQueuedDrawsUploaded;
};

//=========================================================================
// GLOBAL INSTANCE
//=========================================================================

extern PrimitiveMgr g_PrimitiveMgr;

//=========================================================================
// INLINE FUNCTIONS
//=========================================================================

inline xbool PrimitiveMgr::HasQueuedDraws( void ) const
{
    return m_queuedDraws.GetCount() > 0;
}

//=========================================================================

inline s32 PrimitiveMgr::GetQueuedDrawCount( void ) const
{
    return m_queuedDraws.GetCount();
}

//=========================================================================
#endif // PRIMITIVE_MANAGER_HPP
//=========================================================================
