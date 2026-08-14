//=========================================================================
//
//  SoftVertexMgr.hpp
//
//=========================================================================

#ifndef SOFT_VERTEX_MANAGER_HPP
#define SOFT_VERTEX_MANAGER_HPP

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"

//=========================================================================
// INCLUDES
//=========================================================================

#include "VertexMgr.hpp"

//=========================================================================
// CLASS
//=========================================================================

struct SkinGpuVertex
{
    vector4 Position;
    vector4 Normal;
    vector4 UVWeights;
};

class SoftVertexMgr
{
public:
    enum
    {
        INVALID_BONE_REMAP = 0xFFFF,
        MAX_BONE_PALETTE = 96
    };
    
    struct SoftSection
    {
        s32 FirstIndex;
        s32 IndexCount;
        u16 BoneRemap[MAX_BONE_PALETTE];
    
        SoftSection( void );
    };
    
    struct PreparedMesh
    {
        VertexMgr::PreparedMesh VertexData;
        xarray<SoftSection>     Sections;
    
        void Clear( void );
    };
    
    struct SectionInput
    {
        s32        FirstIndex;
        s32        IndexCount;
        u16 const* pBoneRemap;
        s32        BoneCount;
    };
    
    struct MeshView
    {
        VertexMgr::mesh_range Geometry;
        SoftSection const*    pSections;
        s32                   SectionCount;
    
        MeshView( void ) : Geometry(), pSections( NULL ), SectionCount( 0 )
        {
        }
    };
    
public:
    void Init ( void );
    void Kill ( void );
    
    static xbool PrepareMesh ( PreparedMesh& prepared, SkinGpuVertex const* pVertex, s32 nVertices, u16 const* pIndex,
                               s32 nIndices, s32 nBones, SectionInput const* pSections, s32 nSections );
    
    xhandle AddMesh    ( PreparedMesh const& prepared );
    void    RemoveMesh ( xhandle hMesh );
    
    xbool BindPools   ( VertexMgr::mesh_range const& range ) const;
    xbool GetMeshView ( xhandle hMesh, MeshView& view ) const;
    
private:
    struct SkinMesh
    {
        xhandle             hGeometry;
        xarray<SoftSection> Sections;
    
        SkinMesh( void ) : hGeometry(), Sections()
        {
            hGeometry.Handle = HNULL;
        }
    };
    
private:
    VertexMgr         m_vertexMgr;
    xharray<SkinMesh> m_meshes;
};

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

extern SoftVertexMgr g_SkinVertMgr;

//=========================================================================
#endif // SOFT_VERTEX_MANAGER_HPP
//=========================================================================
