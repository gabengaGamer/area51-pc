
#include "stdafx.h"
#include "MeshViewer.hpp"
#include "aux_Bitmap.hpp"



//=========================================================================
// FUNCTIONS
//=========================================================================

//=========================================================================
void mesh_viewer::CleanUp( void )
{
    for (s32 j = 0; j < MAX_MESHES; j++)
    {
        for( s32 i=0; i<m_Mesh[j].m_nTextures; i++ )
        {
            if( m_Bitmap[j][i].GetVRAMID() != 0 )
            {
                vram_Unregister( m_Bitmap[j][i] );
                m_Bitmap[j][i].Kill();
            }
        }

        m_Mesh[j].Kill();
        m_Anim[j].~rawanim();
        m_Anim[j].rawanim::rawanim();
    }

    m_BBox.Set(vector3(0,0,0),10);
    m_nCurrentMeshCount = 0;
    m_AnimFrameRate = 30;
}

//=========================================================================

mesh_viewer::mesh_viewer()
{
    //set bbox so view doesn't corrupt
    m_BBox.Set(vector3(0,0,0),10);
    m_nCurrentMeshCount = 0;
    m_bBackFacets = FALSE;
    m_bPlayInPlace = FALSE;
    m_bRenderToBind = FALSE;
    m_bRenderSkeleton = FALSE;
    m_bRenderSkeletonLabels = FALSE;
}

//=========================================================================

mesh_viewer::~mesh_viewer( void )
{
    for (s32 j = 0; j < MAX_MESHES; j++)
    {
        for( s32 i=0; i<m_Mesh[j].m_nTextures; i++ )
        {
            if( m_Bitmap[j][i].GetVRAMID() != 0 )
            {
                vram_Unregister( m_Bitmap[j][i] );
                m_Bitmap[j][i].Kill();
            }
        }
    }
}

//=========================================================================

void mesh_viewer::Load( const char* pFileName )
{
    CleanUp();
    LoadAdditional( pFileName, vector3(0,0,0), radian3(0,0,0) );
}

//=========================================================================

void mesh_viewer::LoadAdditional( const char* pFileName, const vector3& Pos, const radian3& Rot )
{
    x_try;

    if ((m_nCurrentMeshCount+1) >= MAX_MESHES)
    {
        return;
    }

    m_Mesh[m_nCurrentMeshCount].Load( pFileName );
    m_Anim[m_nCurrentMeshCount].Load( pFileName );
    m_Mesh[m_nCurrentMeshCount].ApplyNewSkeleton( m_Anim[m_nCurrentMeshCount] );

    m_L2W[m_nCurrentMeshCount].Identity();
    m_L2W[m_nCurrentMeshCount].Rotate(Rot);
    m_L2W[m_nCurrentMeshCount].Translate(Pos);

    m_Frame         = 0;
    m_bPlayAnim     = FALSE;

    m_BBox += m_Mesh[m_nCurrentMeshCount].GetBBox();
    m_LightDir.Set( -0.5f, 1, -0.5f );
    m_LightDir.Normalize();

    if( m_Mesh[m_nCurrentMeshCount].m_nFacets <= 0 )
    {
        CleanUp();
        x_throw( "The mesh didn't have any facets" );
    }

    m_Mesh[m_nCurrentMeshCount].SortFacetsByMaterial();
    
    for( s32 i=0; i<m_Mesh[m_nCurrentMeshCount].m_nTextures; i++ )
    {
        if( auxbmp_LoadD3D( m_Bitmap[m_nCurrentMeshCount][i], m_Mesh[m_nCurrentMeshCount].m_pTexture[i].FileName ) )
        {
            vram_Register( m_Bitmap[m_nCurrentMeshCount][i] );
        }
        else
        {
            x_DebugMsg("Failed to load texture: %s\n", m_Mesh[m_nCurrentMeshCount].m_pTexture[i].FileName);
        }
    }

    m_Ambient.Set( 0.5f, 0.5f, 0.5f );
    m_nCurrentMeshCount++;

    x_catch_display;
}

//=========================================================================

