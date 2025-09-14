//=========================================================================
//
// Vertex Manager for PC
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

#include "VertexMgr.hpp"

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

vertex_mgr g_RigidVertMgr;

//=========================================================================
// IMPLEMENTATION
//=========================================================================

s32 vertex_mgr::NextLog2( u32 n )
{
    if( n == 0 ) return 0;
    f32 f  = (f32)((n << 1) - 1);
    u32 rn = *((u32*)&f);
    return (s32)((rn >> 23) - 127);
}

//=========================================================================

s32 vertex_mgr::GetHashEntry( s32 nItems, xbool bVertex )  
{ 
    s32 iHash;

    (void)( bVertex );
    iHash = iMin( (NUM_HASH_ENTRIES-1), iMax( 0, (NextLog2(nItems) - START_HASH_ENTRY) ) );   
    
    ASSERT( iHash >= 0 );
    ASSERT( iHash < NUM_HASH_ENTRIES );
    return iHash;
}

//=========================================================================

void vertex_mgr::Init( s32 VStride )
{
    s32 i;

    // Clear all the vertices
    for( i=0; i<NUM_HASH_ENTRIES; i++ )
    {
        m_VertHash[i].Handle = HNULL;
    }

    // Clear all the Indices
    for( i=0; i<NUM_HASH_ENTRIES; i++ )
    {
        m_IndexHash[i].Handle = HNULL;
    }

    // Init fields
    m_Stride = VStride;

    m_LastVertexPool.Handle = HNULL;
    m_LastIndexPool.Handle  = HNULL;
}


//=========================================================================

void vertex_mgr::RemoveNodeFormHash( xhandle hNode, xbool bVertex )
{
    node& Node      = m_lNode( hNode );
    s32   HashEntry = GetHashEntry( Node.Count, bVertex );

    if( Node.hHashNext.IsNull() == FALSE ) 
        m_lNode( Node.hHashNext ).hHashPrev = Node.hHashPrev;

    if( Node.hHashPrev.IsNull() == FALSE )
    {
        m_lNode( Node.hHashPrev ).hHashNext = Node.hHashNext;
    }
    else if( bVertex )
    {
        ASSERT( m_VertHash[HashEntry] == hNode );
        m_VertHash[HashEntry]  = Node.hHashNext;
    }
    else
    {
        ASSERT( m_IndexHash[HashEntry] == hNode );
        m_IndexHash[HashEntry] = Node.hHashNext;
    }

    Node.hHashPrev.Handle = HNULL;
    Node.hHashNext.Handle = HNULL;
    Node.Flags           |= FLAGS_FULL;
}

//=========================================================================

void vertex_mgr::AddNodeToHash( xhandle hNode, xbool bVertex )
{
    node& Node      = m_lNode( hNode );
    s32   HashEntry = GetHashEntry( Node.Count, bVertex );

    Node.hHashPrev.Handle = HNULL;
    Node.Flags &= ~FLAGS_FULL;

    if( bVertex )
    {
        Node.hHashNext          = m_VertHash[ HashEntry ];
        m_VertHash[ HashEntry ] = hNode;
    }
    else
    {
        Node.hHashNext           = m_IndexHash[ HashEntry ];
        m_IndexHash[ HashEntry ] = hNode;
    }

    if( Node.hHashNext.IsNull() == FALSE )
        m_lNode( Node.hHashNext ).hHashPrev = hNode;
}

//=========================================================================

