//=============================================================================
//
//  Render.cpp
//
//=============================================================================

#include "Entropy.hpp"
#include "GeometryDraw.hpp"
#include "Render.hpp"
#include "ProjTextureMgr.hpp"

#define RENDER_PRIVATE
#include "MaterialArray.hpp"
#undef RENDER_PRIVATE

//=========================================================================
//  TYPES
//=========================================================================

//-------------------------------------------------------------------------

enum class GeometryType
{
    Rigid = 0,
    Skin
};

//-------------------------------------------------------------------------

struct PrivateInstance
{
    geom*        m_pGeometry;
    GeometryType m_Type;
    xhandle      m_geometryHandle;
};

//-------------------------------------------------------------------------

struct PrivateGeometry
{
    geom*           m_pGeometry;
    GeometryType    m_Type;
    s32             m_referenceCount;
    xhandle         m_renderGeometryHandle;
    xarray<xhandle> m_materialHandles;
};

//=============================================================================
// Statics
//=============================================================================

// Stats for determining how much data has been loaded
#ifndef X_RETAIL
static s32 s_loadedGeometryCount = 0;
static s32 s_loadedBoneCount = 0;
static s32 s_loadedMeshCount = 0;
static s32 s_loadedSubmeshCount = 0;
static s32 s_loadedMaterialCount = 0;
static s32 s_loadedTextureCount = 0;
static s32 s_loadedUvKeyCount = 0;
static s32 s_loadedVirtualMaterialCount = 0;
#endif

static const s32 kMaxRegisteredGeoms = 512;
static s32 const kMaxRegisteredInstances = 12800;
static s32 const kMaxRegisteredMaterials = 640;
static s32 const kMaxTexAnims = 2048;
static s32 const kMaxTexAnimInstances = 1024;
static s32 const kMaxRegisteredTexAnims = 1024;
static s32 const kMaxRenderedInstances = 32768;

// arrays for rendering everything
static xharray<PrivateGeometry>          s_registeredGeometry;
static xharray<PrivateInstance>          s_registeredInstances;
static MaterialArray                     s_registeredMaterials;
static xarray<geometry_draw_item>        s_geometryDraws;
static xarray<dynamic_geometry_draw>     s_dynamicGeometryDraws;
static xarray<dynamic_geometry_shadow_draw> s_dynamicShadowDraws;
static xarray<geometry_draw_item>        s_shadowDraws;
static xarray<geometry_draw_item const*> s_orderedShadowDraws;
static u32                               s_nextGeometrySequence;
static u32                               s_nextShadowSequence;

// sanity check data
static xbool s_isPrimitiveRenderActive = FALSE;
static xbool s_isRenderActive = FALSE;
static xbool s_isShadowRenderActive = FALSE;

// misc. data
static cubemap* s_pCurrentCubeMap = NULL;
static f32      s_pulseTime;

// debugging options
#ifndef X_RETAIL
render::debug_options g_renderDebug = { FALSE, FALSE, FALSE, FALSE };
#endif

// Filter light data
static s32    s_isLightFilteringEnabled = FALSE;
static xcolor s_filterLightColor( xcolor( 30, 0, 0, 255 ) );

// dynamic shadow-map sources generated for the current frame
static s32 s_shadowSourceCount;

//=============================================================================
// Platform-specific code
//=============================================================================

#include "LightMgr.hpp"
#include "PrimitiveBatch.hpp"
#include "platform_Render.hpp"

static xhandle GetRegisteredMaterialHandle( xhandle hGeom, s32 iVirtualMaterial );

#include "PC/pc_platform.inl"

//=============================================================================
// Static declarations that we can put X_SECTION's on.
//=============================================================================

static xhandle                        FindMaterial( material& sourceMaterial ) X_SECTION( init );
static xhandle                        FindRegisteredGeom( geom const& geom ) X_SECTION( init );
static void                           RegisterMaterials( PrivateGeometry& registeredGeom ) X_SECTION( init );
static void                           UnregisterMaterials( PrivateGeometry& registeredGeom ) X_SECTION( init );
static xhandle                        RegisterGeom( geom& geom, GeometryType type ) X_SECTION( init );
static void                           UnregisterGeom( xhandle hGeom ) X_SECTION( init );
static xhandle                        RegisterRigidGeom( rigid_geom& geom ) X_SECTION( init );
static void                           UnregisterRigidGeom( xhandle hGeom ) X_SECTION( init );
static xhandle                        RegisterSkinGeom( skin_geom& geom ) X_SECTION( init );
static void                           UnregisterSkinGeom( xhandle hGeom ) X_SECTION( init );
static render::GeometryInstanceHandle AddPrivateInstance( xhandle hGeom, GeometryType type ) X_SECTION( init );
static void                           RemovePrivateInstance( render::GeometryInstanceHandle hInst ) X_SECTION( init );
static s32                            ShadowCompareFn( void const* p1, void const* p2 ) X_SECTION( render_infrequent );
static geometry_render_pass           ClassifyGeometryPass( material const& material, u32 flags, xbool fadingAlpha )
    X_SECTION( render_add );
static xbool ComputeGeometryDepth( f32& depth, bbox const& localBounds, matrix4 const* pLocalToWorld )
    X_SECTION( render_add );
static void AppendRigidGeometryDraw( PrivateInstance const& registeredInst, rigid_geom& geom, xhandle hMaterial,
                                     s32 iSurface, matrix4 const* pL2W, u32 const* pColorInfo, void const* pLighting,
                                     u32 flags, u32 projectionFlags, u8 alpha, xbool fadingAlpha )
    X_SECTION( render_add );
static void  AppendSkinGeometryDraw( PrivateInstance const& registeredInst, skin_geom& geom, xhandle hMaterial,
                                     s32 iSurface, matrix4 const* pBones, void const* pLighting, u32 flags,
                                     u32 projectionFlags, u8 alpha, xbool fadingAlpha ) X_SECTION( render_add );
static void  AppendRigidShadowDraw( PrivateInstance const& registeredInst, rigid_geom& geom, xhandle hMaterial,
                                    s32 iSurface, matrix4 const* pL2W, s32 shadowSourceIndex )
    X_SECTION( render_add_shadow );
static void  AppendSkinShadowDraw( PrivateInstance const& registeredInst, skin_geom& geom, xhandle hMaterial,
                                   s32 iSurface, matrix4 const* pBones, s32 boneCount, s32 shadowSourceIndex )
    X_SECTION( render_add_shadow );
static void  GetUVOffset( u8& uOffset, u8& vOffset, geom* pGeom, material& mat ) X_SECTION( render_add );
static xbool IntersectsView( view const& v, bbox const& bBox ) X_SECTION( render_add );
static void  CalcVMatOffsets( s32* pVMatOffsets, geom const* pGeom, u32 vTextureMask ) X_SECTION( render_infrequent );

//=============================================================================

// Internal functions
//=============================================================================

static xhandle FindMaterial( material& sourceMaterial )
{
    for ( s32 i = 0; i < s_registeredMaterials.GetCount(); i++ )
    {
        material& m = s_registeredMaterials[i];

        if ( m == sourceMaterial )
        {
            return s_registeredMaterials.GetHandleByIndex( i );
        }
    }

    return HNULL;
}

//=============================================================================

static xhandle FindRegisteredGeom( geom const& geom )
{
    for ( s32 i = 0; i < s_registeredGeometry.GetCount(); ++i )
    {
        if ( s_registeredGeometry[i].m_pGeometry == &geom )
        {
            return s_registeredGeometry.GetHandleByIndex( i );
        }
    }

    return HNULL;
}

//=============================================================================

static xhandle GetRegisteredMaterialHandle( xhandle hGeom, s32 iVirtualMaterial )
{
    PrivateGeometry const& registeredGeom = s_registeredGeometry( hGeom );
    ASSERT( iVirtualMaterial >= 0 );
    ASSERT( iVirtualMaterial < registeredGeom.m_materialHandles.GetCount() );
    return registeredGeom.m_materialHandles[iVirtualMaterial];
}

//=============================================================================

