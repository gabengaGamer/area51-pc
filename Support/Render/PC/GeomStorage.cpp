//=============================================================================
//
//  GeomStorage.cpp
//
//=============================================================================

//=============================================================================
//  INCLUDES
//=============================================================================

#include "GeomStorage.hpp"
#include "SoftVertexMgr.hpp"
#include "VertexMgr.hpp"
#include "x_workers.hpp"

//=============================================================================
//  STORAGE
//=============================================================================

GeomStorage g_GeomStorage;

//=============================================================================
//  PREPARE JOBS
//=============================================================================

namespace
{
struct RigidPrepareJob
{
    rigid_geom const*       pGeom;
    s32                     iSurface;
    VertexMgr::PreparedMesh Prepared;
    xbool                   Success;
};

struct SkinPrepareJob
{
    skin_geom const*            pGeom;
    s32                         iSurface;
    SoftVertexMgr::PreparedMesh Prepared;
    xbool                       Success;
};

//=========================================================================

void PrepareRigidSurface( void* pData, s32 iJob )
{
    RigidPrepareJob* pJob = static_cast<RigidPrepareJob*>( pData );

    static_cast<void>( iJob );

    ASSERT( pJob );
    if ( !pJob )
    {
        return;
    }

    pJob->Success = FALSE;
    pJob->Prepared.Clear();

    ASSERT( pJob->pGeom );
    if ( !pJob->pGeom )
    {
        return;
    }

    rigid_geom const& geom = *pJob->pGeom;
    if ( pJob->iSurface < 0 || pJob->iSurface >= geom.m_nSubMeshes )
    {
        return;
    }

    geom::submesh const& surface = geom.m_pSubMesh[pJob->iSurface];
    if ( surface.nSections != 1 )
    {
        return;
    }

    rigid_geom::section const& section = geom.m_pSection[surface.iSection];
    ASSERT( geom.m_pVertex );
    ASSERT( geom.m_pIndex );
    ASSERT( section.nVertices > 0 );
    ASSERT( section.nIndices > 0 );

    if ( !geom.m_pVertex || !geom.m_pIndex || ( section.nVertices <= 0 ) || ( section.nVertices > 65536 ) ||
         ( section.nIndices <= 0 ) )
    {
        return;
    }

    if ( !pJob->Prepared.Allocate( section.nVertices, section.nIndices, sizeof( rigid_geom::vertex ) ) )
    {
        return;
    }

    x_memcpy( pJob->Prepared.GetVertexData(), geom.m_pVertex + section.FirstVertex,
              sizeof( rigid_geom::vertex ) * section.nVertices );

    u16* pIndices = pJob->Prepared.GetIndexData();
    for ( s32 i = 0; i < section.nIndices; ++i )
    {
        u32 const sourceIndex = geom.m_pIndex[section.FirstIndex + i];
        if ( sourceIndex < static_cast<u32>( section.FirstVertex ) ||
             sourceIndex >= static_cast<u32>( section.FirstVertex + section.nVertices ) )
        {
            pJob->Prepared.Clear();
            return;
        }

        pIndices[i] = static_cast<u16>( sourceIndex - section.FirstVertex );
    }

    pJob->Success = pJob->Prepared.IsValid();
    if ( !pJob->Success )
    {
        pJob->Prepared.Clear();
    }
}

//=========================================================================

void PrepareSkinSurface( void* pData, s32 iJob )
{
    SkinPrepareJob* pJob = static_cast<SkinPrepareJob*>( pData );

    static_cast<void>( iJob );

    ASSERT( pJob );
    if ( !pJob )
    {
        return;
    }

    pJob->Success = FALSE;
    pJob->Prepared.Clear();

    ASSERT( pJob->pGeom );
    if ( !pJob->pGeom )
    {
        return;
    }

    skin_geom const& geom = *pJob->pGeom;
    if ( pJob->iSurface < 0 || pJob->iSurface >= geom.m_nSubMeshes )
    {
        return;
    }

    geom::submesh const& surface = geom.m_pSubMesh[pJob->iSurface];
    if ( surface.nSections <= 0 )
    {
        return;
    }

    skin_geom::section const& firstSection = geom.m_pSection[surface.iSection];
    if ( firstSection.nVertices <= 0 || firstSection.nVertices > 65536 )
    {
        return;
    }

    s32 nIndices = 0;
    for ( s32 i = 0; i < surface.nSections; ++i )
    {
        skin_geom::section const& section = geom.m_pSection[surface.iSection + i];
        if ( section.FirstVertex != firstSection.FirstVertex || section.nVertices != firstSection.nVertices ||
             section.nIndices <= 0 )
        {
            return;
        }

        nIndices += section.nIndices;
    }

    xarray<SkinGpuVertex>               vertices;
    xarray<u16>                         indices;
    xarray<SoftVertexMgr::SectionInput> sections;
    vertices.SetCount( firstSection.nVertices );
    indices.SetCount( nIndices );
    sections.SetCount( surface.nSections );

    for ( s32 i = 0; i < firstSection.nVertices; ++i )
    {
        skin_geom::vertex const& source = geom.m_pVertex[firstSection.FirstVertex + i];
        SkinGpuVertex&           destination = vertices[i];
        destination.Position.Set( source.Position.X, source.Position.Y, source.Position.Z,
                                  static_cast<f32>( source.Bones[0] ) );
        destination.Normal.Set( source.Normal.X, source.Normal.Y, source.Normal.Z,
                                static_cast<f32>( source.Bones[1] ) );
        destination.UVWeights.Set( source.UV.X, source.UV.Y, source.Weights.X, source.Weights.Y );
    }

    s32 firstIndex = 0;
    for ( s32 i = 0; i < surface.nSections; ++i )
    {
        skin_geom::section const&    sourceSection = geom.m_pSection[surface.iSection + i];
        SoftVertexMgr::SectionInput& destinationSection = sections[i];
        destinationSection.FirstIndex = firstIndex;
        destinationSection.IndexCount = sourceSection.nIndices;
        destinationSection.pBoneRemap = geom.m_pBonePalette + sourceSection.FirstBone;
        destinationSection.BoneCount = sourceSection.nBones;

        for ( s32 j = 0; j < sourceSection.nIndices; ++j )
        {
            u32 const sourceIndex = geom.m_pIndex[sourceSection.FirstIndex + j];
            if ( sourceIndex < static_cast<u32>( firstSection.FirstVertex ) ||
                 sourceIndex >= static_cast<u32>( firstSection.FirstVertex + firstSection.nVertices ) )
            {
                return;
            }

            indices[firstIndex + j] = static_cast<u16>( sourceIndex - firstSection.FirstVertex );
        }

        firstIndex += sourceSection.nIndices;
    }

    pJob->Success =
        SoftVertexMgr::PrepareMesh( pJob->Prepared, vertices.GetPtr(), vertices.GetCount(), indices.GetPtr(),
                                    indices.GetCount(), geom.m_nBones, sections.GetPtr(), sections.GetCount() );
}
} // namespace

