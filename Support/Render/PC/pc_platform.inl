//=============================================================================
//
//  PC Specific Render Routines
//
//=============================================================================

#include "..\LightMgr.hpp"
#include "..\platform_Render.hpp"
#include "..\ProjTextureMgr.hpp"
#include "VertexMgr.hpp"
#include "SoftVertexMgr.hpp"
#include "PostMgr/PostMgr.hpp"
#include "GBufferMgr.hpp"
#include "Entropy/D3DEngine/d3deng_rtarget.hpp"

//=============================================================================
// Static data specific to the pc-implementation
//=============================================================================

// KSS, remove me!
static const material*         s_pMaterial   = NULL;
static rigid_geom*             s_pRigidGeom  = NULL;
static skin_geom*              s_pSkinGeom   = NULL;
static const xbitmap*          s_pDrawBitmap = NULL;
static s32                     s_BlendMode   = 0;

//=============================================================================
// Implementation
//=============================================================================

static
void platform_Init( void )
{
    g_GBufferMgr.Init();
    g_MaterialMgr.Init();
    g_PostMgr.Init(); 
    g_RigidVertMgr.Init( sizeof( rigid_geom::vertex_pc ) );
    g_SkinVertMgr.Init();
}

//=============================================================================

static
void platform_Kill( void )
{
    g_SkinVertMgr.Kill(); 
    g_RigidVertMgr.Kill();
    g_PostMgr.Kill();
    g_MaterialMgr.Kill();
    g_GBufferMgr.Kill();  
}

//=============================================================================

static
void platform_BeginSession( u32 nPlayers )
{
    (void)nPlayers;
    // TODO:
}

//=============================================================================

static
void platform_EndSession( void )
{
    // TODO:
}

//=============================================================================

static
void platform_ActivateMaterial( const material& Material )
{
    if( !g_pd3dDevice )
        return;

    x_try;

    s_pMaterial = &Material;

    // Get diffuse texture
    texture* pDiffuse = Material.m_DiffuseMap.GetPointer();
    const xbitmap* pDiffuseMap = pDiffuse ? &pDiffuse->m_Bitmap : NULL;
    
    // Get detail texture
    texture* pDetail = Material.m_DetailMap.GetPointer();
    const xbitmap* pDetailMap = pDetail ? &pDetail->m_Bitmap : NULL;
    
    // Get env texture
    texture* pEnvironment = Material.m_EnvironmentMap.GetPointer();
    const xbitmap* pEnvironmentMap = pEnvironment ? &pEnvironment->m_Bitmap : NULL;

    // TODO: GS: Make proj tex

    // Set primary textures through MaterialMgr
    g_MaterialMgr.SetBitmap( pDiffuseMap, TEXTURE_SLOT_DIFFUSE );
    g_MaterialMgr.SetBitmap( pDetailMap, TEXTURE_SLOT_DETAIL  );
    g_MaterialMgr.SetBitmap( pEnvironmentMap, TEXTURE_SLOT_ENVIRONMENT );

    x_catch_display;
}

//=============================================================================

static
void platform_ActivateDistortionMaterial( const material* pMaterial, const radian3& NormalRot )
{
    (void)pMaterial;
    (void)NormalRot;
    // TODO:
}

//=============================================================================

static
void platform_ActivateZPrimeMaterial( void )
{
    // TODO:
}

//=============================================================================

static
void platform_BeginRigidGeom( geom* pGeom, s32 iSubMesh )
{
    (void)iSubMesh;
    ASSERT( s_pRigidGeom == NULL );
    s_pRigidGeom = (rigid_geom*)pGeom;
    g_RigidVertMgr.BeginRender();
    g_MaterialMgr.InvalidateCache();
}

//=============================================================================

static
void platform_EndRigidGeom( void )
{
    ASSERT( s_pRigidGeom );
    s_pRigidGeom = NULL;
}

//=============================================================================

static
void platform_BeginSkinGeom( geom* pGeom, s32 iSubMesh )
{
    (void)iSubMesh;
    ASSERT( s_pSkinGeom == NULL );
    s_pSkinGeom = (skin_geom*)pGeom;
    g_SkinVertMgr.BeginRender();
    g_MaterialMgr.InvalidateCache();
}

//=============================================================================

static
void platform_EndSkinGeom( void )
{
    ASSERT( s_pSkinGeom );
    s_pSkinGeom = NULL;
}

//=============================================================================

