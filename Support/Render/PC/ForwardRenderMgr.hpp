//=============================================================================
//
//  ForwardRenderMgr.hpp
//
//=============================================================================

#ifndef FORWARD_RENDER_MGR_HPP
#define FORWARD_RENDER_MGR_HPP

//=============================================================================
//  INCLUDES
//=============================================================================

#include "FrameRenderContext.hpp"
#include "x_array.hpp"

//=============================================================================

class GeomMgr;
class PrimitiveMgr;
class DecalRenderer;

//=============================================================================

enum forward_render_phase
{
    FORWARD_PHASE_SURFACE = 0,
    FORWARD_PHASE_DECAL,
    FORWARD_PHASE_TRANSPARENT,
    FORWARD_PHASE_ADDITIVE,
    FORWARD_PHASE_FORCE_LAST,
    FORWARD_PHASE_ZPRIME,
    FORWARD_PHASE_FADING,
    FORWARD_PHASE_DISTORTION,
    FORWARD_PHASE_COUNT
};

//=============================================================================

class ForwardRenderMgr
{
public:
    ForwardRenderMgr( void );

    void Init               ( GeomMgr& geometry, PrimitiveMgr& primitives, DecalRenderer& decals );
    void Kill               ( void );
    void BeginFrame         ( void );
    void DiscardQueuedDraws ( void );

    xbool QueueGeometry      ( s32 packetIndex, forward_render_phase phase, f32 sortDepth, u32 sequence );
    xbool ExecutePreEffects  ( frame_render_targets const& targets );
    xbool ExecutePostEffects ( frame_render_targets const& targets );
    xbool Execute            ( frame_render_targets const& targets );

    xbool                  BeginScenePass         ( frame_render_targets const& targets ) const;
    xbool                  CaptureDistortionScene ( frame_render_targets const& targets );
    shader_resource const* GetDistortionScene     ( void ) const;

private:
    enum class DrawSource
    {
        Geometry,
        Decal,
        Primitive
    };

    struct DrawItem
    {
        DrawItem( void )
            : m_source( DrawSource::Geometry ), m_phase( FORWARD_PHASE_TRANSPARENT ), m_sortDepth( 0.0f ),
              m_sequence( 0 ), m_packetIndex( -1 )
        {
        }

        //-------------------------------------------------------------------------

        DrawSource           m_source;
        forward_render_phase m_phase;
        f32                  m_sortDepth;
        u32                  m_sequence;
        s32                  m_packetIndex;
    };

    static s32 CompareItems( void const* pA, void const* pB );

    xbool         QueueDecals            ( void );
    xbool         QueuePrimitives        ( void );
    xbool         Prepare                ( frame_render_targets const& targets );
    void          Sort                   ( void );
    xbool         ExecutePhaseRange      ( frame_render_targets const& targets, forward_render_phase firstPhase,
                                           forward_render_phase lastPhase );
    xbool         ExecuteGlowItems       ( frame_render_targets const& targets, forward_render_phase firstPhase,
                                           forward_render_phase lastPhase );
    xbool         ExecuteDistortionItems ( frame_render_targets const& targets );
    xbool         ExecuteItem            ( DrawItem const& item, frame_render_targets const& targets,
                                           xbool isGlowPass );
    void          Clear                  ( void );

    xbool         BeginColorDepthPass    ( rtarget const& color, rtarget const* pDepth ) const;
    xbool         EnsureDistortionScene  ( rtarget const& source );
    void          ReleaseDistortionScene ( void );

    GeomMgr&      Geometry               ( void ) const;
    DecalRenderer& Decals                ( void ) const;
    PrimitiveMgr& Primitives             ( void ) const;

    xarray<DrawItem>     m_items;
    u32                  m_nextSequence;
    xbool                m_isPrepared;
    frame_render_targets m_preparedTargets;

    rtarget              m_distortionScene;
    u32                  m_distortionWidth;
    u32                  m_distortionHeight;
    rtarget_format       m_distortionFormat;

    GeomMgr*             m_pGeometry;
    DecalRenderer*       m_pDecals;
    PrimitiveMgr*        m_pPrimitives;
};

//=============================================================================

extern ForwardRenderMgr g_ForwardRenderMgr;

//=============================================================================
#endif // FORWARD_RENDER_MGR_HPP
//=============================================================================
