//=========================================================================
//
//  Runtime Vertex Manager for PC
//
//=========================================================================

//=========================================================================
//  PLATFORM CHECK
//=========================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//=========================================================================
// INCLUDES
//=========================================================================

#include "RuntimeVertexMgr.hpp"

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

runtime_vertex_mgr g_RuntimeVertexMgr;

//=========================================================================
// IMPLEMENTATION
//=========================================================================

runtime_vertex_mgr::runtime_vertex_mgr( void )
{
    m_pVertexBuffer     = NULL;
    m_pIndexBuffer      = NULL;
    m_pSystemVertexData = NULL;
    m_pSystemIndexData  = NULL;
    m_pMappedVertexData = NULL;
    m_pMappedIndexData  = NULL;
    m_VertexStride      = 0;
    m_MaxVertices       = 0;
    m_MaxIndices        = 0;
    m_iVertex           = 0;
    m_iIndex            = 0;
    m_bMapped           = FALSE;
    m_bHasDrawn         = FALSE;
    m_bStreamsBound     = FALSE;
}

//=========================================================================

void runtime_vertex_mgr::Init( s32 VStride, s32 MaxVertices, s32 MaxIndices )
{
    ASSERT( m_pVertexBuffer     == NULL );
    ASSERT( m_pIndexBuffer      == NULL );
    ASSERT( m_pSystemVertexData == NULL );
    ASSERT( m_pSystemIndexData  == NULL );
    ASSERT( VStride > 0 );
    ASSERT( MaxVertices > 0 );
    ASSERT( MaxIndices  > 0 );

    m_VertexStride = VStride;
    m_MaxVertices  = MaxVertices;
    m_MaxIndices   = MaxIndices;

    if( g_pd3dDevice )
    {
        D3D11_BUFFER_DESC bd;
        x_memset( &bd, 0, sizeof(bd) );
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth      = m_VertexStride * m_MaxVertices;
        bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &m_pVertexBuffer );
        if( FAILED(hr) )
        {
            x_throw( "Unable to create runtime primitive vertex buffer in D3D11" );
        }

        x_memset( &bd, 0, sizeof(bd) );
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth      = sizeof(u16) * m_MaxIndices;
        bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &m_pIndexBuffer );
        if( FAILED(hr) )
        {
            if( m_pVertexBuffer )
            {
                m_pVertexBuffer->Release();
                m_pVertexBuffer = NULL;
            }

            x_throw( "Unable to create runtime primitive index buffer in D3D11" );
        }
    }
    else
    {
        m_pSystemVertexData = (byte*)x_malloc( m_VertexStride * m_MaxVertices );
        m_pSystemIndexData  = (u16*)x_malloc( sizeof(u16) * m_MaxIndices );
    }
    BeginRender();
}

//=========================================================================

void runtime_vertex_mgr::Kill( void )
{
    UnmapBuffers();

    if( m_pIndexBuffer )
    {
        m_pIndexBuffer->Release();
        m_pIndexBuffer = NULL;
    }

    if( m_pVertexBuffer )
    {
        m_pVertexBuffer->Release();
        m_pVertexBuffer = NULL;
    }

    if( m_pSystemIndexData )
    {
        x_free( m_pSystemIndexData );
        m_pSystemIndexData = NULL;
    }

    if( m_pSystemVertexData )
    {
        x_free( m_pSystemVertexData );
        m_pSystemVertexData = NULL;
    }

    m_VertexStride  = 0;
    m_MaxVertices   = 0;
    m_MaxIndices    = 0;
    m_iVertex       = 0;
    m_iIndex        = 0;
    m_bHasDrawn     = FALSE;
    m_bStreamsBound = FALSE;
}

//=========================================================================

void runtime_vertex_mgr::BeginRender( void )
{
    UnmapBuffers();

    m_iVertex       = 0;
    m_iIndex        = 0;
    m_bHasDrawn     = FALSE;
    m_bStreamsBound = FALSE;
}

//=========================================================================

xbool runtime_vertex_mgr::MapBuffers( D3D11_MAP MapType )
{
    if( m_bMapped )
        return TRUE;

    if( g_pd3dDevice && g_pd3dContext )
    {
        D3D11_MAPPED_SUBRESOURCE mappedVertex;
        D3D11_MAPPED_SUBRESOURCE mappedIndex;

        HRESULT hr = g_pd3dContext->Map( m_pVertexBuffer, 0, MapType, 0, &mappedVertex );
        if( FAILED(hr) )
            return FALSE;

        hr = g_pd3dContext->Map( m_pIndexBuffer, 0, MapType, 0, &mappedIndex );
        if( FAILED(hr) )
        {
            g_pd3dContext->Unmap( m_pVertexBuffer, 0 );
            return FALSE;
        }

        m_pMappedVertexData = (byte*)mappedVertex.pData;
        m_pMappedIndexData  = (u16*)mappedIndex.pData;
    }
    else
    {
        m_pMappedVertexData = m_pSystemVertexData;
        m_pMappedIndexData  = m_pSystemIndexData;
    }

    m_bMapped = TRUE;
    return TRUE;
}

