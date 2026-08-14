//=========================================================================
//
//  VertexMgr.cpp
//
//=========================================================================

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"

//=========================================================================
// INCLUDES
//=========================================================================

#include "VertexMgr.hpp"

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

VertexMgr g_RigidVertMgr;

//=========================================================================
// PREPARED DLIST
//=========================================================================

VertexMgr::PreparedMesh::PreparedMesh( void ) : m_vertices(), m_indices(), m_vertexCount( 0 ), m_vertexStride( 0 )
{
}

//=========================================================================

xbool VertexMgr::PreparedMesh::Allocate( s32 nVertices, s32 nIndices, s32 vertexStride )
{
    Clear();

    if ( ( nVertices <= 0 ) || ( nIndices <= 0 ) || ( vertexStride <= 0 ) ||
         ( nVertices > ( 0x7fffffff / vertexStride ) ) )
    {
        return FALSE;
    }

    m_vertices.SetCount( nVertices * vertexStride );
    m_indices.SetCount( nIndices );
    m_vertexCount = nVertices;
    m_vertexStride = vertexStride;
    return TRUE;
}

//=========================================================================

void VertexMgr::PreparedMesh::Clear( void )
{
    m_vertices.Clear();
    m_indices.Clear();
    m_vertexCount = 0;
    m_vertexStride = 0;
}

//=========================================================================

