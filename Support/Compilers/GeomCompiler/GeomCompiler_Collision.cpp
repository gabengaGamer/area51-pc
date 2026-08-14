#include "GeomCompiler.hpp"
#include "Render/RigidGeom.hpp"

extern xbool g_DoCollision;

s32 DetermineMeshBone( const rawmesh2& Mesh, s32 iMesh )
{
    for( s32 i = 0; i < Mesh.m_nFacets; i++ )
    {
        const rawmesh2::facet& Facet = Mesh.m_pFacet[i];
        if( Facet.iMesh != iMesh )
            continue;

        const s32 iVertex = Facet.iVertex[0];
        if( (iVertex >= 0) &&
            (iVertex < Mesh.m_nVertices) &&
            (Mesh.m_pVertex[iVertex].nWeights > 0) )
        {
            return Mesh.m_pVertex[iVertex].Weight[0].iBone;
        }
    }

    return (Mesh.m_nBones == 1) ? 0 : -1;
}
 
//=============================================================================

void geom_compiler::CompileLowCollision( rigid_geom& RigidGeom,
                                         rawmesh2&   LowMesh,
                                         rawmesh2&   HighMesh )
{
    collision_data& Collision = RigidGeom.m_collision;
    Collision.nLowClusters = 0;
    Collision.nLowVectors  = 0;
    Collision.nLowQuads    = 0;
    Collision.pLowCluster  = NULL;
    Collision.pLowVector   = NULL;
    Collision.pLowQuad     = NULL;

    if( !g_DoCollision )
        return;

    if( m_RawMeshToCompiled.GetCount() != HighMesh.m_nSubMeshs )
        ThrowError( "Internal mesh mapping does not match collision source" );

    xarray<s32> LowToHigh;
    xarray<s32> HighBone;
    xarray<xbool> HighMeshUsed;
    LowToHigh.SetCount( LowMesh.m_nSubMeshs );
    HighBone.SetCount( HighMesh.m_nSubMeshs );
    HighMeshUsed.SetCount( HighMesh.m_nSubMeshs );

    for( s32 i = 0; i < HighMesh.m_nSubMeshs; i++ )
    {
        HighBone[i]     = DetermineMeshBone( HighMesh, i );
        HighMeshUsed[i] = FALSE;
    }

    for( s32 iLowMesh = 0;
         iLowMesh < LowMesh.m_nSubMeshs;
         iLowMesh++ )
    {
        char LowName[256];
        x_strsavecpy( LowName,
                      LowMesh.m_pSubMesh[iLowMesh].Name,
                      sizeof(LowName) );

        const s32 Length = x_strlen( LowName );
        if( (Length < 3) ||
            (LowName[Length - 2] != '_') ||
            ((LowName[Length - 1] != 'c') &&
             (LowName[Length - 1] != 'C')) )
        {
            ThrowError( xfs( "Collision mesh needs '_c' suffix [%s], File [%s]",
                             LowName,
                             m_FastCollision ) );
        }
        LowName[Length - 2] = 0;

        s32 iHighMesh = 0;
        for( ;
             iHighMesh < HighMesh.m_nSubMeshs;
             iHighMesh++ )
        {
            if( x_stricmp( HighMesh.m_pSubMesh[iHighMesh].Name,
                           LowName ) == 0 )
            {
                break;
            }
        }

        if( iHighMesh == HighMesh.m_nSubMeshs )
        {
            ThrowError( xfs( "Unreferenced collision mesh [%s], File [%s]",
                             LowMesh.m_pSubMesh[iLowMesh].Name,
                             m_FastCollision ) );
        }
        if( HighMeshUsed[iHighMesh] )
        {
            ThrowError( xfs( "Multiple collision meshes reference [%s], File [%s]",
                             HighMesh.m_pSubMesh[iHighMesh].Name,
                             m_FastCollision ) );
        }
        if( m_RawMeshToCompiled[iHighMesh] < 0 )
        {
            ThrowError( xfs( "Collision mesh references empty render mesh [%s]",
                             HighMesh.m_pSubMesh[iHighMesh].Name ) );
        }
        if( HighBone[iHighMesh] < 0 )
        {
            ThrowError( xfs( "Mesh [%s] is not attached to a bone",
                             HighMesh.m_pSubMesh[iHighMesh].Name ) );
        }

        HighMeshUsed[iHighMesh] = TRUE;
        LowToHigh[iLowMesh] = iHighMesh;
    }

    struct low_triangle
    {
        vector3 Point[3];
        vector3 Normal;
        s32     iMesh;
        s32     iBone;
    };

    xarray<low_triangle> Triangles;
    s32 nDegenerateFacets = 0;

    for( s32 iFacet = 0; iFacet < LowMesh.m_nFacets; iFacet++ )
    {
        const rawmesh2::facet& Facet = LowMesh.m_pFacet[iFacet];
        if( Facet.nVertices != 3 )
            ThrowError( "Low collision supports only triangle facets" );
        if( (Facet.iMesh < 0) || (Facet.iMesh >= LowMesh.m_nSubMeshs) )
            ThrowError( "Low collision facet references an invalid mesh" );

        const s32 iHighMesh = LowToHigh[Facet.iMesh];
        low_triangle& Triangle = Triangles.Append();
        Triangle.iMesh = m_RawMeshToCompiled[iHighMesh];
        Triangle.iBone = HighBone[iHighMesh];

        for( s32 i = 0; i < 3; i++ )
        {
            const s32 iVertex = Facet.iVertex[i];
            if( (iVertex < 0) || (iVertex >= LowMesh.m_nVertices) )
                ThrowError( "Low collision facet references an invalid vertex" );
            Triangle.Point[i] = LowMesh.m_pVertex[iVertex].Position;
        }

        Triangle.Normal = v3_Cross( Triangle.Point[1] - Triangle.Point[0],
                                    Triangle.Point[2] - Triangle.Point[0] );
        if( Triangle.Normal.Length() < 0.00001f )
        {
            Triangles.Delete( Triangles.GetCount() - 1 );
            nDegenerateFacets++;
            continue;
        }
        Triangle.Normal.Normalize();
    }

    if( nDegenerateFacets > 0 )
    {
        ReportWarning( xfs( "Ignored %d degenerate low-collision triangle(s)",
                            nDegenerateFacets ) );
    }

    const s32 MaxTrianglesPerCluster = 10;
    s32 nClusters = 0;
    for( s32 iTriangle = 0; iTriangle < Triangles.GetCount(); )
    {
        const s32 iMesh = Triangles[iTriangle].iMesh;
        const s32 iBone = Triangles[iTriangle].iBone;
        s32 nTriangles = 0;
        while( (iTriangle + nTriangles < Triangles.GetCount()) &&
               (nTriangles < MaxTrianglesPerCluster) &&
               (Triangles[iTriangle + nTriangles].iMesh == iMesh) &&
               (Triangles[iTriangle + nTriangles].iBone == iBone) )
        {
            nTriangles++;
        }
        iTriangle += nTriangles;
        nClusters++;
    }

    Collision.nLowClusters = nClusters;
    Collision.nLowQuads    = Triangles.GetCount();
    Collision.nLowVectors  = Triangles.GetCount() * 4;
    Collision.pLowCluster  = nClusters > 0
                           ? new collision_data::low_cluster[nClusters]
                           : NULL;
    Collision.pLowQuad     = Collision.nLowQuads > 0
                           ? new collision_data::low_quad[Collision.nLowQuads]
                           : NULL;
    Collision.pLowVector   = Collision.nLowVectors > 0
                           ? new vector3[Collision.nLowVectors]
                           : NULL;

    s32 iCluster = 0;
    s32 iTriangle = 0;
    s32 iVector = 0;
    while( iTriangle < Triangles.GetCount() )
    {
        const s32 iFirstTriangle = iTriangle;
        const s32 iMesh = Triangles[iTriangle].iMesh;
        const s32 iBone = Triangles[iTriangle].iBone;
        while( (iTriangle < Triangles.GetCount()) &&
               ((iTriangle - iFirstTriangle) < MaxTrianglesPerCluster) &&
               (Triangles[iTriangle].iMesh == iMesh) &&
               (Triangles[iTriangle].iBone == iBone) )
        {
            iTriangle++;
        }

        collision_data::low_cluster& Cluster =
            Collision.pLowCluster[iCluster++];
        Cluster.BBox.Clear();
        Cluster.iVectorOffset = iVector;
        Cluster.nPoints       = (iTriangle - iFirstTriangle) * 3;
        Cluster.nNormals      = iTriangle - iFirstTriangle;
        Cluster.iQuadOffset   = iFirstTriangle;
        Cluster.nQuads        = iTriangle - iFirstTriangle;
        Cluster.iMesh         = iMesh;
        Cluster.iBone         = iBone;

        for( s32 i = 0; i < Cluster.nQuads; i++ )
        {
            const low_triangle& Triangle = Triangles[iFirstTriangle + i];
            const s32 iPoint = i * 3;

            Collision.pLowVector[Cluster.iVectorOffset + iPoint + 0] =
                Triangle.Point[0];
            Collision.pLowVector[Cluster.iVectorOffset + iPoint + 1] =
                Triangle.Point[1];
            Collision.pLowVector[Cluster.iVectorOffset + iPoint + 2] =
                Triangle.Point[2];
            Collision.pLowVector[Cluster.iVectorOffset +
                                 Cluster.nPoints + i] = Triangle.Normal;

            collision_data::low_quad& Quad =
                Collision.pLowQuad[Cluster.iQuadOffset + i];
            Quad.iP[0] = (byte)(iPoint + 0);
            Quad.iP[1] = (byte)(iPoint + 1);
            Quad.iP[2] = (byte)(iPoint + 2);
            Quad.iP[3] = (byte)(iPoint + 0);
            Quad.iN    = (byte)i;
            Quad.Flags = 0;

            Cluster.BBox += Triangle.Point[0];
            Cluster.BBox += Triangle.Point[1];
            Cluster.BBox += Triangle.Point[2];
        }

        iVector += Cluster.nPoints + Cluster.nNormals;
    }

    ASSERT( iCluster == Collision.nLowClusters );
    ASSERT( iTriangle == Collision.nLowQuads );
    ASSERT( iVector == Collision.nLowVectors );
}

