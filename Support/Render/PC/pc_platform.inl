//=============================================================================
//
//  PC Specific Render Routines
//
//=============================================================================

#include "..\LightMgr.hpp"
#include "..\platform_Render.hpp"
#include "..\ProjTextureMgr.hpp"
#include "..\..\Decals\DecalMgr.hpp"
#include "..\..\GameLib\RenderContext.hpp"
#include "VertexMgr.hpp"
#include "SoftVertexMgr.hpp"
#include "ShadowMgr.hpp"
#include "PostMgr/PostMgr.hpp"
#include "GBufferMgr.hpp"
#include "Entropy/D3DEngine/d3deng_rtarget.hpp"

//=============================================================================
// Static data specific to the pc-implementation
//=============================================================================

// KSS, remove me!
static const material*         s_pMaterial        = NULL;
static rigid_geom*             s_pRigidGeom       = NULL;
static skin_geom*              s_pSkinGeom        = NULL;
static const xbitmap*          s_pDrawBitmap      = NULL;
static s32                     s_iRigidSubMesh    = -1;
static s32                     s_iSkinSubMesh     = -1;

static
const rtarget* platform_GetFinalColorTarget( void )
{
    const rtarget* pFinalColor = g_GBufferMgr.GetGBufferTarget( GBUFFER_FINAL_COLOR );
    if( pFinalColor )
        return pFinalColor;

    return rtarget_GetBackBuffer();
}

//=============================================================================

static
const rtarget* platform_GetFinalDepthTarget( void )
{
    if( g_RenderContext.m_bIsPipRender && g_RenderContext.ArePipTargetsActive() )
    {
        pip_render_target_pc* pPipTarget = g_RenderContext.GetActivePipTarget();
        if( pPipTarget &&
            pPipTarget->bValid &&
            pPipTarget->DepthTarget.bIsDepthTarget &&
            pPipTarget->DepthTarget.pDepthStencilView )
        {
            return &pPipTarget->DepthTarget;
        }
    }

    return g_GBufferMgr.GetGBufferTarget( GBUFFER_DEPTH );
}

static
xbool platform_ApplyGDepthTarget( void )
{
    if( !g_GBufferMgr.IsGBufferEnabled() )
        return TRUE;

    const rtarget* pGBufferDepth = platform_GetFinalDepthTarget();
    const rtarget* pFinalColor   = platform_GetFinalColorTarget();

    if( pGBufferDepth && pFinalColor )
    {
        return rtarget_SetTargets( pFinalColor, 1, pGBufferDepth );
    }

    return FALSE;
}

//=============================================================================
// Distortion implementation
//=============================================================================

#include "pc_platform_distortion.inl"

//=============================================================================
// Implementation
//=============================================================================

static
void platform_Init( void )
{
    platform_InitDefaultDistortionMaterial();
    g_GBufferMgr.Init();
    g_GeomMgr.Init();
    g_ShadowMgr.Init();
    g_PostMgr.Init(); 
    g_RigidVertMgr.Init( sizeof( rigid_geom::vertex_pc ) );
    g_SkinVertMgr.Init();
    draw_RegisterGDepthProvider( platform_ApplyGDepthTarget );
}

//=============================================================================

static
void platform_Kill( void )
{
    platform_ReleaseDistortionScene();
    g_SkinVertMgr.Kill(); 
    g_RigidVertMgr.Kill();
    g_PostMgr.Kill();
    g_ShadowMgr.Kill();
    g_GeomMgr.Kill();
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

    if( ((Material.m_Type == Material_Distortion) ||
         (Material.m_Type == Material_Distortion_PerPolyEnv)) &&
        s_bInDistortionPass )
    {
        radian3 ZeroRot;
        ZeroRot.Zero();
        g_GeomMgr.SetDistortionState( ZeroRot );
        platform_ActivateDistortionTextureBindings( &Material );
    }
    else
    {
        g_GeomMgr.ClearDistortionState();

        // Get diffuse texture
        texture* pDiffuse = Material.m_DiffuseMap.GetPointer();
        const xbitmap* pDiffuseMap = pDiffuse ? &pDiffuse->m_Bitmap : NULL;

        // Get detail texture
        texture* pDetail = Material.m_DetailMap.GetPointer();
        const xbitmap* pDetailMap = pDetail ? &pDetail->m_Bitmap : NULL;

        // Get env texture
        texture* pEnvironment = Material.m_EnvironmentMap.GetPointer();
        const xbitmap* pEnvironmentMap = pEnvironment ? &pEnvironment->m_Bitmap : NULL;

        // Set primary textures through MaterialMgr
        g_GeomMgr.SetBitmap( pDiffuseMap, TEXTURE_SLOT_DIFFUSE );
        g_GeomMgr.SetBitmap( pDetailMap, TEXTURE_SLOT_DETAIL  );

        if( Material.m_Flags & geom::material::FLAG_ENV_CUBE_MAP )
        {
            if( !s_pCurrCubeMap )
            {
                x_DebugMsg( "MaterialMgr: WARNING - ENV cube map requested but no cubemap bound\n" );
                ASSERT( s_pCurrCubeMap );
            }

            g_GeomMgr.SetBitmap( NULL, TEXTURE_SLOT_ENVIRONMENT );
            g_GeomMgr.SetEnvironmentCubemap( s_pCurrCubeMap );
        }
        else
        {
            g_GeomMgr.SetEnvironmentCubemap( NULL );
            g_GeomMgr.SetBitmap( pEnvironmentMap, TEXTURE_SLOT_ENVIRONMENT );
        }
    }

    x_catch_display;
}

//=============================================================================

static
void platform_ActivateZPrimeMaterial( void )
{
    s_pMaterial = NULL;
    g_GeomMgr.ClearDistortionState();
    g_GeomMgr.InvalidateCache();
}

//=============================================================================

static
xbool platform_ShouldRenderSceneOnly( const material* pMaterial, u32 RenderFlags, u8 MaterialOverride )
{
    if( MaterialOverride || s_bInDistortionPass || !pMaterial )
        return FALSE;

    if( rtarget_GetCurrentCount() <= 1 )
        return FALSE;

    const u32 FadeMask = (render::FADING_ALPHA | render::INSTFLAG_FADING_ALPHA);
    if( RenderFlags & FadeMask )
        return TRUE;

    switch( pMaterial->m_Type )
    {
        case Material_Alpha:
        case Material_Alpha_PerPixelIllum:
        case Material_Alpha_PerPolyIllum:
        case Material_Alpha_PerPolyEnv:
            return TRUE;

        default:
            return FALSE;
    }
}

