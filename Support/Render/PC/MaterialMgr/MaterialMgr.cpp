//==============================================================================
//
//  MaterialMgr.cpp
//
//  Material Manager for PC platform
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

#include "MaterialMgr.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dContext;

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

material_mgr g_MaterialMgr;

//==============================================================================
//  INITIALIZATION / SHUTDOWN
//==============================================================================

void material_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    x_DebugMsg( "MaterialMgr: Initializing material manager\n" );

    // Initialize member variables
    m_pCurrentTexture       = NULL;
    m_pCurrentDetailTexture = NULL;
    m_pCurrentEnvironmentTexture = NULL;
    m_pCurrentEnvCubemap    = NULL;

    // Initialize shaders and resources
    InitRigidShaders();
    InitSkinShaders();
    InitProjTextures();

    m_bInitialized = TRUE;
    x_DebugMsg( "MaterialMgr: Material manager initialized successfully\n" );
}

//==============================================================================

void material_mgr::Kill( void )
{
    if( !m_bInitialized )
        return;

    x_DebugMsg( "MaterialMgr: Shutting down material manager\n" );

    KillRigidShaders();
    KillSkinShaders();
    KillProjTextures();
    InvalidateCache();
    SetEnvironmentCubemap( NULL );

    m_bInitialized = FALSE;
    x_DebugMsg( "MaterialMgr: Material manager shutdown complete\n" );
}

//==============================================================================

material_mgr::material_constants material_mgr::BuildMaterialFlags( const material* pMaterial,
                                                                  u32             RenderFlags,
                                                                  xbool           SupportsDetailMap,
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
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST;
            break;
        case Material_Diff_PerPixelIllum:
            constants.Flags |= MATERIAL_FLAG_PERPIXEL_ILLUM;
            break;
        case Material_Alpha_PerPixelIllum:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPIXEL_ILLUM;
            break;
        case Material_Alpha_PerPolyIllum:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ILLUM;
            break;
        case Material_Diff_PerPixelEnv:
            constants.Flags |= MATERIAL_FLAG_PERPIXEL_ENV;
            break;
        case Material_Alpha_PerPolyEnv:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ENV;
            break;
        case Material_Distortion:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_DISTORTION;
            break;
        case Material_Distortion_PerPolyEnv:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_DISTORTION | MATERIAL_FLAG_PERPOLY_ENV;
            break;
    }

    if( SupportsDetailMap && pMaterial->m_DetailMap.GetPointer() )
        constants.Flags |= MATERIAL_FLAG_DETAIL;

    if( pMaterial->m_EnvironmentMap.GetPointer() ||
        (pMaterial->m_Flags & geom::material::FLAG_ENV_CUBE_MAP) )
        constants.Flags |= MATERIAL_FLAG_ENVIRONMENT;

    if( pMaterial->m_Flags & geom::material::FLAG_ENV_CUBE_MAP )
        constants.Flags |= MATERIAL_FLAG_ENV_CUBEMAP;

    if( pMaterial->m_Flags & geom::material::FLAG_ENV_VIEW_SPACE )
        constants.Flags |= MATERIAL_FLAG_ENV_VIEWSPACE;

    if( pMaterial->m_Flags & geom::material::FLAG_IS_PUNCH_THRU )
        constants.Flags |= MATERIAL_FLAG_ALPHA_TEST;

    if( pMaterial->m_Flags & geom::material::FLAG_DOUBLE_SIDED )
        constants.Flags |= MATERIAL_FLAG_TWO_SIDED;

    if( pMaterial->m_Flags & geom::material::FLAG_IS_ADDITIVE )
        constants.Flags |= MATERIAL_FLAG_ADDITIVE;
    else if( pMaterial->m_Flags & geom::material::FLAG_IS_SUBTRACTIVE )
        constants.Flags |= MATERIAL_FLAG_SUBTRACTIVE;

    if( IncludeVertexColor )
        constants.Flags |= MATERIAL_FLAG_VERTEX_COLOR;

    if( !(RenderFlags & render::DISABLE_SPOTLIGHT) )
        constants.Flags |= MATERIAL_FLAG_PROJ_LIGHT;

    if( !(RenderFlags & render::DISABLE_PROJ_SHADOWS) )
        constants.Flags |= MATERIAL_FLAG_PROJ_SHADOW;

    if( constants.Flags & MATERIAL_FLAG_ALPHA_TEST )
    {
        // Blended alpha surfaces should only drop fully transparent texels,
        // while explicit punch-through materials keep a tighter cutoff so the
        // binary mask stays crisp.
        constants.AlphaRef = bPunchThru ? 0.5f : (4.0f / 255.0f);
    }

    return constants;
}