xbool VertexMgr::PreparedMesh::IsValid( void ) const
{
    if ( ( m_vertexCount <= 0 ) || ( m_vertexCount > MAX_MESH_VERTICES ) || ( m_vertexStride <= 0 ) ||
         ( m_vertexCount > ( 0x7fffffff / m_vertexStride ) ) ||
         ( m_vertices.GetCount() != ( m_vertexCount * m_vertexStride ) ) || ( m_indices.GetCount() <= 0 ) ||
         ( ( m_indices.GetCount() % 3 ) != 0 ) )
    {
        return FALSE;
    }

    for ( s32 i = 0; i < m_indices.GetCount(); ++i )
    {
        if ( static_cast<s32>( m_indices[i] ) >= m_vertexCount )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void* VertexMgr::PreparedMesh::GetVertexData( void )
{
    return m_vertices.GetPtr();
}

//=========================================================================

void const* VertexMgr::PreparedMesh::GetVertexData( void ) const
{
    return m_vertices.GetPtr();
}

//=========================================================================

u16* VertexMgr::PreparedMesh::GetIndexData( void )
{
    return m_indices.GetPtr();
}

//=========================================================================

u16 const* VertexMgr::PreparedMesh::GetIndexData( void ) const
{
    return m_indices.GetPtr();
}

//=========================================================================

s32 VertexMgr::PreparedMesh::GetVertexCount( void ) const
{
    return m_vertexCount;
}

//=========================================================================

s32 VertexMgr::PreparedMesh::GetIndexCount( void ) const
{
    return m_indices.GetCount();
}

//=========================================================================

s32 VertexMgr::PreparedMesh::GetVertexStride( void ) const
{
    return m_vertexStride;
}

//=========================================================================
// POOL
//=========================================================================

VertexMgr::pool::pool( void ) : Buffer(), VertexIndexBuffer(), Capacity( 0 ), Stride( 0 ), FreeRanges()
{
}

//=========================================================================

VertexMgr::pool::~pool( void )
{
    rbuffer_Destroy( VertexIndexBuffer );
    rbuffer_Destroy( Buffer );
}

//=========================================================================
// VERTEX MANAGER
//=========================================================================

VertexMgr::VertexMgr( void )
    : m_meshes(), m_vertexPools(), m_indexPools(), m_vertexStride( 0 ), m_isInitialized( FALSE )
{
}

//=========================================================================

VertexMgr::~VertexMgr( void )
{
    Kill();
}

//=========================================================================

void VertexMgr::Init( s32 vertexStride )
{
    ASSERT( vertexStride > 0 );

    if ( m_isInitialized )
    {
        Kill();
    }

    if ( vertexStride <= 0 )
    {
        return;
    }

    m_vertexStride = vertexStride;
    m_isInitialized = TRUE;
}

//=========================================================================

void VertexMgr::Kill( void )
{
    m_meshes.Clear();

    for ( s32 i = 0; i < m_indexPools.GetCount(); ++i )
    {
        delete m_indexPools[i];
    }

    for ( s32 i = 0; i < m_vertexPools.GetCount(); ++i )
    {
        delete m_vertexPools[i];
    }

    m_indexPools.Clear();
    m_vertexPools.Clear();

    m_vertexStride = 0;
    m_isInitialized = FALSE;
}

//=========================================================================

xbool VertexMgr::PrepareMesh( PreparedMesh& prepared, void const* pVertex, s32 nVertices, u16 const* pIndex,
                              s32 nIndices, s32 vertexStride )
{
    prepared.Clear();

    if ( !pVertex || !pIndex )
    {
        return FALSE;
    }

    if ( !prepared.Allocate( nVertices, nIndices, vertexStride ) )
    {
        return FALSE;
    }

    x_memcpy( prepared.GetVertexData(), pVertex, nVertices * vertexStride );
    x_memcpy( prepared.GetIndexData(), pIndex, sizeof( u16 ) * nIndices );

    if ( !prepared.IsValid() )
    {
        prepared.Clear();
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool VertexMgr::CreatePool( PoolArray& pools, s32 capacity, s32 stride, u32 usageFlags, char const* pDebugName )
{
    if ( ( capacity <= 0 ) || ( stride <= 0 ) ||
         ( static_cast<u32>( capacity ) > ( 0xffffffffu / static_cast<u32>( stride ) ) ) )
    {
        return FALSE;
    }

    pool* pPool = new pool;
    ASSERT( pPool );
    if ( !pPool )
    {
        return FALSE;
    }

    rbuffer_desc desc;
    desc.Size       = static_cast<u32>( capacity ) * static_cast<u32>( stride );
    desc.Stride     = static_cast<u32>( stride );
    desc.UsageFlags = usageFlags;
    desc.pDebugName = pDebugName;

    if ( !rbuffer_Create( pPool->Buffer, desc ) )
    {
        delete pPool;
        return FALSE;
    }

    if ( usageFlags & RBUFFER_USAGE_VERTEX )
    {
        rbuffer_desc indexDesc;
        indexDesc.Size       = static_cast<u32>( capacity ) * sizeof( u32 );
        indexDesc.Stride     = sizeof( u32 );
        indexDesc.UsageFlags = RBUFFER_USAGE_VERTEX;
        indexDesc.pDebugName = "StaticVertexPoolIndices";
        if ( !rbuffer_Create( pPool->VertexIndexBuffer, indexDesc ) )
        {
            delete pPool;
            return FALSE;
        }
    }

    pPool->Capacity = capacity;
    pPool->Stride = stride;

    free_range& range = pPool->FreeRanges.Append();
    range.Offset = 0;
    range.Count = capacity;

    pools.Append() = pPool;
    return TRUE;
}

//=========================================================================

xbool VertexMgr::AllocateRange( PoolArray& pools, s32 count, s32 stride, u32 usageFlags, s32 defaultCapacity,
                                char const* pDebugName, allocation& outAllocation )
{
    outAllocation = allocation();

    if ( ( count <= 0 ) || ( stride <= 0 ) || ( defaultCapacity <= 0 ) )
    {
        return FALSE;
    }

    s32 bestPool = -1;
    s32 bestRange = -1;
    s32 bestWaste = 0x7fffffff;

    for ( s32 iPool = 0; iPool < pools.GetCount(); ++iPool )
    {
        pool& pool = *pools[iPool];
        if ( pool.Stride != stride )
        {
            continue;
        }

        for ( s32 iRange = 0; iRange < pool.FreeRanges.GetCount(); ++iRange )
        {
            free_range const& range = pool.FreeRanges[iRange];
            if ( range.Count < count )
            {
                continue;
            }

            s32 const waste = range.Count - count;
            if ( waste < bestWaste )
            {
                bestPool = iPool;
                bestRange = iRange;
                bestWaste = waste;
            }
        }
    }

    if ( bestPool < 0 )
    {
        s32 const capacity = MAX( defaultCapacity, count );
        if ( !CreatePool( pools, capacity, stride, usageFlags, pDebugName ) )
        {
            return FALSE;
        }

        bestPool = pools.GetCount() - 1;
        bestRange = 0;
    }

    pool&       pool = *pools[bestPool];
    free_range& range = pool.FreeRanges[bestRange];

    outAllocation.PoolIndex = bestPool;
    outAllocation.Offset = range.Offset;
    outAllocation.Count = count;

    if ( range.Count == count )
    {
        pool.FreeRanges.Delete( bestRange );
    }
    else
    {
        range.Offset += count;
        range.Count -= count;
    }

    return TRUE;
}

//=========================================================================

void VertexMgr::ReleaseRange( PoolArray& pools, allocation const& allocation )
{
    if ( !allocation.IsValid() )
    {
        return;
    }

    ASSERT( allocation.PoolIndex < pools.GetCount() );
    if ( ( allocation.PoolIndex < 0 ) || ( allocation.PoolIndex >= pools.GetCount() ) )
    {
        return;
    }

    pool& pool = *pools[allocation.PoolIndex];
    if ( ( allocation.Count <= 0 ) || ( allocation.Offset < 0 ) ||
         ( allocation.Offset > ( pool.Capacity - allocation.Count ) ) )
    {
        return;
    }

    s32 insertAt = 0;
    while ( ( insertAt < pool.FreeRanges.GetCount() ) && ( pool.FreeRanges[insertAt].Offset < allocation.Offset ) )
    {
        ++insertAt;
    }

    if ( insertAt > 0 )
    {
        free_range const& previous = pool.FreeRanges[insertAt - 1];
        ASSERT( ( previous.Offset + previous.Count ) <= allocation.Offset );
    }

    if ( insertAt < pool.FreeRanges.GetCount() )
    {
        free_range const& next = pool.FreeRanges[insertAt];
        ASSERT( ( allocation.Offset + allocation.Count ) <= next.Offset );
    }

    free_range& range = pool.FreeRanges.Insert( insertAt );
    range.Offset = allocation.Offset;
    range.Count = allocation.Count;

    if ( insertAt > 0 )
    {
        free_range& previous = pool.FreeRanges[insertAt - 1];
        if ( ( previous.Offset + previous.Count ) == range.Offset )
        {
            previous.Count += range.Count;
            pool.FreeRanges.Delete( insertAt );
            --insertAt;
        }
    }

    if ( ( insertAt + 1 ) < pool.FreeRanges.GetCount() )
    {
        free_range&       current = pool.FreeRanges[insertAt];
        free_range const& next = pool.FreeRanges[insertAt + 1];
        if ( ( current.Offset + current.Count ) == next.Offset )
        {
            current.Count += next.Count;
            pool.FreeRanges.Delete( insertAt + 1 );
        }
    }
}

//=========================================================================

xhandle VertexMgr::AddMesh( PreparedMesh const& prepared )
{
    ASSERT( m_isInitialized );
    ASSERT( prepared.GetVertexStride() == m_vertexStride );
    ASSERT( prepared.IsValid() );

    if ( !m_isInitialized || ( prepared.GetVertexStride() != m_vertexStride ) || !prepared.IsValid() )
    {
        x_throw( "Invalid prepared static geometry" );
    }

    allocation vertexAllocation;
    allocation indexAllocation;
    xhandle    hMesh;
    hMesh.Handle = HNULL;

    x_try;

    if ( !AllocateRange( m_vertexPools, prepared.GetVertexCount(), m_vertexStride, RBUFFER_USAGE_VERTEX,
                         DEFAULT_VERTEX_POOL_CAPACITY, "StaticGeometryVertexPool", vertexAllocation ) )
    {
        x_throw( "Unable to allocate static vertex range" );
    }

    if ( !AllocateRange( m_indexPools, prepared.GetIndexCount(), sizeof( u16 ), RBUFFER_USAGE_INDEX,
                         DEFAULT_INDEX_POOL_CAPACITY, "StaticGeometryIndexPool", indexAllocation ) )
    {
        x_throw( "Unable to allocate static index range" );
    }

    pool& vertexPool = *m_vertexPools[vertexAllocation.PoolIndex];
    pool& indexPool  = *m_indexPools[indexAllocation.PoolIndex];

    u32 const vertexUploadOffset = static_cast<u32>( vertexAllocation.Offset ) * static_cast<u32>( m_vertexStride );
    u32 const vertexBytes        = static_cast<u32>( vertexAllocation.Count ) * static_cast<u32>( m_vertexStride );
    u32 const indexUploadOffset  = static_cast<u32>( indexAllocation.Offset ) * static_cast<u32>( sizeof( u16 ) );
    u32 const indexBytes         = static_cast<u32>( indexAllocation.Count ) * static_cast<u32>( sizeof( u16 ) );

    if ( !rbuffer_Upload( vertexPool.Buffer, prepared.GetVertexData(), vertexBytes, vertexUploadOffset, FALSE ) )
    {
        x_throw( "Unable to upload static vertex data" );
    }

    xarray<u32> localVertexIndices;
    localVertexIndices.SetCount( vertexAllocation.Count );
    for ( s32 i = 0; i < vertexAllocation.Count; ++i )
    {
        localVertexIndices[i] = i;
    }

    if ( !rbuffer_Upload( vertexPool.VertexIndexBuffer, localVertexIndices.GetPtr(),
                          vertexAllocation.Count * sizeof( u32 ), vertexAllocation.Offset * sizeof( u32 ), FALSE ) )
    {
        x_throw( "Unable to upload static local vertex indices" );
    }

    if ( !rbuffer_Upload( indexPool.Buffer, prepared.GetIndexData(), indexBytes, indexUploadOffset, FALSE ) )
    {
        x_throw( "Unable to upload static index data" );
    }

    mesh& mesh = m_meshes.Add( hMesh );
    mesh.VertexAllocation = vertexAllocation;
    mesh.IndexAllocation = indexAllocation;

    x_catch_begin;

    if ( hMesh.IsNonNull() )
    {
        m_meshes.DeleteByHandle( hMesh );
    }

    ReleaseRange( m_indexPools, indexAllocation );
    ReleaseRange( m_vertexPools, vertexAllocation );

    x_catch_end_ret;

    return hMesh;
}

//=========================================================================

void VertexMgr::RemoveMesh( xhandle hMesh )
{
    mesh const mesh = m_meshes( hMesh );

    ReleaseRange( m_indexPools, mesh.IndexAllocation );
    ReleaseRange( m_vertexPools, mesh.VertexAllocation );
    m_meshes.DeleteByHandle( hMesh );
}

//=========================================================================

xbool VertexMgr::BindMesh( xhandle hMesh ) const
{
    mesh_range range;
    if ( !GetMeshDrawRange( hMesh, range ) )
    {
        return FALSE;
    }

    return BindPools( range );
}

//=========================================================================

xbool VertexMgr::BindPools( mesh_range const& range ) const
{
    if ( ( range.VertexPool < 0 ) || ( range.VertexPool >= m_vertexPools.GetCount() ) || ( range.IndexPool < 0 ) ||
         ( range.IndexPool >= m_indexPools.GetCount() ) )
    {
        return FALSE;
    }

    pool const& vertexPool = *m_vertexPools[range.VertexPool];
    pool const& indexPool = *m_indexPools[range.IndexPool];
    return rbuffer_BindVertex( vertexPool.Buffer, 0, 0 ) &&
           rbuffer_BindIndex( indexPool.Buffer, RBUFFER_INDEX_FORMAT_U16, 0 );
}

//=========================================================================

xbool VertexMgr::BindVertexIndices( mesh_range const& range, u32 slot ) const
{
    if ( ( range.VertexPool < 0 ) || ( range.VertexPool >= m_vertexPools.GetCount() ) )
    {
        return FALSE;
    }

    pool const& vertexPool = *m_vertexPools[range.VertexPool];
    return rbuffer_BindVertex( vertexPool.VertexIndexBuffer, slot, 0 );
}

//=========================================================================

xbool VertexMgr::GetMeshDrawRange( xhandle hMesh, mesh_range& range ) const
{
    range = mesh_range();

    mesh const& mesh = m_meshes( hMesh );
    if ( !mesh.VertexAllocation.IsValid() || !mesh.IndexAllocation.IsValid() ||
         ( mesh.VertexAllocation.PoolIndex >= m_vertexPools.GetCount() ) ||
         ( mesh.IndexAllocation.PoolIndex >= m_indexPools.GetCount() ) )
    {
        return FALSE;
    }

    range.FirstIndex = mesh.IndexAllocation.Offset;
    range.IndexCount = mesh.IndexAllocation.Count;
    range.BaseVertex = mesh.VertexAllocation.Offset;
    range.VertexPool = mesh.VertexAllocation.PoolIndex;
    range.IndexPool = mesh.IndexAllocation.PoolIndex;
    return TRUE;
}

//=========================================================================
