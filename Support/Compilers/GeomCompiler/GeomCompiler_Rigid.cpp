#include "GeomCompiler.hpp"
#include "GeomDesc.hpp"
#include "Render/GeomFile.hpp"
#include "Render/RigidGeom.hpp"
#include "RawAnim.hpp"

extern xbool g_Verbose;
extern xbool g_DoCollision;

void geom_compiler::RemoveUnusedVMeshes( rawmesh2& RawMesh )
{
    s32 i, j, k, t;

    //
    // nuke any submeshes that aren't contained in a virtual mesh
    //
    if( m_pGeomRscDesc->GetVirtualMeshCount() )
    {
        // figure out which meshes are included in the vmeshes
        xbool* pMeshUsed = new xbool[RawMesh.m_nSubMeshs];
        for( i = 0; i < RawMesh.m_nSubMeshs; i++ )
        {
            pMeshUsed[i] = FALSE;
            for( j = 0; j < m_pGeomRscDesc->GetVirtualMeshCount() && (pMeshUsed[i] == FALSE); j++ )
            {
                const geom_rsc_desc::virtual_mesh& VMesh = m_pGeomRscDesc->GetVirtualMesh( j );
                for( k = 0 ; k < VMesh.LODs.GetCount() && (pMeshUsed[i] == FALSE); k++ )
                {
                    const geom_rsc_desc::lod_info& LODInfo = VMesh.LODs[k];
                    for( t = 0; t < LODInfo.nMeshes && (pMeshUsed[i] == FALSE); t++ )
                    {
                        if( !x_strcmp( LODInfo.MeshName[t], RawMesh.m_pSubMesh[i].Name ) )
                        {
                            pMeshUsed[i] = TRUE;
                        }
                    }
                }
            }
        }

        // Nuke all the facets that represent those meshes
        for( i = 0; i < RawMesh.m_nSubMeshs; i++ )
        {
            if( pMeshUsed[i] )
                continue;

            for( j = 0; j < RawMesh.m_nFacets; j++ )
            {
                if( RawMesh.m_pFacet[j].iMesh == i )
                {
                    RawMesh.m_pFacet[j] = RawMesh.m_pFacet[ RawMesh.m_nFacets-1 ];
                    j--;
                    RawMesh.m_nFacets--;
                }
            }
        }

        // clean up
        delete []pMeshUsed;
    }
}

//=============================================================================

