#include "GeomCompiler.hpp"
#include "GeomDesc.hpp"

namespace
{

enum material_parameter
{
    DETAIL_SCALE,
    ENV_TYPE,
    ENV_BLEND,
    FIXED_ALPHA,
    FORCE_ZFILL,
    USE_DIFFUSE,
};

} // namespace

extern xbool g_Verbose;

//=============================================================================

xbool geom_compiler::IsSameMaterial( const rawmesh2::material& RawMatA,
                                     const rawmesh2::material& RawMatB,
                                     const rawmesh2&           RawMesh )
{
    // type check
    if( RawMatA.Type != RawMatB.Type )  return FALSE;

    // diffuse texture check
    s32 iDiffA = RawMatA.Map[Max_Diffuse1].iTexture;
    s32 iDiffB = RawMatB.Map[Max_Diffuse1].iTexture;
    s32 iEnvA  = RawMatA.Map[Max_Environment].iTexture;
    s32 iEnvB  = RawMatB.Map[Max_Environment].iTexture;
    s32 iDetA  = RawMatA.Map[Max_DetailMap].iTexture;
    s32 iDetB  = RawMatB.Map[Max_DetailMap].iTexture;
    s32 iPunA  = RawMatA.Map[Max_PunchThrough].iTexture;
    s32 iPunB  = RawMatB.Map[Max_PunchThrough].iTexture;
    const char* pDiffNameA = (iDiffA>=0) ? RawMesh.m_pTexture[iDiffA].FileName : "";
    const char* pDiffNameB = (iDiffB>=0) ? RawMesh.m_pTexture[iDiffB].FileName : "";
    const char* pEnvNameA  = (iEnvA >=0) ? RawMesh.m_pTexture[iEnvA].FileName  : "";
    const char* pEnvNameB  = (iEnvB >=0) ? RawMesh.m_pTexture[iEnvB].FileName  : "";
    const char* pDetNameA  = (iDetA >=0) ? RawMesh.m_pTexture[iDetA].FileName  : "";
    const char* pDetNameB  = (iDetB >=0) ? RawMesh.m_pTexture[iDetB].FileName  : "";
    const char* pPunNameA  = (iPunA >=0) ? RawMesh.m_pTexture[iPunA].FileName  : "";
    const char* pPunNameB  = (iPunB >=0) ? RawMesh.m_pTexture[iPunB].FileName  : "";
    if( x_strcmp( pDiffNameA, pDiffNameB ) ) return FALSE;
    if( x_strcmp( pEnvNameA,  pEnvNameB  ) ) return FALSE;
    if( x_strcmp( pDetNameA,  pDetNameB  ) ) return FALSE;
    if( x_strcmp( pPunNameA,  pPunNameB  ) ) return FALSE;
        
    // parameter check
    if( RawMatA.bTwoSided                          != RawMatB.bTwoSided                          ) return FALSE;
    if( RawMatA.Constants[DETAIL_SCALE].Current[0] != RawMatB.Constants[DETAIL_SCALE].Current[0] ) return FALSE;
    if( RawMatA.Constants[FIXED_ALPHA].Current[0]  != RawMatB.Constants[FIXED_ALPHA].Current[0]  ) return FALSE;
    if( RawMatA.Constants[ENV_TYPE].Current[0]     != RawMatB.Constants[ENV_TYPE].Current[0]     ) return FALSE;
    if( RawMatA.Constants[FORCE_ZFILL].Current[0]  != RawMatB.Constants[FORCE_ZFILL].Current[0]  ) return FALSE;
    if( RawMatA.Constants[USE_DIFFUSE].Current[0]  != RawMatB.Constants[USE_DIFFUSE].Current[0]  ) return FALSE;
    if( RawMatA.Constants[ENV_BLEND].Current[0]    != RawMatB.Constants[ENV_BLEND].Current[0]    ) return FALSE;

    // uv anim check
    const rawmesh2::param_pkg& ParamA = RawMatA.Map[Max_Diffuse1].UVTranslation;
    const rawmesh2::param_pkg& ParamB = RawMatB.Map[Max_Diffuse1].UVTranslation;
    if( ParamA.nKeys         != ParamB.nKeys         ) return FALSE;
    if( ParamA.nParamsPerKey != ParamB.nParamsPerKey ) return FALSE;
    for( s32 i = 0; i < ParamA.nKeys * ParamA.nParamsPerKey; i+= ParamA.nParamsPerKey )
    {
        s32 iKeyA = RawMatA.iFirstKey + i;
        s32 iKeyB = RawMatB.iFirstKey + i;
        if( RawMesh.m_pParamKey[iKeyA + 0] != RawMesh.m_pParamKey[iKeyB + 0] ) return FALSE;
        if( RawMesh.m_pParamKey[iKeyA + 1] != RawMesh.m_pParamKey[iKeyB + 1] ) return FALSE;
    }

    return TRUE;
}