void mesh_viewer::Render( void )
{
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,          D3DTOP_MODULATE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1,        D3DTA_DIFFUSE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2,        D3DTA_TEXTURE );

    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,          D3DTOP_DISABLE );

    if( m_bBackFacets )
    {
        g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    }
    else
    {
        g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
    }

    for (s32 j = 0; j < m_nCurrentMeshCount; j++)
    {
        if( m_Mesh[j].m_nBones == 1 )
        {
            RenderSolid(j);
        }
        else
        {
            RenderSoftSkin(j);
        }
    }
}

//=========================================================================

void mesh_viewer::SetBackFacets( xbool bFaceFacets )
{
    m_bBackFacets = bFaceFacets;
}

//=========================================================================

void mesh_viewer::RenderSolid( s32 nMesh )
{
    g_pd3dDevice->SetFVF( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 );
    struct lovert
    {
        vector3p P;
        xcolor   C;
        vector2  UV;
    };

    s32 iLastTex = -1;
    for( s32 i=0; i<m_Mesh[nMesh].m_nFacets; i++ )
    {
        lovert      V[3];

        // This needs to update
        if ( m_Mesh[nMesh].m_nMaterials <= 0 )
        {
            x_DebugMsg("Missing material for MATX\n");
            continue;
        }

        const s32 iMaterial = m_Mesh[nMesh].m_pFacet[i].iMaterial;
        if (iMaterial < 0 || iMaterial >= m_Mesh[nMesh].m_nMaterials)
        {
            x_DebugMsg("Missing material for MATX\n");
            continue;
        }

        const rawmesh2::material& material = m_Mesh[nMesh].m_pMaterial[iMaterial];

        s32 textureIndex = -1;
        if (material.nMaps > 0)
        {
            textureIndex = material.Map[0].iTexture;
        }

        if (textureIndex != iLastTex)
        {
            iLastTex = textureIndex;
            if (textureIndex >= 0 && textureIndex < m_Mesh[nMesh].m_nTextures)
            {
                if (m_Bitmap[nMesh][textureIndex].GetVRAMID())
                    vram_Activate(m_Bitmap[nMesh][textureIndex]);
                else
                    vram_Activate();
            }
            else
            {
                vram_Activate();
            }
        }

        for( s32 j=0; j<3; j++ )
        {
            const s32 vertexIndex = m_Mesh[nMesh].m_pFacet[i].iVertex[j];
            if (vertexIndex < 0 || vertexIndex >= m_Mesh[nMesh].m_nVertices)
            {
                x_DebugMsg("Invalid vertex index\n");
                continue;
            }

            const rawmesh2::vertex& vertex = m_Mesh[nMesh].m_pVertex[vertexIndex];

            vector3 P = vertex.Position;
            P.Rotate( m_L2W[nMesh].GetRotation() );
            P += m_L2W[nMesh].GetTranslation();
            V[j].P = P;

            if (vertex.nUVs > 0)
            {
                s32 uvChannel = 0;
                if (material.nMaps > 0)
                {
                    uvChannel = material.Map[0].iUVChannel;
                    if (uvChannel >= vertex.nUVs)
                        uvChannel = 0;
                }
                V[j].UV = vertex.UV[uvChannel];
            }
            else
            {
                V[j].UV.X = 0;
                V[j].UV.Y = 0;
            }

            vector3 N = (vertex.nNormals > 0) ? vertex.Normal[0] : vector3(0, 1, 0);
            vector3 L = m_LightDir;
            f32 I = fMax(0, L.Dot(N));

            vector3 Amb(m_Ambient);
            Amb += vector3(I, I, I);
            
            if (material.TintType != rawmesh2::TINT_TYPE_NONE)
            {
                f32 r = material.TintColor.Current[0] / 255.0f;
                f32 g = material.TintColor.Current[1] / 255.0f;
                f32 b = material.TintColor.Current[2] / 255.0f;
                
                if (material.TintType == rawmesh2::TINT_TYPE_FINAL_OUTPUT)
                {
                    Amb.GetX() = Amb.GetX() * r;
                    Amb.GetY() = Amb.GetY() * g;
                    Amb.GetZ() = Amb.GetZ() * b;
                }
            }
            
            Amb.Min(1.0f);
            V[j].C.SetfRGBA(Amb.GetX(), Amb.GetY(), Amb.GetZ(), 1);
        }

        if (material.bTwoSided)
        {
            g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        }
        else if (m_bBackFacets)
        {
            g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        }
        else
        {
            g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
        }

        g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLELIST, 1, V, sizeof(lovert) );
    }
}