//=============================================================================

void geom_compiler::CompileLowCollisionFromBBox( rigid_geom& RigidGeom, 
                                                 rawmesh2&   HighMesh )
{
    s32 iTargetMesh = 0;
    while( (iTargetMesh < m_RawMeshToCompiled.GetCount()) &&
           (m_RawMeshToCompiled[iTargetMesh] < 0) )
    {
        iTargetMesh++;
    }
    if( iTargetMesh == m_RawMeshToCompiled.GetCount() )
        ThrowError( "Cannot build collision for an empty mesh" );

    rawmesh2 BBoxMesh;
    BBoxMesh.m_nBones = 1;
    BBoxMesh.m_pBone = new rawmesh2::bone;
    BBoxMesh.m_nVFrames = 0;
    BBoxMesh.m_nTextures = 0;
    BBoxMesh.m_pTexture = NULL;
    BBoxMesh.m_nMaterials = 1;
    BBoxMesh.m_pMaterial = new rawmesh2::material;
    BBoxMesh.m_nParamKeys = 0;
    BBoxMesh.m_pParamKey = NULL;


    BBoxMesh.m_nVertices = 8;
    BBoxMesh.m_pVertex   = new rawmesh2::vertex[8];
    BBoxMesh.m_nFacets   = 0;
    BBoxMesh.m_pFacet    = new rawmesh2::facet[12];
    BBoxMesh.m_nSubMeshs = 1;
    BBoxMesh.m_pSubMesh  = new rawmesh2::sub_mesh;

    BBoxMesh.m_pBone[0] = HighMesh.m_pBone[0];
    BBoxMesh.m_pMaterial[0] = HighMesh.m_pMaterial[0];
    BBoxMesh.m_pSubMesh[0] = HighMesh.m_pSubMesh[iTargetMesh];
    x_strcpy(BBoxMesh.m_pSubMesh[0].Name,xfs("%s_c",HighMesh.m_pSubMesh[iTargetMesh].Name));

    bbox HighBBox = HighMesh.GetBBox();

    // If Size is too thin along a certain dimension then inflate the bbox.
    {
        f32 MinSize = 1.0f;
        for( s32 i=0; i<3; i++ )
        {
            if( (HighBBox.Max[i]-HighBBox.Min[i]) < MinSize )
            {
                f32 Center = (HighBBox.Max[i]+HighBBox.Min[i])*0.5f;
                HighBBox.Min[i] = Center - MinSize*0.5f;
                HighBBox.Max[i] = Center + MinSize*0.5f;
            }
        }
    }

    // Indices used to convert min + max of bbox into 8 corners
    static byte CornerIndices[8*3]   = { 0,1,2,     // 0: MinX, MinY, MinZ
                                         4,1,2,     // 1: MaxX, MinY, MinZ
                                         0,5,2,     // 2: MinX, MaxY, MinZ
                                         4,5,2,     // 3: MaxX, MaxY, MinZ
                                         0,1,6,     // 4: MinX, MinY, MaxZ
                                         4,1,6,     // 5: MaxX, MinY, MaxZ
                                         0,5,6,     // 6: MinX, MaxY, MaxZ
                                         4,5,6 };   // 7: MaxX, MaxY, MaxZ;

    // Indices used to convert 8 corners into a 4 sided NGon
    static byte SideIndices[6*4]     = { 4,6,2,0,   // X -ve
                                         1,3,7,5,   // X +ve
                                         4,0,1,5,   // Y -ve
                                         2,6,7,3,   // Y +ve
                                         0,2,3,1,   // Z -ve
                                         5,7,6,4 }; // Z +ve

    // Build the corners
    vector3     Corner[8];
    {
        const byte*  pI = CornerIndices;
        const f32*   pBBoxF = &HighBBox.Min.GetX();
        for( s32 i=0; i<8; i++ )
        {
            BBoxMesh.m_pVertex[i].Position.Set( pBBoxF[pI[0]],
                                                pBBoxF[pI[1]],
                                                pBBoxF[pI[2]] );
            BBoxMesh.m_pVertex[i].nColors          = 0;
            BBoxMesh.m_pVertex[i].nNormals         = 0;
            BBoxMesh.m_pVertex[i].nUVs             = 0;
            BBoxMesh.m_pVertex[i].nWeights         = 1;
            BBoxMesh.m_pVertex[i].Weight[0].iBone  = 0;
            BBoxMesh.m_pVertex[i].Weight[0].Weight = 1.0f;
            pI += 3;
        }
    }

    //
    // We need to build 2 triangles for each side
    //
    {
        const byte*  pI = SideIndices;
        s32 iFacet = 0;
        for( s32 i=0; i<6; i++, pI += 4 )
        {
            // Build normal of bbox side
            plane BBoxPlane( BBoxMesh.m_pVertex[ pI[0] ].Position,
                             BBoxMesh.m_pVertex[ pI[1] ].Position,
                             BBoxMesh.m_pVertex[ pI[2] ].Position );             

            // Check high mesh faces to see if any faces point this direction
            s32 f;
            for( f = 0 ; f < HighMesh.m_nFacets; f++ )
            {
                // Lookup facet
                rawmesh2::facet& Facet = HighMesh.m_pFacet[f];
                    
                // Build facet normal
                plane FacetPlane( HighMesh.m_pVertex[ Facet.iVertex[0] ].Position,
                                  HighMesh.m_pVertex[ Facet.iVertex[1] ].Position,
                                  HighMesh.m_pVertex[ Facet.iVertex[2] ].Position );                
                
                // If bbox side and face normal are pointing the same direction, then keep this side
                if( v3_Dot( BBoxPlane.Normal, FacetPlane.Normal ) > 0.0f )
                    break;
                    
                // Can material be seen from both sides?
                rawmesh2::material& Mat = HighMesh.m_pMaterial[ Facet.iMaterial ];
                if(     ( Mat.bTwoSided )   // Two sided?
                    ||  ( Mat.Map[Max_PunchThrough].iTexture != -1 )   // Punch thru alpha?
                    ||  ( Mat.Type == Material_Alpha )                  // Alpha blended?
                    ||  ( Mat.Type == Material_Alpha_PerPolyEnv )
                    ||  ( Mat.Type == Material_Alpha_PerPixelIllum )
                    ||  ( Mat.Type == Material_Alpha_PerPolyIllum ) )
                {                    
                    // Keep if bbox side and face normal are pointing the opposite direction
                    if( v3_Dot( BBoxPlane.Normal, FacetPlane.Normal ) < 0.0f )
                        break;
                }                    
            }

            // Skip if no faces point same direction as the bbox side
            // (will happen for single sided floor/ceiling pieces as required by the physics)
            if( f == HighMesh.m_nFacets )
                continue;

            // Add facets                
            BBoxMesh.m_pFacet[iFacet].iMaterial = 0;
            BBoxMesh.m_pFacet[iFacet].iMesh     = 0;
            BBoxMesh.m_pFacet[iFacet].nVertices = 3;
            BBoxMesh.m_pFacet[iFacet].iVertex[0] = pI[0];
            BBoxMesh.m_pFacet[iFacet].iVertex[1] = pI[1];
            BBoxMesh.m_pFacet[iFacet].iVertex[2] = pI[2];
            BBoxMesh.m_pFacet[iFacet].Plane.Setup(1,0,0,0);
            iFacet++;

            BBoxMesh.m_pFacet[iFacet].iMaterial = 0;
            BBoxMesh.m_pFacet[iFacet].iMesh     = 0;
            BBoxMesh.m_pFacet[iFacet].nVertices = 3;
            BBoxMesh.m_pFacet[iFacet].iVertex[0] = pI[0];
            BBoxMesh.m_pFacet[iFacet].iVertex[1] = pI[2];
            BBoxMesh.m_pFacet[iFacet].iVertex[2] = pI[3];
            BBoxMesh.m_pFacet[iFacet].Plane.Setup(1,0,0,0);
            iFacet++;
        }
        BBoxMesh.m_nFacets = iFacet;

    }

    CompileLowCollision( RigidGeom, BBoxMesh, HighMesh );
}

