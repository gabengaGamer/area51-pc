//=========================================================================
//
//  RuntimeVertexMgr.hpp
//
//=========================================================================

#ifndef RUNTIME_VERTEX_MANAGER_HPP
#define RUNTIME_VERTEX_MANAGER_HPP

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"

//=========================================================================
// INCLUDES
//=========================================================================

#include "e_RenderBuffer.hpp"
#include "e_RenderDraw.hpp"

//=========================================================================
// CLASS
//=========================================================================

class RuntimeVertexMgr
{
public:
    struct primitive
    {
        void const* pVertex;
        u16 const*  pIndex;
        s32         nVertices;
        s32         nIndices;
    
        primitive( void ) : pVertex( NULL ), pIndex( NULL ), nVertices( 0 ), nIndices( 0 )
        {
        }
    };

public:
    RuntimeVertexMgr( void );
    
    xbool Init ( s32 vStride, s32 initialVertices = 8192, s32 initialIndices = 24576 );
    void  Kill ( void );
    
    xbool Reserve         ( s32 nVertices, s32 nIndices );
    xbool UploadPrimitive ( primitive const& primitive );
    xbool BindBuffers     ( void ) const;
    xbool DrawIndexed     ( s32 indexCount, s32 startIndex, s32 baseVertex ) const;

protected:
    xbool EnsureCapacity ( s32 requiredVertices, s32 requiredIndices );
    xbool CreateBuffers  ( s32 maxVertices, s32 maxIndices );
    void  DestroyBuffers ( void );

protected:
    rbuffer m_vertexBuffer;
    rbuffer m_indexBuffer;
    
    s32   m_vertexStride;
    s32   m_maxVertices;
    s32   m_MaxIndices;
    s32   m_uploadedVertices;
    s32   m_uploadedIndices;
    xbool m_isInitialized;
};

//=========================================================================
#endif // RUNTIME_VERTEX_MANAGER_HPP
//=========================================================================