//=============================================================================
//  LIFETIME
//=============================================================================

GeomStorage::GeomStorage( void ) : m_pRigidVertices( NULL ), m_pSkinVertices( NULL )
{
}

//=============================================================================

void GeomStorage::Init( VertexMgr& rigidVertices, SoftVertexMgr& skinVertices )
{
    ASSERT( !m_pRigidVertices && !m_pSkinVertices );
    m_pRigidVertices = &rigidVertices;
    m_pSkinVertices = &skinVertices;

    m_rigidGeoms.Clear();
    m_skinGeoms.Clear();
}

//=============================================================================

void GeomStorage::Kill( void )
{
    ASSERT( m_rigidGeoms.GetCount() == 0 );
    ASSERT( m_skinGeoms.GetCount() == 0 );

    m_rigidGeoms.Clear();
    m_skinGeoms.Clear();

    m_pRigidVertices = NULL;
    m_pSkinVertices = NULL;
}

//=============================================================================
//  REGISTRATION
//=============================================================================

xhandle GeomStorage::AddRigid( rigid_geom const& geom )
{
    xarray<RigidPrepareJob> jobs;
    xarray<xhandle>         surfaces;
    xhandle                 hJob;
    xhandle                 hGeom;
    hJob.Handle = HNULL;
    hGeom.Handle = HNULL;

    x_try;

    s32 const nSurfaces = geom.m_nSubMeshes;
    if ( nSurfaces > 0 )
    {
        ASSERTS( x_WorkersIsInit(), "Rigid geom registration requires x_workers" );
        if ( !x_WorkersIsInit() )
        {
            x_throw( "Rigid geom registration requires x_workers" );
        }

        jobs.SetCount( nSurfaces );
        surfaces.SetCapacity( nSurfaces );

        for ( s32 i = 0; i < nSurfaces; ++i )
        {
            jobs[i].pGeom = &geom;
            jobs[i].iSurface = i;
            jobs[i].Success = FALSE;
        }

        if ( !x_WorkerJobSubmitBatch( PrepareRigidSurface, jobs.GetPtr(), sizeof( RigidPrepareJob ), nSurfaces,
                                      "RigidVertexPrep", hJob ) )
        {
            x_throw( "Failed to submit rigid vertex prepare job" );
        }

        x_WorkerJobWait( hJob );
        x_WorkerJobRelease( hJob );
        hJob.Handle = HNULL;

        for ( s32 i = 0; i < nSurfaces; ++i )
        {
            if ( !jobs[i].Success )
            {
                x_throw( "Rigid vertex prepare job failed" );
            }

            ASSERT( m_pRigidVertices );
            surfaces.Append() = m_pRigidVertices->AddMesh( jobs[i].Prepared );
            jobs[i].Prepared.Clear();
        }
    }

    GeomResource& resource = m_rigidGeoms.Add( hGeom );
    resource.m_surfaces = surfaces;

    x_catch_begin;

    if ( hJob.IsNonNull() )
    {
        x_WorkerJobWait( hJob );
        x_WorkerJobRelease( hJob );
    }

    for ( s32 i = 0; i < surfaces.GetCount(); ++i )
    {
        m_pRigidVertices->RemoveMesh( surfaces[i] );
    }

    if ( hGeom.IsNonNull() )
    {
        m_rigidGeoms.DeleteByHandle( hGeom );
    }

    x_catch_end_ret;

    return hGeom;
}

