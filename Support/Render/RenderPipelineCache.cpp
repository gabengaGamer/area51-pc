//=============================================================================
//
//  RenderPipelineCache.cpp
//
//=============================================================================

//=============================================================================
//  INCLUDES
//=============================================================================

#include "RenderPipelineCache.hpp"

#include "x_debug.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

RenderPipelineCache::Entry::Entry() : m_key( 0 ), m_pipeline(), m_isPrewarmed( FALSE ), m_isUsed( FALSE )
{
}

//=========================================================================

RenderPipelineCache::RenderPipelineCache()
    : m_pDebugName( NULL ), m_entries(), m_numPrewarmed( 0 ), m_numRuntimeHits( 0 ), m_numRuntimeMisses( 0 )
{
}

//=========================================================================

RenderPipelineCache::~RenderPipelineCache()
{
    Reset();
}

//=========================================================================

RenderPipelineCache::Entry* RenderPipelineCache::FindEntry( u64 key ) const
{
    for ( s32 i = 0; i < m_entries.GetCount(); ++i )
    {
        Entry* pEntry = m_entries[i];
        if ( pEntry && pEntry->m_key == key )
        {
            return pEntry;
        }
    }

    return NULL;
}

//=========================================================================

render_pipeline* RenderPipelineCache::Find( u64 key )
{
    Entry* pEntry = FindEntry( key );
    if ( !pEntry )
    {
        return NULL;
    }

    ++m_numRuntimeHits;
    pEntry->m_isUsed = TRUE;
    return &pEntry->m_pipeline;
}

//=========================================================================

render_pipeline* RenderPipelineCache::Prewarm( u64 key, render_pipeline_desc const& desc )
{
    return FindOrCreate( key, desc, TRUE );
}

//=========================================================================

render_pipeline* RenderPipelineCache::GetOrCreate( u64 key, render_pipeline_desc const& desc )
{
    return FindOrCreate( key, desc, FALSE );
}

//=========================================================================

render_pipeline* RenderPipelineCache::FindOrCreate( u64 key, render_pipeline_desc const& desc, xbool isPrewarm )
{
    Entry* pEntry = FindEntry( key );
    if ( pEntry )
    {
        if ( isPrewarm )
        {
            if ( !pEntry->m_isPrewarmed )
            {
                x_DebugMsg( "RenderPipelineCache: late prewarm cache=%s key=%llu pipeline=%s\n",
                            m_pDebugName ? m_pDebugName : "Unnamed", static_cast<unsigned long long>( key ),
                            desc.pDebugName ? desc.pDebugName : "Unnamed" );
            }
        }
        else
        {
            ++m_numRuntimeHits;
            pEntry->m_isUsed = TRUE;
        }

        return &pEntry->m_pipeline;
    }

    pEntry = new Entry;
    if ( !pEntry )
    {
        return NULL;
    }

    pEntry->m_key = key;
    pEntry->m_isPrewarmed = isPrewarm;
    pEntry->m_isUsed = !isPrewarm;

    if ( !render_CreatePipeline( pEntry->m_pipeline, desc ) )
    {
        delete pEntry;
        return NULL;
    }

    if ( !m_pDebugName )
    {
        m_pDebugName = desc.pDebugName ? desc.pDebugName : "Unnamed";
    }

    m_entries.Append() = pEntry;
    if ( isPrewarm )
    {
        ++m_numPrewarmed;
    }
    else
    {
        ++m_numRuntimeMisses;

        render_color_target_desc const* pColorTarget = desc.ColorCount ? &desc.ColorTargets[0] : NULL;

        x_DebugMsg( "RenderPipelineCache: runtime miss cache=%s key=%llu pipeline=%s "
                    "colors=%u color0=%d depth=%d samples=%u blend=%d "
                    "depthTest=%d depthWrite=%d cull=%d\n",
                    m_pDebugName, static_cast<unsigned long long>( key ), desc.pDebugName ? desc.pDebugName : "Unnamed",
                    desc.ColorCount,
                    pColorTarget ? static_cast<s32>( pColorTarget->Format ) : static_cast<s32>( RTARGET_FORMAT_COUNT ),
                    static_cast<s32>( desc.DepthFormat ), desc.SampleCount,
                    pColorTarget ? pColorTarget->Blend.bBlendEnable : FALSE, desc.Depth.bDepthEnable,
                    desc.Depth.bDepthWrite, static_cast<s32>( desc.Raster.CullMode ) );
    }

    return &pEntry->m_pipeline;
}

//=========================================================================

void RenderPipelineCache::Reset()
{
    u32 numUnused = 0;
    for ( s32 i = 0; i < m_entries.GetCount(); ++i )
    {
        Entry* pEntry = m_entries[i];
        if ( !pEntry )
        {
            continue;
        }

        if ( pEntry->m_isPrewarmed && !pEntry->m_isUsed )
        {
            ++numUnused;
        }

        render_DestroyPipeline( pEntry->m_pipeline );
        delete pEntry;
    }

    if ( m_numRuntimeMisses || numUnused )
    {
        x_DebugMsg( "RenderPipelineCache: cache=%s prewarmed=%u hits=%u misses=%u unused=%u\n",
                    m_pDebugName ? m_pDebugName : "Unnamed", m_numPrewarmed, m_numRuntimeHits, m_numRuntimeMisses,
                    numUnused );
    }

    m_pDebugName = NULL;
    m_numPrewarmed = 0;
    m_numRuntimeHits = 0;
    m_numRuntimeMisses = 0;
    m_entries.Clear();
}