//=============================================================================
//=============================================================================
//=============================================================================
// BUILDING HIGH POLY COLLISION GEOMETRY
//=============================================================================
//=============================================================================
//=============================================================================

void geom_compiler::CompileHighCollisionFromGeometry(
    rigid_geom& RigidGeom,
    collision_data::mat_info* pMaterialInfo )
{
    collision_data& Collision = RigidGeom.m_collision;
    Collision.nHighClusters     = 0;
    Collision.nHighIndices      = 0;
    Collision.pHighCluster      = NULL;
    Collision.pHighIndexToVert0 = NULL;
    Collision.BBox.Clear();

    if( !g_DoCollision )
    {
        Collision.BBox = RigidGeom.m_BBox;
        return;
    }

    s32 nTriangles = 0;
    for( s32 i = 0; i < RigidGeom.m_nSections; i++ )
    {
        const rigid_geom::section& Section = RigidGeom.m_pSection[i];
        if( (Section.nIndices % 3) != 0 )
            ThrowError( "Rigid section contains a partial triangle" );
        if( Section.nIndices > 32769 )
            ThrowError( "Rigid collision section exceeds the encoded triangle offset limit" );

        if( Section.nIndices > 0 )
        {
            Collision.nHighClusters++;
            nTriangles += Section.nIndices / 3;
        }
    }

    Collision.nHighIndices = nTriangles;
    Collision.pHighCluster = Collision.nHighClusters > 0
                           ? new collision_data::high_cluster[Collision.nHighClusters]
                           : NULL;
    Collision.pHighIndexToVert0 = nTriangles > 0 ? new u16[nTriangles] : NULL;

    s32 iCluster  = 0;
    s32 iTriangle = 0;
    for( s32 iMesh = 0; iMesh < RigidGeom.m_nMeshes; iMesh++ )
    {
        const geom::mesh& MeshInfo = RigidGeom.m_pMesh[iMesh];
        for( s32 i = 0; i < MeshInfo.nSubMeshes; i++ )
        {
            const geom::submesh& Submesh =
                RigidGeom.m_pSubMesh[MeshInfo.iSubMesh + i];

            for( s32 iSubSection = 0;
                 iSubSection < Submesh.nSections;
                 iSubSection++ )
            {
                const s32 iSection = Submesh.iSection + iSubSection;
                const rigid_geom::section& Section =
                    RigidGeom.m_pSection[iSection];
                if( Section.nIndices == 0 )
                    continue;

                collision_data::high_cluster& Cluster =
                    Collision.pHighCluster[iCluster++];
                Cluster.BBox.Clear();
                Cluster.nTris        = Section.nIndices / 3;
                Cluster.iMesh        = iMesh;
                Cluster.iBone        = Section.iBone;
                Cluster.iSection     = iSection;
                Cluster.iOffset      = iTriangle;
                Cluster.MaterialInfo = pMaterialInfo[iSection];

                for( s32 iIndex = 0; iIndex < Section.nIndices; iIndex += 3 )
                {
                    Collision.pHighIndexToVert0[iTriangle++] = (u16)iIndex;

                    for( s32 iCorner = 0; iCorner < 3; iCorner++ )
                    {
                        const u32 iVertex =
                            RigidGeom.m_pIndex[Section.FirstIndex + iIndex + iCorner];
                        Cluster.BBox += RigidGeom.m_pVertex[iVertex].Pos;
                    }
                }

                Cluster.BBox.Inflate( 1.0f, 1.0f, 1.0f );
                Collision.BBox += Cluster.BBox;
            }
        }
    }

    ASSERT( iCluster == Collision.nHighClusters );
    ASSERT( iTriangle == Collision.nHighIndices );
}

