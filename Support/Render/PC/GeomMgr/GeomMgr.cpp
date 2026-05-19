//==============================================================================
//
//  GeomMgr.cpp
//
//  Geom Manager for PC platform
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GeomMgr.hpp"
#include "../../LightMgr.hpp"

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

geom_mgr g_GeomMgr;

namespace
{
    s32 FindLightCookieFaceSlot( const s32* pSlots, s32 nSlots, s32 SourceSlot )
    {
        for( s32 i = 0; i < nSlots; i++ )
        {
            if( pSlots[i] == SourceSlot )
                return i;
        }

        return -1;
    }

    xbool AddLightCookieFaceSlot( s32* pSlots, s32& nSlots, s32 SourceSlot )
    {
        if( SourceSlot <= 0 )
            return TRUE;

        if( FindLightCookieFaceSlot( pSlots, nSlots, SourceSlot ) >= 0 )
            return TRUE;

        if( nSlots >= MAX_GEOM_LIGHTS )
            return FALSE;

        pSlots[nSlots++] = SourceSlot;
        return TRUE;
    }

    xbool AddLightingCookieFaces( const cb_geom_lighting* pLighting, s32* pSlots, s32& nSlots )
    {
        if( !pLighting )
            return TRUE;

        const s32 nLights = MIN( pLighting->LightCount, MAX_GEOM_LIGHTS );
        for( s32 i = 0; i < nLights; i++ )
        {
            const s32 SourceSlot = (s32)pLighting->LightCookieU[i].GetW();
            if( !AddLightCookieFaceSlot( pSlots, nSlots, SourceSlot ) )
                return FALSE;
        }

        return TRUE;
    }

    template<class T_INSTANCE>
    xbool AddInstanceCookieFaces( const T_INSTANCE* pInstances, s32 nInstances, s32* pSlots, s32& nSlots )
    {
        if( !pInstances )
            return TRUE;

        for( s32 iInstance = 0; iInstance < nInstances; iInstance++ )
        {
            const T_INSTANCE& Instance = pInstances[iInstance];
            const s32 nLights = MIN( Instance.LightCount, MAX_GEOM_LIGHTS );
            for( s32 iLight = 0; iLight < nLights; iLight++ )
            {
                const s32 SourceSlot = (s32)Instance.LightCookieU[iLight].GetW();
                if( !AddLightCookieFaceSlot( pSlots, nSlots, SourceSlot ) )
                    return FALSE;
            }
        }

        return TRUE;
    }

    ID3D11ShaderResourceView* GetLightCookieFaceSRV( s32 SourceSlot )
    {
        const xbitmap* pBitmap = g_LightMgr.GetLightCookieFaceBitmap( SourceSlot - 1 );
        if( !pBitmap )
            return NULL;

        const s32 vramID = pBitmap->GetVRAMID();
        if( vramID <= 0 )
        {
            x_DebugMsg( "GeomMgr: WARNING - light cookie texture has no VRAM id\n" );
            return NULL;
        }

        ID3D11ShaderResourceView* pSRV = vram_GetSRV( *pBitmap );
        if( !pSRV )
        {
            x_DebugMsg( "GeomMgr: ERROR - Failed to get light cookie SRV (vram id %d)\n", vramID );
        }

        return pSRV;
    }

    const char* GeomTextureSlotName( texture_slot Slot )
    {
        switch( Slot )
        {
            case TEXTURE_SLOT_DIFFUSE:          return "diffuse";
            case TEXTURE_SLOT_DETAIL:           return "detail";
            case TEXTURE_SLOT_ENVIRONMENT:      return "environment";
            case TEXTURE_SLOT_ENVIRONMENT_CUBE: return "environment cube";
            case TEXTURE_SLOT_DISTORTION_SCENE: return "distortion scene";
            case TEXTURE_SLOT_LIGHT_COOKIE:     return "light cookie";
            default:                            return "unknown";
        }
    }
}