static void RegisterMaterials( PrivateGeometry& registeredGeom )
{
    geom& geom = *registeredGeom.m_pGeometry;

    registeredGeom.m_materialHandles.SetCount( geom.m_nVirtualMaterials );
    for ( s32 i = 0; i < registeredGeom.m_materialHandles.GetCount(); ++i )
    {
        registeredGeom.m_materialHandles[i].Handle = HNULL;
    }

    for ( s32 iMat = 0; iMat < geom.m_nMaterials; iMat++ )
    {
        material        mat;
        geom::material& geomMat = geom.m_pMaterial[iMat];

        mat.m_Type = geomMat.Type;
        mat.m_Flags = geomMat.Flags;
        mat.m_detailScale = geomMat.DetailScale;
        mat.m_fixedAlpha = geomMat.FixedAlpha;

        mat.m_uvAnim.CurrentFrame = 0.0f;
        mat.m_uvAnim.iKey = geomMat.UVAnim.iKey;
        mat.m_uvAnim.iFrame = 0;
        mat.m_uvAnim.Dir = 1;
        mat.m_uvAnim.Type = geomMat.UVAnim.Type;
        mat.m_uvAnim.nFrames = geomMat.UVAnim.nKeys;
        mat.m_uvAnim.FPS = geomMat.UVAnim.FPS;
        mat.m_uvAnim.StartFrame = geomMat.UVAnim.StartFrame;

        if ( ( ( mat.m_Type == Material_Diff_PerPixelEnv ) || ( mat.m_Type == Material_Alpha_PerPolyEnv ) ) &&
             !( mat.m_Flags & geom::material::FLAG_ENV_CUBE_MAP ) &&
             !( geomMat.Flags & geom::material::FLAG_HAS_ENV_MAP ) )
        {
            x_throw( "Environment mapped material without an env texture!" );
        }

        s32 const iDiffuse = geomMat.iTexture;
        s32 const iEnvironment = iDiffuse + geomMat.nVirtualMats;
        s32 const iDetail = iEnvironment + ( ( geomMat.Flags & geom::material::FLAG_HAS_ENV_MAP ) ? 1 : 0 );

        for ( s32 iVMat = 0; iVMat < geomMat.nVirtualMats; iVMat++ )
        {
            mat.m_diffuseMap.SetName( geom.GetTextureName( iDiffuse + iVMat ) );

            if ( geomMat.Flags & geom::material::FLAG_HAS_ENV_MAP )
            {
                mat.m_environmentMap.SetName( geom.GetTextureName( iEnvironment ) );
            }
            else
            {
                mat.m_environmentMap.SetName( "" );
            }

            if ( geomMat.Flags & geom::material::FLAG_HAS_DETAIL_MAP )
            {
                mat.m_detailMap.SetName( geom.GetTextureName( iDetail ) );
            }
            else
            {
                mat.m_detailMap.SetName( "" );
            }

            mat.Finalize();

            xhandle handle = FindMaterial( mat );
            if ( handle == HNULL )
            {
                ASSERT( s_registeredMaterials.GetCount() < kMaxRegisteredMaterials );
                material& newMat = s_registeredMaterials.Add( handle );
                newMat = mat;
            }

            material& finalMat = s_registeredMaterials( handle );
            finalMat.AddRef();

            s32 const iRegisteredMaterial = geomMat.iVirtualMat + iVMat;
            ASSERT( iRegisteredMaterial >= 0 );
            ASSERT( iRegisteredMaterial < registeredGeom.m_materialHandles.GetCount() );
            registeredGeom.m_materialHandles[iRegisteredMaterial] = handle;
        }
    }
}

//=============================================================================

static void UnregisterMaterials( PrivateGeometry& registeredGeom )
{
    for ( s32 i = 0; i < registeredGeom.m_materialHandles.GetCount(); ++i )
    {
        xhandle const handle = registeredGeom.m_materialHandles[i];
        if ( handle.IsNull() )
        {
            continue;
        }

        material& mat = s_registeredMaterials( handle );
        mat.Release();
        if ( mat.GetRefCount() == 0 )
        {
            s_registeredMaterials.DeleteByHandle( handle );
        }
    }

    registeredGeom.m_materialHandles.Clear();
}

//=============================================================================

static xhandle RegisterGeom( geom& geom, GeometryType type )
{
    ASSERT( FindRegisteredGeom( geom ).IsNull() );
    ASSERT( s_registeredGeometry.GetCount() < kMaxRegisteredGeoms );

    xhandle          hGeom;
    PrivateGeometry& registeredGeom = s_registeredGeometry.Add( hGeom );
    registeredGeom.m_pGeometry = &geom;
    registeredGeom.m_Type = type;
    registeredGeom.m_referenceCount = 0;
    registeredGeom.m_renderGeometryHandle.Handle = HNULL;
    registeredGeom.m_materialHandles.Clear();

    x_try;

    RegisterMaterials( registeredGeom );

    x_catch_begin;

    UnregisterMaterials( registeredGeom );
    s_registeredGeometry.DeleteByHandle( hGeom );

    x_catch_end_ret;

#ifndef X_RETAIL
    s_loadedGeometryCount += 1;
    s_loadedBoneCount += geom.m_nBones;
    s_loadedMeshCount += geom.m_nMeshes;
    s_loadedSubmeshCount += geom.m_nSubMeshes;
    s_loadedMaterialCount += geom.m_nMaterials;
    s_loadedTextureCount += geom.m_nTextures;
    s_loadedUvKeyCount += geom.m_nUVKeys;
    s_loadedVirtualMaterialCount += geom.m_nVirtualMaterials;
#endif

    return hGeom;
}

//=============================================================================

static void UnregisterGeom( xhandle hGeom )
{
    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );
    geom&            geom = *registeredGeom.m_pGeometry;

    ASSERT( registeredGeom.m_referenceCount == 0 );
    ASSERT( registeredGeom.m_renderGeometryHandle.IsNull() );

    UnregisterMaterials( registeredGeom );

#ifndef X_RETAIL
    s_loadedGeometryCount -= 1;
    s_loadedBoneCount -= geom.m_nBones;
    s_loadedMeshCount -= geom.m_nMeshes;
    s_loadedSubmeshCount -= geom.m_nSubMeshes;
    s_loadedMaterialCount -= geom.m_nMaterials;
    s_loadedTextureCount -= geom.m_nTextures;
    s_loadedUvKeyCount -= geom.m_nUVKeys;
    s_loadedVirtualMaterialCount -= geom.m_nVirtualMaterials;
#endif

    s_registeredGeometry.DeleteByHandle( hGeom );
}

//=============================================================================

static xhandle RegisterRigidGeom( rigid_geom& geom )
{
    xhandle const    hGeom = RegisterGeom( geom, GeometryType::Rigid );
    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );

    x_try;

    registeredGeom.m_renderGeometryHandle = platform_RegisterRigidGeom( geom );

    x_catch_begin;

    registeredGeom.m_renderGeometryHandle.Handle = HNULL;
    UnregisterGeom( hGeom );

    x_catch_end_ret;

    return hGeom;
}

//=============================================================================

static void UnregisterRigidGeom( xhandle hGeom )
{
    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );
    ASSERT( registeredGeom.m_Type == GeometryType::Rigid );

    platform_UnregisterRigidGeom( registeredGeom.m_renderGeometryHandle );
    registeredGeom.m_renderGeometryHandle.Handle = HNULL;
    UnregisterGeom( hGeom );
}

//=============================================================================

static xhandle RegisterSkinGeom( skin_geom& geom )
{
    xhandle const    hGeom = RegisterGeom( geom, GeometryType::Skin );
    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );

    x_try;

    registeredGeom.m_renderGeometryHandle = platform_RegisterSkinGeom( geom );

    x_catch_begin;

    registeredGeom.m_renderGeometryHandle.Handle = HNULL;
    UnregisterGeom( hGeom );

    x_catch_end_ret;

    return hGeom;
}

//=============================================================================

static void UnregisterSkinGeom( xhandle hGeom )
{
    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );
    ASSERT( registeredGeom.m_Type == GeometryType::Skin );

    platform_UnregisterSkinGeom( registeredGeom.m_renderGeometryHandle );
    registeredGeom.m_renderGeometryHandle.Handle = HNULL;
    UnregisterGeom( hGeom );
}

//=============================================================================

static render::GeometryInstanceHandle AddPrivateInstance( xhandle hGeom, GeometryType type )
{
    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );
    ASSERT( registeredGeom.m_Type == type );
    registeredGeom.m_referenceCount++;

    render::GeometryInstanceHandle handle;
    ASSERT( s_registeredInstances.GetCount() < kMaxRegisteredInstances );
    PrivateInstance& inst = s_registeredInstances.Add( handle );
    inst.m_pGeometry = registeredGeom.m_pGeometry;
    inst.m_Type = type;
    inst.m_geometryHandle = hGeom;

    return handle;
}

//=============================================================================

static void RemovePrivateInstance( render::GeometryInstanceHandle hInst )
{
    PrivateInstance& inst = s_registeredInstances( hInst );
    PrivateGeometry& registeredGeom = s_registeredGeometry( inst.m_geometryHandle );

    ASSERT( registeredGeom.m_referenceCount > 0 );
    registeredGeom.m_referenceCount--;
    s_registeredInstances.DeleteByHandle( hInst );
}

//=============================================================================

static s32 ShadowCompareFn( void const* p1, void const* p2 )
{
    geometry_draw_item const& a = **(geometry_draw_item const* const*)p1;
    geometry_draw_item const& b = **(geometry_draw_item const* const*)p2;

    if ( a.ShadowSourceIndex < b.ShadowSourceIndex )
    {
        return -1;
    }
    if ( a.ShadowSourceIndex > b.ShadowSourceIndex )
    {
        return 1;
    }
    if ( a.Type < b.Type )
    {
        return -1;
    }
    if ( a.Type > b.Type )
    {
        return 1;
    }
    if ( a.hRenderGeom.Handle < b.hRenderGeom.Handle )
    {
        return -1;
    }
    if ( a.hRenderGeom.Handle > b.hRenderGeom.Handle )
    {
        return 1;
    }
    if ( a.iSurface < b.iSurface )
    {
        return -1;
    }
    if ( a.iSurface > b.iSurface )
    {
        return 1;
    }
    if ( a.Sequence < b.Sequence )
    {
        return -1;
    }
    if ( a.Sequence > b.Sequence )
    {
        return 1;
    }
    return 0;
}