xhandle vertex_mgr::AllocNode( s32 nItems, xbool bVertex, s32 Stride )
{
    xhandle     hNode;

    hNode.Handle = HNULL;
    ASSERT( Stride != 0 );

    //
    // Do search throw the hash table and see if we find a good node.
    // 
    for( s32 HashEntry = GetHashEntry( nItems, bVertex ); HashEntry < NUM_HASH_ENTRIES; HashEntry++ )
    {
        if( bVertex )
        {
            hNode = m_VertHash[ HashEntry ];
        }
        else
        {
            hNode = m_IndexHash[ HashEntry ];
        }

        for( ; hNode.IsNull() == FALSE; hNode = m_lNode( hNode ).hHashNext )
        {
            node& Node = m_lNode( hNode );

            if( Node.Count > nItems )
                break;
        }

        if( hNode.IsNull() == FALSE )
            break;
    }

    //
    // Okay we didn't find any hole in any of the pools to put this vertex set.
    // Here we have two options. One try to clean all vertex buffers (compact them)
    // Hopping to get some hole big enoft and then try to find the hole ones again or
    // just allocate another vertex pool.
    // DONE: For now we will just allocate another vertex pool.
    //
    if( hNode.IsNull() ) 
    {
        //
        // Create the physical pool
        //
        HRESULT         hr;
        xhandle         hPool;
        pool*           pPool;

        if( bVertex )
        {
            vertex_pool&    Pool = m_lVertexPool.Add( hPool );

            pPool           = &Pool;

            Pool.nItems     = MAX_VERTEX_POOL;
            Pool.Stride     = Stride;
            Pool.pBuffer    = NULL;

            if( g_pd3dDevice )
            {
                D3D11_BUFFER_DESC bd = {0};
                bd.Usage = D3D11_USAGE_DYNAMIC;
                bd.ByteWidth = Pool.Stride * Pool.nItems;
                bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

                hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &Pool.pBuffer );

                if( FAILED(hr) )
                {
                    m_lVertexPool.DeleteByHandle( hPool );
                    x_throw( "Unable to create vertex buffer in D3D11" );
                }
            }
            else
            {
                Pool.pBuffer = (ID3D11Buffer*)x_malloc( Pool.Stride * Pool.nItems );
            }
        }
        else
        {
            index_pool&    Pool = m_lIndexPool.Add( hPool );

            pPool           = &Pool;

            Pool.nItems     = MAX_INDEX_POOL;
            Pool.Stride     = Stride;
            Pool.pBuffer    = NULL;

            if( g_pd3dDevice )
            {
                D3D11_BUFFER_DESC bd = {0};
                bd.Usage = D3D11_USAGE_DYNAMIC;
                bd.ByteWidth = Pool.Stride * Pool.nItems;
                bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
                bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

                hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &Pool.pBuffer );

                ASSERT( Pool.Stride == sizeof(u16) );
                if( FAILED(hr) )
                {
                    m_lIndexPool.DeleteByHandle( hPool );
                    x_throw( "Unable to create index buffer in D3D11" );
                }
            }
            else
            {
                Pool.pBuffer = (ID3D11Buffer*)x_malloc( Pool.Stride * Pool.nItems );
            }
        }

        //
        // Add the empty node that represents the new pool
        //
        node&   EmptyNode  = m_lNode.Add( hNode );
        EmptyNode.Count    = pPool->nItems;
        EmptyNode.Offset   = 0;
        EmptyNode.hPool    = hPool;
        EmptyNode.Flags    = bVertex?FLAGS_VERTEX:0;
        EmptyNode.User     = -1;

        EmptyNode.hGlobalNext.Handle = HNULL;
        EmptyNode.hGlobalPrev.Handle = HNULL;

        AddNodeToHash( hNode, bVertex );

        pPool->hFirstNode = hNode;
    }

    //
    // Now we need to alloc the node and possibly add the rest of the empty space
    // back into a node. 
    //
    ASSERT( hNode.IsNull() == FALSE );

    //
    // Do a quick sanity check
    //
    {
        node& Node      = m_lNode( hNode );
        ASSERT( Node.Count >= nItems );
        ASSERT( Node.hGlobalNext.Handle == -1 || Node.hGlobalNext.Handle >= 0 );
        ASSERT( Node.hGlobalPrev.Handle == -1 || Node.hGlobalPrev.Handle >= 0 );
        ASSERT( Node.hHashNext.Handle   == -1 || Node.hHashNext.Handle   >= 0 );
        ASSERT( Node.hHashPrev.Handle   == -1 || Node.hHashPrev.Handle   >= 0 );
        ASSERT( (Node.Flags & FLAGS_FULL) == 0 );
    }

    //
    // Remove the node from the hash table
    //
    RemoveNodeFormHash( hNode, bVertex );

    //
    // Check whether we need to create an empty node or not. If so we must insert an
    // empty node into the list
    //
    s32   Bias      = 100;
    if( ( m_lNode( hNode ).Count - Bias ) > nItems )
    {        
        xhandle hEmptyNode;
        node&   EmptyNode  = m_lNode.Add( hEmptyNode );
        node&   Node       = m_lNode( hNode );
        EmptyNode.Count    = Node.Count  - nItems;
        EmptyNode.Offset   = Node.Offset + nItems;
        EmptyNode.Flags    = bVertex?FLAGS_VERTEX:0;
        EmptyNode.User     = -1;
        EmptyNode.hPool    = Node.hPool;

        EmptyNode.hGlobalNext = Node.hGlobalNext;
        EmptyNode.hGlobalPrev = hNode;
        Node.hGlobalNext = hEmptyNode;

        if( EmptyNode.hGlobalNext.IsNull() == FALSE )
            m_lNode( EmptyNode.hGlobalNext ).hGlobalPrev = hEmptyNode;

        AddNodeToHash( hEmptyNode, bVertex );

        Node.Count = nItems;
    }

    //
    // Set the number of items that were actually allocated
    //
    m_lNode( hNode ).User   = nItems;

    return hNode;
}