static
void platform_RenderRigidInstance( render_instance& Inst )
{
    if( !g_pd3dDevice )
        return;

    g_MaterialMgr.SetRigidMaterial( Inst.Data.Rigid.pL2W,
                                    &Inst.Data.Rigid.pGeom->m_BBox,
                                    (d3d_lighting*)Inst.pLighting,
                                    s_pMaterial,
                                    Inst.Flags );

    // TODO: GS: At the moment we use a lightmap every call to platform_RenderRigidInstance. 
    // In theory, this is not the best solution, should definitely come up with something else

    if( Inst.Data.Rigid.pColInfo )
    {
        // Get the geometry to determine vertex count and submesh info
        rigid_geom* pGeom = Inst.Data.Rigid.pGeom;
        if( pGeom )
        {
            // Get submesh info  
            geom::submesh& SubMesh = pGeom->m_pSubMesh[Inst.SortKey.GeomSubMesh];
            rigid_geom::dlist_pc& DList = pGeom->m_System.pPC[SubMesh.iDList];
            
            // Apply lightmap colors
            g_RigidVertMgr.ApplyLightmapColors( Inst.hDList,
                                                (const u32*)Inst.Data.Rigid.pColInfo,
                                                DList.nVerts,
                                                DList.iColor );
        }
    }

    g_RigidVertMgr.DrawDList( Inst.hDList, Inst.Data.Rigid.pL2W, NULL );

    // Deprecated since we already got and push render flags for material mgr.

    //if( !(Inst.Flags & render::PULSED) )
    //{
    //    g_RigidVertMgr.DrawDList( Inst.hDList, Inst.Data.Rigid.pL2W, NULL );
    //}
    //
    //if( Inst.Flags & render::PULSED )
    //{
    //    s32 I = (s32)( 128.0f + (80.0f * x_sin( x_fmod( s_PulseTime * 4, PI*2.0f ) )) );
    //
    //    g_RigidVertMgr.DrawDList( Inst.hDList, Inst.Data.Rigid.pL2W, NULL );
    //}
    //
    //if( Inst.Flags & render::WIREFRAME )
    //{
    //    g_RigidVertMgr.DrawDList( Inst.hDList, Inst.Data.Rigid.pL2W, NULL );
    //}
    //
    //if( Inst.Flags & render::WIREFRAME2 )
    //{
    //    g_RigidVertMgr.DrawDList( Inst.hDList, Inst.Data.Rigid.pL2W, NULL );
    //}
    
    g_MaterialMgr.ResetProjTextures();    
}

//=============================================================================

static
void platform_RenderSkinInstance( render_instance& Inst )
{
    if( !g_pd3dDevice )
        return;

    g_MaterialMgr.SetSkinMaterial( &Inst.Data.Skin.pBones[0],
                                   &Inst.Data.Skin.pGeom->m_BBox,
                                   (d3d_lighting*)Inst.pLighting,
                                   s_pMaterial,
                                   Inst.Flags );

    g_SkinVertMgr.DrawDList( Inst.hDList, Inst.Data.Skin.pBones, (d3d_lighting*)Inst.pLighting );

    // Deprecated since we already got and push render flags for material mgr.

    //if( Inst.Flags & render::FADING_ALPHA )
    //{
    //    // TODO: Render transparent geometry        
    //    g_SkinVertMgr.DrawDList( Inst.hDList, Inst.Data.Skin.pBones, (d3d_lighting*)Inst.pLighting );
    //}
    //else
    //{
    //    // Normal render
    //    g_SkinVertMgr.DrawDList( Inst.hDList, Inst.Data.Skin.pBones, (d3d_lighting*)Inst.pLighting );
    //}
    
    g_MaterialMgr.ResetProjTextures();
}

//=============================================================================

static
void platform_RegisterMaterial( material& Mat )
{
    (void)Mat;
    // TODO:
}

//=============================================================================

static
void platform_RegisterRigidGeom( rigid_geom& Geom )
{
    (void)Geom;
    // TODO:
}

//=============================================================================

static
void platform_UnregisterRigidGeom( rigid_geom& Geom )
{
    (void)Geom;
    // TODO:
}

//=============================================================================

static
void platform_RegisterSkinGeom( skin_geom& Geom )
{
    private_geom& PrivateGeom = s_lRegisteredGeoms(Geom.m_hGeom);
    PrivateGeom.SkinDList.Clear();

    // make a private copy of the display lists and register them with the
    // skin vert manager
    skin_geom::dlist_pc* pPCDList = Geom.m_System.pPC;

    // Allocate work memory and setup pointers for the copy loop
    s32 nVerts = Geom.GetNVerts();
    skin_geom::vertex_pc* pBuffer = new skin_geom::vertex_pc[nVerts];
    skin_geom::vertex_pc* pVertex = pBuffer;
    skin_geom::vertex_pc* pEnd    = (pVertex + nVerts);

    // Loop through all SubMeshs
    for ( s32 iSubMesh = 0; iSubMesh < Geom.m_nSubMeshes; iSubMesh++ )
    {
        // Get the DList for this submesh
        geom::submesh&       GeomSubMesh = Geom.m_pSubMesh[iSubMesh];
        skin_geom::dlist_pc& DList       = pPCDList[GeomSubMesh.iDList];

        // Copy vertex data
        for ( s32 iVert = 0; iVert < DList.nVertices; iVert++ )
        {
            // Copy vert
            pVertex[iVert].Position  = DList.pVertex[iVert].Position;
            pVertex[iVert].Normal    = DList.pVertex[iVert].Normal;
            pVertex[iVert].UVWeights = DList.pVertex[iVert].UVWeights;
        }

        // Create a new handle
        xhandle& hDList = PrivateGeom.SkinDList.Append();

        // Create the dlist and store out the handle
        hDList = g_SkinVertMgr.AddDList( pVertex,
                                         DList.nVertices,
                                         (u16*)DList.pIndex,
                                         DList.nIndices,
                                         DList.nIndices / 3,
                                         DList.nCommands,
                                         DList.pCmd );

        // Advance ptrs
        pVertex += DList.nVertices;
    }

    // Free the work memory
    delete []pBuffer;
    ASSERT( pVertex == pEnd );
}

//=============================================================================

static
void platform_UnregisterSkinGeom( skin_geom& Geom )
{
    private_geom& PrivateGeom = s_lRegisteredGeoms(Geom.m_hGeom);
    for ( s32 i = 0; i < PrivateGeom.SkinDList.GetCount(); i++ )
    {
        g_SkinVertMgr.DelDList( PrivateGeom.SkinDList[i] );
    }
    PrivateGeom.SkinDList.Clear();
}

//=============================================================================

static
void platform_StartRawDataMode( void )
{
    // TODO:
}

//=============================================================================

