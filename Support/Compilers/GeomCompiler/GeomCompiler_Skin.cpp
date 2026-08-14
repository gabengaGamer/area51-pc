#include "GeomCompiler.hpp"
#include "GeomDesc.hpp"
#include "Render/GeomFile.hpp"
#include "Render/SkinGeom.hpp"
#include "RawAnim.hpp"

extern xbool g_Verbose;

void geom_compiler::ExportSkinGeom( const char* pFileName )
{
    // load the resource
    LoadResource( pFileName, TRUE );

    //
    // Load the raw-mesh
    //

    rawmesh2 RawMesh;

    rawanim RawAnim;
    LoadSourceMesh( m_pGeomRscDesc->GetSourceFileName(), RawMesh );
    LoadSourceAnimation( m_pGeomRscDesc->GetSourceFileName(), RawAnim );
    SetUserInfo( RawMesh.m_UserName, RawMesh.m_ComputerName );
    SetExportDate( RawMesh.m_ExportDate[0], RawMesh.m_ExportDate[1], RawMesh.m_ExportDate[2] );
    RemoveUnusedVMeshes( RawMesh );

    RawMesh.CleanWeights( 2, 0.00001f );

    // Do remapping of the skeleton
    RawAnim.DeleteDummyBones();
    RawMesh.ApplyNewSkeleton( RawAnim );
    
    // Show hierarchy?
    if( g_Verbose )
        RawAnim.PrintHierarchy();

    skin_geom SkinGeom;
    mesh      Mesh;
    BuildCompileModel( SkinGeom, RawMesh, Mesh, FALSE );
    BuildSkinGeometry( Mesh, SkinGeom );

    xstring Error;
    if( !geom_file::SaveSkin( m_OutputFile, SkinGeom, Error ) )
        ThrowError( xfs( "Unable to save v42 skin geometry: %s", (const char*)Error ) );

    X_FILE* pVerifyFile = x_fopen( m_OutputFile, "rb" );
    skin_geom* pVerifiedGeom = NULL;
    if( !pVerifyFile ||
        !geom_file::LoadSkin( pVerifyFile, pVerifiedGeom, Error ) )
    {
        if( pVerifyFile )
            x_fclose( pVerifyFile );
        ThrowError( xfs( "Unable to read back v42 skin geometry: %s",
                         (const char*)Error ) );
    }
    x_fclose( pVerifyFile );
    delete pVerifiedGeom;

    if( g_Verbose )
        PrintSummary( SkinGeom );
}

//=============================================================================