//=============================================================================

static geometry_render_pass ClassifyGeometryPass( material const& material, u32 flags, xbool fadingAlpha )
{
    if ( material.IsDistortion() )
    {
        return GEOMETRY_PASS_DISTORTION;
    }

    if ( fadingAlpha )
    {
        return GEOMETRY_PASS_FADING;
    }

    if ( flags & render::FORCE_LAST )
    {
        return GEOMETRY_PASS_FORCE_LAST;
    }

    if ( material.RequiresForwardPass() )
    {
        return material.IsPostEffectBlend() ? GEOMETRY_PASS_ADDITIVE : GEOMETRY_PASS_TRANSPARENT;
    }

    return GEOMETRY_PASS_GBUFFER;
}

//=============================================================================

static xbool ComputeGeometryDepth( f32& depth, bbox const& localBounds, matrix4 const* pLocalToWorld )
{
    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    vector3 center = localBounds.GetCenter();
    if ( pLocalToWorld )
    {
        center = *pLocalToWorld * center;
    }

    depth = ( pView->GetW2V() * center ).GetZ();
    return x_isvalid( depth );
}

//=============================================================================

static xbool ShouldSubmitGeometry( u32 flags )
{
#ifndef X_RETAIL
    if ( g_renderDebug.RenderClippedOnly && !( flags & render::CLIPPED ) )
    {
        return FALSE;
    }

    if ( g_renderDebug.RenderShadowedOnly && !( flags & render::INSTFLAG_PROJ_SHADOW ) )
    {
        return FALSE;
    }
#else
    static_cast<void>( flags );
#endif

    return TRUE;
}

//=============================================================================

static void AppendZPrimeDraw( geometry_draw_item const& source )
{
    ASSERT( s_geometryDraws.GetCount() < kMaxRenderedInstances );
    if ( s_geometryDraws.GetCount() >= kMaxRenderedInstances )
    {
        return;
    }

    geometry_draw_item  zPrime = source;
    geometry_draw_item& item = s_geometryDraws.Append();
    item = zPrime;
    item.Pass = GEOMETRY_PASS_ZPRIME;
    item.pMaterial = NULL;
    item.MaterialOrder = 0;
    item.Flags = source.Flags & render::CLIPPED;
    item.Alpha = 0x80;
    item.MaterialOverride = TRUE;
    item.Sequence = s_nextGeometrySequence++;
}

//=============================================================================

static void AppendRigidGeometryDraw( PrivateInstance const& registeredInst, rigid_geom& geom, xhandle hMaterial,
                                     s32 iSurface, matrix4 const* pL2W, u32 const* pColorInfo, void const* pLighting,
                                     u32 flags, u32 projectionFlags, u8 alpha, xbool fadingAlpha )
{
    if ( !ShouldSubmitGeometry( flags ) )
    {
        return;
    }

    ASSERT( s_geometryDraws.GetCount() < kMaxRenderedInstances );
    if ( s_geometryDraws.GetCount() >= kMaxRenderedInstances )
    {
        return;
    }

    PrivateGeometry&           registeredGeom = s_registeredGeometry( registeredInst.m_geometryHandle );
    material&                  material = s_registeredMaterials( hMaterial );
    geometry_render_pass const pass = ClassifyGeometryPass( material, flags, fadingAlpha );

    f32 sortDepth = 0.0f;
    if ( ( ( pass == GEOMETRY_PASS_TRANSPARENT ) || ( pass == GEOMETRY_PASS_FADING ) ||
           ( pass == GEOMETRY_PASS_DISTORTION ) ) &&
         !ComputeGeometryDepth( sortDepth, geom.m_BBox, pL2W ) )
    {
        return;
    }

    geometry_draw_item& item = s_geometryDraws.Append();
    item.Pass = pass;
    item.Type = GEOMETRY_DRAW_RIGID;
    item.hRenderGeom = registeredGeom.m_renderGeometryHandle;
    item.pMaterial = &material;
    item.MaterialOrder = s_registeredMaterials.GetIndexByHandle( hMaterial );
    item.iSurface = iSurface;
    item.Flags = flags;
    item.pLighting = pLighting;
    item.Data.Rigid.pGeom = &geom;
    item.Data.Rigid.pL2W = pL2W;
    item.Data.Rigid.pColorInfo = pColorInfo;
    item.SortDepth = sortDepth;
    item.Sequence = s_nextGeometrySequence++;
    item.ShadowSourceIndex = -1;
    item.Alpha = alpha;
    item.MaterialOverride = FALSE;
    item.DistortionNormalRot.Zero();
    GetUVOffset( item.UOffset, item.VOffset, &geom, material );

    if ( ( item.Pass == GEOMETRY_PASS_GBUFFER ) || ( item.Pass == GEOMETRY_PASS_TRANSPARENT ) ||
         ( item.Pass == GEOMETRY_PASS_ADDITIVE ) )
    {
        if ( material.ReceivesProjection() )
        {
            item.Flags |= projectionFlags;
        }
    }

    if ( fadingAlpha )
    {
        AppendZPrimeDraw( item );
    }
}

//=============================================================================

static void AppendSkinGeometryDraw( PrivateInstance const& registeredInst, skin_geom& geom, xhandle hMaterial,
                                    s32 iSurface, matrix4 const* pBones, void const* pLighting, u32 flags,
                                    u32 projectionFlags, u8 alpha, xbool fadingAlpha )
{
    if ( !ShouldSubmitGeometry( flags ) )
    {
        return;
    }

    ASSERT( s_geometryDraws.GetCount() < kMaxRenderedInstances );
    if ( s_geometryDraws.GetCount() >= kMaxRenderedInstances )
    {
        return;
    }

    PrivateGeometry&           registeredGeom = s_registeredGeometry( registeredInst.m_geometryHandle );
    material&                  material = s_registeredMaterials( hMaterial );
    geometry_render_pass const pass = ClassifyGeometryPass( material, flags, fadingAlpha );

    f32 sortDepth = 0.0f;
    if ( ( ( pass == GEOMETRY_PASS_TRANSPARENT ) || ( pass == GEOMETRY_PASS_FADING ) ||
           ( pass == GEOMETRY_PASS_DISTORTION ) ) &&
         !ComputeGeometryDepth( sortDepth, geom.m_BBox, pBones ) )
    {
        return;
    }

    geometry_draw_item& item = s_geometryDraws.Append();
    item.Pass = pass;
    item.Type = GEOMETRY_DRAW_SKIN;
    item.hRenderGeom = registeredGeom.m_renderGeometryHandle;
    item.pMaterial = &material;
    item.MaterialOrder = s_registeredMaterials.GetIndexByHandle( hMaterial );
    item.iSurface = iSurface;
    item.Flags = flags;
    item.pLighting = pLighting;
    item.Data.Skin.pGeom = &geom;
    item.Data.Skin.pBones = pBones;
    item.Data.Skin.BoneCount = 0;
    item.SortDepth = sortDepth;
    item.Sequence = s_nextGeometrySequence++;
    item.ShadowSourceIndex = -1;
    item.Alpha = alpha;
    item.MaterialOverride = FALSE;
    item.DistortionNormalRot.Zero();
    GetUVOffset( item.UOffset, item.VOffset, &geom, material );

    if ( ( item.Pass == GEOMETRY_PASS_GBUFFER ) || ( item.Pass == GEOMETRY_PASS_TRANSPARENT ) ||
         ( item.Pass == GEOMETRY_PASS_ADDITIVE ) )
    {
        if ( material.ReceivesProjection() )
        {
            item.Flags |= projectionFlags;
        }
    }

    if ( fadingAlpha )
    {
        AppendZPrimeDraw( item );
    }
}

//=============================================================================

static void AppendRigidShadowDraw( PrivateInstance const& registeredInst, rigid_geom& geom, xhandle hMaterial,
                                   s32 iSurface, matrix4 const* pL2W, s32 shadowSourceIndex )
{
    ASSERT( s_shadowDraws.GetCount() < kMaxRenderedInstances );
    if ( s_shadowDraws.GetCount() >= kMaxRenderedInstances )
    {
        return;
    }

    PrivateGeometry& registeredGeom = s_registeredGeometry( registeredInst.m_geometryHandle );
    material&        material = s_registeredMaterials( hMaterial );

    geometry_draw_item& item = s_shadowDraws.Append();
    item.Pass = GEOMETRY_PASS_SHADOW_CAST;
    item.Type = GEOMETRY_DRAW_RIGID;
    item.hRenderGeom = registeredGeom.m_renderGeometryHandle;
    item.pMaterial = &material;
    item.MaterialOrder = s_registeredMaterials.GetIndexByHandle( hMaterial );
    item.iSurface = iSurface;
    item.Flags = 0;
    item.pLighting = NULL;
    item.Data.Rigid.pGeom = &geom;
    item.Data.Rigid.pL2W = pL2W;
    item.Data.Rigid.pColorInfo = NULL;
    item.SortDepth = 0.0f;
    item.ShadowSourceIndex = shadowSourceIndex;
    item.Sequence = s_nextShadowSequence++;
    item.Alpha = 255;
    item.MaterialOverride = FALSE;
    item.DistortionNormalRot.Zero();
    GetUVOffset( item.UOffset, item.VOffset, &geom, material );
}