//=========================================================================

void mesh_viewer::RenderSoftSkin( s32 nMesh )
{
    struct lovert
    {
        vector3p P;
        xcolor   C;
        vector2  UV;
    };

    vector3 LightDir(1,0,0);

    // Check whether we have something to do
    if( m_Anim[nMesh].m_nBones == 0 )
        return;

    g_pd3dDevice->SetFVF( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 );

    // Allocate all the matrices
    smem_StackPushMarker();
    matrix4* pMatrix = (matrix4*)smem_StackAlloc( m_Anim[nMesh].m_nBones * sizeof(matrix4) );
    if( pMatrix == NULL )
        return;

    // Compute the frame
    if( m_bPlayAnim )
    {
        f32 Time = m_Timer.TripSec();
        m_Frame += Time*m_AnimFrameRate;
    }
    m_Anim[nMesh].ComputeBonesL2W( pMatrix, m_Frame );

    // Compute final matrices
    for( s32 i=0; i<m_Anim[nMesh].m_nBones; i++ )
    {
        m_BonePos[i] = pMatrix[i] * m_Anim[nMesh].m_pBone[i].BindTranslation;
        pMatrix[i] = m_L2W[nMesh] * pMatrix[i];
    }

    if( m_bPlayInPlace )
    {
        vector3 Pos = pMatrix[0].GetTranslation();
        for( s32 i=0; i<m_Anim[nMesh].m_nBones; i++ )
        {
            pMatrix[i].SetTranslation( pMatrix[i].GetTranslation() - Pos );
        }
    }   

    // Renders the mesh to the bind position this is usefull for debugging
    if( m_bRenderToBind )
    {
        for( s32 i=0; i<m_Anim[nMesh].m_nBones; i++ )
        {
            pMatrix[i] = m_Anim[nMesh].m_pBone[i].BindMatrixInv;
        }
    }

    // Renders the skeleton handy for debugging
    if( m_bRenderSkeleton )
    {
        for( s32 i=0; i<m_Anim[nMesh].m_nBones; i++ )
        {
            draw_Marker( m_BonePos[i], XCOLOR_RED );
            if( m_Anim[nMesh].m_pBone[i].iParent != -1 )
            {
                draw_Line( m_BonePos[i], m_BonePos[m_Anim[nMesh].m_pBone[i].iParent], XCOLOR_RED );
            }
        }
    }

    // Renders the skeleton handy for debugging
    if( m_bRenderSkeletonLabels )
    {
        for( s32 i=0; i<m_Anim[nMesh].m_nBones; i++ )
        {
            draw_Label( m_BonePos[i], XCOLOR_WHITE, m_Anim[nMesh].m_pBone[i].Name );
        }
    }

    // Render triangles
    s32 iLastTex = -1;
    s32 iLastMat = -1;
    for( s32 i=0; i<m_Mesh[nMesh].m_nFacets; i++ )
    {
        lovert      V[3];
        vector3     N[3];
        
        const rawmesh2::facet& Facet = m_Mesh[nMesh].m_pFacet[i];
        const s32 iMaterial = Facet.iMaterial;
        
        // This needs to update
        if (iMaterial < 0 || iMaterial >= m_Mesh[nMesh].m_nMaterials)
        {
            x_DebugMsg("Missing material for MATX\n");
            continue;
        }
        
        const rawmesh2::material& material = m_Mesh[nMesh].m_pMaterial[iMaterial];
        
        if (iLastMat != iMaterial)
        {
            iLastMat = iMaterial;
            
            if (material.bTwoSided)
            {
                g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            }
            else if (m_bBackFacets)
            {
                g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            }
            else
            {
                g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
            }
        }
        
        s32 textureIndex = -1;
        s32 uvChannel = 0;
        
        if (material.nMaps > 0)
        {
            textureIndex = material.Map[0].iTexture;
            uvChannel = material.Map[0].iUVChannel;
        }
        
        if (textureIndex != iLastTex)
        {
            iLastTex = textureIndex;
            if (textureIndex >= 0 && textureIndex < m_Mesh[nMesh].m_nTextures)
            {
                if (m_Bitmap[nMesh][textureIndex].GetVRAMID())
                    vram_Activate(m_Bitmap[nMesh][textureIndex]);
                else
                    vram_Activate();
            }
            else
            {
                vram_Activate();
            }
        }
        
        for( s32 j=0; j<3; j++ )
        {
            const s32 vertexIndex = Facet.iVertex[j];
            if (vertexIndex < 0 || vertexIndex >= m_Mesh[nMesh].m_nVertices)
            {
                x_DebugMsg("Invalid vertex index\n");
                continue;
            }
            
            const rawmesh2::vertex& Vert = m_Mesh[nMesh].m_pVertex[vertexIndex];
            
            V[j].P.Set(0.0f, 0.0f, 0.0f);
            N[j].Zero();
            
            for( s32 w=0; w<Vert.nWeights; w++ )
            {
                const rawmesh2::weight& W = Vert.Weight[w];
                if (W.iBone < 0 || W.iBone >= m_Anim[nMesh].m_nBones)
                    continue;
                
                vector3 P = (pMatrix[W.iBone] * Vert.Position) * W.Weight;
                V[j].P.X += P.GetX();
                V[j].P.Y += P.GetY();
                V[j].P.Z += P.GetZ();
                
                if (Vert.nNormals > 0)
                {
                    N[j] += pMatrix[W.iBone].RotateVector(Vert.Normal[0]) * W.Weight;
                }
            }
            
            if (N[j].LengthSquared() > 0.00000001f)
                N[j].Normalize();
            else
                N[j].Set(0, 1, 0);
            
            if (Vert.nUVs > 0)
            {
                if (uvChannel < Vert.nUVs)
                    V[j].UV = Vert.UV[uvChannel];
                else
                    V[j].UV = Vert.UV[0];
            }
            else
            {
                V[j].UV.X = 0;
                V[j].UV.Y = 0;
            }
            
            vector3 LightDir(m_LightDir);
            f32 I = fMax(0, LightDir.Dot(N[j]));
            I = fMin(1, I);
            
            vector3 Amb = m_Ambient;
            Amb += vector3(I, I, I);
            
            if (material.TintType != rawmesh2::TINT_TYPE_NONE)
            {
                f32 r = material.TintColor.Current[0] / 255.0f;
                f32 g = material.TintColor.Current[1] / 255.0f;
                f32 b = material.TintColor.Current[2] / 255.0f;
                
                if (material.TintType == rawmesh2::TINT_TYPE_FINAL_OUTPUT)
                {
                    Amb.GetX() = Amb.GetX() * r;
                    Amb.GetY() = Amb.GetY() * g;
                    Amb.GetZ() = Amb.GetZ() * b;
                }
            }
            
            Amb.Min(1.0f);
            V[j].C.SetfRGBA(Amb.GetX(), Amb.GetY(), Amb.GetZ(), 1);
        }
        
        g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLELIST, 1, V, sizeof(lovert) );
    }
    
    smem_StackPopToMarker();
}

//=========================================================================

void mesh_viewer::PlayAnimation( xbool bPlayInPlace )
{
    m_bPlayInPlace = bPlayInPlace;
    
    // Nothing to do
    if( m_Mesh[0].m_nBones == 0 )
        return;

    if( m_Anim[0].m_nFrames == 0 )
        x_throw( "Mesh doesn't have animation" );

    m_bPlayAnim = TRUE;
    m_Timer.Start();
}

//=========================================================================

void mesh_viewer::PauseAnimation  ( void )
{
    m_bPlayAnim = FALSE;
    m_Timer.Stop();
}