void geom_compiler::BuildSkinGeometry( mesh& Mesh, skin_geom& SkinGeom )
{
    if( Mesh.SubMesh.GetCount() == 0 )
        ThrowError( "Skin geometry contains no meshes" );

    const rawmesh2& RawMesh = *Mesh.SubMesh[0].pRawMesh;
    if( RawMesh.m_nBones > 96 )
        ThrowError( "Skin geometry exceeds the 96-bone runtime palette limit" );

    xarray<skin_geom::vertex> Vertices;
    xarray<u32>               Indices;
    xarray<s32>               VertexRemap;

    const s32 MaxFacetsPerSection = 21845;
    s32 nSubMeshes = 0;
    s32 nSections  = 0;
    for( s32 i = 0; i < Mesh.SubMesh.GetCount(); i++ )
    {
        const sub_mesh& SourceMesh = Mesh.SubMesh[i];
        nSubMeshes += SourceMesh.Surfaces.GetCount();
        for( s32 j = 0; j < SourceMesh.Surfaces.GetCount(); j++ )
        {
            nSections += (SourceMesh.Surfaces[j].Facets.GetCount() +
                          MaxFacetsPerSection - 1) / MaxFacetsPerSection;
        }
    }

    if( nSections == 0 )
        ThrowError( "Skin geometry contains no drawable surfaces" );

    SkinGeom.m_nMeshes    = Mesh.SubMesh.GetCount();
    SkinGeom.m_pMesh      = new geom::mesh[SkinGeom.m_nMeshes];
    SkinGeom.m_nSubMeshes = nSubMeshes;
    SkinGeom.m_pSubMesh   = new geom::submesh[nSubMeshes];
    SkinGeom.m_nSections  = nSections;
    SkinGeom.m_pSection   = new skin_geom::section[nSections];

    SkinGeom.m_nBonePalette = RawMesh.m_nBones;
    SkinGeom.m_pBonePalette = RawMesh.m_nBones > 0
                            ? new u16[RawMesh.m_nBones]
                            : NULL;
    for( s32 i = 0; i < RawMesh.m_nBones; i++ )
        SkinGeom.m_pBonePalette[i] = (u16)i;

    VertexRemap.SetCount( RawMesh.m_nVertices );
    SkinGeom.m_BBox.Clear();

    s32 iSubMesh = 0;
    s32 iSection = 0;
    for( s32 iMesh = 0; iMesh < Mesh.SubMesh.GetCount(); iMesh++ )
    {
        const sub_mesh& SourceMesh = Mesh.SubMesh[iMesh];
        geom::mesh&     TargetMesh = SkinGeom.m_pMesh[iMesh];

        TargetMesh.BBox.Clear();
        TargetMesh.NameOffset = m_Dictionary.Add( SourceMesh.Name );
        TargetMesh.iSubMesh   = iSubMesh;
        TargetMesh.nSubMeshes = SourceMesh.Surfaces.GetCount();
        TargetMesh.nBones     = SourceMesh.pRawSubMesh->nBones;
        TargetMesh.nFaces     = 0;
        TargetMesh.nVertices  = 0;

        for( s32 iSurface = 0;
             iSurface < SourceMesh.Surfaces.GetCount();
             iSurface++ )
        {
            const surface& SourceSurface = SourceMesh.Surfaces[iSurface];
            geom::submesh& Submesh = SkinGeom.m_pSubMesh[iSubMesh++];
            Submesh.iSection       = iSection;
            Submesh.nSections      = (SourceSurface.Facets.GetCount() +
                                      MaxFacetsPerSection - 1) /
                                      MaxFacetsPerSection;
            Submesh.iMaterial      = SourceSurface.iMaterial;
            Submesh.WorldPixelSize = 0.001f;

            for( s32 iFirstFacet = 0;
                 iFirstFacet < SourceSurface.Facets.GetCount();
                 iFirstFacet += MaxFacetsPerSection, iSection++ )
            {
                skin_geom::section& Section = SkinGeom.m_pSection[iSection];

                for( s32 i = 0; i < VertexRemap.GetCount(); i++ )
                    VertexRemap[i] = -1;

                Section.FirstVertex = Vertices.GetCount();
                Section.FirstIndex  = Indices.GetCount();
                Section.FirstBone   = 0;
                Section.nBones      = RawMesh.m_nBones;

                const s32 iEndFacet = x_min( iFirstFacet + MaxFacetsPerSection,
                                             SourceSurface.Facets.GetCount() );
                for( s32 iFacet = iFirstFacet; iFacet < iEndFacet; iFacet++ )
                {
                    const rawmesh2::facet& Facet =
                        RawMesh.m_pFacet[SourceSurface.Facets[iFacet]];

                    if( Facet.nVertices != 3 )
                        ThrowError( "Only triangle facets are supported" );

                    for( s32 iCorner = 0; iCorner < 3; iCorner++ )
                    {
                        const s32 iRawVertex = Facet.iVertex[iCorner];
                        if( (iRawVertex < 0) || (iRawVertex >= RawMesh.m_nVertices) )
                            ThrowError( "Facet references an invalid vertex" );

                        s32 iVertex = VertexRemap[iRawVertex];
                        if( iVertex == -1 )
                        {
                            const rawmesh2::vertex& SourceVertex =
                                RawMesh.m_pVertex[iRawVertex];
                            if( (SourceVertex.nNormals < 1) ||
                                (SourceVertex.nUVs < 1) ||
                                (SourceVertex.nWeights < 1) ||
                                (SourceVertex.nWeights > 2) )
                            {
                                ThrowError( "Skin vertex has invalid render attributes" );
                            }

                            skin_geom::vertex& Vertex = Vertices.Append();
                            Vertex.Position   = SourceVertex.Position;
                            Vertex.Normal     = SourceVertex.Normal[0];
                            Vertex.UV         = SourceVertex.UV[0];
                            Vertex.Weights.X  = SourceVertex.Weight[0].Weight;
                            Vertex.Weights.Y  = SourceVertex.nWeights > 1
                                              ? SourceVertex.Weight[1].Weight
                                              : 0.0f;

                            const s32 Bone0 = SourceVertex.Weight[0].iBone;
                            const s32 Bone1 = SourceVertex.nWeights > 1
                                            ? SourceVertex.Weight[1].iBone
                                            : 0;
                            if( (Bone0 < 0) || (Bone0 >= RawMesh.m_nBones) ||
                                (Bone1 < 0) || (Bone1 >= RawMesh.m_nBones) )
                            {
                                ThrowError( "Skin vertex references an invalid bone" );
                            }

                            Vertex.Bones[0] = (u16)Bone0;
                            Vertex.Bones[1] = (u16)Bone1;

                            iVertex = Vertices.GetCount() - 1;
                            VertexRemap[iRawVertex] = iVertex;
                            TargetMesh.BBox += SourceVertex.Position;
                        }

                        Indices.Append( (u32)iVertex );
                    }
                }

                Section.nVertices = Vertices.GetCount() - Section.FirstVertex;
                Section.nIndices  = Indices.GetCount() - Section.FirstIndex;
                TargetMesh.nFaces    += Section.nIndices / 3;
                TargetMesh.nVertices += Section.nVertices;
            }
        }

        SkinGeom.m_BBox += TargetMesh.BBox;
    }

    ASSERT( iSubMesh == SkinGeom.m_nSubMeshes );
    ASSERT( iSection == SkinGeom.m_nSections );

    SkinGeom.m_nFaces    = 0;
    SkinGeom.m_nVertices = 0;
    for( s32 i = 0; i < SkinGeom.m_nMeshes; i++ )
    {
        SkinGeom.m_nFaces    += SkinGeom.m_pMesh[i].nFaces;
        SkinGeom.m_nVertices += SkinGeom.m_pMesh[i].nVertices;
    }

    SkinGeom.m_nVertexData = Vertices.GetCount();
    SkinGeom.m_pVertex = new skin_geom::vertex[SkinGeom.m_nVertexData];
    if( SkinGeom.m_nVertexData > 0 )
    {
        x_memcpy( SkinGeom.m_pVertex,
                  Vertices.GetPtr(),
                  SkinGeom.m_nVertexData * sizeof(skin_geom::vertex) );
    }

    SkinGeom.m_nIndices = Indices.GetCount();
    SkinGeom.m_pIndex = new u32[SkinGeom.m_nIndices];
    if( SkinGeom.m_nIndices > 0 )
    {
        x_memcpy( SkinGeom.m_pIndex,
                  Indices.GetPtr(),
                  SkinGeom.m_nIndices * sizeof(u32) );
    }

    ExportMaterial( Mesh, SkinGeom );
    ExportVirtualMeshes( Mesh, SkinGeom );
    CompileDictionary( SkinGeom );
}

//=============================================================================