void geom_compiler::ExportRigidGeom( const char* pFileName )
{
    // load the resource
    LoadResource( pFileName, FALSE );

    //
    // Load the raw-mesh
    //

    rigid_geom  RigidGeom;
    rawmesh2    RawMesh;
    mesh        Mesh;
    s32         j;
    rawmesh2    RawMeshFC;

    LoadSourceMesh( m_pGeomRscDesc->GetSourceFileName(), RawMesh );
    SetUserInfo( RawMesh.m_UserName, RawMesh.m_ComputerName );
    SetExportDate( RawMesh.m_ExportDate[0], RawMesh.m_ExportDate[1], RawMesh.m_ExportDate[2] );
    RemoveUnusedVMeshes( RawMesh );

    if( RawMesh.m_nBones > 1 )
    {
        rawanim     RawAnim;
        xarray<s32> BadSubMesh;
        
        LoadSourceAnimation( m_pGeomRscDesc->GetSourceFileName(), RawAnim );

        //
        // Nuke all the submeshes that are bones.
        //
        BadSubMesh.Clear();
        for( j=0; j<RawMesh.m_nSubMeshs; j++ )
        {            
            if( x_stristr( RawMesh.m_pSubMesh[j].Name, "Human_" ) )
                BadSubMesh.Append() = j;
        }
        
        // Nuke all the facets that represent those bones.
        for( j=0; j<RawMesh.m_nFacets; j++ )
        {
            s32 t;
            for( t=0; t<BadSubMesh.GetCount(); t++ )
            {
                if( BadSubMesh[t] == RawMesh.m_pFacet[j].iMesh )
                    break;
            }
        
            if( t < BadSubMesh.GetCount() )
            {
                RawMesh.m_pFacet[j] = RawMesh.m_pFacet[ RawMesh.m_nFacets-1 ];
                j--;
                RawMesh.m_nFacets--;
            }
        }            
    
        RawMesh.CleanWeights( 1, 0.00001f );
        
        // Do remapping of the skeleton
        RawAnim.DeleteDummyBones();
        RawMesh.ApplyNewSkeleton( RawAnim );
        
        // Show hierarchy?
        if( g_Verbose )
            RawAnim.PrintHierarchy();
    }
    else
    {
    }


    BuildCompileModel( RigidGeom, RawMesh, Mesh, TRUE );

    //
    // Check whether we have to load the fast collision data
    //
    if( !g_DoCollision )
    {
        RigidGeom.m_collision.BBox = RawMesh.GetBBox();
    }
    else if( m_FastCollision[0] )
    {
        LoadSourceMesh( m_FastCollision, RawMeshFC );

        // Low collision uses only positions and one rigid bone assignment.
        for( s32 j=0; j<RawMeshFC.m_nVertices; j++ )
        {
            RawMeshFC.m_pVertex[j].nColors  = 0;
            RawMeshFC.m_pVertex[j].nUVs     = 0;
            RawMeshFC.m_pVertex[j].nNormals = 0;
        }

        RawMeshFC.CleanWeights( 1, 0.00001f );

        CompileLowCollision( RigidGeom, RawMeshFC, RawMesh );
    }
    else
    {
        CompileLowCollisionFromBBox( RigidGeom, RawMesh );
    }

    BuildRigidGeometry( Mesh, RigidGeom );

    xstring Error;
    if( !geom_file::SaveRigid( m_OutputFile, RigidGeom, Error ) )
        ThrowError( xfs( "Unable to save v42 rigid geometry: %s", (const char*)Error ) );

    X_FILE* pVerifyFile = x_fopen( m_OutputFile, "rb" );
    rigid_geom* pVerifiedGeom = NULL;
    if( !pVerifyFile ||
        !geom_file::LoadRigid( pVerifyFile, pVerifiedGeom, Error ) )
    {
        if( pVerifyFile )
            x_fclose( pVerifyFile );
        ThrowError( xfs( "Unable to read back v42 rigid geometry: %s",
                         (const char*)Error ) );
    }
    x_fclose( pVerifyFile );
    delete pVerifiedGeom;

    // print out a material and texture listing
    if( g_Verbose )
    {
        PrintSummary( RigidGeom );

        // print out a summary of the bboxes
        bbox BBox = RigidGeom.m_collision.BBox;
        x_printf( "\n--BBox sizes-----------------------------------\n" );
        x_printf( "Collision:\n" );
        x_printf( "(%3.3f, %3.3f, %3.3f)-(%3.3f, %3.3f, %3.3f)\n",
            BBox.Min.GetX(), BBox.Min.GetY(), BBox.Min.GetZ(),
            BBox.Max.GetX(), BBox.Max.GetY(), BBox.Max.GetZ() );
        x_printf( "Extent: (%3.3f, %3.3f, %3.3f)\n",
            BBox.Max.GetX() - BBox.Min.GetX(),
            BBox.Max.GetY() - BBox.Min.GetY(),
            BBox.Max.GetZ() - BBox.Min.GetZ() );

        BBox = RigidGeom.m_BBox;
        x_printf( "Geom:\n" );
        x_printf( "(%3.3f, %3.3f, %3.3f)-(%3.3f, %3.3f, %3.3f)\n",
            BBox.Min.GetX(), BBox.Min.GetY(), BBox.Min.GetZ(),
            BBox.Max.GetX(), BBox.Max.GetY(), BBox.Max.GetZ() );
        x_printf( "Extent: (%3.3f, %3.3f, %3.3f)\n",
            BBox.Max.GetX() - BBox.Min.GetX(),
            BBox.Max.GetY() - BBox.Min.GetY(),
            BBox.Max.GetZ() - BBox.Min.GetZ() );
    }
}

//=============================================================================