//==============================================================================
//  INITIALIZATION / SHUTDOWN
//==============================================================================

void geom_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    x_DebugMsg( "GeomMgr: Initializing shaders\n" );

    // Initialize member variables
    m_pCurrentTexture       = NULL;
    m_pCurrentDetailTexture = NULL;
    m_pCurrentEnvironmentTexture = NULL;
    m_pCurrentEnvCubemap    = NULL;
    m_bDistortionStateActive = FALSE;
    m_DistortionNormalRot.Zero();
    m_LastLightCookieCount = 0;

    // Initialize shaders and resources
    InitRigidShaders();
    InitSkinShaders();
    InitProjTextures();
    InitShadowMaps();

    m_bInitialized = TRUE;
    x_DebugMsg( "GeomMgr: Shaders initialized successfully\n" );
}

//==============================================================================

void geom_mgr::Kill( void )
{
    if( !m_bInitialized )
        return;

    x_DebugMsg( "GeomMgr: Shutting down shaders\n" );

    KillRigidShaders();
    KillSkinShaders();
    ResetLightCookies();
    KillProjTextures();
    KillShadowMaps();
    ClearDistortionState();
    SetEnvironmentCubemap( NULL );
    InvalidateCache();

    m_bInitialized = FALSE;
    x_DebugMsg( "GeomMgr: Shaders shutdown complete\n" );
}

//==============================================================================

void geom_mgr::ResetLightCookies( void )
{
    if( !g_pd3dContext || !m_LastLightCookieCount )
        return;

    ID3D11ShaderResourceView* nullSRV[MAX_GEOM_LIGHTS] = { NULL };
    g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_LIGHT_COOKIE,
                                          m_LastLightCookieCount,
                                          nullSRV );
    m_LastLightCookieCount = 0;
}

//==============================================================================

xbool geom_mgr::CanAppendLightCookies( const cb_rigid_instance* pInstances,
                                       s32                      nInstances,
                                       const cb_geom_lighting*  pLighting ) const
{
    s32 Slots[MAX_GEOM_LIGHTS] = { 0 };
    s32 nSlots = 0;

    return AddInstanceCookieFaces( pInstances, nInstances, Slots, nSlots ) &&
           AddLightingCookieFaces( pLighting, Slots, nSlots );
}

//==============================================================================

xbool geom_mgr::CanAppendLightCookies( const cb_skin_instance* pInstances,
                                       s32                     nInstances,
                                       const cb_geom_lighting* pLighting ) const
{
    s32 Slots[MAX_GEOM_LIGHTS] = { 0 };
    s32 nSlots = 0;

    return AddInstanceCookieFaces( pInstances, nInstances, Slots, nSlots ) &&
           AddLightingCookieFaces( pLighting, Slots, nSlots );
}

//==============================================================================

