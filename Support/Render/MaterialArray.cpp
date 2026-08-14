//=============================================================================
//
//  MaterialArray.cpp
//
//=============================================================================

//=============================================================================
// Includes
//=============================================================================

#include "Render.hpp"

#define RENDER_PRIVATE
#include "MaterialArray.hpp"
#undef RENDER_PRIVATE

//=============================================================================
// Implementation
//=============================================================================

MaterialArray::MaterialArray( void )
    : m_sorted( FALSE ), m_capacity( FALSE ), m_nNodes( 0 ), m_pNodes( NULL ), m_pIndices( NULL )
{
}

//=============================================================================

MaterialArray::~MaterialArray( void )
{
    Clear();
    if ( m_pNodes )
    {
        x_free( m_pNodes );
    }
}

//=============================================================================

s32 NodeCompareFn( void const* pA, void const* pB )
{
    MaterialArray::node_info const* pNodeA = static_cast<MaterialArray::node_info const*>( pA );
    MaterialArray::node_info const* pNodeB = static_cast<MaterialArray::node_info const*>( pB );
    material const*                 pMatA = &pNodeA->Item;
    material const*                 pMatB = &pNodeB->Item;

    if ( pMatA->GetSortPriority() > pMatB->GetSortPriority() )
    {
        return 1;
    }
    if ( pMatA->GetSortPriority() < pMatB->GetSortPriority() )
    {
        return -1;
    }
    if ( pMatA->m_diffuseMap.GetPointer() > pMatB->m_diffuseMap.GetPointer() )
    {
        return 1;
    }
    if ( pMatA->m_diffuseMap.GetPointer() < pMatB->m_diffuseMap.GetPointer() )
    {
        return -1;
    }
    if ( pMatA->m_environmentMap.GetPointer() > pMatB->m_environmentMap.GetPointer() )
    {
        return 1;
    }
    if ( pMatA->m_environmentMap.GetPointer() < pMatB->m_environmentMap.GetPointer() )
    {
        return -1;
    }
    if ( pMatA->m_detailMap.GetPointer() > pMatB->m_detailMap.GetPointer() )
    {
        return 1;
    }
    if ( pMatA->m_detailMap.GetPointer() < pMatB->m_detailMap.GetPointer() )
    {
        return -1;
    }

    return 0;
}

//=============================================================================

void MaterialArray::Sort( void )
{
    if ( m_sorted )
    {
        return;
    }

    if ( m_nNodes )
    {
        // sort the materials
        x_qsort( m_pNodes, m_nNodes, sizeof( node_info ), NodeCompareFn );

        // remap the material indices
        for ( s32 i = 0; i < m_nNodes; i++ )
        {
            ASSERT( ( m_pNodes[i].Handle.Handle >= 0 ) && ( m_pNodes[i].Handle.Handle < m_capacity ) );
            m_pIndices[m_pNodes[i].Handle] = i;
        }
    }

    // sanity check
    if ( 0 )
    {
        for ( s32 iNode = 0; iNode < m_nNodes - 1; iNode++ )
        {
            ASSERT( NodeCompareFn( &m_pNodes[iNode + 1], &m_pNodes[iNode] ) >= 0 );
        }
    }

    m_sorted = TRUE;
}

//=============================================================================

void MaterialArray::Clear( void )
{
    s32 i;

    // destruct all of the objects
    for ( i = 0; i < m_nNodes; i++ )
    {
        ( reinterpret_cast<destructor*>( &m_pNodes[i].Item ) )->~destructor();
    }

    // clear all of the handle indices
    for ( i = 0; i < m_capacity; i++ )
    {
        m_pIndices[i] = -1;
    }

    m_nNodes = 0;
    m_sorted = FALSE;
}

//=============================================================================

void MaterialArray::GrowListBy( s32 nNodes )
{
    ASSERT( nNodes > 0 );

    // increase the capacity
    s32 newCapacity = m_capacity + nNodes;

    // allocate the new arrays
    node_info* pNewNodes = static_cast<node_info*>( x_malloc( ( sizeof( node_info ) + sizeof( s32 ) ) * newCapacity ) );
    ASSERT( pNewNodes );

    s32* pNewIndices = reinterpret_cast<s32*>( pNewNodes + newCapacity );
    ASSERT( pNewIndices );

    // copy all the previous nodes to the new arrays
    if ( m_nNodes )
    {
        ASSERT( m_pNodes );
        x_memcpy( pNewNodes, m_pNodes, sizeof( node_info ) * m_nNodes );
    }
    if ( m_capacity )
    {
        ASSERT( m_pIndices );
        x_memcpy( pNewIndices, m_pIndices, sizeof( s32 ) * m_capacity );
    }

    // set the new indices to -1
    for ( s32 i = m_capacity; i < newCapacity; i++ )
    {
        pNewIndices[i] = -1;
    }

    // update the arrays
    if ( m_pNodes )
    {
        x_free( m_pNodes );
    }
    m_pNodes = pNewNodes;
    m_pIndices = pNewIndices;
    m_capacity = newCapacity;
}