//=============================================================================

static void AppendSkinShadowDraw( PrivateInstance const& registeredInst, skin_geom& geom, xhandle hMaterial,
                                  s32 iSurface, matrix4 const* pBones, s32 boneCount, s32 shadowSourceIndex )
{
    ASSERT( s_shadowDraws.GetCount() < kMaxRenderedInstances );
    if ( s_shadowDraws.GetCount() >= kMaxRenderedInstances )
    {
        return;
    }

    PrivateGeometry& registeredGeom = s_registeredGeometry( registeredInst.m_geometryHandle );
    material&        material = s_registeredMaterials( hMaterial );

    geometry_draw_item& item = s_shadowDraws.Append();
    item.Pass = GEOMETRY_PASS_SHADOW_CAST;
    item.Type = GEOMETRY_DRAW_SKIN;
    item.hRenderGeom = registeredGeom.m_renderGeometryHandle;
    item.pMaterial = &material;
    item.MaterialOrder = s_registeredMaterials.GetIndexByHandle( hMaterial );
    item.iSurface = iSurface;
    item.Flags = 0;
    item.pLighting = NULL;
    item.Data.Skin.pGeom = &geom;
    item.Data.Skin.pBones = pBones;
    item.Data.Skin.BoneCount = boneCount;
    item.SortDepth = 0.0f;
    item.ShadowSourceIndex = shadowSourceIndex;
    item.Sequence = s_nextShadowSequence++;
    item.Alpha = 255;
    item.MaterialOverride = FALSE;
    item.DistortionNormalRot.Zero();
    GetUVOffset( item.UOffset, item.VOffset, &geom, material );
}

//=============================================================================

static void GetUVOffset( u8& uOffset, u8& vOffset, geom* pGeom, material& mat )
{
    if ( mat.m_uvAnim.nFrames == 0 )
    {
        uOffset = vOffset = 0;
        return;
    }

    s32 iKey = mat.m_uvAnim.iKey + mat.m_uvAnim.iFrame;
    uOffset = pGeom->m_pUVKey[iKey].OffsetU;
    vOffset = pGeom->m_pUVKey[iKey].OffsetV;
}

//=============================================================================

static xbool IntersectsView( view const& v, bbox const& bBox )
{
    return ( v.BBoxInView( bBox ) != view::VISIBLE_NONE );
}

//=============================================================================

static void CalcVMatOffsets( s32* pVMatOffsets, geom const* pGeom, u32 vTextureMask )
{
    x_memset( pVMatOffsets, 0, sizeof( s32 ) * 32 );

    s32 i, j;
    for ( i = 0; i < pGeom->m_nVirtualTextures; i++ )
    {
        s32 offset = vTextureMask & 0xf;
        vTextureMask >>= 4;

        geom::virtual_texture& vTexture = pGeom->m_pVirtualTextures[i];
        for ( j = 0; j < pGeom->m_nMaterials; j++ )
        {
            ASSERT( j < 32 );
            if ( vTexture.MaterialMask & ( 1 << j ) )
            {
                offset = MINMAX( 0, offset, pGeom->m_pMaterial[j].nVirtualMats - 1 );
                pVMatOffsets[j] = offset;
            }
        }
    }
}

//=============================================================================

s32 render::GetHardwareBufferSize( void )
{
    return 80;
}

//=============================================================================

void render::Init( void )
{
    s_pulseTime = 0.0f;

    s_registeredGeometry.Clear();
    s_registeredGeometry.GrowListBy( kMaxRegisteredGeoms );
    s_registeredInstances.Clear();
    s_registeredInstances.GrowListBy( kMaxRegisteredInstances );
    s_registeredMaterials.Clear();
    s_registeredMaterials.GrowListBy( kMaxRegisteredMaterials );
    s_geometryDraws.Clear();
    s_geometryDraws.SetCapacity( kMaxRenderedInstances );
    s_dynamicGeometryDraws.Clear();
    s_dynamicGeometryDraws.SetCapacity( kMaxRenderedInstances );
    s_dynamicShadowDraws.Clear();
    s_dynamicShadowDraws.SetCapacity( kMaxRenderedInstances );
    s_shadowDraws.Clear();
    s_shadowDraws.SetCapacity( kMaxRenderedInstances );
    s_orderedShadowDraws.Clear();
    s_orderedShadowDraws.SetCapacity( kMaxRenderedInstances );
    s_nextGeometrySequence = 0;
    s_nextShadowSequence = 0;

    platform_Init();
}

//=============================================================================

void render::Kill( void )
{
    platform_Kill();

    ASSERT( s_registeredGeometry.GetCount() == 0 );
    ASSERT( s_registeredInstances.GetCount() == 0 );
    ASSERT( s_registeredMaterials.GetCount() == 0 );
    s_registeredGeometry.Clear();
    s_registeredInstances.Clear();
    s_registeredMaterials.Clear();
    s_geometryDraws.Clear();
    s_dynamicGeometryDraws.Clear();
    s_dynamicShadowDraws.Clear();
    s_shadowDraws.Clear();
    s_orderedShadowDraws.Clear();
}

//=============================================================================

void render::Update( f32 deltaTime )
{
    s_pulseTime += deltaTime;
    platform_UpdatePostEffects( deltaTime );

    // update all uv animations
    s_registeredMaterials.Update( deltaTime );
}

//=============================================================================

xbool render::SubmitPrimitives( primitive_draw_desc const& desc, matrix4 const& localToWorld,
                                primitive_vertex const* pVertices, s32 nVertices, u16 const* pIndices, s32 nIndices )
{
    ASSERTS( s_isPrimitiveRenderActive, "Primitive geometry submitted outside primitive session" );
    if ( !s_isPrimitiveRenderActive )
    {
        return FALSE;
    }

    return platform_SubmitPrimitives( desc, localToWorld, pVertices, nVertices, pIndices, nIndices );
}

//=============================================================================

xbool render::SubmitDecalBatch( decal_draw_desc const& desc, decal_vertex const* pVertices, s32 nVertices,
                                u16 const* pIndices, s32 nIndices )
{
    ASSERTS( s_isRenderActive, "Decal geometry submitted outside normal render session" );
    if ( !s_isRenderActive )
    {
        return FALSE;
    }

    return platform_SubmitDecalBatch( desc, s_pCurrentCubeMap, pVertices, nVertices, pIndices, nIndices );
}

//=============================================================================

void render::ReserveDecalSubmissionCapacity( s32 nVertices, s32 nIndices, s32 nDraws )
{
    platform_ReserveDecalSubmissionCapacity( nVertices, nIndices, nDraws );
}

//=============================================================================

xbool render::SetDepthRect( irect const& rect, f32 depth )
{
    ASSERTS( s_isPrimitiveRenderActive, "Depth rectangle submitted outside primitive session" );
    if ( !s_isPrimitiveRenderActive )
    {
        return FALSE;
    }

    return platform_SetDepthRect( rect, depth );
}

//=============================================================================

xbool render::BeginPrimitiveRender( void )
{
    ASSERT( !s_isRenderActive && !s_isShadowRenderActive && !s_isPrimitiveRenderActive );

    if ( !platform_BeginPrimitiveRender() )
    {
        return FALSE;
    }

    s_isPrimitiveRenderActive = TRUE;
    return TRUE;
}

//=============================================================================

void render::EndPrimitiveRender( void )
{
    ASSERT( s_isPrimitiveRenderActive );
    if ( !s_isPrimitiveRenderActive )
    {
        return;
    }

    platform_EndPrimitiveRender();
    s_isPrimitiveRenderActive = FALSE;
}

//=============================================================================

void render::ExecuteForwardRender( forward_render_stage stage )
{
    ASSERT( !s_isRenderActive && !s_isShadowRenderActive && !s_isPrimitiveRenderActive );
    platform_ExecuteForwardRender( stage );
}

//=============================================================================

render::GeometryInstanceHandle render::RegisterRigidInstance( rigid_geom& geom )
{
    ASSERT( !s_isRenderActive && !s_isShadowRenderActive && !s_isPrimitiveRenderActive );

    xhandle hGeom = FindRegisteredGeom( geom );
    if ( hGeom.IsNull() )
    {
        hGeom = RegisterRigidGeom( geom );
    }

    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );
    ASSERT( registeredGeom.m_pGeometry == &geom );
    ASSERT( registeredGeom.m_Type == GeometryType::Rigid );

    render::GeometryInstanceHandle handle = AddPrivateInstance( hGeom, GeometryType::Rigid );

    return handle;
}

//=============================================================================

