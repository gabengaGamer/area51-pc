//=============================================================================
//
//  ForwardRenderMgr.cpp
//
//=============================================================================

#include "ForwardRenderMgr.hpp"

#include "DecalRenderer.hpp"
#include "GeomMgr/GeomMgr.hpp"
#include "PrimitiveMgr/PrimitiveMgr.hpp"

#include "e_Engine.hpp"
#include "x_debug.hpp"

//=============================================================================

ForwardRenderMgr g_ForwardRenderMgr;

//=============================================================================

ForwardRenderMgr::ForwardRenderMgr( void )
    : m_items(), m_nextSequence( 0 ), m_isPrepared( FALSE ), m_preparedTargets(), m_distortionScene(),
      m_distortionWidth( 0 ), m_distortionHeight( 0 ), m_distortionFormat( RTARGET_FORMAT_COUNT ), m_pGeometry( NULL ),
      m_pDecals( NULL ), m_pPrimitives( NULL )
{
}

//=============================================================================

void ForwardRenderMgr::Init( GeomMgr& geometry, PrimitiveMgr& primitives, DecalRenderer& decals )
{
    ASSERT( !m_pGeometry && !m_pDecals && !m_pPrimitives );
    m_pGeometry = &geometry;
    m_pDecals = &decals;
    m_pPrimitives = &primitives;

    Clear();
    ReleaseDistortionScene();
}

//=============================================================================

void ForwardRenderMgr::Kill( void )
{
    Clear();
    m_items.Clear();
    ReleaseDistortionScene();

    m_pGeometry = NULL;
    m_pDecals = NULL;
    m_pPrimitives = NULL;
}

//=============================================================================

void ForwardRenderMgr::BeginFrame( void )
{
    ASSERTS( m_items.GetCount() == 0, "Forward draw queue survived the previous frame" );
    ASSERTS( !m_isPrepared, "Prepared forward queue survived the previous frame" );
    Clear();
}

//=============================================================================

void ForwardRenderMgr::DiscardQueuedDraws( void )
{
    Geometry().ClearDistortionState();
    Primitives().SetDistortionScene( NULL );
    Geometry().ClearPackets();
    Decals().DiscardQueuedDraws();
    Primitives().DiscardQueuedDraws();
    Clear();
}

//=============================================================================

xbool ForwardRenderMgr::QueueGeometry( s32 packetIndex, forward_render_phase phase, f32 sortDepth, u32 sequence )
{
    if ( ( packetIndex < 0 ) || ( phase < FORWARD_PHASE_SURFACE ) || ( phase >= FORWARD_PHASE_COUNT ) ||
         !x_isvalid( sortDepth ) )
    {
        return FALSE;
    }

    DrawItem& item = m_items.Append();
    item.m_source = DrawSource::Geometry;
    item.m_phase = phase;
    item.m_sortDepth = sortDepth;
    item.m_sequence = sequence;
    item.m_packetIndex = packetIndex;
    m_nextSequence = MAX( m_nextSequence, sequence + 1 );
    return TRUE;
}

//=============================================================================

xbool ForwardRenderMgr::QueueDecals( void )
{
    DecalRenderer& decalDraws = Decals();
    for ( s32 i = 0; i < decalDraws.GetQueuedDrawCount(); ++i )
    {
        DrawItem& item = m_items.Append();
        item.m_source = DrawSource::Decal;
        item.m_phase = FORWARD_PHASE_DECAL;
        item.m_sortDepth = 0.0f;
        item.m_sequence = m_nextSequence++;
        item.m_packetIndex = i;
    }

    return TRUE;
}

//=============================================================================

