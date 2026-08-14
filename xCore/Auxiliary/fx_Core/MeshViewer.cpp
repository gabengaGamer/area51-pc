
#include "MeshViewer.hpp"
#include "Entropy/e_VRAM.hpp"
#include "Entropy/e_ScratchMem.hpp"
#include "Auxiliary/Bitmap/aux_Bitmap.hpp"
#include "PreviewRender.hpp"

namespace fx_core
{

//=========================================================================
// FUNCTIONS
//=========================================================================

void mesh_viewer::CleanUp( void )
{
    for( s32 i=0; i<m_Mesh.m_nTextures; i++ )
    {
        if( m_Texture[i].m_texture )
            vram_DestroyTexture( m_Texture[i].m_texture );
        m_Texture[i].m_bitmap.Kill();
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
    m_bBox.Set(vector3(0,0,0),10);
    m_bBackFacets = FALSE;
}

//=========================================================================

mesh_viewer::mesh_viewer( const mesh_viewer& mViewer )
{
    //set bbox so view doesn't corrupt
    m_bBox.Set(vector3(0,0,0),10);
    m_bBackFacets = FALSE;
}

//=========================================================================

mesh_viewer::~mesh_viewer( void )
{
    for( s32 i=0; i<m_Mesh.m_nTextures; i++ )
    {
        if( m_Texture[i].m_texture )
            vram_DestroyTexture( m_Texture[i].m_texture );
        m_Texture[i].m_bitmap.Kill();
    }
}

//=========================================================================

void mesh_viewer::Load( const char* pFileName )
{
    CleanUp();

    m_Mesh.Load( pFileName );
    m_Anim.Load( pFileName );

    m_Frame         = 0;
    m_bPlayAnim     = FALSE;

    m_bBox = m_Mesh.GetBBox();
    m_LightDir.Set( -0.5f, 1, -0.5f );
    m_LightDir.Normalize();

    m_Mesh.SortFacetsByMaterialAndBone();
    for( s32 i=0; i<m_Mesh.m_nTextures; i++ )
    {
        if( auxbmp_LoadD3D( m_Texture[i].m_bitmap, m_Mesh.m_pTexture[i].FileName ) )
            VERIFY( vram_CreateTexture( m_Texture[i].m_texture,
                                        m_Texture[i].m_bitmap,
                                        TRUE,
                                        "fx_mesh_viewer" ) );
    }

    m_Ambient.Set( 0.5f, 0.5f, 0.5f );
}

//=========================================================================

void mesh_viewer::Unload( void )
{
    CleanUp();
}

//=========================================================================

void mesh_viewer::Render( xcolor TintColor, const matrix4& LocalToWorld )
{
    if( m_Mesh.m_nBones == 1 )
    {
        RenderSolid( TintColor, LocalToWorld );
    }
    else
    {
        RenderSoftSkin( LocalToWorld );
    }
}

//=========================================================================

void mesh_viewer::SetBackFacets( xbool bFaceFacets )
{
    m_bBackFacets = bFaceFacets;
}

//=========================================================================

void mesh_viewer::RenderSolid( xcolor TintColor, const matrix4& LocalToWorld )
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
    for( iMaterial = 0; iMaterial < m_Mesh.m_nMaterials; iMaterial++ )
    {
        // Activate the diffuse texture for this material
        iTexture    = m_Mesh.m_pMaterial[ iMaterial ].TexMaterial[0].iTexture;
        const texture* pTexture = NULL;
        if( (iTexture >= 0) && (iTexture < 32) && m_Texture[iTexture].GetShaderResource() )
            pTexture = &m_Texture[iTexture];

        const render::primitive_draw_desc Material(
            pTexture,
            render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            render::PRIMITIVE_BLEND_ALPHA,
            render::PRIMITIVE_DEPTH_READ_ONLY,
            m_bBackFacets ? render::PRIMITIVE_RASTER_SOLID_NO_CULL
                          : render::PRIMITIVE_RASTER_SOLID,
            render::PRIMITIVE_SAMPLER_LINEAR_WRAP,
            render::PRIMITIVE_LAYER_TRANSPARENT );
        render::PrimitiveBatch Batch( Material );

        for( i = 0; i < m_Mesh.m_nFacets; i++ )
        {
            if( m_Mesh.m_pFacet[i].iMaterial == iMaterial )
            {
                render::primitive_vertex Triangle[3];
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

                    Triangle[j] = render::primitive_vertex( pVertex->Position,
                                                            pVertex->UV[0],
                                                            VertColor );
                }
                VERIFY( Batch.AddTriangle( Triangle[0], Triangle[1], Triangle[2] ) );
            }
        }
        VERIFY( Batch.Submit( LocalToWorld ) );
    }
}

