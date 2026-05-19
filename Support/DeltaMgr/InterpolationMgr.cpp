#include "DeltaMgr\InterpolationMgr.hpp"

interpolation_mgr g_InterpolationMgr;

//==============================================================================

interpolation_mgr::interpolation_mgr( void )
{
}

//==============================================================================

interpolation_mgr::provider*& interpolation_mgr::provider::GetHead( void )
{
    static provider* s_pHead = NULL;
    return s_pHead;
}

//==============================================================================

interpolation_mgr::provider::provider( capture_fn* pCapture,
                                       update_fn*  pUpdate,
                                       clear_fn*   pClear,
                                       clear_stage ClearStage,
                                       s32         Priority ) :
    m_pNext         ( NULL       ),
    m_pCapture      ( pCapture   ),
    m_pUpdate       ( pUpdate    ),
    m_pClear        ( pClear     ),
    m_ClearStage    ( ClearStage ),
    m_Priority      ( Priority   )
{
    provider** ppProvider = &GetHead();
    while( *ppProvider && ( (*ppProvider)->m_Priority <= m_Priority ) )
        ppProvider = &( (*ppProvider)->m_pNext );

    m_pNext     = *ppProvider;
    *ppProvider = this;
}

//==============================================================================

void interpolation_mgr::Capture( void ) const
{
    for( provider* pProvider = provider::GetHead(); pProvider; pProvider = pProvider->m_pNext )
    {
        if( pProvider->m_pCapture )
            pProvider->m_pCapture();
    }
}

//==============================================================================

void interpolation_mgr::Update( f32 Alpha ) const
{
    for( provider* pProvider = provider::GetHead(); pProvider; pProvider = pProvider->m_pNext )
    {
        if( pProvider->m_pUpdate )
            pProvider->m_pUpdate( Alpha );
    }
}

//==============================================================================

void interpolation_mgr::Clear( clear_stage Stage ) const
{
    for( provider* pProvider = provider::GetHead(); pProvider; pProvider = pProvider->m_pNext )
    {
        if( (pProvider->m_ClearStage == Stage) && pProvider->m_pClear )
            pProvider->m_pClear();
    }
}
