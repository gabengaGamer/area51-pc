//=========================================================================
//
//  VertexMgr.hpp
//
//=========================================================================

#ifndef VERTEX_MANAGER_HPP
#define VERTEX_MANAGER_HPP

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_files.hpp"

//=========================================================================
// INCLUDES
//=========================================================================

#include "x_array.hpp"
#include "e_RenderBuffer.hpp"

//=========================================================================
// CLASS
//=========================================================================

class VertexMgr
{
public:
    struct PreparedMesh
    {
        PreparedMesh ( void );
    
        xbool Allocate ( s32 nVertices, s32 nIndices, s32 vertexStride );
        void  Clear    ( void );
        xbool IsValid  ( void ) const;
    
        void*       GetVertexData ( void );
        void const* GetVertexData ( void ) const;
        u16*        GetIndexData  ( void );
        u16 const*  GetIndexData  ( void ) const;
    
        s32 GetVertexCount  ( void ) const;
        s32 GetIndexCount   ( void ) const;
        s32 GetVertexStride ( void ) const;
    
private:
        xarray<byte> m_vertices;
        xarray<u16>  m_indices;
        s32          m_vertexCount;
        s32          m_vertexStride;
    };
    
    // Indices remain local to the mesh. Draws bind the owning pools at offset
    // zero and select the vertex suballocation through BaseVertex. Pool IDs are
    // part of the range so callers can form multi-draw runs without rebinding
    // buffers for every mesh.
    struct mesh_range
    {
        s32 FirstIndex;
        s32 IndexCount;
        s32 BaseVertex;
        s32 VertexPool;
        s32 IndexPool;
    
        mesh_range( void ) : FirstIndex( 0 ), IndexCount( 0 ), BaseVertex( 0 ), VertexPool( -1 ), IndexPool( -1 )
        {
        }
    };
    
public:
    VertexMgr  ( void );
    ~VertexMgr ( void );
    
    void Init ( s32 vertexStride );
    void Kill ( void );
    
    static xbool PrepareMesh ( PreparedMesh& prepared, void const* pVertex, s32 nVertices, u16 const* pIndex,
                               s32 nIndices, s32 vertexStride );
    
    xhandle AddMesh    ( PreparedMesh const& prepared );
    void    RemoveMesh ( xhandle hMesh );
    
    xbool BindMesh          ( xhandle hMesh ) const;
    xbool BindPools         ( mesh_range const& range ) const;
    xbool BindVertexIndices ( mesh_range const& range, u32 slot ) const;
    xbool GetMeshDrawRange  ( xhandle hMesh, mesh_range& range ) const;

private:
    enum
    {
        DEFAULT_VERTEX_POOL_CAPACITY = 65536,
        DEFAULT_INDEX_POOL_CAPACITY = 65536,
        MAX_MESH_VERTICES = 65536
    };
    
    struct free_range
    {
        s32 Offset;
        s32 Count;
    
        free_range( void ) : Offset( 0 ), Count( 0 )
        {
        }
    };
    
    struct pool
    {
        rbuffer            Buffer;
        rbuffer            VertexIndexBuffer;
        s32                Capacity;
        s32                Stride;
        xarray<free_range> FreeRanges;
    
        pool( void );
        ~pool( void );
    
        pool( pool const& ) = delete;
        pool& operator=( pool const& ) = delete;
    };
    
    // Render-buffer backends retain their owner's address. The pools therefore
    // live separately from the pointer array that indexes them.
    using PoolArray = xarray<pool*>;
    
    struct allocation
    {
        s32 PoolIndex;
        s32 Offset;
        s32 Count;
    
        allocation( void ) : PoolIndex( -1 ), Offset( 0 ), Count( 0 )
        {
        }
    
        xbool IsValid( void ) const
        {
            return ( PoolIndex >= 0 ) && ( Offset >= 0 ) && ( Count > 0 );
        }
    };
    
    struct mesh
    {
        allocation VertexAllocation;
        allocation IndexAllocation;
    };

private:
    xbool AllocateRange ( PoolArray& pools, s32 count, s32 stride, u32 usageFlags, s32 defaultCapacity,
                          char const* pDebugName, allocation& allocation );
    void  ReleaseRange  ( PoolArray& pools, allocation const& allocation );
    xbool CreatePool    ( PoolArray& pools, s32 capacity, s32 stride, u32 usageFlags, char const* pDebugName );

private:
    xharray<mesh> m_meshes;
    PoolArray     m_vertexPools;
    PoolArray     m_indexPools;
    s32           m_vertexStride;
    xbool         m_isInitialized;
};

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

extern VertexMgr g_RigidVertMgr;

//=========================================================================
#endif // VERTEX_MANAGER_HPP
//=========================================================================