//=============================================================================

material& MaterialArray::Add( xhandle& hHandle )
{
    // do we need to grow?
    if ( m_nNodes >= m_capacity )
    {
        GrowListBy( MAX( m_capacity / 2, 100 ) );
    }

    // find the first available index
    s32 iHandle;
    for ( iHandle = 0; iHandle < m_capacity; iHandle++ )
    {
        if ( m_pIndices[iHandle] == -1 )
        {
            break;
        }
    }
    ASSERT( iHandle != m_capacity );

    // fill in the index information
    m_pIndices[iHandle] = m_nNodes;
    hHandle.Handle = iHandle;

    // construct the node info
    xConstruct( &m_pNodes[m_nNodes].Item );
    m_pNodes[m_nNodes].Handle.Handle = iHandle;
    m_nNodes++;

    // we'll need to re-sort
    m_sorted = FALSE;

    return m_pNodes[m_pIndices[iHandle]].Item;
}

//=============================================================================

void MaterialArray::DeleteByHandle( xhandle hHandle )
{
    s32 index = GetIndexByHandle( hHandle );
    ASSERT( ( index >= 0 ) && ( index < m_nNodes ) );

    // call destructor
    ( reinterpret_cast<destructor*>( &m_pNodes[index].Item ) )->~destructor();

    // clear out the index
    m_pIndices[hHandle.Handle] = -1;

    // copy the last node into the deleted node
    if ( index != ( m_nNodes - 1 ) )
    {
        m_pNodes[index].Item = m_pNodes[m_nNodes - 1].Item;
        m_pNodes[index].Handle = m_pNodes[m_nNodes - 1].Handle;
        m_pIndices[m_pNodes[index].Handle.Handle] = index;
    }
    m_nNodes--;

    // flag that we'll need to sort again
    m_sorted = FALSE;
}

//=============================================================================

xbool MaterialArray::SanityCheck( void ) const
{
    s32 i;

    for ( i = 0; i < m_nNodes; i++ )
    {
        ASSERT( m_pIndices[m_pNodes[i].Handle.Handle] == i );
    }

    for ( i = 0; i < m_capacity; i++ )
    {
        if ( m_pIndices[i] != -1 )
        {
            ASSERT( m_pNodes[m_pIndices[i]].Handle.Handle == i );
        }
    }

    return TRUE;
}

//=============================================================================

void MaterialArray::Update( f32 deltaTime )
{
    for ( s32 i = 0; i < GetCount(); i++ )
    {
        material&         mat = operator[]( i );
        material::uvanim& uvAnim = mat.m_uvAnim;
        if ( uvAnim.nFrames <= 1 )
        {
            continue;
        }

        // Artists duplicate the first and last frame to be consistent
        // with the sketal animations (this is to make blending and
        // looped anims work). We really don't want to do this for
        // uv anims, so consider nFrames to be one smaller.
        s32 nFrames = uvAnim.nFrames - 1;

        // calculate the next frame
        uvAnim.CurrentFrame += deltaTime * uvAnim.FPS * uvAnim.Dir;

        // determine the type of animation
        switch ( uvAnim.Type )
        {
            case geom::material::uvanim::FIXED:
                {
                    uvAnim.Dir = 0;
                    uvAnim.CurrentFrame = static_cast<f32>( uvAnim.StartFrame );
                }
                break;
            case geom::material::uvanim::LOOPED:
                {
                    uvAnim.Dir = 1;
                    if ( uvAnim.CurrentFrame >= static_cast<f32>( nFrames ) )
                    {
                        uvAnim.CurrentFrame = static_cast<f32>( uvAnim.StartFrame );
                    }
                }
                break;
            case geom::material::uvanim::PINGPONG:
                {
                    if ( uvAnim.Dir > 0 )
                    {
                        uvAnim.Dir = 1;
                        if ( uvAnim.CurrentFrame >= static_cast<f32>( nFrames ) )
                        {
                            uvAnim.CurrentFrame = static_cast<f32>( nFrames - 1 );
                            uvAnim.Dir = -uvAnim.Dir;
                        }
                    }
                    else
                    {
                        uvAnim.Dir = -1;
                        if ( uvAnim.CurrentFrame <= 0.0f )
                        {
                            uvAnim.CurrentFrame = 0.0f;
                            uvAnim.Dir = -uvAnim.Dir;
                        }
                    }
                }
                break;
            case geom::material::uvanim::ONESHOT:
                {
                    uvAnim.Dir = 1;
                    if ( uvAnim.CurrentFrame >= static_cast<f32>( nFrames ) )
                    {
                        uvAnim.CurrentFrame = static_cast<f32>( nFrames - 1 );
                        uvAnim.Dir = 0;
                    }
                }
                break;
        }

        uvAnim.iFrame = static_cast<s8>( x_floor( uvAnim.CurrentFrame ) );
    }
}