void render::UnregisterRigidInstance( GeometryInstanceHandle hInst )
{
    ASSERT( !s_isRenderActive && !s_isShadowRenderActive && !s_isPrimitiveRenderActive );

    PrivateInstance& inst = s_registeredInstances( hInst );
    ASSERT( inst.m_Type == GeometryType::Rigid );
    xhandle const    hGeom = inst.m_geometryHandle;
    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );
    xbool const      bUnregisterGeom = ( registeredGeom.m_referenceCount == 1 );

    RemovePrivateInstance( hInst );

    if ( bUnregisterGeom )
    {
        UnregisterRigidGeom( hGeom );
    }
}

//=============================================================================

render::GeometryInstanceHandle render::RegisterSkinInstance( skin_geom& geom )
{
    ASSERT( !s_isRenderActive && !s_isShadowRenderActive && !s_isPrimitiveRenderActive );

    xhandle hGeom = FindRegisteredGeom( geom );
    if ( hGeom.IsNull() )
    {
        hGeom = RegisterSkinGeom( geom );
    }

    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );
    ASSERT( registeredGeom.m_pGeometry == &geom );
    ASSERT( registeredGeom.m_Type == GeometryType::Skin );

    render::GeometryInstanceHandle handle = AddPrivateInstance( hGeom, GeometryType::Skin );

    return handle;
}

//=============================================================================

void render::UnregisterSkinInstance( GeometryInstanceHandle hInst )
{
    ASSERT( !s_isRenderActive && !s_isShadowRenderActive && !s_isPrimitiveRenderActive );

    PrivateInstance& inst = s_registeredInstances( hInst );
    ASSERT( inst.m_Type == GeometryType::Skin );
    xhandle const    hGeom = inst.m_geometryHandle;
    PrivateGeometry& registeredGeom = s_registeredGeometry( hGeom );
    xbool const      bUnregisterGeom = ( registeredGeom.m_referenceCount == 1 );

    RemovePrivateInstance( hInst );

    if ( bUnregisterGeom )
    {
        UnregisterSkinGeom( hGeom );
    }
}

//=============================================================================

geom const* render::GetGeom( GeometryInstanceHandle hInst )
{
    if ( hInst.IsNull() )
    {
        return NULL;
    }

    PrivateInstance& inst = s_registeredInstances( hInst );
    return inst.m_pGeometry;
}

//=============================================================================

void render::SetCustomFogPalette( texture::handle const& texture, xbool immediateSwitch, s32 paletteIndex )
{
    platform_SetCustomFogPalette( texture, immediateSwitch, paletteIndex );
}

//=============================================================================

xcolor render::GetFogValue( vector3 const& worldPos, s32 paletteIndex )
{
    return platform_GetFogValue( worldPos, paletteIndex );
}

//=============================================================================

void render::BeginNormalRender( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "render::Begin" );

    // sort the materials
    s_registeredMaterials.Sort();

    // safety check
    ASSERT( eng_InBeginEnd() );
    ASSERT( !s_isRenderActive && !s_isShadowRenderActive );
    s_isRenderActive = TRUE;

    ASSERT( s_geometryDraws.GetCount() == 0 );
    s_geometryDraws.SetCount( 0 );
    ASSERT( s_dynamicGeometryDraws.GetCount() == 0 );
    s_dynamicGeometryDraws.SetCount( 0 );
    s_dynamicShadowDraws.SetCount( 0 );
    s_nextGeometrySequence = 0;

    platform_BeginNormalRender();
}

//=============================================================================

void render::EndNormalRender( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "render::End" );

    // safety check
    ASSERT( eng_InBeginEnd() );
    ASSERT( s_isRenderActive );
    s_isRenderActive = FALSE;

    static xprofile_counter geometryDraws = x_GetProfiler().RegisterCounter( "RenderInstances", "RenderCounter" );
    geometryDraws.Add( s_geometryDraws.GetCount() );

    {
        X_PROFILE_SCOPE_CATEGORY( "RenderSection", "Render/SubmitGeometry" );
        platform_SubmitGeometry( s_geometryDraws, s_dynamicGeometryDraws, s_pCurrentCubeMap );
    }

    s_geometryDraws.SetCount( 0 );
    s_dynamicGeometryDraws.SetCount( 0 );
    platform_EndNormalRender();
}

//=============================================================================

void render::ResetAfterException( void )
{
    platform_ResetAfterException();
    s_isRenderActive = FALSE;
    s_isShadowRenderActive = FALSE;
    s_isPrimitiveRenderActive = FALSE;
    s_geometryDraws.SetCount( 0 );
    s_dynamicGeometryDraws.SetCount( 0 );
    s_shadowDraws.SetCount( 0 );
    s_orderedShadowDraws.SetCount( 0 );
    s_dynamicShadowDraws.SetCount( 0 );
    s_nextGeometrySequence = 0;
    s_nextShadowSequence = 0;
}

//=============================================================================

void render::AddDynamicGeometry( dynamic_geometry_draw const& draw )
{
    ASSERT( s_isRenderActive );
    ASSERT( draw.pVertices );
    ASSERT( draw.pIndices );
    ASSERT( draw.pDiffuseTexture );
    ASSERT( draw.pDamageMask );
    ASSERT( draw.pDamageTexture );
    ASSERT( draw.pDamageUploadPending );
    ASSERT( ( draw.VertexCount > 0 ) && ( draw.VertexCount <= 65535 ) );
    ASSERT( ( draw.IndexCount > 0 ) && ( ( draw.IndexCount % 3 ) == 0 ) );

    if( !s_isRenderActive || !draw.pVertices || !draw.pIndices || !draw.pDiffuseTexture || !draw.pDamageMask ||
        !draw.pDamageTexture || !draw.pDamageUploadPending ||
        ( draw.VertexCount <= 0 ) || ( draw.VertexCount > 65535 ) || ( draw.IndexCount <= 0 ) ||
        ( ( draw.IndexCount % 3 ) != 0 ) || ( s_dynamicGeometryDraws.GetCount() >= kMaxRenderedInstances ) )
    {
        return;
    }

    dynamic_geometry_draw& queued = s_dynamicGeometryDraws.Append();
    queued = draw;
    matrix4 Identity;
    Identity.Identity();
    queued.pLighting = platform_CalculateRigidLighting( Identity, draw.Bounds );
    if( queued.pLighting )
        queued.Flags |= INSTFLAG_DYNAMICLIGHT;

    queued.Flags |= g_ProjTextureMgr.CollectProjectionFlags( queued.Flags, draw.Bounds );
    if( s_isLightFilteringEnabled && ( ( queued.Flags & DISABLE_FILTERLIGHT ) == 0 ) )
        queued.Flags |= INSTFLAG_FILTERLIGHT;

    queued.Sequence = s_nextGeometrySequence++;
}

//=============================================================================

void render::AddDynamicShadowCaster( dynamic_geometry_shadow_draw const& draw )
{
    ASSERT( s_isShadowRenderActive );
    if( !s_isShadowRenderActive || !draw.pVertices || !draw.pIndices || !draw.pDiffuse || !draw.pDamageMask ||
        !draw.pDamageTexture || !draw.pDamageUploadPending ||
        ( draw.VertexCount <= 0 ) || ( draw.VertexCount > 65535 ) || ( draw.IndexCount <= 0 ) ||
        ( ( draw.IndexCount % 3 ) != 0 ) || ( draw.ShadowSourceMask == 0 ) ||
        ( s_dynamicShadowDraws.GetCount() >= kMaxRenderedInstances ) )
        return;

    s_dynamicShadowDraws.Append() = draw;
}

//=============================================================================