//=========================================================================

void* vertex_mgr::LockDListVerts( xhandle hDList )
{
    BYTE*           pDest = NULL;
    node&           Node  = m_lNode( m_lDList(hDList).hVertexNode );
    vertex_pool&    Pool  = m_lVertexPool( Node.hPool );
    ASSERT( (Node.Flags&FLAGS_VERTEX) == FLAGS_VERTEX );

    if( g_pd3dDevice && g_pd3dContext )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( Pool.pBuffer, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_throw( "Unable to map vertex buffer in D3D11" );
        }

        pDest = ((BYTE*)mappedResource.pData) + (Pool.Stride * Node.Offset);
        return pDest;
    }
    else
    {   
        pDest = ((BYTE*)Pool.pBuffer) + (Pool.Stride * Node.Offset);
        return pDest;
    }
}

//=========================================================================

void vertex_mgr::UnlockDListVerts( xhandle hDList )
{
    node&           Node  = m_lNode( m_lDList(hDList).hVertexNode );
    vertex_pool&    Pool  = m_lVertexPool( Node.hPool );
    ASSERT( (Node.Flags&FLAGS_VERTEX) == FLAGS_VERTEX );

    if( g_pd3dDevice && g_pd3dContext )
    {
        g_pd3dContext->Unmap( Pool.pBuffer, 0 );
    }
}

//=========================================================================

void* vertex_mgr::LockDListIndices( xhandle hDList, s32& Index )
{
    BYTE*        pDest = NULL;
    node&        IndexNode   = m_lNode( m_lDList(hDList).hIndexNode );
    node&        VertexNode  = m_lNode( m_lDList(hDList).hVertexNode );
    index_pool&  Pool  = m_lIndexPool( IndexNode.hPool );
    ASSERT( (IndexNode.Flags&FLAGS_VERTEX) == FALSE );

    if( g_pd3dDevice && g_pd3dContext )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( Pool.pBuffer, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_throw( "Unable to map index buffer in D3D11" );
        }

        Index = VertexNode.Offset;
        pDest = ((BYTE*)mappedResource.pData) + (Pool.Stride * IndexNode.Offset);
        return pDest;
    }
    else
    {
        Index = VertexNode.Offset;
        pDest = ((BYTE*)Pool.pBuffer) + (Pool.Stride * IndexNode.Offset);
        return pDest;
    }
}

//=========================================================================

void vertex_mgr::UnlockDListIndices( xhandle hDList )
{
    node&           Node  = m_lNode( m_lDList(hDList).hIndexNode );
    index_pool&     Pool  = m_lIndexPool( Node.hPool );
    ASSERT( (Node.Flags&FLAGS_VERTEX) == FALSE );

    if( g_pd3dDevice && g_pd3dContext )
    {
        g_pd3dContext->Unmap( Pool.pBuffer, 0 );
    }
}

//=========================================================================

xhandle vertex_mgr::AllocVertexSet( s32 nVertices, s32 Stride )
{
    ASSERT( MAX_VERTEX_POOL == 0xffff );
    if( nVertices > 0xffff )
        x_throw( "Unable to allocated object that have more than 65,000 vertices" );

    return AllocNode( nVertices, TRUE, Stride );
}

//=========================================================================

