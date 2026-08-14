//=========================================================================
//
//  SoftVertexMgr.cpp
//
//=========================================================================

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"

//=========================================================================
// INCLUDES
//=========================================================================

#include "SoftVertexMgr.hpp"

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

SoftVertexMgr g_SkinVertMgr;

//=========================================================================
// PREPARED DATA
//=========================================================================

SoftVertexMgr::SoftSection::SoftSection( void ) : FirstIndex( 0 ), IndexCount( 0 )
{
    for ( s32 i = 0; i < MAX_BONE_PALETTE; ++i )
    {
        BoneRemap[i] = INVALID_BONE_REMAP;
    }
}

//=========================================================================

void SoftVertexMgr::PreparedMesh::Clear( void )
{
    VertexData.Clear();
    Sections.Clear();
}

//=========================================================================
// IMPLEMENTATION
//=========================================================================

void SoftVertexMgr::Init( void )
{
    m_vertexMgr.Init( sizeof( SkinGpuVertex ) );
}

//=========================================================================

void SoftVertexMgr::Kill( void )
{
    m_meshes.Clear();
    m_vertexMgr.Kill();
}

//=========================================================================

xbool SoftVertexMgr::PrepareMesh( PreparedMesh& prepared, SkinGpuVertex const* pVertex, s32 nVertices,
                                  u16 const* pIndex, s32 nIndices, s32 nBones, SectionInput const* pSections,
                                  s32 nSections )
{
    prepared.Clear();

    if ( ( nBones <= 0 ) || ( nSections <= 0 ) || !pSections )
    {
        return FALSE;
    }

    if ( !VertexMgr::PrepareMesh( prepared.VertexData, pVertex, nVertices, pIndex, nIndices, sizeof( SkinGpuVertex ) ) )
    {
        return FALSE;
    }

    for ( s32 i = 0; i < nSections; ++i )
    {
        SectionInput const& input = pSections[i];
        if ( ( input.FirstIndex < 0 ) || ( input.IndexCount <= 0 ) || ( input.FirstIndex > nIndices ) ||
             ( input.IndexCount > nIndices - input.FirstIndex ) || ( input.BoneCount < 0 ) ||
             ( input.BoneCount > MAX_BONE_PALETTE ) || ( ( input.BoneCount > 0 ) && !input.pBoneRemap ) )
        {
            prepared.Clear();
            return FALSE;
        }

        SoftSection& section = prepared.Sections.Append();
        section.FirstIndex = input.FirstIndex;
        section.IndexCount = input.IndexCount;
        for ( s32 j = 0; j < input.BoneCount; ++j )
        {
            if ( input.pBoneRemap[j] != INVALID_BONE_REMAP && input.pBoneRemap[j] >= nBones )
            {
                prepared.Clear();
                return FALSE;
            }
            section.BoneRemap[j] = input.pBoneRemap[j];
        }
    }

    if ( prepared.Sections.GetCount() <= 0 )
    {
        prepared.Clear();
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xhandle SoftVertexMgr::AddMesh( PreparedMesh const& prepared )
{
    ASSERT( prepared.VertexData.IsValid() );
    ASSERT( prepared.Sections.GetCount() > 0 );

    if ( !prepared.VertexData.IsValid() || ( prepared.Sections.GetCount() <= 0 ) )
    {
        x_throw( "Invalid prepared skinned geometry" );
    }

    xhandle   hMesh;
    SkinMesh& mesh = m_meshes.Add( hMesh );

    mesh.hGeometry.Handle = HNULL;
    mesh.Sections.Clear();

    x_try;

    mesh.hGeometry = m_vertexMgr.AddMesh( prepared.VertexData );
    mesh.Sections = prepared.Sections;

    x_catch_begin;

    if ( mesh.hGeometry.IsNonNull() )
    {
        m_vertexMgr.RemoveMesh( mesh.hGeometry );
    }

    mesh.Sections.Clear();
    m_meshes.DeleteByHandle( hMesh );

    x_catch_end_ret;

    return hMesh;
}

//=========================================================================

void SoftVertexMgr::RemoveMesh( xhandle hMesh )
{
    SkinMesh& mesh = m_meshes( hMesh );

    m_vertexMgr.RemoveMesh( mesh.hGeometry );
    mesh.hGeometry.Handle = HNULL;
    mesh.Sections.Clear();
    m_meshes.DeleteByHandle( hMesh );
}

//=========================================================================

xbool SoftVertexMgr::BindPools( VertexMgr::mesh_range const& range ) const
{
    return m_vertexMgr.BindPools( range );
}

//=========================================================================

xbool SoftVertexMgr::GetMeshView( xhandle hMesh, MeshView& view ) const
{
    view = MeshView();

    SkinMesh const& mesh = m_meshes( hMesh );
    if ( mesh.hGeometry.IsNull() || ( mesh.Sections.GetCount() <= 0 ) ||
         !m_vertexMgr.GetMeshDrawRange( mesh.hGeometry, view.Geometry ) )
    {
        return FALSE;
    }

    view.pSections = mesh.Sections.GetPtr();
    view.SectionCount = mesh.Sections.GetCount();
    return TRUE;
}

//=========================================================================
