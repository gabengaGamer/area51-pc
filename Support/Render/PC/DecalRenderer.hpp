//=============================================================================
//
//  DecalRenderer.hpp
//
//  Forward renderer for lit surface decals.
//
//=============================================================================

#ifndef DECAL_RENDERER_HPP
#define DECAL_RENDERER_HPP

//=============================================================================
//  INCLUDES
//=============================================================================

#include "../DecalBatch.hpp"
#include "../RenderPipelineCache.hpp"
#include "FrameRenderContext.hpp"
#include "GeomMgr/GeomMgr.hpp"
#include "RuntimeVertexMgr.hpp"

#include "e_Engine.hpp"
#include "e_RenderState.hpp"
#include "e_VRAM.hpp"

#include <stddef.h>

//=============================================================================

class cubemap;

//=============================================================================

class DecalRenderer
{
public:
    using Vertex = render::decal_vertex;

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

    struct DrawConstants
    {
        matrix4 WorldToClip;
        vector4 CameraPosition;
        vector4 GeometricNormal;
        vector4 EnvParams;
        u32     LightingIndex;
        u32     Flags;
        u32     BlendMode;
        u32     OutputMode;

        DrawConstants( void );
    };

    static_assert( sizeof( Vertex ) == 24, "Decal vertex layout must match HLSL" );
    static_assert( offsetof( Vertex, Position ) == 0, "Decal position offset must match PSO" );
    static_assert( offsetof( Vertex, Color ) == 12, "Decal color offset must match PSO" );
    static_assert( offsetof( Vertex, UV ) == 16, "Decal UV offset must match PSO" );
    static_assert( sizeof( DrawConstants ) == 128, "Decal constants must match HLSL" );

public:
    DecalRenderer ( void );

    void  Init        ( void );
    void  Kill        ( void );
    void  BeginRender ( void );
    xbool Reserve     ( s32 nVertices, s32 nIndices, s32 nDraws );

    xbool Submit ( render::decal_draw_desc const& desc, cubemap const* pCubeMap, Vertex const* pVertices,
                   s32 nVertices, u16 const* pIndices, s32 nIndices );

    xbool RequestProjectionTextures ( void ) const;
    xbool UploadQueuedDraws         ( GeomMgr const& geometry );
    xbool DrawQueuedDraw            ( s32 drawIndex, PassDesc const& pass );
    void  DiscardQueuedDraws        ( void );

    s32   GetQueuedDrawCount ( void ) const;
    xbool IsQueuedDrawGlow   ( s32 drawIndex ) const;

private:
    struct PipelineDesc
    {
        PassDesc             Pass;
        rstate_blend_preset  Blend;
        rstate_depth_preset  Depth;
        rstate_raster_preset Raster;
        char const*          pDebugName;

        PipelineDesc( void )
            : Pass(), Blend( RSTATE_BLEND_PRESET_ALPHA ), Depth( RSTATE_DEPTH_PRESET_NO_WRITE ),
              Raster( RSTATE_RASTER_PRESET_SOLID_NO_CULL ), pDebugName( NULL )
        {
        }
    };

    struct ShaderBindings
    {
        u32 DrawConstantsVertex;
        u32 DrawConstantsPixel;
        u32 ProjectionConstantsPixel;
        u32 DecalTexturePixel;
        u32 EnvironmentCubeTexturePixel;
        u32 FaceShadowTexturePixel;
        u32 FaceShadowDepthTexturePixel;
        u32 ProjectionAtlasTexturePixel;
        u32 LightingDataPixel;
        u32 ShadowMatricesPixel;
        u32 ShadowDataPixel;

        ShaderBindings( void );
    };

    struct QueuedDraw
    {
        DrawConstants             Constants;
        cb_geom_lighting          SourceLighting;
        shader_resource const*    pTexture;
        shader_resource const*    pEnvironmentCube;
        render::decal_blend_mode  Blend;
        u32                       Flags;
        s32                       StartIndex;
        s32                       BaseVertex;
        s32                       IndexCount;

        QueuedDraw( void );
    };

private:
    xbool LoadShaders       ( void );
    xbool ResolveBindings   ( void );
    xbool CreateSamplers    ( void );
    xbool CreateBlackCube   ( void );
    xbool PrewarmPipelines  ( void );
    void  DestroySamplers   ( void );
    void  DestroyPipelines  ( void );

    static xbool BuildSceneLighting  ( cb_geom_lighting& lighting, bbox const& bounds, xcolor ambient );
    static xbool BlendPresetFromMode ( render::decal_blend_mode blend, rstate_blend_preset& preset );
    xbool        CopyLightingData    ( GeomLightingConstants& destination, cb_geom_lighting const& source,
                                      GeomMgr const& geometry ) const;
    xbool        BuildProjectionData ( void );
    xbool        BindResources       ( QueuedDraw const& draw, s32 drawIndex, xbool isGlowPass );

    static xbool ValidatePass        ( PipelineDesc const& desc );
    static u64   MakePipelineKey     ( PipelineDesc const& desc );
    xbool        BuildPipelineDesc   ( render_pipeline_desc& out, PipelineDesc const& desc ) const;
    render_pipeline* GetOrCreatePipeline ( PipelineDesc const& desc, xbool isPrewarm = FALSE );
    xbool            BindPipeline        ( PipelineDesc const& desc );

private:
    RuntimeVertexMgr    m_vertexMgr;
    shader              m_vertexShader;
    shader              m_pixelShader;
    ShaderBindings      m_bindings;
    RenderPipelineCache m_pipelines;
    rstate_sampler      m_samplers[RSTATE_SAMPLER_PRESET_COUNT];
    vram_texture        m_blackCubeTexture;
    rbuffer             m_lightingBuffer;
    u32                 m_lightingCapacity;

    ProjectionTextureConstants   m_projectionConstants;
    xarray<GeomLightingConstants> m_frameLighting;
    xarray<Vertex>                m_queuedVertices;
    xarray<u16>                   m_queuedIndices;
    xarray<QueuedDraw>            m_queuedDraws;
    s32                           m_requestedVertices;
    s32                           m_requestedIndices;
    s32                           m_requestedDraws;
    xbool                         m_areQueuedDrawsUploaded;
    xbool                         m_isInitialized;
};

//=============================================================================

extern DecalRenderer g_DecalRenderer;

//=============================================================================
#endif // DECAL_RENDERER_HPP
//=============================================================================