void geom_compiler::BuildRigidGeometry( mesh& Mesh, rigid_geom& RigidGeom )
{
    if( Mesh.SubMesh.GetCount() == 0 )
        ThrowError( "Rigid geometry contains no meshes" );

    const rawmesh2& RawMesh = *Mesh.SubMesh[0].pRawMesh;
    xarray<rigid_geom::vertex> Vertices;
    xarray<u32>                Indices;
    xarray<s32>                VertexRemap;

    const s32 MaxFacetsPerSection = 10923;
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
        ThrowError( "Rigid geometry contains no drawable surfaces" );

    RigidGeom.m_nMeshes    = Mesh.SubMesh.GetCount();
    RigidGeom.m_pMesh      = new geom::mesh[RigidGeom.m_nMeshes];
    RigidGeom.m_nSubMeshes = nSubMeshes;
    RigidGeom.m_pSubMesh   = new geom::submesh[nSubMeshes];
    RigidGeom.m_nSections  = nSections;
    RigidGeom.m_pSection   = new rigid_geom::section[nSections];

    collision_data::mat_info* pMaterialInfo =
        new collision_data::mat_info[nSections];

    VertexRemap.SetCount( RawMesh.m_nVertices );
    RigidGeom.m_BBox.Clear();

    s32 iSubMesh = 0;
    s32 iSection = 0;
    for( s32 iMesh = 0; iMesh < Mesh.SubMesh.GetCount(); iMesh++ )
    {
        const sub_mesh& SourceMesh = Mesh.SubMesh[iMesh];
        geom::mesh&     TargetMesh = RigidGeom.m_pMesh[iMesh];

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
            geom::submesh& Submesh = RigidGeom.m_pSubMesh[iSubMesh++];
            Submesh.iSection       = iSection;
            Submesh.nSections      = (SourceSurface.Facets.GetCount() +
                                      MaxFacetsPerSection - 1) /
                                      MaxFacetsPerSection;
            Submesh.iMaterial     = SourceSurface.iMaterial;
            Submesh.WorldPixelSize = 0.001f;

            material& Material = Mesh.Material[SourceSurface.iMaterial];
            const rawmesh2::material& RawMaterial =
                Material.pRawMesh->m_pMaterial[Material.iRawMaterial];

            for( s32 iFirstFacet = 0;
                 iFirstFacet < SourceSurface.Facets.GetCount();
                 iFirstFacet += MaxFacetsPerSection, iSection++ )
            {
                rigid_geom::section& Section = RigidGeom.m_pSection[iSection];

                for( s32 i = 0; i < VertexRemap.GetCount(); i++ )
                    VertexRemap[i] = -1;

                Section.FirstVertex = Vertices.GetCount();
                Section.FirstIndex  = Indices.GetCount();
                Section.iBone       = SourceSurface.iBone;
                Section.iColor      = Section.FirstVertex;

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
                            if( (SourceVertex.nNormals < 1) || (SourceVertex.nUVs < 1) )
                                ThrowError( "Rigid vertex is missing a normal or UV set" );

                            rigid_geom::vertex& Vertex = Vertices.Append();
                            Vertex.Pos    = SourceVertex.Position;
                            Vertex.Normal = SourceVertex.Normal[0];
                            Vertex.UV     = SourceVertex.UV[0];

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

                pMaterialInfo[iSection].SoundType = Material.TexInfo.SoundMat;
                pMaterialInfo[iSection].Flags     = 0;
                if( RawMaterial.bTwoSided )
                {
                    pMaterialInfo[iSection].Flags |=
                        collision_data::mat_info::FLAG_DOUBLESIDED;
                }

                switch( RawMaterial.Type )
                {
                    case Material_Alpha:
                    case Material_Alpha_PerPolyEnv:
                    case Material_Alpha_PerPixelIllum:
                    case Material_Alpha_PerPolyIllum:
                        pMaterialInfo[iSection].Flags |=
                            collision_data::mat_info::FLAG_TRANSPARENT;
                        break;

                    default:
                        break;
                }
            }
        }

        RigidGeom.m_BBox += TargetMesh.BBox;
        x_printf( "%30s - %10d Vertices\n",
                  SourceMesh.Name,
                  TargetMesh.nVertices );
    }

    ASSERT( iSubMesh == RigidGeom.m_nSubMeshes );
    ASSERT( iSection == RigidGeom.m_nSections );

    RigidGeom.m_nFaces    = 0;
    RigidGeom.m_nVertices = 0;
    for( s32 i = 0; i < RigidGeom.m_nMeshes; i++ )
    {
        RigidGeom.m_nFaces    += RigidGeom.m_pMesh[i].nFaces;
        RigidGeom.m_nVertices += RigidGeom.m_pMesh[i].nVertices;
    }

    RigidGeom.m_nVertexData = Vertices.GetCount();
    RigidGeom.m_pVertex = new rigid_geom::vertex[RigidGeom.m_nVertexData];
    if( RigidGeom.m_nVertexData > 0 )
    {
        x_memcpy( RigidGeom.m_pVertex,
                  Vertices.GetPtr(),
                  RigidGeom.m_nVertexData * sizeof(rigid_geom::vertex) );
    }

    RigidGeom.m_nIndices = Indices.GetCount();
    RigidGeom.m_pIndex = new u32[RigidGeom.m_nIndices];
    if( RigidGeom.m_nIndices > 0 )
    {
        x_memcpy( RigidGeom.m_pIndex,
                  Indices.GetPtr(),
                  RigidGeom.m_nIndices * sizeof(u32) );
    }

    ExportMaterial( Mesh, RigidGeom );
    ExportVirtualMeshes( Mesh, RigidGeom );
    CompileHighCollisionFromGeometry( RigidGeom, pMaterialInfo );

    for( s32 i = 0; i < RigidGeom.m_collision.nLowClusters; i++ )
        RigidGeom.m_BBox += RigidGeom.m_collision.pLowCluster[i].BBox;

    delete[] pMaterialInfo;
    CompileDictionary( RigidGeom );
}

//=============================================================================