static
void platform_EndRawDataMode( void )
{
}

//=============================================================================

static
void platform_RenderRawStrips( s32               nVerts,
                               const matrix4&    L2W,
                               const vector4*    pPos,
                               const s16*        pUV,
                               const u32*        pColor )
{
    static const f32 ItoFScale = 1.0f/4096.0f;

    // sanity check
    ASSERTS( s_pDrawBitmap, "You must set a material first!" );
    if( nVerts < 3 )
        return;

    const rtarget* pGBufferDepth = g_GBufferMgr.GetGBufferTarget( GBUFFER_DEPTH );
    const rtarget* pBackBuffer = rtarget_GetBackBuffer();
    
    if( pGBufferDepth && pBackBuffer )
    {
        // Make sure we're using the same depth target as geometry
        rtarget_SetTargets( pBackBuffer, 1, pGBufferDepth );
    }

    // fill in the l2w...note we have to reset draw to do this
    // NOTE: DRAW_NO_ZWRITE because we don't need spoil the depth buffer
    draw_Begin( DRAW_TRIANGLES, DRAW_TEXTURED | DRAW_CULL_NONE | DRAW_NO_ZWRITE );
    draw_SetTexture( *s_pDrawBitmap );
    draw_SetL2W( L2W );

    // GS: Probably hack ? 
    // Currently, render states need to be placed AFTER draw_Begin, 
    // otherwise they will be overwritten by internal draw_Begin states. 
    // Maybe it makes sense to add a flag for custom render states ?
    
    // Setup render states
    
    g_MaterialMgr.SetBlendMode( s_BlendMode );

    // prime the loop by grabbing the data for the first two verts
    xcolor  C0( pColor[0]&0xff, (pColor[0]&0xff00)>>8, (pColor[0]&0xff0000)>>16, (pColor[0]&0xff000000)>>24 );
    xcolor  C1( pColor[1]&0xff, (pColor[1]&0xff00)>>8, (pColor[1]&0xff0000)>>16, (pColor[1]&0xff000000)>>24 );
    vector2 UV0( pUV[0] * ItoFScale, pUV[1] * ItoFScale );
    vector2 UV1( pUV[2] * ItoFScale, pUV[3] * ItoFScale );
    vector3 Pos0( pPos[0].GetX(), pPos[0].GetY(), pPos[0].GetZ() );
    vector3 Pos1( pPos[1].GetX(), pPos[1].GetY(), pPos[1].GetZ() );

    // Intensity decals ignore vertex color and always render white
    xbool   bIntensity = ( s_BlendMode == render::BLEND_MODE_INTENSITY );
    xcolor  White( 255, 255, 255, 255 );

    // Now render the raw strips. Having 0x8000 in the w component means don't
    // kick this triangle. (Just like the ADC bit on the PS2.)
    s32 i;
    for( i = 2; i < nVerts; i++ )
    {
        // grab the next vert
        xcolor  C2( pColor[i]&0xff, (pColor[i]&0xff00)>>8, (pColor[i]&0xff0000)>>16, (pColor[i]&0xff000000)>>24 );
        vector2 UV2( pUV[i*2+0] * ItoFScale, pUV[i*2+1] * ItoFScale );
        vector3 Pos2( pPos[i].GetX(), pPos[i].GetY(), pPos[i].GetZ() );

        // kick the triangle
        if( (pPos[i].GetIW() & 0x8000) != 0x8000 )
        {
            if( bIntensity )
            {
                draw_Color( White ); draw_UV( UV0 ); draw_Vertex( Pos0 );
                draw_Color( White ); draw_UV( UV1 ); draw_Vertex( Pos1 );
                draw_Color( White ); draw_UV( UV2 ); draw_Vertex( Pos2 );
            }
            else
            {
                draw_Color( C0 );   draw_UV( UV0 ); draw_Vertex( Pos0 );
                draw_Color( C1 );   draw_UV( UV1 ); draw_Vertex( Pos1 );
                draw_Color( C2 );   draw_UV( UV2 ); draw_Vertex( Pos2 );
            }
        }

        // move to the next triangle
        C0 = C1;    UV0 = UV1;  Pos0 = Pos1;
        C1 = C2;    UV1 = UV2;  Pos1 = Pos2;
    }

    // finished
    draw_End();
}

//=============================================================================