xhandle vertex_mgr::AllocIndexSet( s32 nIndices )
{
    if( nIndices > (MAX_INDEX_POOL/3) )
        x_throw( xfs( "Unable to allocated object that have more than %d facets", (MAX_INDEX_POOL/3) ) );

    return AllocNode( nIndices, FALSE, sizeof(u16) );
}

//=========================================================================

void vertex_mgr::FreeNode( xhandle hNode, xbool bVertex )
{
    //
    // First thing lets do any merging
    //
    {
        node&   Node  = m_lNode( hNode );

        //
        // Can we merge on the right?
        //
        if( Node.hGlobalNext.IsNull() == FALSE && (m_lNode( Node.hGlobalNext ).Flags & FLAGS_FULL) == 0 ) 
        {
            xhandle hDelNode = Node.hGlobalNext;
            node&   DelNode  = m_lNode( Node.hGlobalNext );

            RemoveNodeFormHash( hDelNode, bVertex );

            Node.Count += DelNode.Count;
            Node.hGlobalNext = DelNode.hGlobalNext;

            if( Node.hGlobalNext.IsNull() == FALSE )
                m_lNode( Node.hGlobalNext ).hGlobalPrev = hNode;

            m_lNode.DeleteByHandle( hDelNode );        
        }

        //
        // Can we merge Left
        //
        if( Node.hGlobalPrev.IsNull() == FALSE && (m_lNode( Node.hGlobalPrev ).Flags & FLAGS_FULL) == 0 ) 
        {
            xhandle hNewNode = Node.hGlobalPrev;
            node&   NewNode  = m_lNode( Node.hGlobalPrev );

            RemoveNodeFormHash( hNewNode, bVertex );

            NewNode.Count += Node.Count;

            NewNode.hGlobalNext = Node.hGlobalNext;

            if( Node.hGlobalNext.IsNull() == FALSE )
                m_lNode( NewNode.hGlobalNext ).hGlobalPrev = hNewNode;

            m_lNode.DeleteByHandle( hNode );        

            hNode = hNewNode;
        }
    }

    //
    // Now we must add our node to the hash
    //
    AddNodeToHash( hNode, bVertex );
}

//=========================================================================

void vertex_mgr::DelDList( xhandle hDList )
{
    dlist&  DList = m_lDList( hDList );

    if( DList.hVertexNode.IsNull() == FALSE )
        FreeNode( DList.hVertexNode, TRUE );

    if( DList.hIndexNode.IsNull() == FALSE )
        FreeNode( DList.hIndexNode, FALSE );

    m_lDList.DeleteByHandle( hDList );
}

//=========================================================================

xhandle vertex_mgr::AddDList( void* pVertex, s32 nVertices, u16* pIndex, s32 nIndices, s32 nPrims )
{
    xhandle hDList;
    dlist&  DList = m_lDList.Add( hDList );

    ASSERT( m_Stride > 0 );
    ASSERT( nPrims*3 == nIndices );
    DList.hVertexNode.Handle = HNULL;
    DList.hIndexNode.Handle  = HNULL;
    DList.nPrims             = nPrims;

    x_try;

    DList.hIndexNode  = AllocIndexSet   ( nIndices );
    DList.hVertexNode = AllocVertexSet  ( nVertices, m_Stride );

    // Copy the verts
    {
        void* pNewVert;
        pNewVert = LockDListVerts( hDList );
        x_memcpy( pNewVert, pVertex, m_Stride*nVertices );
        UnlockDListVerts( hDList );
    }

    // Copy the Indices
    {
        u16* pNewIndex;
        node& VNode = m_lNode( DList.hVertexNode );
        node& INode = m_lNode( DList.hIndexNode );
        s32   Index;

        pNewIndex = (u16*)LockDListIndices( hDList, Index );
        for( s32 i=0; i<nIndices; i++ )
        {
            ASSERT( ((s32)pIndex[i] + VNode.Offset) < MAX_VERTEX_POOL ) ;
            ASSERT( pIndex[i] < VNode.User );
            pNewIndex[i] = pIndex[i] + VNode.Offset;
        }
        UnlockDListIndices( hDList );
    }

    x_catch_begin;
    
    DelDList( hDList );

    x_catch_end_ret;

    return hDList;
}

//=========================================================================

void vertex_mgr::InvalidateCache( void )
{
    m_LastVertexPool.Handle = HNULL;
    m_LastIndexPool.Handle  = HNULL;
}