//==============================================================================

cb_lighting material_mgr::BuildLightingConstants( const d3d_lighting* pLighting,
                                                  const vector4&      AmbientBias ) const
{
    cb_lighting lighting;

    for( s32 i = 0; i < MAX_GEOM_LIGHTS; i++ )
    {
        lighting.LightVec[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
        lighting.LightCol[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
    }

    lighting.LightCount = 0;
    lighting.Padding[0] = 0.0f;
    lighting.Padding[1] = 0.0f;
    lighting.Padding[2] = 0.0f;

    vector4 ambient( 0.0f, 0.0f, 0.0f, 1.0f );

    if( pLighting )
    {
        for( s32 i = 0; i < MAX_GEOM_LIGHTS; i++ )
        {
            if( i < pLighting->LightCount )
            {
                lighting.LightVec[i] = pLighting->LightVec[i];
                lighting.LightCol[i] = pLighting->LightCol[i];
            }
        }

        ambient      = pLighting->AmbCol;
        lighting.LightCount = pLighting->LightCount;
    }

    f32 ambientX = ambient.GetX();
    f32 ambientY = ambient.GetY();
    f32 ambientZ = ambient.GetZ();
    f32 ambientW = ambient.GetW();

    const f32 biasX = AmbientBias.GetX();
    const f32 biasY = AmbientBias.GetY();
    const f32 biasZ = AmbientBias.GetZ();
    const f32 biasW = AmbientBias.GetW();

    ambientX = (biasX > 0.0f) ? x_max( ambientX, biasX ) : (ambientX + biasX);
    ambientY = (biasY > 0.0f) ? x_max( ambientY, biasY ) : (ambientY + biasY);
    ambientZ = (biasZ > 0.0f) ? x_max( ambientZ, biasZ ) : (ambientZ + biasZ);
    ambientW = (biasW > 0.0f) ? x_max( ambientW, biasW ) : (ambientW + biasW);

    ambient.Set( ambientX, ambientY, ambientZ, ambientW );
    lighting.LightAmbCol = ambient;

    return lighting;
}

//==============================================================================
//  GENERAL STATE HELPERS
//==============================================================================

void material_mgr::ApplyRenderStates( const material* pMaterial, u32 RenderFlags )
{
    s32 blendMode   = STATE_BLEND_NONE;
    s32 depthMode   = STATE_DEPTH_NORMAL;
    s32 rasterMode  = STATE_RASTER_SOLID;
    s32 samplerMode = STATE_SAMPLER_LINEAR_WRAP;

    material_type materialType = Material_Diff;

    xbool bAlphaMaterial = FALSE;
    xbool bForceZFill    = FALSE;
    xbool bDoubleSided   = FALSE;
    xbool bAdditive      = FALSE;
    xbool bSubtractive   = FALSE;

    if( pMaterial )
    {
        materialType   = (material_type)pMaterial->m_Type;
        bAlphaMaterial = IsAlphaMaterial( materialType );
        bForceZFill    = !!(pMaterial->m_Flags & geom::material::FLAG_FORCE_ZFILL);
        bDoubleSided   = !!(pMaterial->m_Flags & geom::material::FLAG_DOUBLE_SIDED);
        bAdditive      = !!(pMaterial->m_Flags & geom::material::FLAG_IS_ADDITIVE);
        bSubtractive   = !!(pMaterial->m_Flags & geom::material::FLAG_IS_SUBTRACTIVE);
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
                blendMode    = STATE_BLEND_NONE;
                bEnableBlend = FALSE;
            }
            break;

            default:
                break;
        }
    }

    const xbool bDisableDepthWrite = ((bAlphaMaterial || bEnableBlend) && !bForceZFill);

    depthMode = bDisableDepthWrite ? STATE_DEPTH_NO_WRITE : STATE_DEPTH_NORMAL;

    //const xbool bWireframe       = !!(RenderFlags & render::WIREFRAME);
    //const xbool bWireframeNoCull = !!(RenderFlags & render::WIREFRAME2);
    const xbool bDisableCull     = bDoubleSided || bAlphaMaterial;

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
        rasterMode = bDisableCull ? STATE_RASTER_SOLID_NO_CULL : STATE_RASTER_SOLID;
    }

    //if( RenderFlags & (render::CLIPPED | render::INSTFLAG_CLIPPED) )
    //{
    //    samplerMode = STATE_SAMPLER_LINEAR_CLAMP;
    //}

    state_SetState( STATE_TYPE_BLEND,      blendMode );
    state_SetState( STATE_TYPE_DEPTH,      depthMode );
    state_SetState( STATE_TYPE_RASTERIZER, rasterMode );
    state_SetState( STATE_TYPE_SAMPLER,    samplerMode );
}