static
void platform_Render3dSprites( s32               nSprites,
                               f32               UniScale,
                               const matrix4*    pL2W,
                               const vector4*    pPositions,
                               const vector2*    pRotScales,
                               const u32*        pColors )
{
    // sanity check
    ASSERTS( s_pDrawBitmap, "You must set a material first!" );
    if( nSprites == 0 )
        return;
    
    const rtarget* pGBufferDepth = g_GBufferMgr.GetGBufferTarget( GBUFFER_DEPTH );
    const rtarget* pBackBuffer = rtarget_GetBackBuffer();
    
    if( pGBufferDepth && pBackBuffer )
    {
        // Make sure we're using the same depth target as geometry
        rtarget_SetTargets( pBackBuffer, 1, pGBufferDepth );
    }

    // start up draw
    const matrix4& V2W = eng_GetView()->GetV2W();
    const matrix4& W2V = eng_GetView()->GetW2V();
    matrix4 S2V;
    if( pL2W )
        S2V = W2V * (*pL2W);
    else
        S2V = W2V;
    draw_ClearL2W();
    
    // NOTE: DRAW_NO_ZWRITE because we don't need spoil the depth buffer
    draw_Begin( DRAW_TRIANGLES, DRAW_TEXTURED | DRAW_USE_ALPHA | DRAW_CULL_NONE | DRAW_NO_ZWRITE );
    draw_SetTexture( *s_pDrawBitmap );
    draw_SetL2W( V2W );

    g_MaterialMgr.SetBlendMode( s_BlendMode );

    // loop through the sprites and render them
    s32 i, j;
    for( i = 0; i < nSprites; i++ )
    {
        // 0x8000 is an active flag, meaning to skip this sprite, similar
        // to the ADC bit on the ps2.
        if( (pPositions[i].GetIW() & 0x8000) != 0x8000 )
        {
            vector3 Center( pPositions[i].GetX(), pPositions[i].GetY(), pPositions[i].GetZ() );
            Center = S2V * Center;

            // calc the four sprite corners
            vector3 Corners[4];
            f32 Sine, Cosine;
            x_sincos( -pRotScales[i].X, Sine, Cosine );

            vector3 v0( Cosine - Sine, Sine + Cosine, 0.0f );
            vector3 v1( Cosine + Sine, Sine - Cosine, 0.0f );
            Corners[0] = v0;
            Corners[1] = v1;
            Corners[2] = -v0;
            Corners[3] = -v1;
            
            for( j = 0; j < 4; j++ )
            {
                Corners[j].Scale( pRotScales[i].Y * UniScale );
                Corners[j] += Center;
            }

            // now render it through draw
            xcolor Color( pColors[i] & 0xff,
                        ( pColors[i] & 0xff00) >> 8,
                        ( pColors[i] & 0xff0000) >> 16,
                        ( pColors[i] & 0xff000000) >> 24 );
            
            draw_Color( Color );
            draw_UV( 0.0f, 0.0f );  draw_Vertex( Corners[0] );
            draw_UV( 1.0f, 0.0f );  draw_Vertex( Corners[3] );
            draw_UV( 0.0f, 1.0f );  draw_Vertex( Corners[1] );
            
            draw_UV( 1.0f, 0.0f );  draw_Vertex( Corners[3] );
            draw_UV( 0.0f, 1.0f );  draw_Vertex( Corners[1] );
            draw_UV( 1.0f, 1.0f );  draw_Vertex( Corners[2] );
        }
    }

    // finished
    draw_End();
}

//=============================================================================

static
void platform_RenderHeatHazeSprites( s32 nSprites, f32 UniScale, const matrix4* pL2W, const vector4* pPositions, const vector2* pRotScales, const u32* pColors )
{
    (void)nSprites;
    (void)UniScale;
    (void)pL2W;
    (void)pPositions;
    (void)pRotScales;
    (void)pColors;
/*    
    ASSERTS( s_pDrawBitmap, "You must set a material first!" );
    if( (nSprites == 0) || !g_pd3dDevice )
        return;

    const rtarget* pGBufferDepth = g_GBufferMgr.GetGBufferTarget( GBUFFER_DEPTH );
    const rtarget* pBackBuffer   = rtarget_GetBackBuffer();

    if( pGBufferDepth && pBackBuffer )
    {
        rtarget_SetTargets( pBackBuffer, 1, pGBufferDepth );
    }

    const view* pView = eng_GetView();
    if( !pView )
        return;

    const matrix4& V2W = pView->GetV2W();
    const matrix4& W2V = pView->GetW2V();
    matrix4 S2V;

    if( pL2W )
        S2V = W2V * (*pL2W);
    else
        S2V = W2V;

    draw_ClearL2W();
    draw_Begin( DRAW_TRIANGLES, DRAW_TEXTURED | DRAW_USE_ALPHA | DRAW_CULL_NONE | DRAW_NO_ZWRITE );
    draw_SetTexture( *s_pDrawBitmap );
    draw_SetL2W( V2W );

    g_MaterialMgr.SetBlendMode( s_BlendMode );

    for( s32 i = 0; i < nSprites; ++i )
    {
        if( (pPositions[i].GetIW() & 0x8000) == 0x8000 )
            continue;

        vector3 Center( pPositions[i].GetX(), pPositions[i].GetY(), pPositions[i].GetZ() );
        Center = S2V * Center;

        f32 Sine, Cosine;
        x_sincos( -pRotScales[i].X, Sine, Cosine );

        vector3 Corners[4];
        vector3 v0( Cosine - Sine, Sine + Cosine, 0.0f );
        vector3 v1( Cosine + Sine, Sine - Cosine, 0.0f );
        Corners[0] = v0;
        Corners[1] = v1;
        Corners[2] = -v0;
        Corners[3] = -v1;

        for( s32 j = 0; j < 4; ++j )
        {
            Corners[j].Scale( pRotScales[i].Y * UniScale );
            Corners[j] += Center;
        }

        xcolor Color( pColors[i] & 0xff,
                    ( pColors[i] & 0xff00) >> 8,
                    ( pColors[i] & 0xff0000) >> 16,
                    ( pColors[i] & 0xff000000) >> 24 );

        draw_Color( Color );
        draw_UV( 0.0f, 0.0f ); draw_Vertex( Corners[0] );
        draw_UV( 1.0f, 0.0f ); draw_Vertex( Corners[3] );
        draw_UV( 0.0f, 1.0f ); draw_Vertex( Corners[1] );

        draw_UV( 1.0f, 0.0f ); draw_Vertex( Corners[3] );
        draw_UV( 0.0f, 1.0f ); draw_Vertex( Corners[1] );
        draw_UV( 1.0f, 1.0f ); draw_Vertex( Corners[2] );
    }

    draw_End();
*/    
}

//=============================================================================

