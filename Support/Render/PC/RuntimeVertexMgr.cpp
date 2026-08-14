//=========================================================================
//
//  RuntimeVertexMgr.cpp
//
//=========================================================================

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"

//=========================================================================
// INCLUDES
//=========================================================================

#include "RuntimeVertexMgr.hpp"

#include "x_debug.hpp"

//=========================================================================
// HELPERS
//=========================================================================

static s32 runtime_vertex_NextCapacity( s32 current, s32 required )
{
    s32 capacity = current;

    if ( capacity < 1 )
    {
        capacity = 1;
    }

    while ( capacity < required )
    {
        if ( capacity > ( 0x7fffffff / 2 ) )
        {
            return required;
        }

        capacity *= 2;
    }

    return capacity;
}

//=========================================================================
// IMPLEMENTATION
//=========================================================================

RuntimeVertexMgr::RuntimeVertexMgr( void )
    : m_vertexBuffer(), m_indexBuffer(), m_vertexStride( 0 ), m_maxVertices( 0 ), m_MaxIndices( 0 ),
      m_uploadedVertices( 0 ), m_uploadedIndices( 0 ), m_isInitialized( FALSE )
{
}

//=========================================================================

xbool RuntimeVertexMgr::Init( s32 vStride, s32 initialVertices, s32 initialIndices )
{
    ASSERT( vStride > 0 );
    ASSERT( initialVertices > 0 );
    ASSERT( initialIndices > 0 );

    if ( ( vStride <= 0 ) || ( initialVertices <= 0 ) || ( initialIndices <= 0 ) )
    {
        return FALSE;
    }

    if ( m_isInitialized )
    {
        Kill();
    }

    m_vertexStride = vStride;

    if ( !CreateBuffers( initialVertices, initialIndices ) )
    {
        DestroyBuffers();
        m_vertexStride = 0;
        return FALSE;
    }

    m_isInitialized = TRUE;
    return TRUE;
}

//=========================================================================

void RuntimeVertexMgr::Kill( void )
{
    DestroyBuffers();

    m_vertexStride = 0;
    m_maxVertices = 0;
    m_MaxIndices = 0;
    m_uploadedVertices = 0;
    m_uploadedIndices = 0;
    m_isInitialized = FALSE;
}

//=========================================================================

xbool RuntimeVertexMgr::Reserve( s32 nVertices, s32 nIndices )
{
    if ( ( nVertices <= 0 ) || ( nIndices <= 0 ) )
    {
        return FALSE;
    }

    return EnsureCapacity( nVertices, nIndices );
}

//=========================================================================

xbool RuntimeVertexMgr::CreateBuffers( s32 maxVertices, s32 maxIndices )
{
    ASSERT( m_vertexStride > 0 );
    ASSERT( maxVertices > 0 );
    ASSERT( maxIndices > 0 );

    if ( ( m_vertexStride <= 0 ) || ( maxVertices <= 0 ) || ( maxIndices <= 0 ) )
    {
        return FALSE;
    }

    if ( ( static_cast<u32>( maxVertices ) > ( 0xffffffffu / static_cast<u32>( m_vertexStride ) ) ) ||
         ( static_cast<u32>( maxIndices ) > ( 0xffffffffu / static_cast<u32>( sizeof( u16 ) ) ) ) )
    {
        return FALSE;
    }

    DestroyBuffers();

    u32 const vertexBytes = static_cast<u32>( m_vertexStride * maxVertices );
    u32 const indexBytes = static_cast<u32>( sizeof( u16 ) * maxIndices );

    rbuffer_desc vertexDesc;
    vertexDesc.Size       = vertexBytes;
    vertexDesc.Stride     = static_cast<u32>( m_vertexStride );
    vertexDesc.UsageFlags = RBUFFER_USAGE_VERTEX;
    vertexDesc.pDebugName = "RuntimeVertexMgrVertex";

    rbuffer_desc indexDesc;
    indexDesc.Size       = indexBytes;
    indexDesc.Stride     = sizeof( u16 );
    indexDesc.UsageFlags = RBUFFER_USAGE_INDEX;
    indexDesc.pDebugName = "RuntimeVertexMgrIndex";

    if ( !rbuffer_Create( m_vertexBuffer, vertexDesc ) || !rbuffer_Create( m_indexBuffer, indexDesc ) )
    {
        DestroyBuffers();
        return FALSE;
    }

    m_maxVertices = maxVertices;
    m_MaxIndices = maxIndices;
    return TRUE;
}