void render::AddRigidInstanceSimple( GeometryInstanceHandle hInst, u32 const* pCol, matrix4 const* pL2W,
                                     bbox const& worldBBox, u32 flags )
{
#ifndef X_RETAIL
    if ( g_renderDebug.RenderSkinOnly )
    {
        return;
    }
#endif

    // safety check
    ASSERT( s_isRenderActive );
    ASSERT( pL2W->IsValid() );

    // grab the useful pointers out
    PrivateInstance& registeredInst = s_registeredInstances( hInst );
    ASSERT( registeredInst.m_Type == GeometryType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( registeredInst.m_pGeometry );

    // calculate lighting
    void* pLighting = platform_CalculateRigidLighting( *pL2W, worldBBox );
    if ( pLighting )
    {
        flags |= INSTFLAG_DYNAMICLIGHT;
    }

    // collect texture projections
    u32 projFlags = g_ProjTextureMgr.CollectProjectionFlags( flags, worldBBox );

    // Use filter light?
    if ( ( s_isLightFilteringEnabled ) && ( ( flags & DISABLE_FILTERLIGHT ) == 0 ) )
    {
        flags |= INSTFLAG_FILTERLIGHT;
    }

    // add each of the submeshes to the render list
    for ( s32 iMesh = 0; iMesh < pGeom->m_nMeshes; iMesh++ )
    {
        geom::mesh& mesh = pGeom->m_pMesh[iMesh];
        for ( s32 iSubMesh = mesh.iSubMesh; iSubMesh < mesh.iSubMesh + mesh.nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  subMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& geometryMaterial = pGeom->m_pMaterial[subMesh.iMaterial];
            xhandle hMat = GetRegisteredMaterialHandle( registeredInst.m_geometryHandle, geometryMaterial.iVirtualMat );
            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            AppendRigidGeometryDraw( registeredInst, *pGeom, hMat, iSubMesh, pL2W, pCol, pLighting, flags, projFlags,
                                     255, FALSE );
        }
    }
}

//=============================================================================

void render::AddRigidInstance( GeometryInstanceHandle hInst, u32 const* pCol, matrix4 const* pL2W, u64 mask, u32 flags,
                               s32 alpha )
{
#ifndef X_RETAIL
    if ( g_renderDebug.RenderSkinOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "render::AddRigidInstance" );

    // safety check
    ASSERT( s_isRenderActive );
    ASSERT( pL2W->IsValid() );

    // grab the useful pointers out
    PrivateInstance& registeredInst = s_registeredInstances( hInst );
    ASSERT( registeredInst.m_Type == GeometryType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( registeredInst.m_pGeometry );

    // calculate lighting
    bbox worldBBox( pGeom->m_BBox );
    worldBBox.Transform( *pL2W );
    void* pLighting = platform_CalculateRigidLighting( *pL2W, worldBBox );
    if ( pLighting )
    {
        flags |= INSTFLAG_DYNAMICLIGHT;
    }

    // collect texture projections
    u32 projFlags = g_ProjTextureMgr.CollectProjectionFlags( flags, worldBBox );

    // Use filter light?
    if ( ( s_isLightFilteringEnabled ) && ( ( flags & DISABLE_FILTERLIGHT ) == 0 ) )
    {
        flags |= INSTFLAG_FILTERLIGHT;
    }

    xbool const bFadingAlpha = ( ( flags & render::FADING_ALPHA ) != 0 ) || ( alpha != 255 );
    if ( bFadingAlpha )
    {
        flags |= INSTFLAG_FADING_ALPHA;
    }

    // add the meshes and submeshes to the render list
    s32         iMesh = 0;
    geom::mesh* pMesh = pGeom->m_pMesh;
    geom::mesh* pEndMesh = pMesh + pGeom->m_nMeshes;
    while ( pMesh < pEndMesh )
    {
        // skip this mesh?
        if ( ( mask & 1 ) == 0 )
        {
            pMesh++;
            iMesh++;
            mask >>= 1;
            continue;
        }

        // add each of the submeshes to the render list
        for ( s32 iSubMesh = pMesh->iSubMesh; iSubMesh < pMesh->iSubMesh + pMesh->nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  subMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& geomMaterial = pGeom->m_pMaterial[subMesh.iMaterial];

            // get the material handle info
            xhandle hMat = GetRegisteredMaterialHandle( registeredInst.m_geometryHandle, geomMaterial.iVirtualMat );

            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            s32      iBone = pGeom->m_pSection[subMesh.iSection].iBone;
            matrix4* pMat = reinterpret_cast<matrix4*>( smem_BufferAlloc( sizeof( matrix4 ) ) );
            *pMat = pL2W[iBone];
            ASSERT( pMat->IsValid() );

            AppendRigidGeometryDraw( registeredInst, *pGeom, hMat, iSubMesh, pMat, pCol, pLighting, flags, projFlags,
                                     static_cast<u8>( alpha ), bFadingAlpha );
        }
        // next mesh
        iMesh++;
        pMesh++;
        mask >>= 1;
    }
}

//=============================================================================

void render::AddRigidInstance( GeometryInstanceHandle hInst, u32 const* pCol, matrix4 const* pL2W, u64 mask,
                               u32 vTextureMask, u32 flags, s32 alpha )
{
#ifndef X_RETAIL
    if ( g_renderDebug.RenderSkinOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "render::AddRigidInstance" );

    // safety check
    ASSERT( s_isRenderActive );
    ASSERT( pL2W->IsValid() );

    // grab the useful pointers out
    PrivateInstance& registeredInst = s_registeredInstances( hInst );
    ASSERT( registeredInst.m_Type == GeometryType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( registeredInst.m_pGeometry );

    // calculate lighting
    bbox worldBBox( pGeom->m_BBox );
    worldBBox.Transform( *pL2W );
    void* pLighting = platform_CalculateRigidLighting( *pL2W, worldBBox );
    if ( pLighting )
    {
        flags |= INSTFLAG_DYNAMICLIGHT;
    }

    // collect texture projections
    u32 projFlags = g_ProjTextureMgr.CollectProjectionFlags( flags, worldBBox );

    // Use filter light?
    if ( ( s_isLightFilteringEnabled ) && ( ( flags & DISABLE_FILTERLIGHT ) == 0 ) )
    {
        flags |= INSTFLAG_FILTERLIGHT;
    }

    xbool const bFadingAlpha = ( ( flags & render::FADING_ALPHA ) != 0 ) || ( alpha != 255 );
    if ( bFadingAlpha )
    {
        flags |= INSTFLAG_FADING_ALPHA;
    }

    // calculate the virtual mesh offsets
    s32 vMatOffsets[32];
    CalcVMatOffsets( vMatOffsets, pGeom, vTextureMask );

    // add the meshes and submeshes to the render list
    s32         iMesh = 0;
    geom::mesh* pMesh = pGeom->m_pMesh;
    geom::mesh* pEndMesh = pMesh + pGeom->m_nMeshes;
    while ( pMesh < pEndMesh )
    {
        // skip this mesh?
        if ( ( mask & 1 ) == 0 )
        {
            pMesh++;
            iMesh++;
            mask >>= 1;
            continue;
        }

        // add each of the submeshes to the render list
        for ( s32 iSubMesh = pMesh->iSubMesh; iSubMesh < pMesh->iSubMesh + pMesh->nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  subMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& geometryMaterial = pGeom->m_pMaterial[subMesh.iMaterial];

            // get the material handle info
            xhandle hMat = GetRegisteredMaterialHandle( registeredInst.m_geometryHandle,
                                                        geometryMaterial.iVirtualMat + vMatOffsets[subMesh.iMaterial] );

            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            s32      iBone = pGeom->m_pSection[subMesh.iSection].iBone;
            matrix4* pMat = reinterpret_cast<matrix4*>( smem_BufferAlloc( sizeof( matrix4 ) ) );
            *pMat = pL2W[iBone];
            ASSERT( pMat->IsValid() );

            AppendRigidGeometryDraw( registeredInst, *pGeom, hMat, iSubMesh, pMat, pCol, pLighting, flags, projFlags,
                                     static_cast<u8>( alpha ), bFadingAlpha );
        }
        // next mesh
        iMesh++;
        pMesh++;
        mask >>= 1;
    }
}

//=============================================================================

void render::AddSkinInstance( GeometryInstanceHandle hInst, matrix4 const* pBone, u64 mask, u32 vTextureMask, u32 flags,
                              xcolor const& ambient )
{
#ifndef X_RETAIL
    if ( g_renderDebug.RenderRigidOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "render::AddSkinInstance" );

    // safety check
    ASSERT( s_isRenderActive );

    // grab the useful pointers out
    PrivateInstance& registeredInst = s_registeredInstances( hInst );
    ASSERT( registeredInst.m_Type == GeometryType::Skin );
    skin_geom* pGeom = static_cast<skin_geom*>( registeredInst.m_pGeometry );

    // calculate lighting
    void* pLighting = platform_CalculateSkinLighting( flags, pBone[0], pGeom->m_BBox, ambient );
    flags |= INSTFLAG_DYNAMICLIGHT;
    xbool const bGlowing = ( flags & render::GLOWING ) != 0;
    xbool const bFadingAlpha = ( ( flags & render::FADING_ALPHA ) != 0 ) || ( !bGlowing && ( ambient.A != 255 ) );
    if ( bFadingAlpha )
    {
        flags |= INSTFLAG_FADING_ALPHA;
    }

    // collect texture projections
    bbox worldBBox( pGeom->m_BBox );
    worldBBox.Transform( pBone[0] );
    u32 projFlags = g_ProjTextureMgr.CollectProjectionFlags( flags, worldBBox );

    // calculate the virtual mesh offsets
    s32 vMatOffsets[32];
    CalcVMatOffsets( vMatOffsets, pGeom, vTextureMask );

    // add the meshes and submeshes to the render list
    for ( s32 iMesh = 0; iMesh < pGeom->m_nMeshes; iMesh++ )
    {
        // skip this mesh?
        if ( ( mask & ( u64{ 1 } << iMesh ) ) == 0 )
        {
            continue;
        }

        // add each of the submeshes to the render list
        geom::mesh& mesh = pGeom->m_pMesh[iMesh];
        for ( s32 iSubMesh = mesh.iSubMesh; iSubMesh < mesh.iSubMesh + mesh.nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  subMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& geometryMaterial = pGeom->m_pMaterial[subMesh.iMaterial];

            // get the material handle info
            xhandle hMat = GetRegisteredMaterialHandle( registeredInst.m_geometryHandle,
                                                        geometryMaterial.iVirtualMat + vMatOffsets[subMesh.iMaterial] );

            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            AppendSkinGeometryDraw( registeredInst, *pGeom, hMat, iSubMesh, pBone, pLighting, flags, projFlags,
                                    ambient.A, bFadingAlpha );
        }
    }
}

//=============================================================================

void render::AddSkinInstanceDistorted( GeometryInstanceHandle hInst, matrix4 const* pBone, u64 mask, u32 flags,
                                       radian3 const& normalRot, xcolor ambient )
{
#ifndef X_RETAIL
    if ( g_renderDebug.RenderRigidOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "render::AddSkinInstance" );

    // safety check
    ASSERT( s_isRenderActive );

    // grab the useful pointers out
    PrivateInstance& registeredInst = s_registeredInstances( hInst );
    ASSERT( registeredInst.m_Type == GeometryType::Skin );
    skin_geom* pGeom = static_cast<skin_geom*>( registeredInst.m_pGeometry );

    // calculate lighting
    // TODO: Ignore dynamic lights for "cloaked" objects
    void* pLighting = platform_CalculateSkinLighting( flags, pBone[0], pGeom->m_BBox, ambient );
    flags |= INSTFLAG_DYNAMICLIGHT;

    // add the meshes and submeshes to the render list
    for ( s32 iMesh = 0; iMesh < pGeom->m_nMeshes; iMesh++ )
    {
        // skip this mesh?
        if ( ( mask & ( u64{ 1 } << iMesh ) ) == 0 )
        {
            continue;
        }

        // add each of the submeshes to the render list
        geom::mesh& mesh = pGeom->m_pMesh[iMesh];
        for ( s32 iSubMesh = mesh.iSubMesh; iSubMesh < mesh.iSubMesh + mesh.nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  subMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& geometryMaterial = pGeom->m_pMaterial[subMesh.iMaterial];

            // get the material handle info
            xhandle hMat = GetRegisteredMaterialHandle( registeredInst.m_geometryHandle, geometryMaterial.iVirtualMat );

            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            if ( !ShouldSubmitGeometry( flags ) )
            {
                continue;
            }

            f32 sortDepth;
            if ( !ComputeGeometryDepth( sortDepth, pGeom->m_BBox, pBone ) )
            {
                continue;
            }

            ASSERT( s_geometryDraws.GetCount() < kMaxRenderedInstances );
            if ( s_geometryDraws.GetCount() >= kMaxRenderedInstances )
            {
                continue;
            }

            PrivateGeometry& registeredGeom = s_registeredGeometry( registeredInst.m_geometryHandle );
            material&        renderMaterial = s_registeredMaterials( hMat );

            geometry_draw_item& item = s_geometryDraws.Append();
            item.Pass = GEOMETRY_PASS_DISTORTION;
            item.Type = GEOMETRY_DRAW_SKIN;
            item.hRenderGeom = registeredGeom.m_renderGeometryHandle;
            item.pMaterial = renderMaterial.IsDistortion() ? &renderMaterial : NULL;
            item.MaterialOrder = renderMaterial.IsDistortion() ? s_registeredMaterials.GetIndexByHandle( hMat ) : 0;
            item.iSurface = iSubMesh;
            item.Flags = flags;
            item.pLighting = pLighting;
            item.Data.Skin.pGeom = pGeom;
            item.Data.Skin.pBones = pBone;
            item.Data.Skin.BoneCount = 0;
            item.DistortionNormalRot = normalRot;
            item.SortDepth = sortDepth;
            item.Sequence = s_nextGeometrySequence++;
            item.ShadowSourceIndex = -1;
            item.Alpha = ambient.A;
            item.MaterialOverride = FALSE;
            GetUVOffset( item.UOffset, item.VOffset, pGeom, renderMaterial );
        }
    }
}

//=============================================================================

void render::BeginMidPostEffects( void ) // Deprecated ?
{
    platform_BeginPostEffects();
}

//=============================================================================

void render::EndMidPostEffects( void ) // Deprecated ?
{
    platform_EndPostEffects();
}

//=============================================================================

void render::BeginPostEffects( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "render::BeginPostEffects" );
    platform_BeginPostEffects();
}

//=============================================================================

void render::ApplySelfIllumGlows( f32 motionBlurIntensity, s32 glowCutoff )
{
    platform_ApplySelfIllumGlows( motionBlurIntensity, glowCutoff );
}

//=============================================================================

void render::ZFogFilter( render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2 )
{
    platform_ZFogFilter( fn, color, param1, param2 );
}

//=============================================================================

void render::ZFogFilter( render::post_falloff_fn fn, s32 paletteIndex )
{
    platform_ZFogFilter( fn, paletteIndex );
}

//=============================================================================

void render::AddScreenWarp( vector3 const& worldPos, f32 radius, f32 warpAmount )
{
    platform_AddScreenWarp( worldPos, radius, warpAmount );
}

//=============================================================================

void render::MotionBlur( f32 intensity )
{
    platform_MotionBlur( intensity );
}

//=============================================================================

void render::MipFilter( s32 nFilters, f32 offset, render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2,
                        s32 paletteIndex )
{
    platform_MipFilter( nFilters, offset, fn, color, param1, param2, paletteIndex );
}

//=============================================================================

void render::MipFilter( s32 nFilters, f32 offset, render::post_falloff_fn fn, texture::handle const& texture,
                        s32 paletteIndex )
{
    platform_MipFilter( nFilters, offset, fn, texture, paletteIndex );
}

//=============================================================================

void render::MultScreen( xcolor multColor, post_screen_blend finalBlend )
{
    platform_MultScreen( multColor, finalBlend );
}

//=============================================================================

void render::RadialBlur( f32 zoom, radian angle, f32 alphaSub, f32 alphaScale )
{
    platform_RadialBlur( zoom, angle, alphaSub, alphaScale );
}

//=============================================================================

void render::NoiseFilter( xcolor color )
{
    platform_NoiseFilter( color );
}

//=============================================================================

void render::ScreenFade( xcolor color )
{
    platform_ScreenFade( color );
}

//=============================================================================

void render::EndPostEffects( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "render::EndPostEffects" );
    platform_EndPostEffects();
}

//=============================================================================

material& render::GetMaterial( GeometryInstanceHandle hInst, s32 iSubMesh )
{
    // grab the useful pointers out
    PrivateInstance& registeredInst = s_registeredInstances( hInst );

    geom* pGeom = registeredInst.m_pGeometry;
    ASSERT( pGeom );

    // get the internal registered material from the geometry material
    geom::submesh&  subMesh = pGeom->m_pSubMesh[iSubMesh];
    geom::material& geometryMaterial = pGeom->m_pMaterial[subMesh.iMaterial];
    xhandle         hMat = GetRegisteredMaterialHandle( registeredInst.m_geometryHandle, geometryMaterial.iVirtualMat );
    ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

    return s_registeredMaterials( hMat );
}

//=============================================================================

texture* render::GetVTexture( geom const* pGeom, s32 iMaterial, s32 vTextureMask )
{
    // assume no offset to start with
    s32 vMatOffset = 0;

    // find any virtual textures that might affect this material, and if so
    // the material offset will come directly from the mask
    s32 i;
    for ( i = 0; i < pGeom->m_nVirtualTextures; i++ )
    {
        // grab the associated bits for this vtexture from the texture mask
        s32 offset = vTextureMask & 0xf;
        vTextureMask >>= 4;

        // does this virtual texture affect this material?
        geom::virtual_texture const& vTexture = pGeom->m_pVirtualTextures[i];
        if ( vTexture.MaterialMask & ( 1 << iMaterial ) )
        {
            vMatOffset = offset;
            break;
        }
    }

    // now, using the offset, get the texture (requires a good bit of
    // redirection, but all comes out in the end)
    xhandle hGeom = FindRegisteredGeom( *pGeom );
    ASSERT( !hGeom.IsNull() );

    geom::material const& geomMat = pGeom->m_pMaterial[iMaterial];
    xhandle               hMat = GetRegisteredMaterialHandle( hGeom, geomMat.iVirtualMat + vMatOffset );
    material&             mat = s_registeredMaterials( hMat );

    return mat.m_diffuseMap.GetPointer();
}

//=============================================================================

void render::SetAreaCubeMap( cubemap::handle const& cubeMap )
{
    s_pCurrentCubeMap = cubeMap.GetPointer();
}

//=============================================================================

void render::EnableFilterLight( xbool bEnable )
{
    s_isLightFilteringEnabled = bEnable;
}

//=============================================================================

xbool render::IsFilterLightEnabled( void )
{
    return s_isLightFilteringEnabled;
}

//=============================================================================

void render::SetFilterLightColor( xcolor color )
{
    s_filterLightColor = color;
}

//=============================================================================

xcolor render::GetFilterLightColor( void )
{
    return s_filterLightColor;
}

//=============================================================================

void render::BeginShadowCreation( void )
{
    ASSERT( !s_isRenderActive && !s_isShadowRenderActive && !s_isPrimitiveRenderActive );

    s_isShadowRenderActive = TRUE;

    s_shadowDraws.SetCount( 0 );
    s_dynamicShadowDraws.SetCount( 0 );
    s_orderedShadowDraws.SetCount( 0 );
    s_nextShadowSequence = 0;

    // clear out any current shadow-map sources
    platform_ClearShadowSourceList();
    s_shadowSourceCount = 0;
}

//=============================================================================

void render::EndShadowCreation( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "render::EndShadowCreation" );

    // safety check
    ASSERT( eng_InBeginEnd() );
    ASSERT( s_isShadowRenderActive );
    s_isShadowRenderActive = FALSE;

    static xprofile_counter shadowCasterDraws = x_GetProfiler().RegisterCounter( "ShadowCasterDraws", "RenderCounter" );
    static xprofile_counter shadowSources = x_GetProfiler().RegisterCounter( "ShadowSources", "RenderCounter" );
    shadowCasterDraws.Add( s_shadowDraws.GetCount() + s_dynamicShadowDraws.GetCount() );
    shadowSources.Add( s_shadowSourceCount );

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/FinalizeSources" );
        platform_FinalizeShadowSourceList();
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/Prepare" );
        platform_BeginShadowShaders();
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/SortCasters" );
        for ( s32 i = 0; i < s_shadowDraws.GetCount(); ++i )
        {
            s_orderedShadowDraws.Append() = &s_shadowDraws[i];
        }

        if ( s_orderedShadowDraws.GetCount() > 1 )
        {
            x_qsort( s_orderedShadowDraws.GetPtr(), s_orderedShadowDraws.GetCount(),
                     sizeof( geometry_draw_item const* ), ShadowCompareFn );
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/RenderCasters" );
        platform_RenderShadowCasters( s_orderedShadowDraws, s_dynamicShadowDraws );
    }
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/EndShaders" );
        platform_EndShadowShaders();
    }
    s_orderedShadowDraws.SetCount( 0 );
    s_shadowDraws.SetCount( 0 );
    s_dynamicShadowDraws.SetCount( 0 );
}