static
void platform_RenderVelocitySprites( s32            nSprites,
                                     f32            UniScale,
                                     const matrix4* pL2W,
                                     const matrix4* pVelMatrix,
                                     const vector4* pPositions,
                                     const vector4* pVelocities,
                                     const u32*     pColors )
{
    // sanity check
    ASSERTS( s_pDrawBitmap, "You must set a material first!" );
    if( nSprites == 0 )
        return;

    const rtarget* pGBufferDepth = g_GBufferMgr.GetGBufferTarget( GBUFFER_DEPTH );
    const rtarget* pBackBuffer = rtarget_GetBackBuffer();
    
    if( pGBufferDepth && pBackBuffer )
    {
        // Make sure we're using the same depth target as geometry
        rtarget_SetTargets( pBackBuffer, 1, pGBufferDepth );
    }

    // start up draw
    draw_ClearL2W();
    // NOTE: DRAW_NO_ZWRITE because we don't need spoil the depth buffer
    draw_Begin( DRAW_TRIANGLES, DRAW_TEXTURED | DRAW_USE_ALPHA | DRAW_CULL_NONE | DRAW_NO_ZWRITE );
    draw_SetTexture( *s_pDrawBitmap );

    g_MaterialMgr.SetBlendMode( s_BlendMode );

    // Grab out a l2w matrix to use. If one is not specified, then
    // we will use the identity matrix.
    matrix4 L2W;
    if( pL2W )
        L2W = *pL2W;
    else
        L2W.Identity();

    // calculate the velocity l2w matrix
    matrix4 L2WNoTranslate = L2W;
    L2WNoTranslate.ClearTranslation();
    matrix4 VL2W = L2WNoTranslate * (*pVelMatrix);

    // grab out the view direction
    vector3 ViewDir = eng_GetView()->GetViewZ();

    // render the sprites
    s32 i;
    for( i = 0; i < nSprites; i++ )
    {
        // 0x8000 is an active flag, meaning to skip this sprite, similar
        // to the ADC bit on the ps2.
        if( (pPositions[i].GetIW() & 0x8000) != 0x8000 )
        {
            // calculate the sprite points
            vector3 P = L2W * vector3( pPositions[i].GetX(), pPositions[i].GetY(), pPositions[i].GetZ() );

            vector3 Right( pVelocities[i].GetX(), pVelocities[i].GetY(), pVelocities[i].GetZ() );
            Right = VL2W * Right;
            Right.Normalize();
            vector3 Up   = ViewDir.Cross( Right );
            Right *= pVelocities[i].GetW()*UniScale;
            Up    *= pVelocities[i].GetW()*UniScale;
            vector3 Fore = P + Right;
            vector3 Aft  = P - Right;
            vector3 V0   = Fore - Up;
            vector3 V1   = Aft  - Up;
            vector3 V2   = Aft  + Up;
            vector3 V3   = Fore + Up;

            // now render it through draw
            xcolor Color( pColors[i] & 0xff,
                        ( pColors[i] & 0xff00) >> 8,
                        ( pColors[i] & 0xff0000) >> 16,
                        ( pColors[i] & 0xff000000) >> 24 );
            
            draw_Color( Color );
            draw_UV( 1.0f, 0.0f );  draw_Vertex( V0 );
            draw_UV( 0.0f, 0.0f );  draw_Vertex( V1 );
            draw_UV( 1.0f, 1.0f );  draw_Vertex( V3 );
            
            draw_UV( 0.0f, 0.0f );  draw_Vertex( V1 );
            draw_UV( 1.0f, 1.0f );  draw_Vertex( V3 );
            draw_UV( 0.0f, 1.0f );  draw_Vertex( V2 );
        }
    }

    // finished
    draw_End();
}

//=============================================================================

static
void platform_SetDiffuseMaterial( const xbitmap& Bitmap, s32 BlendMode, xbool ZTestEnabled )
{
    g_MaterialMgr.SetBitmap( &Bitmap, TEXTURE_SLOT_DIFFUSE );
    g_MaterialMgr.SetBlendMode( BlendMode );
    g_MaterialMgr.SetDepthTestEnabled( ZTestEnabled );
    
    s_pDrawBitmap = &Bitmap;
    s_BlendMode = BlendMode;
}

//=============================================================================

static
void platform_SetGlowMaterial( const xbitmap& Bitmap, s32 BlendMode, xbool ZTestEnabled )
{
    g_MaterialMgr.SetBitmap( &Bitmap, TEXTURE_SLOT_DIFFUSE );
    g_MaterialMgr.SetBlendMode( BlendMode );
    g_MaterialMgr.SetDepthTestEnabled( ZTestEnabled );
    
    s_pDrawBitmap = &Bitmap;
    s_BlendMode = BlendMode;    
}

//=============================================================================

static
void platform_SetEnvMapMaterial( const xbitmap& Bitmap, s32 BlendMode, xbool ZTestEnabled )
{
    g_MaterialMgr.SetBitmap( &Bitmap, TEXTURE_SLOT_DIFFUSE );
    g_MaterialMgr.SetBlendMode( BlendMode );
    g_MaterialMgr.SetDepthTestEnabled( ZTestEnabled );
    
    s_pDrawBitmap = &Bitmap;
    s_BlendMode = BlendMode;    
}

//=============================================================================

static
void platform_SetDistortionMaterial( s32 BlendMode, xbool ZTestEnabled )
{
    g_MaterialMgr.SetBlendMode( BlendMode );
    g_MaterialMgr.SetDepthTestEnabled( ZTestEnabled );    
}

//=============================================================================