template<class T_INSTANCE>
static void BindLightCookieInstances( T_INSTANCE* pInstances, s32 nInstances, u32& LastLightCookieCount )
{
    if( !g_pd3dContext || !pInstances || (nInstances <= 0) )
    {
        if( LastLightCookieCount )
        {
            ID3D11ShaderResourceView* nullSRV[MAX_GEOM_LIGHTS] = { NULL };
            g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_LIGHT_COOKIE,
                                                 LastLightCookieCount,
                                                 nullSRV );
            LastLightCookieCount = 0;
        }
        return;
    }

    s32 Slots[MAX_GEOM_LIGHTS] = { 0 };
    s32 nSlots = 0;
    ID3D11ShaderResourceView* cookieSRV[MAX_GEOM_LIGHTS] = { NULL };

    for( s32 iInstance = 0; iInstance < nInstances; iInstance++ )
    {
        T_INSTANCE& Instance = pInstances[iInstance];
        const s32 nLights = MIN( Instance.LightCount, MAX_GEOM_LIGHTS );

        for( s32 iLight = 0; iLight < nLights; iLight++ )
        {
            const s32 SourceSlot = (s32)Instance.LightCookieU[iLight].GetW();
            if( SourceSlot <= 0 )
                continue;

            s32 LocalSlot = FindLightCookieFaceSlot( Slots, nSlots, SourceSlot );
            if( LocalSlot < 0 )
            {
                if( nSlots >= MAX_GEOM_LIGHTS )
                {
                    Instance.LightCookieU[iLight].GetW() = 0.0f;
                    continue;
                }

                ID3D11ShaderResourceView* pSRV = GetLightCookieFaceSRV( SourceSlot );
                if( !pSRV )
                {
                    Instance.LightCookieU[iLight].GetW() = 0.0f;
                    continue;
                }

                LocalSlot = nSlots;
                Slots[nSlots] = SourceSlot;
                cookieSRV[nSlots] = pSRV;
                nSlots++;
            }

            Instance.LightCookieU[iLight].GetW() = (f32)(LocalSlot + 1);
        }
    }

    if( nSlots )
    {
        g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_LIGHT_COOKIE,
                                             nSlots,
                                             cookieSRV );
    }
    else if( LastLightCookieCount )
    {
        ID3D11ShaderResourceView* nullSRV[MAX_GEOM_LIGHTS] = { NULL };
        g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_LIGHT_COOKIE,
                                             LastLightCookieCount,
                                             nullSRV );
    }

    if( (LastLightCookieCount > (u32)nSlots) && nSlots )
    {
        const u32 nUnused = LastLightCookieCount - nSlots;
        ID3D11ShaderResourceView* nullSRV[MAX_GEOM_LIGHTS] = { NULL };
        g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_LIGHT_COOKIE + nSlots,
                                             nUnused,
                                             nullSRV );
    }

    LastLightCookieCount = nSlots;
}

//==============================================================================

void geom_mgr::BindLightCookies( cb_rigid_instance* pInstances, s32 nInstances )
{
    BindLightCookieInstances( pInstances, nInstances, m_LastLightCookieCount );
}

//==============================================================================

void geom_mgr::BindLightCookies( cb_skin_instance* pInstances, s32 nInstances )
{
    BindLightCookieInstances( pInstances, nInstances, m_LastLightCookieCount );
}

//==============================================================================