//=========================================================================

void vertex_mgr::BeginRender( void )
{        
    InvalidateCache();
}

//=========================================================================

void vertex_mgr::ActivateStreams( xhandle hDList )
{
    dlist& DList  = m_lDList( hDList );
    node&  Vertex = m_lNode ( DList.hVertexNode );
    node&  Index  = m_lNode ( DList.hIndexNode  );

    if( Vertex.hPool != m_LastVertexPool )
    {
        vertex_pool& Pool = m_lVertexPool( Vertex.hPool );
        if( g_pd3dDevice && g_pd3dContext )
        {
            UINT stride = Pool.Stride;
            UINT offset = 0;
            g_pd3dContext->IASetVertexBuffers( 0, 1, &Pool.pBuffer, &stride, &offset );
        }
        m_LastVertexPool = Vertex.hPool;
    }

    if( Index.hPool != m_LastIndexPool )
    {
        index_pool& Pool = m_lIndexPool( Index.hPool );
        if( g_pd3dDevice && g_pd3dContext )
        {
            g_pd3dContext->IASetIndexBuffer( Pool.pBuffer, DXGI_FORMAT_R16_UINT, 0 );
        }
        m_LastIndexPool = Index.hPool;
    }
}

//=========================================================================

// TODO: GS: Now shaders and settings are set for each DList separately, am I doing it right?
// IDK, maybe it shoud be reworked.

void vertex_mgr::DrawDList( xhandle hDList, const matrix4* pWorld, const d3d_lighting* pLighting )
{
    ASSERT( hDList.Handle >= 0 );

    dlist& DList  = m_lDList( hDList );
    node&  Vertex = m_lNode ( DList.hVertexNode );
    node&  Index  = m_lNode ( DList.hIndexNode  );

    ActivateStreams( hDList );
    
    if( g_pd3dDevice && g_pd3dContext )
    {
        ASSERT( DList.nPrims*3 == Index.User );
        ASSERT( (Index.Offset + Index.User) < MAX_INDEX_POOL );
        
        g_pd3dContext->DrawIndexed( Index.User, Index.Offset, 0 );
    }
}

//=========================================================================

void vertex_mgr::ApplyLightmapColors( xhandle hDList, const u32* pColors, s32 nColors, s32 iColor )
{
    if( !pColors || !nColors )
        return;
       
    //x_DebugMsg( "VertexMGR: ApplyLightmapColors: offset %d count %d\n", iColor, nColors );
	   
    BYTE* pVertData = (BYTE*)LockDListVerts( hDList );
    if( pVertData )
    {
        const u32* pColorData = pColors + iColor;
        
        for( s32 i = 0; i < nColors; i++ )
        {
            // Color field at offset 24 in each vertex
            BYTE* pVertexStart = pVertData + (i * m_Stride);
            xcolor* pColor = (xcolor*)(pVertexStart + 24);
            
            u32 color = pColorData[i];
            *pColor = xcolor( (color >> 16) & 0xFF,  // R
                              (color >> 8)  & 0xFF,  // G
                               color & 0xFF,         // B
                              (color >> 24) & 0xFF); // A
        }        
        UnlockDListVerts( hDList );
    }
}

//=========================================================================

void vertex_mgr::Kill( void )
{
    s32 i;

    for( i=0; i<m_lIndexPool.GetCount(); i++ )
    {
        if( m_lIndexPool[i].pBuffer ) 
        {
            if( g_pd3dDevice )
            {
                m_lIndexPool[i].pBuffer->Release();
                m_lIndexPool[i].pBuffer = NULL;
            }
            else
            {
                x_free( m_lIndexPool[i].pBuffer );
                m_lIndexPool[i].pBuffer = NULL;
            }
        }
    }

    for( i=0; i<m_lVertexPool.GetCount(); i++ )
    {
        if( m_lVertexPool[i].pBuffer ) 
        {
            if( g_pd3dDevice )
            {
                m_lVertexPool[i].pBuffer->Release();
                m_lVertexPool[i].pBuffer = NULL;
            }
            else
            {
                x_free(m_lVertexPool[i].pBuffer);
                m_lVertexPool[i].pBuffer = NULL;
            }
        }
    }

    m_lIndexPool.Clear();
    m_lVertexPool.Clear();
}