xbool ForwardRenderMgr::QueuePrimitives( void )
{
    PrimitiveMgr& primitiveDraws = Primitives();
    for ( s32 i = 0; i < primitiveDraws.GetQueuedDrawCount(); ++i )
    {
        forward_render_phase phase;
        switch ( primitiveDraws.GetQueuedDrawLayer( i ) )
        {
            case render::PRIMITIVE_LAYER_SURFACE:
            {
                phase = FORWARD_PHASE_SURFACE;
            }
            break;

            case render::PRIMITIVE_LAYER_TRANSPARENT:
            {
                phase = FORWARD_PHASE_TRANSPARENT;
            }
            break;

            case render::PRIMITIVE_LAYER_ADDITIVE:
            {
                phase = FORWARD_PHASE_ADDITIVE;
            }
            break;

            case render::PRIMITIVE_LAYER_DISTORTION:
            {
                phase = FORWARD_PHASE_DISTORTION;
            }
            break;

            default:
            {
                return FALSE;
            }
        }

        DrawItem& item = m_items.Append();
        item.m_source = DrawSource::Primitive;
        item.m_phase = phase;
        item.m_sortDepth = primitiveDraws.GetQueuedDrawSortDepth( i );
        item.m_sequence = m_nextSequence++;
        item.m_packetIndex = i;
    }

    return TRUE;
}

//=============================================================================

s32 ForwardRenderMgr::CompareItems( void const* pA, void const* pB )
{
    DrawItem const& a = *static_cast<DrawItem const*>( pA );
    DrawItem const& b = *static_cast<DrawItem const*>( pB );

    if ( a.m_phase < b.m_phase )
    {
        return -1;
    }
    if ( a.m_phase > b.m_phase )
    {
        return 1;
    }

    if ( ( a.m_phase == FORWARD_PHASE_TRANSPARENT ) || ( a.m_phase == FORWARD_PHASE_FADING ) ||
         ( a.m_phase == FORWARD_PHASE_DISTORTION ) )
    {
        if ( a.m_sortDepth > b.m_sortDepth )
        {
            return -1;
        }
        if ( a.m_sortDepth < b.m_sortDepth )
        {
            return 1;
        }
    }

    if ( a.m_sequence < b.m_sequence )
    {
        return -1;
    }
    if ( a.m_sequence > b.m_sequence )
    {
        return 1;
    }
    return 0;
}

//=============================================================================

void ForwardRenderMgr::Sort( void )
{
    if ( m_items.GetCount() > 1 )
    {
        x_qsort( m_items.GetPtr(), m_items.GetCount(), sizeof( DrawItem ), CompareItems );
    }
}

//=============================================================================

xbool ForwardRenderMgr::BeginColorDepthPass( rtarget const& colorTarget, rtarget const* pDepthTarget ) const
{
    if ( !rtarget_HasRenderTarget( colorTarget ) )
    {
        x_DebugMsg( "ForwardRenderMgr: color target is not renderable name=%s format=%d size=%ux%u samples=%u\n",
                    colorTarget.Desc.pDebugName ? colorTarget.Desc.pDebugName : "<unnamed>",
                    static_cast<s32>( colorTarget.Desc.Format ), colorTarget.Desc.Width, colorTarget.Desc.Height,
                    colorTarget.Desc.SampleCount );
        return FALSE;
    }

    rtarget_color_attachment_desc color;
    color.pTarget = &colorTarget;
    color.LoadOp  = RTARGET_LOAD_LOAD;
    color.StoreOp = RTARGET_STORE_STORE;

    rtarget_depth_attachment_desc        depth;
    rtarget_depth_attachment_desc const* pDepth = NULL;
    if ( pDepthTarget )
    {
        if ( !rtarget_HasDepthStencil( *pDepthTarget ) )
        {
            x_DebugMsg( "ForwardRenderMgr: depth target is not usable name=%s format=%d size=%ux%u samples=%u\n",
                        pDepthTarget->Desc.pDebugName ? pDepthTarget->Desc.pDebugName : "<unnamed>",
                        static_cast<s32>( pDepthTarget->Desc.Format ), pDepthTarget->Desc.Width,
                        pDepthTarget->Desc.Height, pDepthTarget->Desc.SampleCount );
            return FALSE;
        }

        depth.pTarget        = pDepthTarget;
        depth.DepthLoadOp    = RTARGET_LOAD_LOAD;
        depth.DepthStoreOp   = RTARGET_STORE_STORE;
        depth.StencilLoadOp  = RTARGET_LOAD_LOAD;
        depth.StencilStoreOp = RTARGET_STORE_STORE;
        pDepth = &depth;
    }

    rtarget_EndPass();
    if ( !rtarget_BeginPass( &color, 1, pDepth ) )
    {
        x_DebugMsg( "ForwardRenderMgr: failed to begin color/depth pass color=%s depth=%s\n",
                    colorTarget.Desc.pDebugName ? colorTarget.Desc.pDebugName : "<unnamed>",
                    ( pDepthTarget && pDepthTarget->Desc.pDebugName ) ? pDepthTarget->Desc.pDebugName : "<none>" );
        return FALSE;
    }

    view const* pView = eng_GetView();
    if ( pView )
    {
        eng_SetViewport( *pView );
    }

    return TRUE;
}