//=============================================================================

static
xbool platform_BindSceneOnlyTargets( const material* pMaterial, u32 RenderFlags, u8 MaterialOverride )
{
    if( !platform_ShouldRenderSceneOnly( pMaterial, RenderFlags, MaterialOverride ) )
        return FALSE;

    const rtarget* pFinalColor = platform_GetFinalColorTarget();
    const rtarget* pDepthTarget = platform_GetFinalDepthTarget();
    if( !pFinalColor || !pDepthTarget )
        return FALSE;

    // Transparent/fading geometry should not populate auxiliary GBuffer targets.
    return rtarget_SetTargets( pFinalColor, 1, pDepthTarget );
}

//=============================================================================

static
void platform_RestoreGBufferTargets( xbool bSceneOnlyBound )
{
    if( bSceneOnlyBound )
        g_GBufferMgr.SetGBufferTargets();
}

//=============================================================================

static
void platform_FlushRigidBatch( void )
{
    if( !g_GeomMgr.HasRigidBatch() )
    {
        g_GeomMgr.FlushRigidBatch( s_pMaterial, FALSE );
        return;
    }

    const u8 MaterialOverride = s_bInDistortionPass ? FALSE : g_GeomMgr.GetRigidBatchOverrideMat();
    const xbool bSceneOnlyBound = platform_BindSceneOnlyTargets( s_pMaterial,
                                                                 g_GeomMgr.GetRigidBatchFlags(),
                                                                 MaterialOverride );

    g_GeomMgr.FlushRigidBatch( s_pMaterial, MaterialOverride );
    platform_RestoreGBufferTargets( bSceneOnlyBound );
}

//=============================================================================

static
void platform_FlushSkinBatch( void )
{
    if( !g_GeomMgr.HasSkinBatch() )
    {
        g_GeomMgr.FlushSkinBatch( s_pMaterial, FALSE );
        return;
    }

    const u8 MaterialOverride = s_bInDistortionPass ? FALSE : g_GeomMgr.GetSkinBatchOverrideMat();
    const xbool bSceneOnlyBound = platform_BindSceneOnlyTargets( s_pMaterial,
                                                                 g_GeomMgr.GetSkinBatchFlags(),
                                                                 MaterialOverride );

    g_GeomMgr.FlushSkinBatch( s_pMaterial, MaterialOverride );
    platform_RestoreGBufferTargets( bSceneOnlyBound );
}

//=============================================================================

static
void platform_BeginRigidGeom( geom* pGeom, s32 iSubMesh )
{
    ASSERT( s_pRigidGeom == NULL );
    s_pRigidGeom = (rigid_geom*)pGeom;
    s_iRigidSubMesh = iSubMesh;
    g_GeomMgr.BeginRigidBatch();
    g_RigidVertMgr.BeginRender();
    g_GeomMgr.InvalidateCache();
}

//=============================================================================

static
void platform_EndRigidGeom( void )
{
    ASSERT( s_pRigidGeom );
    platform_FlushRigidBatch();
    s_pRigidGeom = NULL;
    s_iRigidSubMesh = -1;
}

//=============================================================================

static
void platform_BeginSkinGeom( geom* pGeom, s32 iSubMesh )
{
    ASSERT( s_pSkinGeom == NULL );
    s_pSkinGeom = (skin_geom*)pGeom;
    s_iSkinSubMesh = iSubMesh;
    g_GeomMgr.BeginSkinBatch();
    g_SkinVertMgr.BeginRender();
    g_GeomMgr.InvalidateCache();
}

//=============================================================================

static
void platform_EndSkinGeom( void )
{
    ASSERT( s_pSkinGeom );
    platform_FlushSkinBatch();
    s_pSkinGeom = NULL;
    s_iSkinSubMesh = -1;
}

//=============================================================================

static
void platform_RenderRigidInstance( render_instance& Inst )
{
    if( !g_pd3dDevice || !s_pRigidGeom )
        return;

    ASSERT( s_iRigidSubMesh == Inst.SortKey.GeomSubMesh );

    desc_rigid_batch Batch;
    Batch.pGeom        = Inst.Data.Rigid.pGeom;
    Batch.pL2W         = Inst.Data.Rigid.pL2W;
    Batch.pLighting    = (const cb_geom_lighting*)Inst.pLighting;
    Batch.pColorInfo   = (const u32*)Inst.Data.Rigid.pColInfo;
    Batch.hDList       = Inst.hDList;
    Batch.iSubMesh     = Inst.iSubMesh;
    Batch.RenderFlags  = Inst.Flags;
    Batch.UOffset      = Inst.UOffset;
    Batch.VOffset      = Inst.VOffset;
    Batch.Alpha        = Inst.Alpha;
    Batch.OverrideMat  = Inst.OverrideMat;

    if( !g_GeomMgr.CanAppendRigidBatch( Batch ) )
        platform_FlushRigidBatch();

    g_GeomMgr.AddRigidBatchInstance( Batch );
}

//=============================================================================