geom_mgr::material_constants geom_mgr::BuildMaterialFlags( const material* pMaterial,
                                                                  xbool           IncludeVertexColor ) const
{
    material_constants constants;
    constants.Flags    = 0;
    constants.AlphaRef = 0.0f;

    if( !pMaterial )
        return constants;

    const xbool bPunchThru = !!(pMaterial->m_Flags & geom::material::FLAG_IS_PUNCH_THRU);

    switch( pMaterial->m_Type )
    {
        case Material_Diff:
            break;
        case Material_Alpha:
            constants.Flags |= MATERIAL_FLAG_ALPHA_BLEND;
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST;
            break;
        case Material_Diff_PerPixelIllum:
            constants.Flags |= MATERIAL_FLAG_DIFF_PERPIXEL_ILLUM;
            break;
        case Material_Alpha_PerPixelIllum:
            constants.Flags |= MATERIAL_FLAG_ALPHA_BLEND;
            constants.Flags |= MATERIAL_FLAG_ALPHA_PERPIXEL_ILLUM;
            break;
        case Material_Alpha_PerPolyIllum:
            constants.Flags |= MATERIAL_FLAG_ALPHA_BLEND;
            constants.Flags |= MATERIAL_FLAG_ALPHA_PERPOLY_ILLUM;
            break;
        case Material_Diff_PerPixelEnv:
            constants.Flags |= MATERIAL_FLAG_DIFF_PERPIXEL_ENV;
            break;
        case Material_Alpha_PerPolyEnv:
            constants.Flags |= MATERIAL_FLAG_ALPHA_BLEND;
            constants.Flags |= MATERIAL_FLAG_ALPHA_PERPOLY_ENV;
            break;
        case Material_Distortion:
            constants.Flags |= MATERIAL_FLAG_ALPHA_BLEND;
            constants.Flags |= MATERIAL_FLAG_DISTORTION;
            break;
        case Material_Distortion_PerPolyEnv:
            constants.Flags |= MATERIAL_FLAG_ALPHA_BLEND;
            constants.Flags |= MATERIAL_FLAG_DISTORTION_PERPOLY_ENV;
            break;
    }

    if( pMaterial->m_Flags & geom::material::FLAG_HAS_DETAIL_MAP )
        constants.Flags |= MATERIAL_FLAG_DETAIL;

    //if( pMaterial->m_Flags & geom::material::FLAG_HAS_ENV_MAP )
        constants.Flags |= MATERIAL_FLAG_ENVIRONMENT;

    if( pMaterial->m_Flags & geom::material::FLAG_ENV_CUBE_MAP )
        constants.Flags |= MATERIAL_FLAG_ENV_CUBEMAP;

    if( pMaterial->m_Flags & geom::material::FLAG_ENV_VIEW_SPACE )
        constants.Flags |= MATERIAL_FLAG_ENV_VIEWSPACE;

    if( pMaterial->m_Flags & geom::material::FLAG_ENV_WORLD_SPACE )
        constants.Flags |= MATERIAL_FLAG_ENV_WORLDSPACE;

    if( pMaterial->m_Flags & geom::material::FLAG_IS_PUNCH_THRU )
        constants.Flags |= MATERIAL_FLAG_ALPHA_TEST;

    if( pMaterial->m_Flags & geom::material::FLAG_DOUBLE_SIDED )
        constants.Flags |= MATERIAL_FLAG_TWO_SIDED;

    if( pMaterial->m_Flags & geom::material::FLAG_IS_ADDITIVE )
        constants.Flags |= MATERIAL_FLAG_ADDITIVE;
    else if( pMaterial->m_Flags & geom::material::FLAG_IS_SUBTRACTIVE )
        constants.Flags |= MATERIAL_FLAG_SUBTRACTIVE;

    if( pMaterial->m_Flags & geom::material::FLAG_ILLUM_USES_DIFFUSE )
        constants.Flags |= MATERIAL_FLAG_ILLUM_USE_DIFFUSE;

    if( constants.Flags & MATERIAL_FLAG_ALPHA_TEST )
        constants.AlphaRef = bPunchThru ? 0.5f : (4.0f / 255.0f);

    if( IncludeVertexColor )
        constants.Flags |= MATERIAL_FLAG_VERTEX_COLOR;

    return constants;
}

//==============================================================================

u32 geom_mgr::BuildInstanceFlags( u32 RenderFlags )
{
    u32 ShaderFlags = 0;

    if( RenderFlags & render::GLOWING )
        ShaderFlags |= INSTANCE_FLAG_GLOWING;

    if( RenderFlags & render::INSTFLAG_SPOTLIGHT )
        ShaderFlags |= INSTANCE_FLAG_PROJ_LIGHT;

    if( RenderFlags & render::INSTFLAG_PROJ_SHADOW )
        ShaderFlags |= INSTANCE_FLAG_PROJ_SHADOW;

    if( RenderFlags & render::INSTFLAG_FADING_ALPHA )
        ShaderFlags |= INSTANCE_FLAG_FADING_ALPHA;

    if( RenderFlags & render::INSTFLAG_DYNAMICLIGHT )
        ShaderFlags |= INSTANCE_FLAG_DYNAMIC_LIGHT;

    if( RenderFlags & render::INSTFLAG_FILTERLIGHT )
        ShaderFlags |= INSTANCE_FLAG_FILTERLIGHT;

    return ShaderFlags;
}

//==============================================================================