//=========================================================================

void mesh_viewer::RenderSoftSkin( const matrix4& LocalToWorld )
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
        pMatrix[i] = LocalToWorld * pMatrix[i];
    }

    // Render triangles grouped by material so every submission owns all state.
    for( s32 iMaterial = 0; iMaterial < m_Mesh.m_nMaterials; ++iMaterial )
    {
        const s32 iTexture = m_Mesh.m_pMaterial[iMaterial].TexMaterial[0].iTexture;
        const texture* pTexture = NULL;
        if( (iTexture >= 0) && (iTexture < 32) && m_Texture[iTexture].GetShaderResource() )
            pTexture = &m_Texture[iTexture];

        const render::primitive_draw_desc Material(
            pTexture,
            render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            render::PRIMITIVE_BLEND_ALPHA,
            render::PRIMITIVE_DEPTH_READ_ONLY,
            m_bBackFacets ? render::PRIMITIVE_RASTER_SOLID_NO_CULL
                          : render::PRIMITIVE_RASTER_SOLID,
            render::PRIMITIVE_SAMPLER_LINEAR_WRAP,
            render::PRIMITIVE_LAYER_TRANSPARENT );
        render::PrimitiveBatch Batch( Material );

        for( i=0; i<m_Mesh.m_nFacets; i++ )
        {
            const rawmesh::facet& Facet = m_Mesh.m_pFacet[i];
            if( Facet.iMaterial != iMaterial )
                continue;

            lovert V[3];
            vector3 N[3];
            for( s32 j=0; j<3; j++ )
            {
                const rawmesh::vertex& Vert = m_Mesh.m_pVertex[ Facet.iVertex[j] ];
                V[j].P.Zero();
                N[j].Zero();
                
                for( s32 w=0; w<Vert.nWeights; w++ )
                {
                    const rawmesh::weight& W = Vert.Weight[w];
                    V[j].P += (pMatrix[ W.iBone ] * Vert.Position) * W.Weight;
                    N[j]   += pMatrix[ W.iBone ].RotateVector( Vert.Normal[0] ) * W.Weight;
                }

                N[j].Normalize();
                V[j].UV = Vert.UV[0];
                f32 I = fMax( 0, LightDir.Dot( N[j] ) );
                I = fMin( 1, I );
                V[j].C.SetfRGBA( fMin( 1, m_Ambient.GetX() + I),
                                 fMin( 1, m_Ambient.GetY() + I),
                                 fMin( 1, m_Ambient.GetZ() + I), 1 );
            }

            const render::primitive_vertex Triangle[3] =
            {
                render::primitive_vertex( V[0].P, V[0].UV, V[0].C ),
                render::primitive_vertex( V[1].P, V[1].UV, V[1].C ),
                render::primitive_vertex( V[2].P, V[2].UV, V[2].C )
            };
            VERIFY( Batch.AddTriangle( Triangle[0], Triangle[1], Triangle[2] ) );
        }
        matrix4 Identity;
        Identity.Identity();
        VERIFY( Batch.Submit( Identity ) );
    }

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