//=============================================================================

xbool RigidGeom_GetTriangle( const rigid_geom* pRigidGeom,
                             s32               Key,
                             vector3&          P0,
                             vector3&          P1,
                             vector3&          P2 )
{
    if( !pRigidGeom || (Key == -1) )
        return( FALSE );

    const s32 iCluster  = (Key >> 16) & 0x7fff;
    const s32 iTriangle = Key & 0xffff;
    if( (iCluster < 0) ||
        (iCluster >= pRigidGeom->m_collision.nHighClusters) )
    {
        return( FALSE );
    }

    const collision_data::high_cluster& Cluster =
        pRigidGeom->m_collision.pHighCluster[iCluster];
    if( (iTriangle < 0) || (iTriangle >= Cluster.nTris) )
        return( FALSE );

    const rigid_geom::section& Section =
        pRigidGeom->m_pSection[Cluster.iSection];
    const u16 EncodedIndex =
        pRigidGeom->m_collision.pHighIndexToVert0[Cluster.iOffset + iTriangle];
    const s32 FirstIndex = EncodedIndex & 0x7fff;

    P0 = pRigidGeom->m_pVertex[
        pRigidGeom->m_pIndex[Section.FirstIndex + FirstIndex + 0]].Pos;
    P1 = pRigidGeom->m_pVertex[
        pRigidGeom->m_pIndex[Section.FirstIndex + FirstIndex + 1]].Pos;
    P2 = pRigidGeom->m_pVertex[
        pRigidGeom->m_pIndex[Section.FirstIndex + FirstIndex + 2]].Pos;
    return( TRUE );
}

//=============================================================================