//=============================================================================

xbool ForwardRenderMgr::BeginScenePass( frame_render_targets const& targets ) const
{
    if ( !targets.pSceneColor )
    {
        x_DebugMsg( "ForwardRenderMgr: scene color target is missing\n" );
        return FALSE;
    }

    if ( !BeginColorDepthPass( *targets.pSceneColor, targets.pSceneDepth ) )
    {
        x_DebugMsg( "ForwardRenderMgr: failed to begin scene pass\n" );
        return FALSE;
    }

    return TRUE;
}

//=============================================================================

xbool ForwardRenderMgr::ExecuteItem( DrawItem const& item, frame_render_targets const& targets, xbool isGlowPass )
{
    if ( item.m_source == DrawSource::Geometry )
    {
        geom_pass_desc pass;
        pass.TargetWidth  = targets.pSceneColor->Desc.Width;
        pass.TargetHeight = targets.pSceneColor->Desc.Height;

        Geometry().InvalidateCache();
        if ( !Geometry().ExecutePacket( item.m_packetIndex, pass ) )
        {
            x_DebugMsg( "ForwardRenderMgr: geometry draw failed packet=%d phase=%d pass=%s\n",
                        item.m_packetIndex, static_cast<s32>( item.m_phase ), isGlowPass ? "glow" : "scene" );
            return FALSE;
        }
        return TRUE;
    }

    if ( item.m_source == DrawSource::Decal )
    {
        DecalRenderer::PassDesc pass;
        pass.ColorFormat = targets.pSceneColor->Desc.Format;
        pass.DepthFormat = targets.pSceneDepth ? targets.pSceneDepth->Desc.Format : RTARGET_FORMAT_COUNT;
        pass.SampleCount = targets.pSceneColor->Desc.SampleCount ? targets.pSceneColor->Desc.SampleCount : 1;
        pass.IsGlowPass = isGlowPass;
        if ( !Decals().DrawQueuedDraw( item.m_packetIndex, pass ) )
        {
            x_DebugMsg( "ForwardRenderMgr: decal draw failed packet=%d phase=%d pass=%s\n",
                        item.m_packetIndex, static_cast<s32>( item.m_phase ), isGlowPass ? "glow" : "scene" );
            return FALSE;
        }
        return TRUE;
    }

    PrimitiveMgr::PassDesc pass;
    pass.ColorFormat = targets.pSceneColor->Desc.Format;
    pass.DepthFormat = targets.pSceneDepth ? targets.pSceneDepth->Desc.Format : RTARGET_FORMAT_COUNT;
    pass.SampleCount = targets.pSceneColor->Desc.SampleCount ? targets.pSceneColor->Desc.SampleCount : 1;
    pass.IsGlowPass = isGlowPass;

    if ( !Primitives().DrawQueuedDraw( item.m_packetIndex, pass ) )
    {
        x_DebugMsg( "ForwardRenderMgr: primitive draw failed packet=%d phase=%d pass=%s\n",
                    item.m_packetIndex, static_cast<s32>( item.m_phase ), isGlowPass ? "glow" : "scene" );
        return FALSE;
    }
    return TRUE;
}

//=============================================================================

