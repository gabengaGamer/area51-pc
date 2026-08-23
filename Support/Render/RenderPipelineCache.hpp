//=============================================================================
//
//  RenderPipelineCache.hpp
//
//=============================================================================

#ifndef RENDER_PIPELINE_CACHE_HPP
#define RENDER_PIPELINE_CACHE_HPP

//=============================================================================
//  INCLUDES
//=============================================================================

#include "e_RenderState.hpp"
#include "x_array.hpp"

//=============================================================================
//  RENDER PIPELINE CACHE
//=============================================================================

class RenderPipelineCache
{
public:
    RenderPipelineCache();
    ~RenderPipelineCache();

    // Finds an existing pipeline without allowing runtime creation.
    render_pipeline* Find( u64 key );

    // Creates a known pipeline before it is needed by a render pass.
    render_pipeline* Prewarm( u64 key, render_pipeline_desc const& desc );

    // Returns an existing pipeline or records and creates a runtime miss.
    render_pipeline* GetOrCreate( u64 key, render_pipeline_desc const& desc );

    // Releases every pipeline handle owned by this cache.
    void Reset();

private:
    struct Entry
    {
        Entry();

        //-------------------------------------------------------------------------

        u64             m_key;
        render_pipeline m_pipeline;
        xbool           m_isPrewarmed;
        xbool           m_isUsed;
    };

    Entry*           FindEntry( u64 key ) const;
    render_pipeline* FindOrCreate( u64 key, render_pipeline_desc const& desc, xbool isPrewarm );

    //-------------------------------------------------------------------------

    char const*    m_pDebugName;
    xarray<Entry*> m_entries;
    u32            m_numPrewarmed;
    u32            m_numRuntimeHits;
    u32            m_numRuntimeMisses;
};

//=============================================================================
#endif // RENDER_PIPELINE_CACHE_HPP
//=============================================================================