static
void* platform_CalculateRigidLighting( const matrix4&   L2W,
                                       const bbox&      WorldBBox )
{
    d3d_lighting* pLighting = (d3d_lighting*)smem_BufferAlloc( sizeof(d3d_lighting) );
    if( !pLighting )
    {
        static d3d_lighting Default;
        Default.LightCount = 0;
        Default.AmbCol.Set( 0.05f, 0.05f, 0.05f, 1.0f );
        for( s32 i = 0; i < MAX_GEOM_LIGHTS; i++ )
        {
            Default.LightVec[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
            Default.LightCol[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
        }
        pLighting = &Default;
    }
    else
    {
        pLighting->AmbCol.Set( 0.05f, 0.05f, 0.05f, 1.0f );
        s32 NLights = g_LightMgr.CollectLights( WorldBBox, MAX_GEOM_LIGHTS );
        pLighting->LightCount = NLights;

        for( s32 i = 0; i < NLights; i++ )
        {
            vector3 Pos;
            f32     Radius;
            xcolor  Col;
            g_LightMgr.GetCollectedLight( i, Pos, Radius, Col );

            pLighting->LightVec[i].Set( Pos.GetX(), Pos.GetY(), Pos.GetZ(), Radius );
            pLighting->LightCol[i].Set( (f32)Col.R * (1.0f / 255.0f),
                                        (f32)Col.G * (1.0f / 255.0f),
                                        (f32)Col.B * (1.0f / 255.0f),
                                        1.0f );
        }

        for( s32 i = NLights; i < MAX_GEOM_LIGHTS; i++ )
        {
            pLighting->LightVec[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
            pLighting->LightCol[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
        }
    }

    return pLighting;
}

//=============================================================================

static
void* platform_CalculateSkinLighting( u32            Flags,
                                      const matrix4& L2W,
                                      const bbox&    BBox,
                                      xcolor         Ambient )
{
    (void)Flags;

    CONTEXT("platform_CalculateSkinLighting") ;

    // Try allocate
    d3d_lighting* pLighting = (d3d_lighting*)smem_BufferAlloc( sizeof(d3d_lighting) );
    if (!pLighting)
    {
        // Setup default lighting
        static d3d_lighting Default ;
        Default.LightCount = 1;
        Default.LightVec[0].Set(0.0f, 1.0f, 0.0f, 0.0f);
        Default.LightCol[0].Set(1.0f, 1.0f, 1.0f, 1.0f);
        Default.AmbCol.Set(0.3f, 0.3f, 0.3f, 1.0f);
        for( s32 i = 1; i < MAX_GEOM_LIGHTS; i++ )
        {
            Default.LightVec[i].Set(0.0f, 0.0f, 0.0f, 0.0f);
            Default.LightCol[i].Set(0.0f, 0.0f, 0.0f, 0.0f);
        }

        // Use it
        pLighting = &Default ;
    }
    else
    {
        // Setup ambient
        pLighting->AmbCol.Set((f32)Ambient.R * (1.0f / 255.0f),
                              (f32)Ambient.G * (1.0f / 255.0f),
                              (f32)Ambient.B * (1.0f / 255.0f),
                              (f32)Ambient.A * (1.0f / 255.0f) ) ;

        // Grab lights
        s32 NLights = g_LightMgr.CollectCharLights( L2W, BBox, MAX_GEOM_LIGHTS );
        pLighting->LightCount = NLights;
        
        for( s32 i = 0; i < NLights; i++ )
        {
            vector3 Dir;
            xcolor  Col;
            g_LightMgr.GetCollectedCharLight( i, Dir, Col );

            // Setup skin lights
            pLighting->LightVec[i].Set( Dir.GetX(), Dir.GetY(), Dir.GetZ(), 0.0f );
            pLighting->LightCol[i].Set((f32)Col.R * (1.0f / 255.0f),
                                       (f32)Col.G * (1.0f / 255.0f),
                                       (f32)Col.B * (1.0f / 255.0f),
                                       (f32)Col.A * (1.0f / 255.0f) );
        }
        
        for( s32 i = NLights; i < MAX_GEOM_LIGHTS; i++ )
        {
            // Turn off directional lighting
            pLighting->LightVec[i].Set(0.0f, 0.0f, 0.0f, 0.0f);
            pLighting->LightCol[i].Set(0.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    // Store in render instance
    ASSERT(pLighting) ;
    return pLighting;
}

//=============================================================================

static
void* platform_LockRigidDListVertex( render::hgeom_inst hInst, s32 iSubMesh ) //EDITOR SHIT, IGNORE!!!
{
    (void)hInst;
    (void)iSubMesh;
    // TODO:
    return NULL;
}

//=============================================================================

static
void platform_UnlockRigidDListVertex( render::hgeom_inst hInst, s32 iSubMesh ) //EDITOR SHIT, IGNORE!!!
{
    (void)hInst;
    (void)iSubMesh;
    // TODO:
}

//=============================================================================

static
void* platform_LockRigidDListIndex( render::hgeom_inst hInst, s32 iSubMesh, s32& VertexOffset ) //EDITOR SHIT, IGNORE!!!
{
    (void)hInst;
    (void)iSubMesh;
    VertexOffset = 0;
    // TODO:
    return NULL;
}

//=============================================================================

static
void platform_UnlockRigidDListIndex( render::hgeom_inst hInst, s32 iSubMesh ) //EDITOR SHIT, IGNORE!!!
{
    (void)hInst;
    (void)iSubMesh;
    // TODO:
}

//=============================================================================

static
void platform_BeginShaders( void )
{
    // TODO:
}

//=============================================================================

static
void platform_EndShaders( void )
{
    // TODO:
}

//=============================================================================

static
void platform_CreateEnvTexture( void )
{
    // TODO:
}

//=============================================================================
// DEPRECATED - START
//=============================================================================

static
void platform_SetProjectedTexture( texture::handle Texture )
{
    // DEAD
}

//=============================================================================

static
void platform_ComputeProjTextureMatrix( matrix4& Matrix, view& View, const texture_projection& Projection )
{
   // DEAD
}

//=============================================================================

static
void platform_SetTextureProjection( const texture_projection& Projection )
{
    // DEAD
}

//=============================================================================

static
void platform_SetTextureProjectionMatrix( const matrix4& Matrix )
{
    // DEAD
}

//=============================================================================

static
void platform_SetProjectedShadowTexture( s32 Index, texture::handle Texture )
{
    // DEAD
}

//=============================================================================

static
void platform_ComputeProjShadowMatrix( matrix4& Matrix, view& View, const texture_projection& Projection  )
{
    // DEAD
}

//=============================================================================

static
void platform_SetShadowProjectionMatrix( s32 Index, const matrix4& Matrix )
{
    // DEAD
}

//=============================================================================
// DEPRECATED - END
//=============================================================================

static
void platform_SetCustomFogPalette( const texture::handle& Texture, xbool ImmediateSwitch, s32 PaletteIndex )
{
    (void)Texture;
    (void)ImmediateSwitch;
    (void)PaletteIndex;
}

//=============================================================================

static
xcolor platform_GetFogValue( const vector3& WorldPos, s32 PaletteIndex )
{
    return g_PostMgr.GetFogValue( WorldPos, PaletteIndex );
}

//=============================================================================

static
void platform_BeginPostEffects( void )
{
    g_PostMgr.BeginPostEffects();
}

//=============================================================================

static
void platform_AddScreenWarp( const vector3& WorldPos, f32 Radius, f32 WarpAmount )
{
    (void)WorldPos;
    (void)Radius;
    (void)WarpAmount;
}

//=============================================================================

static
void platform_ApplySelfIllumGlows( f32 MotionBlurIntensity, s32 GlowCutoff )
{
    g_PostMgr.ApplySelfIllumGlows( MotionBlurIntensity, GlowCutoff );
}

//=============================================================================

static
void platform_MotionBlur( f32 Intensity )
{
    g_PostMgr.MotionBlur( Intensity );
}

//=============================================================================

static
void platform_ZFogFilter( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    g_PostMgr.ZFogFilter( Fn, Color, Param1, Param2 );
}

//=============================================================================

static
void platform_ZFogFilter( render::post_falloff_fn Fn, s32 PaletteIndex )
{
    g_PostMgr.ZFogFilter( Fn, PaletteIndex );
}

//=============================================================================

static
void platform_MipFilter( s32                        nFilters,
                         f32                        Offset,
                         render::post_falloff_fn    Fn,
                         xcolor                     Color,
                         f32                        Param1,
                         f32                        Param2,
                         s32                        PaletteIndex )
{
    g_PostMgr.MipFilter( nFilters, Offset, Fn, Color, Param1, Param2, PaletteIndex );
}

//=============================================================================

static
void platform_MipFilter( s32                        nFilters,
                         f32                        Offset,
                         render::post_falloff_fn    Fn,
                         const texture::handle&     Texture,
                         s32                        PaletteIndex )
{
    g_PostMgr.MipFilter( nFilters, Offset, Fn, Texture, PaletteIndex );
}

//=============================================================================

static
void platform_NoiseFilter( xcolor Color )
{
    g_PostMgr.NoiseFilter( Color );
}

//=============================================================================

static
void platform_ScreenFade( xcolor Color )
{
    g_PostMgr.ScreenFade( Color );
}

//=============================================================================

static
void platform_MultScreen( xcolor MultColor, render::post_screen_blend FinalBlend )
{
    g_PostMgr.MultScreen( MultColor, FinalBlend );
}

//=============================================================================

void platform_RadialBlur( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale  )
{
    g_PostMgr.RadialBlur( Zoom, Angle, AlphaSub, AlphaScale );
}

//=============================================================================

static
void platform_EndPostEffects( void )
{
    g_PostMgr.EndPostEffects();
}

//=============================================================================

static
void platform_BeginShadowShaders( void )
{
    // TODO:
}

//=============================================================================

static
void platform_EndShadowShaders( void )
{
    // TODO:
}

//=============================================================================

static
void platform_StartShadowCast( void )
{
    // TODO:
}

//=============================================================================

static
void platform_EndShadowCast( void )
{
    // TODO:
}

//=============================================================================

static
void platform_StartShadowReceive( void )
{
    // TODO:
}

//=============================================================================

static
void platform_EndShadowReceive( void )
{
    // TODO:
}

//=============================================================================

static
void platform_ClearShadowProjectorList( void )
{
    // TODO:
}

//=============================================================================

static
void platform_AddPointShadowProjection( const matrix4& L2W, radian FOV, f32 NearZ, f32 FarZ )
{
    (void)L2W;
    (void)FOV;
    (void)NearZ;
    (void)FarZ;
    // TODO:
}

//=============================================================================

static
void platform_AddDirShadowProjection( const matrix4& L2W, f32 Width, f32 Height, f32 NearZ, f32 FarZ )
{
    (void)L2W;
    (void)Width;
    (void)Height;
    (void)NearZ;
    (void)FarZ;
    // TODO:
}

//=============================================================================

static
void platform_BeginShadowCastRigid( geom* pGeom, s32 iSubMesh )
{
    (void)pGeom;
    (void)iSubMesh;
    // TODO:
}

//=============================================================================

static
void platform_RenderShadowCastRigid( render_instance& Inst )
{
    (void)Inst;
    // TODO:
}

//=============================================================================

static
void platform_EndShadowCastRigid( void )
{
    // TODO:
}

//=============================================================================

static
void platform_BeginShadowCastSkin( geom* pGeom, s32 iSubMesh )
{
    (void)pGeom;
    (void)iSubMesh;
    // TODO:
}

//=============================================================================

static
void platform_RenderShadowCastSkin( render_instance& Inst, s32 iProj )
{
    (void)Inst;
    (void)iProj;
    // TODO:
}

//=============================================================================

static
void platform_EndShadowCastSkin( void )
{
    // TODO:
}

//=============================================================================

static
void platform_BeginShadowReceiveRigid( geom* pGeom, s32 iSubMesh )
{
    (void)pGeom;
    (void)iSubMesh;
    // TODO:
}

//=============================================================================

static
void platform_RenderShadowReceiveRigid( render_instance& Inst, s32 iProj )
{
    (void)Inst;
    (void)iProj;
    // TODO:
}

//=============================================================================

static
void platform_EndShadowReceiveRigid( void )
{
    // TODO:
}

//=============================================================================

static
void platform_BeginShadowReceiveSkin( geom* pGeom, s32 iSubMesh )
{
    (void)pGeom;
    (void)iSubMesh;
    // TODO:
}

//=============================================================================

static
void platform_RenderShadowReceiveSkin( render_instance& Inst )
{
    (void)Inst;
    // TODO:
}

//=============================================================================

static
void platform_EndShadowReceiveSkin( void )
{
    // TODO:
}

//=============================================================================

static
void platform_BeginDistortion( void )
{
    // TODO:
}

//=============================================================================

static
void platform_EndDistortion( void )
{
}

//=============================================================================

static
void platform_BeginNormalRender( void )
{
    if( !g_pd3dDevice ) return;
    
    const view* pView = eng_GetView();
    if( !pView ) return;
        
    s32 x0, y0, x1, y1;
    pView->GetViewport( x0, y0, x1, y1 );
    
    u32 screenWidth  = (u32)(x1 - x0);
    u32 screenHeight = (u32)(y1 - y0);
    
    if( g_GBufferMgr.ResizeGBuffer( screenWidth, screenHeight ) )
    {
        g_GBufferMgr.SetGBufferTargets();
        g_GBufferMgr.ClearGBuffer();
    }
    
    g_ProjTextureMgr.ClearProjTextures();
}

//=============================================================================

static
void platform_EndNormalRender( void )
{
    g_GBufferMgr.SetBackBufferTarget();
}

//=============================================================================

static
void platform_RegisterRigidInstance( rigid_geom& Geom, render::hgeom_inst hInst )
{
    private_instance& PrivateInst = s_lRegisteredInst(hInst);
    PrivateInst.RigidDList.Clear();
    PrivateInst.IsLit = FALSE;

    // Copy the instance over (the pc renderer isn't really instance based, so
    // we need to make a copy of the verts)
    rigid_geom::dlist_pc* pPCDList = Geom.m_System.pPC;

    // Allocate work memory and setup pointers for the copy loop
    s32 nVerts = Geom.GetNVerts();
    rigid_geom::vertex_pc* pBuffer = new rigid_geom::vertex_pc[nVerts];
    rigid_geom::vertex_pc* pVertex = pBuffer;
    rigid_geom::vertex_pc* pEnd    = (pVertex + nVerts);

    // Loop through all SubMeshs
    for ( s32 iSubMesh = 0; iSubMesh < Geom.m_nSubMeshes; iSubMesh++ )
    {
        // Get the DList for this submesh
        geom::submesh&        GeomSubMesh = Geom.m_pSubMesh[iSubMesh];
        rigid_geom::dlist_pc& DList       = pPCDList[GeomSubMesh.iDList];

        // Copy vertex data
        for ( s32 iVert = 0; iVert < DList.nVerts; iVert++ )
        {
            pVertex[iVert].Pos    = DList.pVert[iVert].Pos;
            pVertex[iVert].UV     = DList.pVert[iVert].UV;
            pVertex[iVert].Normal = DList.pVert[iVert].Normal;
            pVertex[iVert].Color  = xcolor( 128, 128, 128, 255 );
        }

        // create a new handle in the private instance
        xhandle& hDList = PrivateInst.RigidDList.Append();

        // create the dlist and store the handle out
        hDList = g_RigidVertMgr.AddDList( pVertex,
                                          DList.nVerts,
                                          DList.pIndices,
                                          DList.nIndices,
                                          DList.nIndices/3 );

        // advance the ptr
        pVertex += DList.nVerts;
    }

    // Free the work memory
    delete []pBuffer;
    ASSERT( pVertex == pEnd );
}

//=============================================================================

static
void platform_RegisterSkinInstance( skin_geom& Geom, render::hgeom_inst hInst )
{
    (void)Geom;
    (void)hInst;
    // TODO:
}

//=============================================================================

static
void platform_UnregisterRigidInstance( render::hgeom_inst hInst )
{
    private_instance& PrivateInst = s_lRegisteredInst(hInst);
    for ( s32 i = 0; i < PrivateInst.RigidDList.GetCount(); i++ )
    {
        g_RigidVertMgr.DelDList( PrivateInst.RigidDList[i] );
    }
    PrivateInst.RigidDList.Clear();
}

//=============================================================================

static
void platform_UnregisterSkinInstance( render::hgeom_inst hInst )
{
    (void)hInst;
    // TODO:
}