xbool ForwardRenderMgr::ExecutePhaseRange( frame_render_targets const& targets, forward_render_phase firstPhase,
                                           forward_render_phase lastPhase )
{
    for ( s32 i = 0; i < m_items.GetCount(); ++i )
    {
        DrawItem const& item = m_items[i];
        if ( ( item.m_phase < firstPhase ) || ( item.m_phase > lastPhase ) )
        {
            continue;
        }

        if ( !ExecuteItem( item, targets, FALSE ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=============================================================================

xbool ForwardRenderMgr::ExecuteGlowItems( frame_render_targets const& targets, forward_render_phase firstPhase,
                                          forward_render_phase lastPhase )
{
    xbool hasGlow = FALSE;
    for ( s32 i = 0; i < m_items.GetCount(); ++i )
    {
        DrawItem const& item = m_items[i];
        xbool const isPrimitiveGlow =
            ( item.m_source == DrawSource::Primitive ) &&
            ( Primitives().GetQueuedDrawOutput( item.m_packetIndex ) == render::PRIMITIVE_OUTPUT_GLOW );
        xbool const isDecalGlow =
            ( item.m_source == DrawSource::Decal ) && Decals().IsQueuedDrawGlow( item.m_packetIndex );
        if ( ( item.m_phase >= firstPhase ) && ( item.m_phase <= lastPhase ) &&
             ( isPrimitiveGlow || isDecalGlow ) )
        {
            hasGlow = TRUE;
            break;
        }
    }

    if ( !hasGlow )
    {
        return TRUE;
    }

    if ( targets.IsTargetOverride )
    {
        return TRUE;
    }

    if ( !targets.pGlow || !rtarget_HasRenderTarget( *targets.pGlow ) )
    {
        x_DebugMsg( "ForwardRenderMgr: main glow target is missing or not renderable\n" );
        if ( !BeginScenePass( targets ) )
        {
            x_DebugMsg( "ForwardRenderMgr: scene pass restore failed after invalid glow target\n" );
        }
        return FALSE;
    }

    if ( ( targets.pGlow->Desc.Width != targets.pSceneColor->Desc.Width ) ||
         ( targets.pGlow->Desc.Height != targets.pSceneColor->Desc.Height ) ||
         ( targets.pGlow->Desc.SampleCount != targets.pSceneColor->Desc.SampleCount ) )
    {
        x_DebugMsg(
            "ForwardRenderMgr: glow target mismatch glow=%ux%u/%u scene=%ux%u/%u\n",
            targets.pGlow->Desc.Width, targets.pGlow->Desc.Height, targets.pGlow->Desc.SampleCount,
            targets.pSceneColor->Desc.Width, targets.pSceneColor->Desc.Height, targets.pSceneColor->Desc.SampleCount );
        if ( !BeginScenePass( targets ) )
        {
            x_DebugMsg( "ForwardRenderMgr: scene pass restore failed after glow target mismatch\n" );
        }
        return FALSE;
    }

    if ( !BeginColorDepthPass( *targets.pGlow, targets.pSceneDepth ) )
    {
        x_DebugMsg( "ForwardRenderMgr: failed to begin glow replay pass\n" );
        if ( !BeginScenePass( targets ) )
        {
            x_DebugMsg( "ForwardRenderMgr: scene pass restore failed after glow pass begin failure\n" );
        }
        return FALSE;
    }

    frame_render_targets glowTargets = targets;
    glowTargets.pSceneColor = targets.pGlow;

    xbool drawResult = TRUE;
    for ( s32 i = 0; i < m_items.GetCount(); ++i )
    {
        DrawItem const& item = m_items[i];
        xbool const isPrimitiveGlow =
            ( item.m_source == DrawSource::Primitive ) &&
            ( Primitives().GetQueuedDrawOutput( item.m_packetIndex ) == render::PRIMITIVE_OUTPUT_GLOW );
        xbool const isDecalGlow =
            ( item.m_source == DrawSource::Decal ) && Decals().IsQueuedDrawGlow( item.m_packetIndex );
        if ( ( item.m_phase >= firstPhase ) && ( item.m_phase <= lastPhase ) &&
             ( isPrimitiveGlow || isDecalGlow ) )
        {
            if ( !ExecuteItem( item, glowTargets, TRUE ) )
            {
                drawResult = FALSE;
                break;
            }
        }
    }

    xbool const restoreResult = BeginScenePass( targets );
    if ( !restoreResult )
    {
        x_DebugMsg( "ForwardRenderMgr: scene pass restore failed after glow replay\n" );
    }

    return drawResult && restoreResult;
}

//=============================================================================

xbool ForwardRenderMgr::EnsureDistortionScene( rtarget const& source )
{
    if ( !rtarget_HasTexture( source ) )
    {
        return FALSE;
    }

    if ( !rtarget_HasShaderResource( m_distortionScene ) || ( m_distortionWidth != source.Desc.Width ) ||
         ( m_distortionHeight != source.Desc.Height ) || ( m_distortionFormat != source.Desc.Format ) )
    {
        ReleaseDistortionScene();

        rtarget_desc desc;
        desc.Width          = source.Desc.Width;
        desc.Height         = source.Desc.Height;
        desc.Format         = source.Desc.Format;
        desc.SampleCount    = 1;
        desc.SampleQuality  = 0;
        desc.bBindAsTexture = TRUE;

        if ( !rtarget_Create( m_distortionScene, desc ) )
        {
            ReleaseDistortionScene();
            return FALSE;
        }

        m_distortionWidth  = source.Desc.Width;
        m_distortionHeight = source.Desc.Height;
        m_distortionFormat = source.Desc.Format;
    }

    return TRUE;
}

//=============================================================================

void ForwardRenderMgr::ReleaseDistortionScene( void )
{
    rtarget_Destroy( m_distortionScene );
    m_distortionWidth  = 0;
    m_distortionHeight = 0;
    m_distortionFormat = RTARGET_FORMAT_COUNT;
}

//=============================================================================

xbool ForwardRenderMgr::CaptureDistortionScene( frame_render_targets const& targets )
{
    if ( !targets.pSceneColor )
    {
        return FALSE;
    }

    rtarget_EndPass();
    return EnsureDistortionScene( *targets.pSceneColor ) && rtarget_Copy( m_distortionScene, *targets.pSceneColor );
}

//=============================================================================

shader_resource const* ForwardRenderMgr::GetDistortionScene( void ) const
{
    return rtarget_GetShaderResource( m_distortionScene );
}

//=============================================================================

xbool ForwardRenderMgr::ExecuteDistortionItems( frame_render_targets const& targets )
{
    xbool hasDistortion = FALSE;
    for ( s32 i = 0; i < m_items.GetCount(); ++i )
    {
        if ( m_items[i].m_phase == FORWARD_PHASE_DISTORTION )
        {
            hasDistortion = TRUE;
            break;
        }
    }

    if ( !hasDistortion )
    {
        return TRUE;
    }

    if ( !CaptureDistortionScene( targets ) )
    {
        return FALSE;
    }

    shader_resource const* pScene = GetDistortionScene();
    if ( !pScene )
    {
        return FALSE;
    }

    Primitives().SetDistortionScene( pScene );
    Geometry().SetDistortionScene( pScene );
    if ( !BeginScenePass( targets ) )
    {
        Geometry().ClearDistortionState();
        return FALSE;
    }

    xbool result = TRUE;
    for ( s32 i = 0; result && ( i < m_items.GetCount() ); ++i )
    {
        DrawItem const& item = m_items[i];
        if ( item.m_phase == FORWARD_PHASE_DISTORTION )
        {
            result = ExecuteItem( item, targets, FALSE );
        }
    }

    Geometry().ClearDistortionState();
    return result;
}

//=============================================================================

xbool ForwardRenderMgr::Prepare( frame_render_targets const& targets )
{
    if ( m_isPrepared )
    {
        return FALSE;
    }

    if ( !targets.pSceneColor || !rtarget_HasRenderTarget( *targets.pSceneColor ) ||
         ( targets.pSceneDepth && !rtarget_HasDepthStencil( *targets.pSceneDepth ) ) )
    {
        DiscardQueuedDraws();
        return FALSE;
    }

    PrimitiveMgr& primitiveDraws = Primitives();
    if ( ( Decals().GetQueuedDrawCount() > 0 ) && !QueueDecals() )
    {
        DiscardQueuedDraws();
        return FALSE;
    }

    xbool const   hasPrimitives = primitiveDraws.HasQueuedDraws();
    if ( hasPrimitives )
    {
        rtarget_EndPass();
        if ( !primitiveDraws.UploadQueuedDraws() || !QueuePrimitives() )
        {
            DiscardQueuedDraws();
            BeginScenePass( targets );
            return FALSE;
        }
    }

    Sort();
    m_isPrepared = TRUE;
    m_preparedTargets = targets;
    return TRUE;
}

//=============================================================================

xbool ForwardRenderMgr::ExecutePreEffects( frame_render_targets const& targets )
{
    if ( !Prepare( targets ) )
    {
        return FALSE;
    }

    xbool const result = BeginScenePass( targets ) &&
                         ExecutePhaseRange( targets, FORWARD_PHASE_SURFACE, FORWARD_PHASE_DECAL ) &&
                         ExecuteGlowItems( targets, FORWARD_PHASE_SURFACE, FORWARD_PHASE_DECAL );

    if ( !result )
    {
        DiscardQueuedDraws();
        BeginScenePass( targets );
    }

    return result;
}

//=============================================================================

xbool ForwardRenderMgr::ExecutePostEffects( frame_render_targets const& targets )
{
    if ( !m_isPrepared || ( targets.pSceneColor != m_preparedTargets.pSceneColor ) ||
         ( targets.pSceneDepth != m_preparedTargets.pSceneDepth ) || ( targets.pGlow != m_preparedTargets.pGlow ) ||
         ( targets.IsTargetOverride != m_preparedTargets.IsTargetOverride ) )
    {
        DiscardQueuedDraws();
        return FALSE;
    }

    xbool const result =
        BeginScenePass( targets ) && ExecutePhaseRange( targets, FORWARD_PHASE_TRANSPARENT, FORWARD_PHASE_FADING ) &&
        ExecuteGlowItems( targets, FORWARD_PHASE_TRANSPARENT, FORWARD_PHASE_FADING ) && ExecuteDistortionItems( targets );

    DiscardQueuedDraws();

    if ( !result )
    {
        BeginScenePass( targets );
    }

    return result;
}

//=============================================================================

xbool ForwardRenderMgr::Execute( frame_render_targets const& targets )
{
    if ( !Prepare( targets ) )
    {
        return FALSE;
    }

    xbool const result =
        BeginScenePass( targets ) && ExecutePhaseRange( targets, FORWARD_PHASE_SURFACE, FORWARD_PHASE_TRANSPARENT ) &&
        ExecuteGlowItems( targets, FORWARD_PHASE_SURFACE, FORWARD_PHASE_TRANSPARENT ) &&
        ExecutePhaseRange( targets, FORWARD_PHASE_ADDITIVE, FORWARD_PHASE_FADING ) &&
        ExecuteGlowItems( targets, FORWARD_PHASE_ADDITIVE, FORWARD_PHASE_FADING ) &&
        ExecuteDistortionItems( targets );

    DiscardQueuedDraws();

    if ( !result )
    {
        BeginScenePass( targets );
    }

    return result;
}

//=============================================================================

void ForwardRenderMgr::Clear( void )
{
    m_items.SetCount( 0 );
    m_nextSequence = 0;
    m_isPrepared = FALSE;
    m_preparedTargets = frame_render_targets();
}

//=============================================================================

GeomMgr& ForwardRenderMgr::Geometry( void ) const
{
    ASSERT( m_pGeometry );
    return *m_pGeometry;
}

//=============================================================================

DecalRenderer& ForwardRenderMgr::Decals( void ) const
{
    ASSERT( m_pDecals );
    return *m_pDecals;
}

//=============================================================================

PrimitiveMgr& ForwardRenderMgr::Primitives( void ) const
{
    ASSERT( m_pPrimitives );
    return *m_pPrimitives;
}

//=============================================================================