f32 geom_mgr::BuildInstanceFadeAlpha( u32 RenderFlags, u8 Alpha )
{
    if( ( RenderFlags & ( render::FADING_ALPHA | render::INSTFLAG_FADING_ALPHA ) ) || ( Alpha != 255 ) )
        return (f32)Alpha / 255.0f;

    return 1.0f;
}

//==============================================================================

cb_geom_frame geom_mgr::BuildFrameConstants( const view&     View,
                                             const material* pMaterial,
                                             u8              UOffset,
                                             u8              VOffset,
                                             xbool           IncludeVertexColor,
                                             u8              OverrideMat ) const
{
    const xbool bOverrideMaterial       = (OverrideMat != FALSE);
    const f32   kInvByte                = 1.0f / 255.0f;
    const f32   kDefaultDetailScale     = 1.0f;
    const f32   kDistortionPixelScale   = 8.0f;

    cb_geom_frame frameData;
    x_memset( &frameData, 0, sizeof(cb_geom_frame) );

    f32 nearZ = 0.0f;
    f32 farZ  = 0.0f;
    View.GetZLimits( nearZ, farZ );

    const matrix4 viewMatrix( View.GetW2V() );

    frameData.View       = viewMatrix;
    frameData.Projection = View.GetV2C();
    frameData.NearZ      = nearZ;
    frameData.FarZ       = farZ;

    const vector3& camPos = View.GetPosition();
    frameData.CameraPosition.Set( camPos.GetX(),
                                  camPos.GetY(),
                                  camPos.GetZ(),
                                  1.0f );

    f32 detailScale = pMaterial ? pMaterial->m_DetailScale : kDefaultDetailScale;
    if( detailScale <= 0.0f )
        detailScale = kDefaultDetailScale;

    frameData.UVAnim.Set( (f32)UOffset * kInvByte,
                          (f32)VOffset * kInvByte,
                          detailScale,
                          0.0f );

    material_constants constants;
    if( bOverrideMaterial )
    {
        constants.Flags    = 0;
        constants.AlphaRef = 0.0f;
    }
    else
    {
        constants = BuildMaterialFlags( pMaterial, IncludeVertexColor );
    }

    frameData.MaterialFlags = constants.Flags;
    frameData.AlphaRef      = constants.AlphaRef;

    const f32   fixedAlpha    = (bOverrideMaterial || !pMaterial) ? 0.0f : pMaterial->m_FixedAlpha;
    const f32   cubeIntensity = bOverrideMaterial ? 0.0f : ComputeCubeMapIntensity( pMaterial );
    frameData.EnvParams.Set( fixedAlpha,
                             cubeIntensity,
                             1.0f,
                             bOverrideMaterial ? 1.0f : 0.0f );

    matrix4 distortionNormalMatrix( viewMatrix );
    if( m_bDistortionStateActive )
    {
        matrix4 distortionRot( m_DistortionNormalRot );
        distortionNormalMatrix = viewMatrix * distortionRot;
    }

    f32 invSceneWidth  = 1.0f;
    f32 invSceneHeight = 1.0f;
    const rtarget* pSceneTarget = rtarget_GetCurrentTarget( 0 );
    if( pSceneTarget &&
        (pSceneTarget->Desc.Width > 0) &&
        (pSceneTarget->Desc.Height > 0) )
    {
        invSceneWidth  = 1.0f / (f32)pSceneTarget->Desc.Width;
        invSceneHeight = 1.0f / (f32)pSceneTarget->Desc.Height;
    }

    frameData.DistortionNormalMatrix = distortionNormalMatrix;
    frameData.DistortionParams.Set( kDistortionPixelScale,
                                    0.0f,
                                    invSceneWidth,
                                    invSceneHeight );

    return frameData;
}

//==============================================================================