//=========================================================================

void RuntimeVertexMgr::DestroyBuffers( void )
{
    rbuffer_Destroy( m_indexBuffer );
    rbuffer_Destroy( m_vertexBuffer );

    m_maxVertices = 0;
    m_MaxIndices = 0;
    m_uploadedVertices = 0;
    m_uploadedIndices = 0;
}

//=========================================================================

xbool RuntimeVertexMgr::EnsureCapacity( s32 requiredVertices, s32 requiredIndices )
{
    if ( !m_isInitialized )
    {
        return FALSE;
    }

    if ( ( requiredVertices <= 0 ) || ( requiredIndices <= 0 ) )
    {
        return FALSE;
    }

    if ( ( requiredVertices <= m_maxVertices ) && ( requiredIndices <= m_MaxIndices ) )
    {
        return TRUE;
    }

    s32 const newMaxVertices = runtime_vertex_NextCapacity( m_maxVertices, requiredVertices );
    s32 const newMaxIndices = runtime_vertex_NextCapacity( m_MaxIndices, requiredIndices );

    return CreateBuffers( newMaxVertices, newMaxIndices );
}

//=========================================================================

xbool RuntimeVertexMgr::UploadPrimitive( primitive const& primitive )
{
    if ( !primitive.pVertex || !primitive.pIndex )
    {
        return FALSE;
    }

    if ( ( primitive.nVertices <= 0 ) || ( primitive.nIndices <= 0 ) )
    {
        return FALSE;
    }

    if ( !EnsureCapacity( primitive.nVertices, primitive.nIndices ) )
    {
        return FALSE;
    }

    u32 const vertexBytes = static_cast<u32>( primitive.nVertices * m_vertexStride );
    u32 const indexBytes = static_cast<u32>( primitive.nIndices * static_cast<s32>( sizeof( u16 ) ) );

    m_uploadedVertices = 0;
    m_uploadedIndices = 0;

    if ( !rbuffer_Upload( m_vertexBuffer, primitive.pVertex, vertexBytes, 0, TRUE ) )
    {
        return FALSE;
    }

    if ( !rbuffer_Upload( m_indexBuffer, primitive.pIndex, indexBytes, 0, TRUE ) )
    {
        return FALSE;
    }

    m_uploadedVertices = primitive.nVertices;
    m_uploadedIndices = primitive.nIndices;
    return TRUE;
}

//=========================================================================

xbool RuntimeVertexMgr::BindBuffers( void ) const
{
    if ( !m_isInitialized || ( m_uploadedVertices <= 0 ) || ( m_uploadedIndices <= 0 ) )
    {
        return FALSE;
    }

    if ( !rbuffer_BindVertex( m_vertexBuffer, 0 ) )
    {
        return FALSE;
    }

    return rbuffer_BindIndex( m_indexBuffer, RBUFFER_INDEX_FORMAT_U16 );
}

//=========================================================================

xbool RuntimeVertexMgr::DrawIndexed( s32 indexCount, s32 startIndex, s32 baseVertex ) const
{
    if ( !m_isInitialized || ( indexCount <= 0 ) || ( startIndex < 0 ) || ( baseVertex < 0 ) ||
         ( startIndex > ( m_uploadedIndices - indexCount ) ) || ( baseVertex >= m_uploadedVertices ) )
    {
        return FALSE;
    }

    return rdraw_DrawIndexed( indexCount, startIndex, baseVertex );
}

//=========================================================================