//=============================================================================

void render::AddPointShadowMapSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff,
                                      s32 shadowMapResolution, s32 shadowPriority, f32 shadowScore,
                                      s32 dynamicLightIndex )
{
    ASSERT( s_isShadowRenderActive );
    platform_AddPointShadowMapSource( l2W, fov, lightRadius, lightFalloff, shadowMapResolution, shadowPriority,
                                      shadowScore, dynamicLightIndex );
    s_shadowSourceCount++;
}

//=============================================================================

void render::AddSpotShadowMapSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff,
                                     s32 shadowMapResolution, s32 shadowPriority, f32 shadowScore,
                                     s32 dynamicLightIndex )
{
    ASSERT( s_isShadowRenderActive );
    platform_AddSpotShadowMapSource( l2W, fov, lightRadius, lightFalloff, shadowMapResolution, shadowPriority,
                                     shadowScore, dynamicLightIndex );
    s_shadowSourceCount++;
}

//=============================================================================

void render::AddRigidCasterSimple( render::GeometryInstanceHandle hInst,
                                   matrix4 const*                 pL2W, // will be DMA ref'd to!
                                   u64                            shadowSourceMask )
{
#ifndef X_RETAIL
    if ( g_renderDebug.RenderSkinOnly )
    {
        return;
    }
#endif

    ASSERT( s_isShadowRenderActive );
    ASSERT( pL2W );
    ASSERT( pL2W->IsValid() );

    PrivateInstance& registeredInst = s_registeredInstances( hInst );
    ASSERT( registeredInst.m_Type == GeometryType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( registeredInst.m_pGeometry );

    for ( s32 iShadowSource = 0; iShadowSource < s_shadowSourceCount; iShadowSource++ )
    {
        if ( ( shadowSourceMask & ( u64{ 1 } << iShadowSource ) ) == 0 )
        {
            continue;
        }

        for ( s32 iSubMesh = 0; iSubMesh < pGeom->m_nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  subMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& geometryMaterial = pGeom->m_pMaterial[subMesh.iMaterial];
            xhandle hMat = GetRegisteredMaterialHandle( registeredInst.m_geometryHandle, geometryMaterial.iVirtualMat );
            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );
            if ( s_registeredMaterials( hMat ).IsAlpha() )
            {
                continue;
            }

            AppendRigidShadowDraw( registeredInst, *pGeom, hMat, iSubMesh, pL2W, iShadowSource );
        }
    }
}