xbool geom_mgr::UploadConstantBuffer( ID3D11Buffer* pBuffer,
                                      const void*   pData,
                                      u32           Size,
                                      const char*   pBufferName ) const
{
    if( !g_pd3dContext )
    {
        x_DebugMsg( "GeomMgr: Cannot update %s buffer without a D3D context\n", pBufferName );
        return FALSE;
    }

    if( !pBuffer )
    {
        x_DebugMsg( "GeomMgr: Cannot update %s buffer because it is NULL\n", pBufferName );
        return FALSE;
    }

    if( !pData )
    {
        x_DebugMsg( "GeomMgr: Cannot update %s buffer without source data\n", pBufferName );
        return FALSE;
    }

    D3D11_BUFFER_DESC desc;
    pBuffer->GetDesc( &desc );

    if( Size > desc.ByteWidth )
    {
        x_DebugMsg( "GeomMgr: %s buffer update size (%u) exceeds buffer size (%u)\n",
                    pBufferName,
                    Size,
                    desc.ByteWidth );
        return FALSE;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = g_pd3dContext->Map( pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
    if( FAILED(hr) )
    {
        x_DebugMsg( "GeomMgr: Failed to map %s buffer, HRESULT 0x%08X\n", pBufferName, hr );
        return FALSE;
    }

    x_memcpy( mappedResource.pData, pData, Size );
    g_pd3dContext->Unmap( pBuffer, 0 );
    return TRUE;
}

//==============================================================================
//  GENERAL STATE HELPERS
//==============================================================================

void geom_mgr::ApplyRenderStates( const material* pMaterial,
                                  u32             RenderFlags,
                                  u8              OverrideMat )
{
    state_blend_mode   blendMode   = STATE_BLEND_NONE;
    state_depth_mode   depthMode   = STATE_DEPTH_NORMAL;
    state_raster_mode  rasterMode  = STATE_RASTER_SOLID;
    state_sampler_mode samplerMode = STATE_SAMPLER_ANISOTROPIC_WRAP;

    if( OverrideMat )
    {
        state_SetBlend( STATE_BLEND_COLOR_WRITE_DISABLE );
        state_SetDepth( STATE_DEPTH_NORMAL );
        state_SetRasterizer( STATE_RASTER_SOLID_NO_CULL );
        state_SetSampler( samplerMode, 0, STATE_SAMPLER_STAGE_PS );
        return;
    }

    material_type materialType = Material_Diff;

    xbool bAlphaMaterial      = FALSE;
    xbool bForceZFill         = FALSE;
    xbool bDoubleSided        = FALSE;
    xbool bAdditive           = FALSE;
    xbool bSubtractive        = FALSE;
    xbool bDistortionMaterial = FALSE;

    if( pMaterial )
    {
        materialType         = (material_type)pMaterial->m_Type;
        bAlphaMaterial       = IsAlphaMaterial( materialType );
        bDistortionMaterial = (materialType == Material_Distortion) ||
                              (materialType == Material_Distortion_PerPolyEnv);
        bForceZFill         = !!(pMaterial->m_Flags & geom::material::FLAG_FORCE_ZFILL);
        bDoubleSided        = !!(pMaterial->m_Flags & geom::material::FLAG_DOUBLE_SIDED);
        bAdditive           = !!(pMaterial->m_Flags & geom::material::FLAG_IS_ADDITIVE);
        bSubtractive        = !!(pMaterial->m_Flags & geom::material::FLAG_IS_SUBTRACTIVE);
    }

    const u32   FadeMask     = (render::FADING_ALPHA | render::INSTFLAG_FADING_ALPHA);
    const xbool bFadingAlpha = !!(RenderFlags & FadeMask);

    xbool bEnableBlend = FALSE;

    if( bFadingAlpha )
    {
        blendMode    = STATE_BLEND_ALPHA;
        bEnableBlend = TRUE;
    }
    else
    {
        switch( materialType )
        {
            case Material_Alpha:
            case Material_Alpha_PerPolyEnv:
            case Material_Alpha_PerPixelIllum:
            case Material_Alpha_PerPolyIllum:
            {
                if( bAdditive )
                {
                    blendMode = STATE_BLEND_ADD;
                }
                else if( bSubtractive )
                {
                    blendMode = STATE_BLEND_SUB;
                }
                else
                {
                    blendMode = STATE_BLEND_ALPHA;
                }

                bEnableBlend = TRUE;
            }
            break;

            case Material_Distortion:
            case Material_Distortion_PerPolyEnv:
            {
                // Distortion classes keep blending disabled; they rely on shader
                // authored offsets rather than color compositing.
                samplerMode = STATE_SAMPLER_LINEAR_CLAMP;
            }
            break;

            default:
                break;
        }
    }

    const xbool bDisableDepthWrite = ((bAlphaMaterial || bEnableBlend) &&
                                      !bForceZFill &&
                                      !bDistortionMaterial);

    depthMode = bDisableDepthWrite ? STATE_DEPTH_NO_WRITE : STATE_DEPTH_NORMAL;

    //const xbool bWireframe       = !!(RenderFlags & render::WIREFRAME);
    //const xbool bWireframeNoCull = !!(RenderFlags & render::WIREFRAME2);
    const xbool bDisableCull     = bDoubleSided || (bAlphaMaterial && !bDistortionMaterial);

    //if( bWireframeNoCull )
    //{
    //    rasterMode = STATE_RASTER_WIRE_NO_CULL;
    //}
    //else if( bWireframe )
    //{
    //    rasterMode = bDisableCull ? STATE_RASTER_WIRE_NO_CULL : STATE_RASTER_WIRE;
    //}
    //else
    {
        rasterMode = bDisableCull ? STATE_RASTER_SOLID_NO_CULL
                                  : STATE_RASTER_SOLID;
    }

    //if( RenderFlags & (render::CLIPPED | render::INSTFLAG_CLIPPED) )
    //{
    //    samplerMode = STATE_SAMPLER_LINEAR_CLAMP;
    //}

    state_SetBlend( blendMode );
    state_SetDepth( depthMode );
    state_SetRasterizer( rasterMode );
    state_SetSampler( samplerMode, 0, STATE_SAMPLER_STAGE_PS );
}

//==============================================================================

void geom_mgr::SetBitmap( const xbitmap* pBitmap, texture_slot slot )
{
    if( !g_pd3dContext )
        return;

    const char* pSlotName = GeomTextureSlotName( slot );
    const xbool bEnvironmentSlot = (slot == TEXTURE_SLOT_ENVIRONMENT);
    const xbitmap* pCurrentBitmap = NULL;

    switch( slot )
    {
        case TEXTURE_SLOT_DIFFUSE:
            pCurrentBitmap = m_pCurrentTexture;
            break;
        case TEXTURE_SLOT_DETAIL:
            pCurrentBitmap = m_pCurrentDetailTexture;
            break;
        case TEXTURE_SLOT_ENVIRONMENT:
            pCurrentBitmap = m_pCurrentEnvironmentTexture;
            break;
        default:
            x_DebugMsg( "GeomMgr: WARNING - SetBitmap does not manage slot %d (%s)\n",
                        slot,
                        pSlotName );
            return;
    }

    if( bEnvironmentSlot )
    {
        // Keep the cube slot in sync with the 2D environment slot so stale
        // cubemap bindings do not survive cache invalidation boundaries.
        ID3D11ShaderResourceView* pNullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_ENVIRONMENT_CUBE, 1, &pNullSRV );
        m_pCurrentEnvCubemap = NULL;

        if( pBitmap == pCurrentBitmap )
            return;
    }
    else if( pBitmap == pCurrentBitmap )
    {
        return;
    }

    ID3D11ShaderResourceView* pSRV = NULL;

    if( pBitmap )
    {
        const s32 vramID = pBitmap->GetVRAMID();
        if( vramID <= 0 )
        {
            x_DebugMsg( "GeomMgr: WARNING - %s texture has no VRAM id\n", pSlotName );
        }
        else
        {
            pSRV = vram_GetSRV( *pBitmap );
            if( !pSRV )
            {
                x_DebugMsg( "GeomMgr: ERROR - Failed to get %s texture SRV (vram id %d)\n",
                            pSlotName,
                            vramID );
            }
        }
    }

    g_pd3dContext->PSSetShaderResources( slot, 1, &pSRV );

    if( slot == TEXTURE_SLOT_DIFFUSE )
    {
        m_pCurrentTexture = pSRV ? pBitmap : NULL;
    }
    else if( slot == TEXTURE_SLOT_DETAIL )
    {
        m_pCurrentDetailTexture = pSRV ? pBitmap : NULL;
    }
    else
    {
        m_pCurrentEnvironmentTexture = pSRV ? pBitmap : NULL;
    }
}

//==============================================================================

void geom_mgr::SetEnvironmentCubemap( const cubemap* pCubemap )
{
    if( !g_pd3dContext )
        return;

    if( m_pCurrentEnvCubemap == pCubemap )
        return;

    ID3D11ShaderResourceView* pSRV = NULL;

    if( pCubemap )
    {
        if( !pCubemap->m_hTexture )
        {
            x_DebugMsg( "GeomMgr: WARNING - Cubemap has no VRAM handle\n" );
        }
        else
        {
            const uaddr rawHandle = (uaddr)pCubemap->m_hTexture;
            if( rawHandle > (uaddr)S32_MAX )
            {
                x_DebugMsg( "GeomMgr: ERROR - Cubemap handle %p exceeds VRAM id range\n",
                            pCubemap->m_hTexture );
            }
            else
            {
                const s32 vramID = (s32)rawHandle;
                if( vramID <= 0 )
                {
                    x_DebugMsg( "GeomMgr: WARNING - Cubemap has invalid VRAM id %d\n", vramID );
                }
                else
                {
                    pSRV = vram_GetSRV( vramID );
                    if( !pSRV )
                    {
                        x_DebugMsg( "GeomMgr: ERROR - Failed to get cubemap SRV (id %d)\n", vramID );
                    }
                }
            }
        }
    }

    g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_ENVIRONMENT_CUBE, 1, &pSRV );

    if( pSRV )
    {
        ID3D11ShaderResourceView* pNullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_ENVIRONMENT, 1, &pNullSRV );

        m_pCurrentEnvCubemap      = pCubemap;
        m_pCurrentEnvironmentTexture = NULL;
    }
    else
    {
        m_pCurrentEnvCubemap = NULL;
    }
}

//==============================================================================

void geom_mgr::InvalidateCache( void )
{
    m_pCurrentTexture = NULL;
    m_pCurrentDetailTexture = NULL;
    m_pCurrentEnvironmentTexture = NULL;
    m_pCurrentEnvCubemap = NULL;
    m_bRigidFrameDirty  = TRUE;
    m_bSkinFrameDirty   = TRUE;
}

//==============================================================================

void geom_mgr::SetDistortionState( const radian3& NormalRot )
{
    m_bDistortionStateActive = TRUE;
    m_DistortionNormalRot    = NormalRot;
    m_bRigidFrameDirty       = TRUE;
    m_bSkinFrameDirty        = TRUE;
}

//==============================================================================

void geom_mgr::ClearDistortionState( void )
{
    m_bDistortionStateActive = FALSE;
    m_DistortionNormalRot.Zero();
    m_bRigidFrameDirty       = TRUE;
    m_bSkinFrameDirty        = TRUE;
}

//==============================================================================

// Skin bone buffer access for SoftVertexMgr
ID3D11Buffer* geom_mgr::GetSkinBoneBuffer( void )
{
    return m_pSkinBoneBuffer;
}
