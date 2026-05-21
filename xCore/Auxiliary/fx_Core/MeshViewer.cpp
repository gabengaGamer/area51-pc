
#include "MeshViewer.hpp"
#include "Entropy/e_Draw.hpp"
#include "Entropy/e_VRAM.hpp"
#include "Entropy/e_ScratchMem.hpp"
#include "Auxiliary/Bitmap/aux_Bitmap.hpp"

namespace fx_core
{

//=========================================================================
// FUNCTIONS
//=========================================================================

void mesh_viewer::CleanUp( void )
{
    for( s32 i=0; i<m_Mesh.m_nTextures; i++ )
    {
        if( m_Bitmap[i].GetVRAMID() != 0 )
        {
            vram_Unregister( m_Bitmap[i] );
            m_Bitmap[i].Kill();
        }
    }

    m_Mesh.~rawmesh();
    m_Anim.~rawanim();

    m_Mesh.rawmesh::rawmesh();
    m_Anim.rawanim::rawanim();


    m_AnimFrameRate = 30;
}

//=========================================================================

mesh_viewer::mesh_viewer()
{
    //set bbox so view doesn't corrupt
    m_BBox.Set(vector3(0,0,0),10);
    m_bBackFacets = FALSE;
}

//=========================================================================

mesh_viewer::mesh_viewer( const mesh_viewer& mViewer )
{
    //set bbox so view doesn't corrupt
    m_BBox.Set(vector3(0,0,0),10);
    m_bBackFacets = FALSE;
}

//=========================================================================

mesh_viewer::~mesh_viewer( void )
{
    for( s32 i=0; i<m_Mesh.m_nTextures; i++ )
    {
        if( m_Bitmap[i].GetVRAMID() != 0 )
        {
            vram_Unregister( m_Bitmap[i] );
            m_Bitmap[i].Kill();
        }
    }
}

//=========================================================================

void mesh_viewer::Load( const char* pFileName )
{
    CleanUp();

    m_Mesh.Load( pFileName );
    m_Anim.Load( pFileName );

    m_L2W.Identity();
    m_Frame         = 0;
    m_bPlayAnim     = FALSE;

    m_BBox = m_Mesh.GetBBox();
    m_LightDir.Set( -0.5f, 1, -0.5f );
    m_LightDir.Normalize();

    m_Mesh.SortFacetsByMaterialAndBone();
    for( s32 i=0; i<m_Mesh.m_nTextures; i++ )
    {
        if( auxbmp_LoadD3D( m_Bitmap[i], m_Mesh.m_pTexture[i].FileName ) )
        {
        }        
        vram_Register( m_Bitmap[i] );
    }

    m_Ambient.Set( 0.5f, 0.5f, 0.5f );
}

//=========================================================================

void mesh_viewer::Unload( void )
{
    CleanUp();
}

//=========================================================================

void mesh_viewer::Render( xcolor TintColor )
{
    if( m_Mesh.m_nBones == 1 )
    {
        RenderSolid( TintColor );
    }
    else
    {
        RenderSoftSkin();
    }
}

//=========================================================================

void mesh_viewer::SetBackFacets( xbool bFaceFacets )
{
    m_bBackFacets = bFaceFacets;
}

//=========================================================================

void mesh_viewer::RenderSolid( xcolor TintColor )
{
    rawmesh::vertex*    pVertex         = NULL;

    s32     i, j;

    s32     iMaterial;
    s32     iVertex;
    s32     iTexture;

    vector3 N;
    vector3 L = m_LightDir;
    f32     I;
    xcolor  VertColor;
    u32     DrawFlags = DRAW_TEXTURED | DRAW_USE_ALPHA;

    if( m_bBackFacets )
    {
        DrawFlags |= DRAW_CULL_NONE;
    }

    draw_Begin( DRAW_TRIANGLES, DrawFlags );

    for( iMaterial = 0; iMaterial < m_Mesh.m_nMaterials; iMaterial++ )
    {
        // Activate the diffuse texture for this material
        iTexture    = m_Mesh.m_pMaterial[ iMaterial ].TexMaterial[0].iTexture;

        if( m_Bitmap[iTexture].GetVRAMID() )   { draw_SetTexture( m_Bitmap[iTexture] ); }
        else                                   { draw_SetTexture(); }

        for( i = 0; i < m_Mesh.m_nFacets; i++ )
        {
            if( m_Mesh.m_pFacet[i].iMaterial == iMaterial )
            {
                for( j = 0; j < 3; j++ )
                {
                    iVertex = m_Mesh.m_pFacet[i].iVertex[j];
                    pVertex = &( m_Mesh.m_pVertex[ iVertex ] );

                    N = pVertex->Normal[0];
                    I = fMax( 0, L.Dot( N ) );

                    ASSERT( I >= 0 );

                    VertColor.SetfRGBA( ( m_Ambient.GetX() + (TintColor.R / 255.0f * I) ),
                                        ( m_Ambient.GetY() + (TintColor.G / 255.0f * I) ),
                                        ( m_Ambient.GetZ() + (TintColor.B / 255.0f * I) ),
                                        (f32)TintColor.A / 255.0f );

                    draw_UV( pVertex->UV[0] );
                    draw_Color( VertColor );
                    draw_Vertex( pVertex->Position );
                }
            }
        }
    }

    draw_End();
}

//=========================================================================

void mesh_viewer::RenderSoftSkin( void )
{
    struct lovert
    {
        vector3 P;
        xcolor  C;
        vector2 UV;
    };

    s32 i;
    vector3 LightDir(1,0,0);

    // Check whether we have something to do
    if( m_Anim.m_nBones == 0 )
        return;

    // Allocate all the matrices
    smem_StackPushMarker();
    matrix4* pMatrix = (matrix4*)smem_StackAlloc( m_Anim.m_nBones * sizeof(matrix4) );
    if( pMatrix == NULL )
    {
        smem_StackPopToMarker();
        return;
    }

    // Compute the frame
    if( m_bPlayAnim )
    {
        f32 Time = m_Timer.TripSec();
        m_Frame += Time*m_AnimFrameRate;
    }
    m_Anim.ComputeBonesL2W( pMatrix, m_Frame );

    // Compute final matrices
    for( i=0; i<m_Anim.m_nBones; i++ )
    {
        pMatrix[i] = m_L2W * pMatrix[i];
    }

    // Render triangles
    s32 iLastTex = -1;
    u32 DrawFlags = DRAW_TEXTURED | DRAW_USE_ALPHA;

    if( m_bBackFacets )
    {
        DrawFlags |= DRAW_CULL_NONE;
    }

    draw_Begin( DRAW_TRIANGLES, DrawFlags );

    for( i=0; i<m_Mesh.m_nFacets; i++ )
    {
        lovert      V[3];
        vector3     N[3];
        const rawmesh::facet& Facet = m_Mesh.m_pFacet[i];
        f32         MaxWeight = 0;
        s32         iBone;

        // This needs to update
        if( m_Mesh.m_pMaterial[ Facet.iMaterial ].TexMaterial[0].iTexture != iLastTex )
        {
            iLastTex = m_Mesh.m_pMaterial[ Facet.iMaterial ].TexMaterial[0].iTexture;
            if( m_Bitmap[iLastTex].GetVRAMID() ) draw_SetTexture( m_Bitmap[iLastTex] );
            else                                 draw_SetTexture();
        }

        for( s32 j=0; j<3; j++ )
        {
            const rawmesh::vertex& Vert = m_Mesh.m_pVertex[ Facet.iVertex[j] ];
            V[j].P.Zero();
            N[j].Zero();
            
            for( s32 w=0; w<Vert.nWeights; w++ )
            {
                const rawmesh::weight& W = Vert.Weight[w];

                if( W.Weight > MaxWeight )
                {
                    MaxWeight = W.Weight;
                    iBone     = W.iBone;
                }

                V[j].P += (pMatrix[ W.iBone ] * Vert.Position) * W.Weight;
                N[j]   += pMatrix[ W.iBone ].RotateVector( Vert.Normal[0] ) * W.Weight;
            }

            N[j].Normalize();
            V[j].UV.X = Vert.UV[0].X; 
            V[j].UV.Y = Vert.UV[0].Y;  
            f32 I     = fMax( 0, LightDir.Dot( N[j] ) );
                I     = fMin( 1, I );

            //ASSERT( I >= 0 );

            V[j].C.SetfRGBA( fMin( 1, m_Ambient.GetX() + I), 
                             fMin( 1, m_Ambient.GetY() + I), 
                             fMin( 1, m_Ambient.GetZ() + I), 1 );

        }

        for( s32 j=0; j<3; j++ )
        {
            draw_UV( V[j].UV );
            draw_Color( V[j].C );
            draw_Vertex( V[j].P );
        }
    }

    draw_End();

    // Free alloced memory
    smem_StackPopToMarker();
}

//=========================================================================

void mesh_viewer::PlayAnimation( void )
{
    // Nothing to do
    if( m_Mesh.m_nBones == 0 )
        return;

    if( m_Anim.m_nFrames == 0 )
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

} // namespace fx_core