static
void platform_RenderSkinInstance( render_instance& Inst )
{
    if( !g_pd3dDevice || !s_pSkinGeom )
        return;

    ASSERT( s_iSkinSubMesh == Inst.SortKey.GeomSubMesh );

    desc_skin_batch Batch;
    Batch.pGeom        = Inst.Data.Skin.pGeom;
    Batch.pBones       = Inst.Data.Skin.pBones;
    Batch.pLighting    = (const cb_geom_lighting*)Inst.pLighting;
    Batch.hDList       = Inst.hDList;
    Batch.iSubMesh     = Inst.iSubMesh;
    Batch.RenderFlags  = Inst.Flags;
    Batch.UOffset      = Inst.UOffset;
    Batch.VOffset      = Inst.VOffset;
    Batch.Alpha        = Inst.Alpha;
    Batch.OverrideMat  = Inst.OverrideMat;

    if( !g_GeomMgr.CanAppendSkinBatch( Batch ) )
        platform_FlushSkinBatch();

    g_GeomMgr.AddSkinBatchInstance( Batch );
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
xbool platform_RigidDListsMatch( const rigid_geom::dlist_pc& A, const rigid_geom::dlist_pc& B )
{
    if( (A.nIndices != B.nIndices) || (A.nVerts != B.nVerts) )
        return FALSE;

    if( A.nIndices && (x_memcmp( A.pIndices, B.pIndices, A.nIndices * sizeof(u16) ) != 0) )
        return FALSE;

    for( s32 i = 0; i < A.nVerts; i++ )
    {
        if( (x_memcmp( &A.pVert[i].Pos,    &B.pVert[i].Pos,    sizeof(A.pVert[i].Pos)    ) != 0) ||
            (x_memcmp( &A.pVert[i].Normal, &B.pVert[i].Normal, sizeof(A.pVert[i].Normal) ) != 0) ||
            (x_memcmp( &A.pVert[i].UV,     &B.pVert[i].UV,     sizeof(A.pVert[i].UV)     ) != 0) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=============================================================================

static
s32 platform_FindMatchingRigidDList( const rigid_geom::dlist_pc* pDList, s32 iDList )
{
    for( s32 i = 0; i < iDList; i++ )
    {
        if( platform_RigidDListsMatch( pDList[i], pDList[iDList] ) )
            return i;
    }

    return -1;
}

//=============================================================================

static
xbool platform_SkinDListsMatch( const skin_geom::dlist_pc& A, const skin_geom::dlist_pc& B )
{
    return (A.nIndices  == B.nIndices ) &&
           (A.nVertices == B.nVertices) &&
           (A.nCommands == B.nCommands) &&
           (!A.nIndices  || (x_memcmp( A.pIndex,  B.pIndex,  A.nIndices  * sizeof(s16) ) == 0)) &&
           (!A.nVertices || (x_memcmp( A.pVertex, B.pVertex, A.nVertices * sizeof(skin_geom::vertex_pc) ) == 0)) &&
           (!A.nCommands || (x_memcmp( A.pCmd,    B.pCmd,    A.nCommands * sizeof(skin_geom::command_pc) ) == 0));
}

//=============================================================================

static
s32 platform_FindMatchingSkinDList( const skin_geom::dlist_pc* pDList, s32 iDList )
{
    for( s32 i = 0; i < iDList; i++ )
    {
        if( platform_SkinDListsMatch( pDList[i], pDList[iDList] ) )
            return i;
    }

    return -1;
}

//=============================================================================

static
void platform_RegisterRigidGeom( rigid_geom& Geom )
{
    private_geom& PrivateGeom = s_lRegisteredGeoms(Geom.m_hGeom);
    PrivateGeom.RigidDList.Clear();
    PrivateGeom.RigidDListKey.Clear();
    PrivateGeom.RigidDList.SetCapacity( Geom.m_nDList );
    PrivateGeom.RigidDListKey.SetCapacity( Geom.m_nDList );
    ASSERT( Geom.m_nDList <= 256 );

    rigid_geom::dlist_pc* pPCDList = Geom.m_System.pPC;

    s32 nVerts = 0;
    for( s32 iDList = 0; iDList < Geom.m_nDList; iDList++ )
        nVerts = MAX( nVerts, pPCDList[iDList].nVerts );

    rigid_geom::vertex_pc* pBuffer = new rigid_geom::vertex_pc[nVerts];

    for ( s32 iDList = 0; iDList < Geom.m_nDList; iDList++ )
    {
        rigid_geom::dlist_pc& DList = pPCDList[iDList];

        xhandle& hDList  = PrivateGeom.RigidDList.Append();
        s16&     DListKey = PrivateGeom.RigidDListKey.Append();

        const s32 iMatch = platform_FindMatchingRigidDList( pPCDList, iDList );
        if( iMatch >= 0 )
        {
            hDList   = PrivateGeom.RigidDList[iMatch];
            DListKey = PrivateGeom.RigidDListKey[iMatch];
            continue;
        }

        for ( s32 iVert = 0; iVert < DList.nVerts; iVert++ )
        {
            pBuffer[iVert].Pos    = DList.pVert[iVert].Pos;
            pBuffer[iVert].UV     = DList.pVert[iVert].UV;
            pBuffer[iVert].Normal = DList.pVert[iVert].Normal;
            pBuffer[iVert].Color  = xcolor( 128, 128, 128, 255 );
        }

        hDList = g_RigidVertMgr.AddDList( pBuffer,
                                          DList.nVerts,
                                          DList.pIndices,
                                          DList.nIndices,
                                          DList.nIndices / 3 );
        DListKey = (s16)iDList;
    }

    delete []pBuffer;
}

//=============================================================================

static
void platform_UnregisterRigidGeom( rigid_geom& Geom )
{
    private_geom& PrivateGeom = s_lRegisteredGeoms(Geom.m_hGeom);
    const xbool bHasDListKeys = (PrivateGeom.RigidDListKey.GetCount() == PrivateGeom.RigidDList.GetCount());

    for ( s32 i = 0; i < PrivateGeom.RigidDList.GetCount(); i++ )
    {
        if( !bHasDListKeys || (PrivateGeom.RigidDListKey[i] == i) )
            g_RigidVertMgr.DelDList( PrivateGeom.RigidDList[i] );
    }

    PrivateGeom.RigidDList.Clear();
    PrivateGeom.RigidDListKey.Clear();
}

//=============================================================================

static
void platform_RegisterSkinGeom( skin_geom& Geom )
{
    private_geom& PrivateGeom = s_lRegisteredGeoms(Geom.m_hGeom);
    PrivateGeom.SkinDList.Clear();
    PrivateGeom.SkinDListKey.Clear();
    PrivateGeom.SkinDList.SetCapacity( Geom.m_nDList );
    PrivateGeom.SkinDListKey.SetCapacity( Geom.m_nDList );
    ASSERT( Geom.m_nDList <= 256 );

    // make a private copy of the display lists and register them with the
    // skin vert manager
    skin_geom::dlist_pc* pPCDList = Geom.m_System.pPC;

    s32 nVerts = 0;
    for( s32 iDList = 0; iDList < Geom.m_nDList; iDList++ )
        nVerts = MAX( nVerts, pPCDList[iDList].nVertices );

    skin_geom::vertex_pc* pBuffer = new skin_geom::vertex_pc[nVerts];

    for ( s32 iDList = 0; iDList < Geom.m_nDList; iDList++ )
    {
        skin_geom::dlist_pc& DList = pPCDList[iDList];

        xhandle& hDList   = PrivateGeom.SkinDList.Append();
        s16&     DListKey = PrivateGeom.SkinDListKey.Append();

        const s32 iMatch = platform_FindMatchingSkinDList( pPCDList, iDList );
        if( iMatch >= 0 )
        {
            hDList   = PrivateGeom.SkinDList[iMatch];
            DListKey = PrivateGeom.SkinDListKey[iMatch];
            continue;
        }

        // Copy vertex data
        for ( s32 iVert = 0; iVert < DList.nVertices; iVert++ )
        {
            // Copy vert
            pBuffer[iVert].Position  = DList.pVertex[iVert].Position;
            pBuffer[iVert].Normal    = DList.pVertex[iVert].Normal;
            pBuffer[iVert].UVWeights = DList.pVertex[iVert].UVWeights;
        }

        // Create the dlist and store out the handle
        hDList = g_SkinVertMgr.AddDList( pBuffer,
                                         DList.nVertices,
                                         (u16*)DList.pIndex,
                                         DList.nIndices,
                                         DList.nIndices / 3,
                                         DList.nCommands,
                                         DList.pCmd );
        DListKey = (s16)iDList;
    }

    // Free the work memory
    delete []pBuffer;
}

//=============================================================================

static
void platform_UnregisterSkinGeom( skin_geom& Geom )
{
    private_geom& PrivateGeom = s_lRegisteredGeoms(Geom.m_hGeom);
    const xbool bHasDListKeys = (PrivateGeom.SkinDListKey.GetCount() == PrivateGeom.SkinDList.GetCount());

    for ( s32 i = 0; i < PrivateGeom.SkinDList.GetCount(); i++ )
    {
        if( !bHasDListKeys || (PrivateGeom.SkinDListKey[i] == i) )
            g_SkinVertMgr.DelDList( PrivateGeom.SkinDList[i] );
    }

    PrivateGeom.SkinDList.Clear();
    PrivateGeom.SkinDListKey.Clear();
}

//=============================================================================

//-----------------------------------------------------------------------------
//
// VERY IMPORTANT NOTE: README README README README!!!!! 
//
// NOTE: platform_SetDiffuseMaterial, platform_SetGlowMaterial, platform_SetEnvMapMaterial
// Sets ONLY materials for primitives like sprites and decals! This code NOT for models. 
//
// TODO: This shit needs to change its functions name a long time ago because it's so fucking confusing.
//
//-----------------------------------------------------------------------------

static s32 s_DrawFlags = 0;

static
void platform_SetDiffuseMaterial( const xbitmap& Bitmap, s32 BlendMode, xbool ZTestEnabled )
{
    // do some entropy stuff //////////////////////////////////////////////////

    vram_Activate( Bitmap );          
    
    // we can use draw to set up render states at which point the shader engine
    // will hijack what it needs and route the verts through its pixel pipeline

    s_DrawFlags = DRAW_TEXTURED | DRAW_NO_ZWRITE | DRAW_UV_CLAMP | DRAW_CULL_NONE;
    if( !ZTestEnabled )
        s_DrawFlags |= DRAW_NO_ZBUFFER;
    else
        s_DrawFlags |= DRAW_USE_GDEPTH;  
    
    switch( BlendMode ) 
    { 
        case render::BLEND_MODE_ADDITIVE: 
            s_DrawFlags |=  DRAW_BLEND_ADD;
            break;
        case render::BLEND_MODE_SUBTRACTIVE: 
            s_DrawFlags |= DRAW_BLEND_SUB;
            break;
        case render::BLEND_MODE_INTENSITY: 
            s_DrawFlags |= DRAW_BLEND_INTENSITY;
            break;
        case render::BLEND_MODE_NORMAL: 
            s_DrawFlags |= DRAW_USE_ALPHA;
        default: 
            break;
    }
    
    s_pDrawBitmap = &Bitmap;    
}

//=============================================================================

static
void platform_SetGlowMaterial( const xbitmap& Bitmap, s32 BlendMode, xbool ZTestEnabled )
{
    platform_SetDiffuseMaterial( Bitmap, BlendMode, ZTestEnabled );
}

//=============================================================================

static
void platform_SetEnvMapMaterial( const xbitmap& Bitmap, s32 BlendMode, xbool ZTestEnabled )
{
    platform_SetDiffuseMaterial( Bitmap, BlendMode, ZTestEnabled );
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

    // fill in the l2w...note we have to reset draw to do this
    draw_EnableBilinear();
    draw_Begin( DRAW_TRIANGLES, s_DrawFlags );
    draw_SetTexture( *s_pDrawBitmap );
    draw_SetL2W( L2W );

    for( s32 iVert = 0; iVert < nVerts; ++iVert )
    {
        const f32 W = pPos[iVert].GetW();
        if( (*((const u32*)&W)) & decal_mgr::decal_vert::FLAG_SKIP_TRIANGLE )
            continue;

        if( iVert < 2 )
            continue;

        const vector3 Pos0( pPos[iVert-2].GetX(), pPos[iVert-2].GetY(), pPos[iVert-2].GetZ() );
        const vector3 Pos1( pPos[iVert-1].GetX(), pPos[iVert-1].GetY(), pPos[iVert-1].GetZ() );
        const vector3 Pos2( pPos[iVert-0].GetX(), pPos[iVert-0].GetY(), pPos[iVert-0].GetZ() );

        const vector2 UV0( pUV[(iVert-2)*2+0] * ItoFScale, pUV[(iVert-2)*2+1] * ItoFScale );
        const vector2 UV1( pUV[(iVert-1)*2+0] * ItoFScale, pUV[(iVert-1)*2+1] * ItoFScale );
        const vector2 UV2( pUV[(iVert-0)*2+0] * ItoFScale, pUV[(iVert-0)*2+1] * ItoFScale );

        // Intensity decals ignore vertex color and always render white
        const xcolor White( 255, 255, 255, 255 );
        if( s_DrawFlags & DRAW_BLEND_INTENSITY )
        {
            draw_Color( White ); draw_UV( UV0 ); draw_Vertex( Pos0 );
            draw_Color( White ); draw_UV( UV1 ); draw_Vertex( Pos1 );
            draw_Color( White ); draw_UV( UV2 ); draw_Vertex( Pos2 );
        }
        else
        {
            const xcolor C0( pColor[iVert-2]&0xff,
                             (pColor[iVert-2]&0xff00)>>8,
                             (pColor[iVert-2]&0xff0000)>>16,
                             (pColor[iVert-2]&0xff000000)>>24 );
            const xcolor C1( pColor[iVert-1]&0xff,
                             (pColor[iVert-1]&0xff00)>>8,
                             (pColor[iVert-1]&0xff0000)>>16,
                             (pColor[iVert-1]&0xff000000)>>24 );
            const xcolor C2( pColor[iVert]&0xff,
                             (pColor[iVert]&0xff00)>>8,
                             (pColor[iVert]&0xff0000)>>16,
                             (pColor[iVert]&0xff000000)>>24 );

            draw_Color( C0 ); draw_UV( UV0 ); draw_Vertex( Pos0 );
            draw_Color( C1 ); draw_UV( UV1 ); draw_Vertex( Pos1 );
            draw_Color( C2 ); draw_UV( UV2 ); draw_Vertex( Pos2 );
        }
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

    // start up draw
    const matrix4& V2W = eng_GetView()->GetV2W();
    const matrix4& W2V = eng_GetView()->GetW2V();
    matrix4 S2V;
    if( pL2W )
        S2V = W2V * (*pL2W);
    else
        S2V = W2V;
    
    draw_ClearL2W();
    draw_EnableBilinear();
    draw_Begin( DRAW_TRIANGLES, s_DrawFlags );
    draw_SetTexture( *s_pDrawBitmap );
    draw_SetL2W( V2W );

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
    draw_EnableBilinear();
    draw_Begin( DRAW_TRIANGLES, s_DrawFlags );
    draw_SetTexture( *s_pDrawBitmap );
    draw_SetL2W( V2W );

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

    // start up draw
    draw_ClearL2W();
    draw_EnableBilinear();
    draw_Begin( DRAW_TRIANGLES, s_DrawFlags );
    draw_SetTexture( *s_pDrawBitmap );

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
void* platform_CalculateRigidLighting( const matrix4&   L2W,
                                       const bbox&      WorldBBox )
{
    CONTEXT( "platform_CalculateRigidLighting" );
    
    void* pResult = NULL;
    
    // Grab lights
    s32 NLights = g_LightMgr.CollectLights( WorldBBox, MAX_GEOM_LIGHTS );
    
    if( NLights )
    {
        // Try allocate
        cb_geom_lighting* pLighting = (cb_geom_lighting*)smem_BufferAlloc( sizeof(cb_geom_lighting) );
        x_memset( pLighting, 0, sizeof(cb_geom_lighting) );
        pLighting->LightCount = NLights;
      
        for( s32 i = 0; i < NLights; i++ )
        {
            vector3 Pos;
            f32     Radius;
            xcolor  Col;
            f32     Falloff;
            s32     Shape;
            vector3 Direction;
            f32     InnerAngle;
            f32     OuterAngle;
            s32     CookieIndex;
            vector3 CookieU;
            vector3 CookieV;
            
            g_LightMgr.GetCollectedLightInfo( i,
                                             Pos,
                                             Radius,
                                             Col,
                                             Falloff,
                                             Shape,
                                             Direction,
                                             InnerAngle,
                                             OuterAngle );
            g_LightMgr.GetCollectedLightCookie( i,
                                                CookieIndex,
                                                CookieU,
                                                CookieV );
            
            // Setup rigid lights
            pLighting->LightVec[i].Set( Pos.GetX(),
                                        Pos.GetY(),
                                        Pos.GetZ(),
                                        Radius );
            
            pLighting->LightCol[i].Set( (f32)Col.R / 255.0f,
                                        (f32)Col.G / 255.0f,
                                        (f32)Col.B / 255.0f,
                                        Falloff );

            pLighting->LightDir[i].Set( Direction.GetX(),
                                        Direction.GetY(),
                                        Direction.GetZ(),
                                        (Shape == light_mgr::LIGHT_SHAPE_SPOT) ? 1.0f : 0.0f );

            pLighting->LightCone[i].Set( x_cos( DEG_TO_RAD( InnerAngle ) * 0.5f ),
                                         x_cos( DEG_TO_RAD( OuterAngle ) * 0.5f ),
                                         0.0f,
                                         0.0f );

            pLighting->LightCookieU[i].Set( CookieU.GetX(),
                                            CookieU.GetY(),
                                            CookieU.GetZ(),
                                            (CookieIndex >= 0) ? (f32)(CookieIndex + 1) : 0.0f );
            pLighting->LightCookieV[i].Set( CookieV.GetX(),
                                            CookieV.GetY(),
                                            CookieV.GetZ(),
                                            0.0f );
        }
        
        pResult = pLighting;
    }
    
    // Store in render instance
    return pResult;
}

//=============================================================================

static
void* platform_CalculateSkinLighting( u32            Flags,
                                      const matrix4& L2W,
                                      const bbox&    BBox,
                                      xcolor         Ambient )
{
    (void)Flags;
    CONTEXT( "platform_CalculateSkinLighting" );
    
    void* pResult = NULL;
    bbox WorldBBox = BBox;
    WorldBBox.Transform( L2W );
    
    // Try allocate
    cb_geom_lighting* pLighting = (cb_geom_lighting*)smem_BufferAlloc( sizeof(cb_geom_lighting) );
    x_memset( pLighting, 0, sizeof(cb_geom_lighting) );
    
    // Setup ambient
    pLighting->AmbCol.Set( (f32)Ambient.R / 255.0f,
                           (f32)Ambient.G / 255.0f,
                           (f32)Ambient.B / 255.0f,
                           1.0f );

    s32 NSceneLights = g_LightMgr.CollectLights( WorldBBox, MAX_GEOM_LIGHTS );
    s32 LightIndex   = 0;

    for( s32 i = 0; ( i < NSceneLights ) && ( LightIndex < MAX_GEOM_LIGHTS ); i++, LightIndex++ )
    {
        vector3 Pos;
        f32     Radius;
        xcolor  Col;
        f32     Falloff;
        s32     Shape;
        vector3 Direction;
        f32     InnerAngle;
        f32     OuterAngle;
        s32     CookieIndex;
        vector3 CookieU;
        vector3 CookieV;

        g_LightMgr.GetCollectedLightInfo( i,
                                          Pos,
                                          Radius,
                                          Col,
                                          Falloff,
                                          Shape,
                                          Direction,
                                          InnerAngle,
                                          OuterAngle );
        g_LightMgr.GetCollectedLightCookie( i,
                                            CookieIndex,
                                            CookieU,
                                            CookieV );

        pLighting->LightVec[LightIndex].Set( Pos.GetX(),
                                             Pos.GetY(),
                                             Pos.GetZ(),
                                             Radius );

        pLighting->LightCol[LightIndex].Set( (f32)Col.R / 255.0f,
                                             (f32)Col.G / 255.0f,
                                             (f32)Col.B / 255.0f,
                                             Falloff );

        pLighting->LightDir[LightIndex].Set( Direction.GetX(),
                                             Direction.GetY(),
                                             Direction.GetZ(),
                                             (Shape == light_mgr::LIGHT_SHAPE_SPOT) ? 1.0f : 0.0f );

        pLighting->LightCone[LightIndex].Set( x_cos( DEG_TO_RAD( InnerAngle ) * 0.5f ),
                                              x_cos( DEG_TO_RAD( OuterAngle ) * 0.5f ),
                                              0.0f,
                                              0.0f );

        pLighting->LightCookieU[LightIndex].Set( CookieU.GetX(),
                                                 CookieU.GetY(),
                                                 CookieU.GetZ(),
                                                 (CookieIndex >= 0) ? (f32)(CookieIndex + 1) : 0.0f );
        pLighting->LightCookieV[LightIndex].Set( CookieV.GetX(),
                                                 CookieV.GetY(),
                                                 CookieV.GetZ(),
                                                 0.0f );
    }

    s32 NCharLights = g_LightMgr.CollectCharLightsOnly( L2W, BBox, MAX_GEOM_LIGHTS );
    for( s32 i = 0; ( i < NCharLights ) && ( LightIndex < MAX_GEOM_LIGHTS ); i++, LightIndex++ )
    {
        vector3 Dir;
        xcolor  Col;

        g_LightMgr.GetCollectedCharLight( i, Dir, Col );

        pLighting->LightVec[LightIndex].Set( Dir.GetX(),
                                             Dir.GetY(),
                                             Dir.GetZ(),
                                             0.0f );

        pLighting->LightCol[LightIndex].Set( (f32)Col.R / 255.0f,
                                             (f32)Col.G / 255.0f,
                                             (f32)Col.B / 255.0f,
                                             0.0f );

        pLighting->LightDir[LightIndex].Set( 0.0f,
                                             0.0f,
                                             0.0f,
                                             2.0f );
    }

    pLighting->LightCount = LightIndex;
    
    pResult = pLighting;
    
    // Store in render instance
    return pResult;
}

//=============================================================================
// COMPILATION/EXPORT EDITOR FUNCTIONS
//=============================================================================

#ifdef X_EDITOR

static
xhandle pc_GetRigidDList( render::hgeom_inst hInst, s32 iSubMesh )
{
    ASSERT( hInst.IsNonNull() );

    private_instance& PrivateInst = s_lRegisteredInst(hInst);
    rigid_geom*       pGeom       = (rigid_geom*)PrivateInst.pGeom;
    ASSERT( PrivateInst.Type == TYPE_RIGID );
    ASSERT( (iSubMesh >= 0) && (iSubMesh < pGeom->m_nSubMeshes) );

    geom::submesh& SubMesh     = pGeom->m_pSubMesh[iSubMesh];
    private_geom&  PrivateGeom = s_lRegisteredGeoms(pGeom->m_hGeom);
    return PrivateGeom.RigidDList[(s32)SubMesh.iDList];
}

//=============================================================================

static
void* platform_LockRigidDListVertex( render::hgeom_inst hInst, s32 iSubMesh )
{
    xhandle Handle = pc_GetRigidDList( hInst, iSubMesh );
    return g_RigidVertMgr.LockDListVerts( Handle );
}

//=============================================================================

static
void platform_UnlockRigidDListVertex( render::hgeom_inst hInst, s32 iSubMesh )
{
    xhandle Handle = pc_GetRigidDList( hInst, iSubMesh );
    g_RigidVertMgr.UnlockDListVerts( Handle );
}

//=============================================================================

static
void* platform_LockRigidDListIndex( render::hgeom_inst hInst, s32 iSubMesh,  s32& VertexOffset )
{
    xhandle Handle = pc_GetRigidDList( hInst, iSubMesh );
    return g_RigidVertMgr.LockDListIndices( Handle, VertexOffset );
}

//=============================================================================

static
void platform_UnlockRigidDListIndex( render::hgeom_inst hInst, s32 iSubMesh )
{
    xhandle Handle = pc_GetRigidDList( hInst, iSubMesh );
    g_RigidVertMgr.UnlockDListIndices( Handle );
}

#endif

//=============================================================================
// COMPILATION/EXPORT EDITOR FUNCTIONS - END
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
    g_PostMgr.SetCustomFogPalette( Texture, ImmediateSwitch, PaletteIndex );
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
    g_PostMgr.AddScreenWarp( WorldPos, Radius, WarpAmount );
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
    g_ShadowMgr.BeginShadowShaders();
}

//=============================================================================

static xarray<cb_rigid_instance> s_lShadowRigidBatchInstances;
static xhandle                   s_hShadowRigidBatchDList;
static const material*           s_pShadowRigidBatchMaterial = NULL;
static s32                       s_ShadowRigidBatchSource    = -1;
static u8                        s_ShadowRigidBatchUOffset   = 0;
static u8                        s_ShadowRigidBatchVOffset   = 0;

static xarray<cb_skin_instance>  s_lShadowSkinBatchInstances;
static xarray<matrix4>           s_lShadowSkinBatchBones;
static xhandle                   s_hShadowSkinBatchDList;
static const material*           s_pShadowSkinBatchMaterial = NULL;
static s32                       s_ShadowSkinBatchSource    = -1;
static u8                        s_ShadowSkinBatchUOffset   = 0;
static u8                        s_ShadowSkinBatchVOffset   = 0;

//=============================================================================

static
void platform_ResetShadowCastRigidBatch( void )
{
    s_lShadowRigidBatchInstances.Clear();
    s_hShadowRigidBatchDList.Handle = HNULL;
    s_pShadowRigidBatchMaterial     = NULL;
    s_ShadowRigidBatchSource        = -1;
    s_ShadowRigidBatchUOffset       = 0;
    s_ShadowRigidBatchVOffset       = 0;
}

//=============================================================================

static
void platform_ResetShadowCastSkinBatch( void )
{
    s_lShadowSkinBatchInstances.Clear();
    s_lShadowSkinBatchBones.Clear();
    s_hShadowSkinBatchDList.Handle = HNULL;
    s_pShadowSkinBatchMaterial     = NULL;
    s_ShadowSkinBatchSource        = -1;
    s_ShadowSkinBatchUOffset       = 0;
    s_ShadowSkinBatchVOffset       = 0;
}

//=============================================================================

static
xbool platform_HasShadowCastRigidBatch( void )
{
    return (s_lShadowRigidBatchInstances.GetCount() > 0);
}

//=============================================================================

static
xbool platform_HasShadowCastSkinBatch( void )
{
    return (s_lShadowSkinBatchInstances.GetCount() > 0);
}

//=============================================================================

static
void platform_FlushShadowCastRigidBatch( void )
{
    if( platform_HasShadowCastRigidBatch() )
    {
        g_ShadowMgr.RenderRigidCasterBatch( s_hShadowRigidBatchDList,
                                            &s_lShadowRigidBatchInstances[0],
                                            s_lShadowRigidBatchInstances.GetCount(),
                                            s_pShadowRigidBatchMaterial,
                                            s_ShadowRigidBatchUOffset,
                                            s_ShadowRigidBatchVOffset,
                                            s_ShadowRigidBatchSource );
    }

    platform_ResetShadowCastRigidBatch();
}

//=============================================================================

static
void platform_FlushShadowCastSkinBatch( void )
{
    if( platform_HasShadowCastSkinBatch() )
    {
        g_ShadowMgr.RenderSkinCasterBatch( s_hShadowSkinBatchDList,
                                           &s_lShadowSkinBatchInstances[0],
                                           s_lShadowSkinBatchInstances.GetCount(),
                                           s_lShadowSkinBatchBones.GetCount() ? &s_lShadowSkinBatchBones[0] : NULL,
                                           s_lShadowSkinBatchBones.GetCount(),
                                           s_pShadowSkinBatchMaterial,
                                           s_ShadowSkinBatchUOffset,
                                           s_ShadowSkinBatchVOffset,
                                           s_ShadowSkinBatchSource );
    }

    platform_ResetShadowCastSkinBatch();
}

//=============================================================================

static
xbool platform_CanAppendShadowCastRigidBatch( const render_instance& Inst, const material* pMaterial )
{
    if( !platform_HasShadowCastRigidBatch() )
        return TRUE;

    return (s_hShadowRigidBatchDList.Handle == Inst.hDList.Handle) &&
           (s_pShadowRigidBatchMaterial     == pMaterial) &&
           (s_ShadowRigidBatchUOffset       == Inst.UOffset) &&
           (s_ShadowRigidBatchVOffset       == Inst.VOffset) &&
           (s_ShadowRigidBatchSource        == (s32)Inst.ShadSortKey.ShadowSourceIndex);
}

//=============================================================================

static
xbool platform_CanAppendShadowCastSkinBatch( const render_instance& Inst, const material* pMaterial )
{
    if( !platform_HasShadowCastSkinBatch() )
        return TRUE;

    return (s_hShadowSkinBatchDList.Handle == Inst.hDList.Handle) &&
           (s_pShadowSkinBatchMaterial     == pMaterial) &&
           (s_ShadowSkinBatchUOffset       == Inst.UOffset) &&
           (s_ShadowSkinBatchVOffset       == Inst.VOffset) &&
           (s_ShadowSkinBatchSource        == (s32)Inst.ShadSortKey.ShadowSourceIndex);
}

//=============================================================================

static
void platform_EndShadowShaders( void )
{
    platform_FlushShadowCastRigidBatch();
    platform_FlushShadowCastSkinBatch();
    g_ShadowMgr.EndShadowShaders();
}

//=============================================================================

static
void platform_StartShadowCast( void )
{
    platform_ResetShadowCastRigidBatch();
    platform_ResetShadowCastSkinBatch();
    g_ShadowMgr.BeginCastPass();
}

//=============================================================================

static
void platform_EndShadowCast( void )
{
    platform_FlushShadowCastRigidBatch();
    platform_FlushShadowCastSkinBatch();
    g_ShadowMgr.EndCastPass();
}

//=============================================================================

static
void platform_StartShadowReceive( void )
{
}

//=============================================================================

static
void platform_EndShadowReceive( void )
{
}

//=============================================================================

static
void platform_ClearShadowSourceList( void )
{
    g_ShadowMapMgr.ClearSources();
}

//=============================================================================

static
void platform_FinalizeShadowSourceList( void )
{
    g_ShadowMapMgr.FinalizeSources();
}

//=============================================================================

static
void platform_AddPointShadowMapSource( const matrix4& L2W,
                                       radian         FOV,
                                       f32            LightRadius,
                                       f32            LightFalloff,
                                       s32            ShadowMapResolution,
                                       s32            ShadowPriority,
                                       f32            ShadowScore )
{
    g_ShadowMapMgr.AddPointSource( L2W,
                                   FOV,
                                   LightRadius,
                                   LightFalloff,
                                   ShadowMapResolution,
                                   ShadowPriority,
                                   ShadowScore );
}

//=============================================================================

static
void platform_AddSpotShadowMapSource( const matrix4& L2W,
                                      radian         FOV,
                                      f32            LightRadius,
                                      f32            LightFalloff,
                                      s32            ShadowMapResolution,
                                      s32            ShadowPriority,
                                      f32            ShadowScore )
{
    g_ShadowMapMgr.AddSpotSource( L2W,
                                  FOV,
                                  LightRadius,
                                  LightFalloff,
                                  ShadowMapResolution,
                                  ShadowPriority,
                                  ShadowScore );
}

//=============================================================================

static
void platform_BeginShadowCastRigid( geom* pGeom, s32 iSubMesh )
{
    (void)pGeom;
    (void)iSubMesh;
    platform_ResetShadowCastRigidBatch();
}

//=============================================================================

static
const material* platform_GetShadowCastMaterial( const render_instance& Inst )
{
    const geom* pGeom = ( Inst.ShadSortKey.GeomType == 0 ) ?
                        (const geom*)Inst.Data.Rigid.pGeom :
                        (const geom*)Inst.Data.Skin.pGeom;
    if( !pGeom )
        return NULL;

    const s32 iSubMesh = Inst.iSubMesh;
    if( (iSubMesh < 0) || (iSubMesh >= pGeom->m_nSubMeshes) )
        return NULL;

    const geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
    const geom::material& GeomMat = pGeom->m_pMaterial[SubMesh.iMaterial];
    const xhandle         hMat    = pGeom->m_pVirtualMaterials[GeomMat.iVirtualMat].MatHandle;
    if( (hMat < 0) || (hMat >= kMaxRegisteredMaterials) )
        return NULL;

    return &s_lRegisteredMaterials(hMat);
}

//=============================================================================

static
void platform_RenderShadowCastRigid( render_instance& Inst )
{
    if( !Inst.Data.Rigid.pL2W )
        return;

    const material* pMaterial = platform_GetShadowCastMaterial( Inst );
    if( !platform_CanAppendShadowCastRigidBatch( Inst, pMaterial ) )
        platform_FlushShadowCastRigidBatch();

    if( !platform_HasShadowCastRigidBatch() )
    {
        s_hShadowRigidBatchDList    = Inst.hDList;
        s_pShadowRigidBatchMaterial = pMaterial;
        s_ShadowRigidBatchSource    = Inst.ShadSortKey.ShadowSourceIndex;
        s_ShadowRigidBatchUOffset   = Inst.UOffset;
        s_ShadowRigidBatchVOffset   = Inst.VOffset;
    }

    cb_rigid_instance& GPUInst = s_lShadowRigidBatchInstances.Append();
    x_memset( &GPUInst, 0, sizeof(GPUInst) );
    GPUInst.World       = *Inst.Data.Rigid.pL2W;
    GPUInst.ColorOffset = 0xFFFFFFFFu;
    GPUInst.BaseVertex  = (u32)g_RigidVertMgr.GetDListVertexOffset( Inst.hDList );
    GPUInst.FadeAlpha   = 1.0f;
}

//=============================================================================

static
void platform_EndShadowCastRigid( void )
{
    platform_FlushShadowCastRigidBatch();
}

//=============================================================================

static
void platform_BeginShadowCastSkin( geom* pGeom, s32 iSubMesh )
{
    (void)pGeom;
    (void)iSubMesh;
    platform_ResetShadowCastSkinBatch();
}

//=============================================================================

static
void platform_RenderShadowCastSkin( render_instance& Inst, s32 iShadowSource )
{
    ASSERT( iShadowSource == (s32)Inst.ShadSortKey.ShadowSourceIndex );

    skin_geom* pGeom = Inst.Data.Skin.pGeom;
    ASSERT( pGeom );

    const s32 nBones = pGeom ? pGeom->m_nBones : 0;
    if( (nBones <= 0) || !Inst.Data.Skin.pBones )
        return;

    const material* pMaterial = platform_GetShadowCastMaterial( Inst );
    if( !platform_CanAppendShadowCastSkinBatch( Inst, pMaterial ) )
        platform_FlushShadowCastSkinBatch();

    if( !platform_HasShadowCastSkinBatch() )
    {
        s_hShadowSkinBatchDList    = Inst.hDList;
        s_pShadowSkinBatchMaterial = pMaterial;
        s_ShadowSkinBatchSource    = Inst.ShadSortKey.ShadowSourceIndex;
        s_ShadowSkinBatchUOffset   = Inst.UOffset;
        s_ShadowSkinBatchVOffset   = Inst.VOffset;
    }

    cb_skin_instance& GPUInst = s_lShadowSkinBatchInstances.Append();
    x_memset( &GPUInst, 0, sizeof(GPUInst) );
    GPUInst.BoneOffset = s_lShadowSkinBatchBones.GetCount();
    GPUInst.FadeAlpha  = 1.0f;

    for( s32 i = 0; i < nBones; i++ )
        s_lShadowSkinBatchBones.Append() = Inst.Data.Skin.pBones[i];
}

//=============================================================================

static
void platform_EndShadowCastSkin( void )
{
    platform_FlushShadowCastSkinBatch();
}

//=============================================================================

static
void platform_BeginShadowReceiveRigid( geom* pGeom, s32 iSubMesh )
{
    (void)pGeom;
    (void)iSubMesh;
}

//=============================================================================

static
void platform_RenderShadowReceiveRigid( render_instance& Inst, s32 iShadowSource )
{
    (void)Inst;
    (void)iShadowSource;
}

//=============================================================================

static
void platform_EndShadowReceiveRigid( void )
{
}

//=============================================================================

static
void platform_BeginShadowReceiveSkin( geom* pGeom, s32 iSubMesh )
{
    (void)pGeom;
    (void)iSubMesh;
}

//=============================================================================

static
void platform_RenderShadowReceiveSkin( render_instance& Inst )
{
    (void)Inst;
}

//=============================================================================

static
void platform_EndShadowReceiveSkin( void )
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
    g_GBufferMgr.SetFinalColorTarget();
}

//=============================================================================

static
void platform_RegisterRigidInstance( rigid_geom& Geom, render::hgeom_inst hInst )
{
    (void)Geom;
    (void)hInst;
}

//=============================================================================

static
void platform_RegisterSkinInstance( skin_geom& Geom, render::hgeom_inst hInst )
{
    (void)Geom;
    (void)hInst;
}

//=============================================================================

static
void platform_UnregisterRigidInstance( render::hgeom_inst hInst )
{
    (void)hInst;
}

//=============================================================================

static
void platform_UnregisterSkinInstance( render::hgeom_inst hInst )
{
    (void)hInst;
}
