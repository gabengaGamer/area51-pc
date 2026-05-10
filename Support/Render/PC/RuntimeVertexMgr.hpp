//=========================================================================
//
//  Runtime Vertex Manager for PC
//
//=========================================================================

#ifndef RUNTIME_VERTEX_MANAGER_HPP
#define RUNTIME_VERTEX_MANAGER_HPP

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

#include "Entropy.hpp"

//=========================================================================
// CLASS
//=========================================================================

class runtime_vertex_mgr
{

//=========================================================================

public:

    struct primitive
    {
        const void*                 pVertex;
        const u16*                  pIndex;
        s32                         nVertices;
        s32                         nIndices;
        D3D11_PRIMITIVE_TOPOLOGY    Topology;
    };

//=========================================================================

public:

                runtime_vertex_mgr   ( void );

    void        Init                    ( s32 VStride,
                                          s32 MaxVertices = 8192,
                                          s32 MaxIndices  = 24576 );
    void        Kill                    ( void );
    void        BeginRender             ( void );
    xbool       DrawPrimitive           ( const primitive& Primitive );

    s32         GetVertexStride         ( void ) const;
    s32         GetMaxVertices          ( void ) const;
    s32         GetMaxIndices           ( void ) const;

protected:

    struct draw_buffer
    {
        void*   pVertex;
        u16*    pIndex;
        s32     iVertex;
        s32     iIndex;
        s32     nVertices;
        s32     nIndices;
    };

    xbool       MapBuffers              ( D3D11_MAP MapType );
    void        UnmapBuffers            ( void );
    xbool       EnsureSpace             ( s32 nVertices,
                                          s32 nIndices );
    xbool       Alloc                   ( s32 nVertices,
                                          s32 nIndices,
                                          draw_buffer& DrawBuffer );
    void        Flush                   ( void );
    void        Bind                    ( void );
    void        Draw                    ( s32 iVertex,
                                          s32 iIndex,
                                          s32 nIndices,
                                          D3D11_PRIMITIVE_TOPOLOGY Topology );

protected:

    ID3D11Buffer*   m_pVertexBuffer;
    ID3D11Buffer*   m_pIndexBuffer;
    byte*           m_pSystemVertexData;
    u16*            m_pSystemIndexData;
    byte*           m_pMappedVertexData;
    u16*            m_pMappedIndexData;

    s32             m_VertexStride;
    s32             m_MaxVertices;
    s32             m_MaxIndices;
    s32             m_iVertex;
    s32             m_iIndex;

    xbool           m_bMapped;
    xbool           m_bHasDrawn;
    xbool           m_bStreamsBound;
};

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

extern runtime_vertex_mgr g_RuntimeVertexMgr;

//=========================================================================
// INLINE
//=========================================================================

inline s32 runtime_vertex_mgr::GetVertexStride( void ) const
{
    return m_VertexStride;
}

//=========================================================================

inline s32 runtime_vertex_mgr::GetMaxVertices( void ) const
{
    return m_MaxVertices;
}

//=========================================================================

inline s32 runtime_vertex_mgr::GetMaxIndices( void ) const
{
    return m_MaxIndices;
}

//=========================================================================
#endif
//=========================================================================