//=============================================================================

xhandle GeomStorage::AddSkin( skin_geom const& geom )
{
    xarray<SkinPrepareJob> jobs;
    xarray<xhandle>        surfaces;
    xhandle                hJob;
    xhandle                hGeom;
    hJob.Handle = HNULL;
    hGeom.Handle = HNULL;

    x_try;

    s32 const nSurfaces = geom.m_nSubMeshes;
    if ( nSurfaces > 0 )
    {
        ASSERTS( x_WorkersIsInit(), "Skin geom registration requires x_workers" );
        if ( !x_WorkersIsInit() )
        {
            x_throw( "Skin geom registration requires x_workers" );
        }

        jobs.SetCount( nSurfaces );
        surfaces.SetCapacity( nSurfaces );

        for ( s32 i = 0; i < nSurfaces; ++i )
        {
            jobs[i].pGeom = &geom;
            jobs[i].iSurface = i;
            jobs[i].Success = FALSE;
        }

        if ( !x_WorkerJobSubmitBatch( PrepareSkinSurface, jobs.GetPtr(), sizeof( SkinPrepareJob ), nSurfaces,
                                      "SkinVertexPrep", hJob ) )
        {
            x_throw( "Failed to submit skin vertex prepare job" );
        }

        x_WorkerJobWait( hJob );
        x_WorkerJobRelease( hJob );
        hJob.Handle = HNULL;

        for ( s32 i = 0; i < nSurfaces; ++i )
        {
            if ( !jobs[i].Success )
            {
                x_throw( "Skin vertex prepare job failed" );
            }

            ASSERT( m_pSkinVertices );
            surfaces.Append() = m_pSkinVertices->AddMesh( jobs[i].Prepared );
            jobs[i].Prepared.Clear();
        }
    }

    GeomResource& resource = m_skinGeoms.Add( hGeom );
    resource.m_surfaces = surfaces;

    x_catch_begin;

    if ( hJob.IsNonNull() )
    {
        x_WorkerJobWait( hJob );
        x_WorkerJobRelease( hJob );
    }

    for ( s32 i = 0; i < surfaces.GetCount(); ++i )
    {
        m_pSkinVertices->RemoveMesh( surfaces[i] );
    }

    if ( hGeom.IsNonNull() )
    {
        m_skinGeoms.DeleteByHandle( hGeom );
    }

    x_catch_end_ret;

    return hGeom;
}

//=============================================================================

void GeomStorage::RemoveRigid( xhandle hGeom )
{
    GeomResource& resource = m_rigidGeoms( hGeom );
    for ( s32 i = 0; i < resource.m_surfaces.GetCount(); ++i )
    {
        m_pRigidVertices->RemoveMesh( resource.m_surfaces[i] );
    }

    resource.m_surfaces.Clear();
    m_rigidGeoms.DeleteByHandle( hGeom );
}

//=============================================================================

void GeomStorage::RemoveSkin( xhandle hGeom )
{
    GeomResource& resource = m_skinGeoms( hGeom );
    for ( s32 i = 0; i < resource.m_surfaces.GetCount(); ++i )
    {
        m_pSkinVertices->RemoveMesh( resource.m_surfaces[i] );
    }

    resource.m_surfaces.Clear();
    m_skinGeoms.DeleteByHandle( hGeom );
}

//=============================================================================
//  ACCESS
//=============================================================================

xhandle GeomStorage::GetRigidSurface( xhandle hGeom, s32 iSurface ) const
{
    GeomResource const& resource = m_rigidGeoms( hGeom );
    ASSERT( iSurface >= 0 && iSurface < resource.m_surfaces.GetCount() );
    return resource.m_surfaces[iSurface];
}

//=============================================================================

xhandle GeomStorage::GetSkinSurface( xhandle hGeom, s32 iSurface ) const
{
    GeomResource const& resource = m_skinGeoms( hGeom );
    ASSERT( iSurface >= 0 && iSurface < resource.m_surfaces.GetCount() );
    return resource.m_surfaces[iSurface];
}

//=============================================================================