//==============================================================================

void material_mgr::SetBitmap( const xbitmap* pBitmap, texture_slot slot )
{
    if( !g_pd3dContext )
        return;

    // Cache check to avoid redundant texture changes
    if( slot == TEXTURE_SLOT_DIFFUSE && pBitmap == m_pCurrentTexture )
        return;
    if( slot == TEXTURE_SLOT_DETAIL && pBitmap == m_pCurrentDetailTexture )
        return;
    if( slot == TEXTURE_SLOT_ENVIRONMENT &&
        (pBitmap == m_pCurrentEnvironmentTexture) &&
        (m_pCurrentEnvCubemap == NULL) )
    {
        return;
    }

    if( slot == TEXTURE_SLOT_ENVIRONMENT )
    {
        // Ensure cubemap binding is cleared when switching to 2D textures.
        SetEnvironmentCubemap( NULL );
    }

    if( pBitmap )
    {
        ID3D11ShaderResourceView* pSRV = vram_GetSRV( *pBitmap );

        if( pSRV )
        {
            g_pd3dContext->PSSetShaderResources( slot, 1, &pSRV );

            // Update cache
            if( slot == TEXTURE_SLOT_DIFFUSE )
                m_pCurrentTexture = pBitmap;
            else if( slot == TEXTURE_SLOT_DETAIL )
                m_pCurrentDetailTexture = pBitmap;
            else if( slot == TEXTURE_SLOT_ENVIRONMENT )
                m_pCurrentEnvironmentTexture = pBitmap;
        }
        else
        {
            x_DebugMsg( "MaterialMgr: ERROR - vram_GetSRV returned NULL for slot %d\n", slot );
        }
    }
    else
    {
        // Clear texture slot
        ID3D11ShaderResourceView* pNullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( slot, 1, &pNullSRV );

        // Update cache
        if( slot == TEXTURE_SLOT_DIFFUSE )
            m_pCurrentTexture = NULL;
        else if( slot == TEXTURE_SLOT_DETAIL )
            m_pCurrentDetailTexture = NULL;
        else if( slot == TEXTURE_SLOT_ENVIRONMENT )
            m_pCurrentEnvironmentTexture = NULL;
    }
}

//==============================================================================

void material_mgr::SetEnvironmentCubemap( const cubemap* pCubemap )
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
            x_DebugMsg( "MaterialMgr: WARNING - Cubemap has no VRAM handle\n" );
        }
        else
        {
            s32 vramID = (s32)(uaddr)pCubemap->m_hTexture;
            pSRV = vram_GetSRV( vramID );
            if( !pSRV )
            {
                x_DebugMsg( "MaterialMgr: ERROR - Failed to get cubemap SRV (id %d)\n", vramID );
            }
        }
    }

    g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_ENVIRONMENT_CUBE, 1, &pSRV );

    if( pCubemap && pSRV )
    {
        m_pCurrentEnvCubemap = pCubemap;
    }
    else
    {
        m_pCurrentEnvCubemap = NULL;
    }

    m_pCurrentEnvironmentTexture = NULL;
}

//==============================================================================

void material_mgr::InvalidateCache( void )
{
    m_pCurrentTexture = NULL;
    m_pCurrentDetailTexture = NULL;
    m_pCurrentEnvironmentTexture = NULL;
    m_pCurrentEnvCubemap = NULL;
    m_bRigidMatricesDirty = TRUE;
    m_bRigidLightingDirty = TRUE;
    m_bSkinMatricesDirty = TRUE;
    m_bSkinLightingDirty = TRUE;
}

//==============================================================================

// Skin bone buffer access for SoftVertexMgr
ID3D11Buffer* material_mgr::GetSkinBoneBuffer( void )
{
    return m_pSkinBoneBuffer;
}