//=========================================================================

void runtime_vertex_mgr::UnmapBuffers( void )
{
    if( !m_bMapped )
        return;

    if( g_pd3dDevice && g_pd3dContext )
    {
        g_pd3dContext->Unmap( m_pVertexBuffer, 0 );
        g_pd3dContext->Unmap( m_pIndexBuffer, 0 );
    }

    m_pMappedVertexData = NULL;
    m_pMappedIndexData  = NULL;
    m_bMapped           = FALSE;
}

//=========================================================================

xbool runtime_vertex_mgr::EnsureSpace( s32 nVertices, s32 nIndices )
{
    ASSERT( m_VertexStride > 0 );

    if( ( nVertices <= 0 ) || ( nIndices <= 0 ) )
        return FALSE;

    if( ( nVertices > m_MaxVertices ) || ( nIndices > m_MaxIndices ) )
        return FALSE;

    if( ( m_iVertex + nVertices ) > m_MaxVertices ||
        ( m_iIndex  + nIndices  ) > m_MaxIndices )
    {
        if( !m_bHasDrawn )
            return FALSE;

        UnmapBuffers();
        m_iVertex       = 0;
        m_iIndex        = 0;
        m_bStreamsBound = FALSE;
    }

    if( !m_bMapped )
    {
        const D3D11_MAP MapType = ( ( m_iVertex == 0 ) && ( m_iIndex == 0 ) )
                                ? D3D11_MAP_WRITE_DISCARD
                                : D3D11_MAP_WRITE_NO_OVERWRITE;

        if( !MapBuffers( MapType ) )
            return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool runtime_vertex_mgr::Alloc( s32 nVertices, s32 nIndices, draw_buffer& DrawBuffer )
{
    if( !EnsureSpace( nVertices, nIndices ) )
        return FALSE;

    DrawBuffer.pVertex   = m_pMappedVertexData + ( m_iVertex * m_VertexStride );
    DrawBuffer.pIndex    = m_pMappedIndexData  + m_iIndex;
    DrawBuffer.iVertex   = m_iVertex;
    DrawBuffer.iIndex    = m_iIndex;
    DrawBuffer.nVertices = nVertices;
    DrawBuffer.nIndices  = nIndices;

    m_iVertex += nVertices;
    m_iIndex  += nIndices;

    return TRUE;
}

//=========================================================================

xbool runtime_vertex_mgr::DrawPrimitive( const primitive& Primitive )
{
    if( !Primitive.pVertex || !Primitive.pIndex )
        return FALSE;

    if( ( Primitive.nVertices <= 0 ) || ( Primitive.nIndices <= 0 ) )
        return FALSE;

    draw_buffer DrawBuffer;
    if( !Alloc( Primitive.nVertices, Primitive.nIndices, DrawBuffer ) )
        return FALSE;

    x_memcpy( DrawBuffer.pVertex, Primitive.pVertex, Primitive.nVertices * m_VertexStride );
    x_memcpy( DrawBuffer.pIndex,  Primitive.pIndex,  Primitive.nIndices  * sizeof(u16) );

    Draw( DrawBuffer.iVertex, DrawBuffer.iIndex, DrawBuffer.nIndices, Primitive.Topology );
    return TRUE;
}

//=========================================================================

void runtime_vertex_mgr::Flush( void )
{
    UnmapBuffers();
}

//=========================================================================

void runtime_vertex_mgr::Bind( void )
{
    if( m_bStreamsBound || !g_pd3dDevice || !g_pd3dContext )
        return;

    UINT Stride = (UINT)m_VertexStride;
    UINT Offset = 0;

    g_pd3dContext->IASetVertexBuffers( 0, 1, &m_pVertexBuffer, &Stride, &Offset );
    g_pd3dContext->IASetIndexBuffer( m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );

    m_bStreamsBound = TRUE;
}

//=========================================================================

void runtime_vertex_mgr::Draw( s32 iVertex, s32 iIndex, s32 nIndices, D3D11_PRIMITIVE_TOPOLOGY Topology )
{
    if( nIndices <= 0 )
        return;

    Flush();

    if( !g_pd3dDevice || !g_pd3dContext )
    {
        m_bHasDrawn = TRUE;
        return;
    }

    Bind();
    g_pd3dContext->IASetPrimitiveTopology( Topology );
    g_pd3dContext->DrawIndexed( nIndices, iIndex, iVertex );

    m_bHasDrawn = TRUE;
}