//=============================================================================

void geom_compiler::BuildCompileModel( geom& Geom,
                                       const rawmesh2& RawMesh,
                                       mesh& Mesh,
                                       xbool IsRigid )
{
    rawmesh2 PhysicsMesh;
    const rawmesh2* pPhysicsMesh = &RawMesh;
    if( m_PhysicsSource[0] )
    {
        LoadSourceMesh( m_PhysicsSource, PhysicsMesh );
        pPhysicsMesh = &PhysicsMesh;
    }

    if( g_Verbose )
        pPhysicsMesh->PrintRigidBodies();

    BuildBones( Geom, RawMesh, *pPhysicsMesh );
    BuildRigidBodies( Geom, RawMesh, *pPhysicsMesh );
    BuildSettings( Geom, m_SettingsFile, RawMesh );

    if( RawMesh.m_nMaterials <= 0 )
        ThrowError( "No materials are defined in the source mesh" );
    if( RawMesh.m_nSubMeshs <= 0 )
        ThrowError( "No meshes are defined in the source mesh" );

    xarray<s32> MaterialMap;
    xarray<s32> MeshMap;
    xarray<xbool> FacetUsed;
    MaterialMap.SetCount( RawMesh.m_nMaterials );
    MeshMap.SetCount( RawMesh.m_nSubMeshs );
    FacetUsed.SetCount( RawMesh.m_nFacets );
    for( s32 i = 0; i < MaterialMap.GetCount(); i++ )
        MaterialMap[i] = -1;
    for( s32 i = 0; i < MeshMap.GetCount(); i++ )
        MeshMap[i] = -1;

    s32 nDegenerateFacets = 0;

    for( s32 iFacet = 0; iFacet < RawMesh.m_nFacets; iFacet++ )
    {
        const rawmesh2::facet& Facet = RawMesh.m_pFacet[iFacet];
        FacetUsed[iFacet] = FALSE;
        if( (Facet.iMesh < 0) || (Facet.iMesh >= RawMesh.m_nSubMeshs) )
            ThrowError( xfs( "Facet %d has invalid mesh index %d",
                             iFacet,
                             Facet.iMesh ) );
        if( (Facet.iMaterial < 0) ||
            (Facet.iMaterial >= RawMesh.m_nMaterials) )
        {
            ThrowError( xfs( "Facet %d has invalid material index %d",
                             iFacet,
                             Facet.iMaterial ) );
        }
        if( Facet.nVertices != 3 )
            ThrowError( "Only triangle facets are supported" );

        for( s32 iCorner = 0; iCorner < 3; iCorner++ )
        {
            if( (Facet.iVertex[iCorner] < 0) ||
                (Facet.iVertex[iCorner] >= RawMesh.m_nVertices) )
            {
                ThrowError( xfs( "Facet %d references invalid vertex %d",
                                 iFacet,
                                 Facet.iVertex[iCorner] ) );
            }
        }

        const vector3& P0 = RawMesh.m_pVertex[Facet.iVertex[0]].Position;
        const vector3& P1 = RawMesh.m_pVertex[Facet.iVertex[1]].Position;
        const vector3& P2 = RawMesh.m_pVertex[Facet.iVertex[2]].Position;
        if( v3_Cross( P1 - P0, P2 - P0 ).Length() < 0.00001f )
        {
            nDegenerateFacets++;
            continue;
        }

        MaterialMap[Facet.iMaterial] = 0;
        MeshMap[Facet.iMesh] = 0;
        FacetUsed[iFacet] = TRUE;
    }

    if( nDegenerateFacets > 0 )
    {
        ReportWarning( xfs( "Ignored %d degenerate triangle(s)",
                            nDegenerateFacets ) );
    }
    if( (RawMesh.m_nFacets - nDegenerateFacets) <= 0 )
        ThrowError( "Source mesh contains no renderable triangles" );

    for( s32 iMaterial = 0;
         iMaterial < RawMesh.m_nMaterials;
         iMaterial++ )
    {
        if( MaterialMap[iMaterial] == -1 )
            continue;

        s32 iCompiledMaterial = 0;
        for( ; iCompiledMaterial < Mesh.Material.GetCount(); iCompiledMaterial++ )
        {
            const material& Existing = Mesh.Material[iCompiledMaterial];
            if( IsSameMaterial(
                    Existing.pRawMesh->m_pMaterial[Existing.iRawMaterial],
                    RawMesh.m_pMaterial[iMaterial],
                    RawMesh ) )
            {
                break;
            }
        }

        if( iCompiledMaterial == Mesh.Material.GetCount() )
        {
            material& Material = Mesh.Material.Append();
            Material.pRawMesh     = &RawMesh;
            Material.iRawMaterial = iMaterial;

            rawmesh2::material& RawMaterial =
                RawMesh.m_pMaterial[iMaterial];
            if( (RawMaterial.Map[Max_Diffuse1].iTexture < 0) ||
                (RawMaterial.Map[Max_Diffuse1].iTexture >= RawMesh.m_nTextures) )
            {
                ThrowError( xfs( "Material has an invalid diffuse texture (%s)",
                                 RawMaterial.Name ) );
            }

            rawmesh2::texture& RawTexture =
                RawMesh.m_pTexture[RawMaterial.Map[Max_Diffuse1].iTexture];
            char Extension[X_MAX_EXT];
            x_splitpath( RawTexture.FileName, NULL, NULL, NULL, Extension );
            if( !x_stricmp( Extension, ".ifl" ) )
            {
                ThrowError( xfs( "IFL textures are not supported (%s)",
                                 RawTexture.FileName ) );
            }

            if( !Material.TexInfo.Load( RawTexture.FileName ) )
                ReportWarning( "Unable to load texinfo; defaults will be used" );
        }

        MaterialMap[iMaterial] = iCompiledMaterial;
    }

    for( s32 iMesh = 0; iMesh < RawMesh.m_nSubMeshs; iMesh++ )
    {
        if( MeshMap[iMesh] == -1 )
            continue;

        MeshMap[iMesh] = Mesh.SubMesh.GetCount();
        sub_mesh& Submesh = Mesh.SubMesh.Append();
        Submesh.pRawMesh    = &RawMesh;
        Submesh.pRawSubMesh = &RawMesh.m_pSubMesh[iMesh];
        x_strsavecpy( Submesh.Name,
                      RawMesh.m_pSubMesh[iMesh].Name,
                      sizeof(Submesh.Name) );
    }

    m_RawMeshToCompiled.SetCount( MeshMap.GetCount() );
    for( s32 iMesh = 0; iMesh < MeshMap.GetCount(); iMesh++ )
        m_RawMeshToCompiled[iMesh] = MeshMap[iMesh];

    for( s32 iFacet = 0; iFacet < RawMesh.m_nFacets; iFacet++ )
    {
        if( !FacetUsed[iFacet] )
            continue;

        const rawmesh2::facet& Facet = RawMesh.m_pFacet[iFacet];
        sub_mesh& Submesh = Mesh.SubMesh[MeshMap[Facet.iMesh]];

        s32 iBone = -1;
        if( IsRigid )
        {
            const rawmesh2::vertex& Vertex =
                RawMesh.m_pVertex[Facet.iVertex[0]];
            if( Vertex.nWeights < 1 )
                ThrowError( "Rigid facet has a vertex without a bone weight" );
            iBone = Vertex.Weight[0].iBone;
            if( (iBone < 0) || (iBone >= RawMesh.m_nBones) )
                ThrowError( "Rigid facet references an invalid bone" );

            for( s32 iCorner = 1; iCorner < 3; iCorner++ )
            {
                const rawmesh2::vertex& Corner =
                    RawMesh.m_pVertex[Facet.iVertex[iCorner]];
                if( (Corner.nWeights < 1) ||
                    (Corner.Weight[0].iBone != iBone) )
                {
                    ThrowError( "Rigid triangle spans multiple bones" );
                }
            }
        }

        const s32 iMaterial = MaterialMap[Facet.iMaterial];
        s32 iSurface = 0;
        for( ; iSurface < Submesh.Surfaces.GetCount(); iSurface++ )
        {
            const surface& Existing = Submesh.Surfaces[iSurface];
            if( (Existing.iMaterial == iMaterial) &&
                (Existing.iBone == iBone) )
            {
                break;
            }
        }

        if( iSurface == Submesh.Surfaces.GetCount() )
        {
            surface& Surface = Submesh.Surfaces.Append();
            Surface.iMaterial = iMaterial;
            Surface.iBone     = iBone;
        }

        Submesh.Surfaces[iSurface].Facets.Append( iFacet );
    }
}

//=============================================================================