//=============================================================================

void render::AddRigidCaster( render::GeometryInstanceHandle hInst, matrix4 const* pL2W, u64 mask, u64 shadowSourceMask )
{
#ifndef X_RETAIL
    if ( g_renderDebug.RenderSkinOnly )
    {
        return;
    }
#endif

    ASSERT( s_isShadowRenderActive );
    ASSERT( pL2W );
    ASSERT( pL2W->IsValid() );

    PrivateInstance& registeredInst = s_registeredInstances( hInst );
    ASSERT( registeredInst.m_Type == GeometryType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( registeredInst.m_pGeometry );

    geom::mesh* pMesh = pGeom->m_pMesh;
    geom::mesh* pEndMesh = pMesh + pGeom->m_nMeshes;
    while ( pMesh < pEndMesh )
    {
        if ( ( mask & 1 ) == 0 )
        {
            pMesh++;
            mask >>= 1;
            continue;
        }

        for ( s32 iSubMesh = pMesh->iSubMesh; iSubMesh < pMesh->iSubMesh + pMesh->nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  subMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& geometryMaterial = pGeom->m_pMaterial[subMesh.iMaterial];
            xhandle hMat = GetRegisteredMaterialHandle( registeredInst.m_geometryHandle, geometryMaterial.iVirtualMat );
            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );
            if ( s_registeredMaterials( hMat ).IsAlpha() )
            {
                continue;
            }

            ASSERT( ( registeredInst.m_geometryHandle >= 0 ) &&
                    ( registeredInst.m_geometryHandle < kMaxRegisteredGeoms ) );
            ASSERT( ( iSubMesh >= 0 ) && ( iSubMesh < 256 ) );

            s32 iBone = pGeom->m_pSection[subMesh.iSection].iBone;

            matrix4* pMat = reinterpret_cast<matrix4*>( smem_BufferAlloc( sizeof( matrix4 ) ) );
            {
                *pMat = *( pL2W + iBone );
                ASSERT( pMat->IsValid() );
            }

            for ( s32 iShadowSource = 0; iShadowSource < s_shadowSourceCount; iShadowSource++ )
            {
                if ( ( shadowSourceMask & ( u64{ 1 } << iShadowSource ) ) == 0 )
                {
                    continue;
                }

                AppendRigidShadowDraw( registeredInst, *pGeom, hMat, iSubMesh, pMat, iShadowSource );
            }
        }

        pMesh++;
        mask >>= 1;
    }
}

//=============================================================================

void render::AddSkinCaster( render::GeometryInstanceHandle hInst, matrix4 const* pBone, s32 nBone, u64 mask,
                            u64 shadowSourceMask )
{
#ifndef X_RETAIL
    if ( g_renderDebug.RenderRigidOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "render::AddSkinCaster" );

    // safety check
    ASSERT( s_isShadowRenderActive );

    // grab the useful pointers out
    PrivateInstance& registeredInst = s_registeredInstances( hInst );
    ASSERT( registeredInst.m_Type == GeometryType::Skin );
    skin_geom* pGeom = static_cast<skin_geom*>( registeredInst.m_pGeometry );
    ASSERT( pBone );
    ASSERT( nBone > 0 );
    if ( !pBone || ( nBone <= 0 ) )
    {
        return;
    }

    // for each shadow source, add the meshes and submeshes to the render list
    for ( s32 iShadowSource = 0; iShadowSource < s_shadowSourceCount; iShadowSource++ )
    {
        if ( ( shadowSourceMask & ( u64{ 1 } << iShadowSource ) ) == 0 )
        {
            continue;
        }

        for ( s32 iMesh = 0; iMesh < pGeom->m_nMeshes; iMesh++ )
        {
            // skip this mesh?
            if ( ( mask & ( u64{ 1 } << iMesh ) ) == 0 )
            {
                continue;
            }

            // add each of the submeshes to the render list
            geom::mesh& mesh = pGeom->m_pMesh[iMesh];
            for ( s32 iSubMesh = mesh.iSubMesh; iSubMesh < mesh.iSubMesh + mesh.nSubMeshes; iSubMesh++ )
            {
                // range safety check for the sort key
                ASSERT( ( registeredInst.m_geometryHandle >= 0 ) &&
                        ( registeredInst.m_geometryHandle < kMaxRegisteredGeoms ) );
                ASSERT( ( iSubMesh >= 0 ) && ( iSubMesh < 256 ) );

                // don't let alpha cast shadows
                geom::submesh&  subMesh = pGeom->m_pSubMesh[iSubMesh];
                geom::material& geometryMaterial = pGeom->m_pMaterial[subMesh.iMaterial];
                xhandle         hMat =
                    GetRegisteredMaterialHandle( registeredInst.m_geometryHandle, geometryMaterial.iVirtualMat );
                ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );
                if ( s_registeredMaterials( hMat ).IsAlpha() )
                {
                    continue;
                }

                AppendSkinShadowDraw( registeredInst, *pGeom, hMat, iSubMesh, pBone, nBone, iShadowSource );
            }
        }
    }
}

//=============================================================================

void render::BeginSession( u32 nPlayers )
{
    platform_BeginSession( nPlayers );
}

//=============================================================================

void render::EndSession( void )
{
    platform_EndSession();
}
