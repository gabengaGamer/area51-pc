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
    constants.AlphaRef = 0.5f;

    if( !pMaterial )
        return constants;

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

    if( pMaterial->m_EnvironmentMap.GetPointer() )
        constants.Flags |= MATERIAL_FLAG_ENVIRONMENT;

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

    ambient = ambient + AmbientBias;
    lighting.LightAmbCol = ambient;

    return lighting;
}

//==============================================================================
//  GENERAL STATE HELPERS
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
    }
}

//==============================================================================

void material_mgr::InvalidateCache( void )
{
    m_pCurrentTexture = NULL;
    m_pCurrentDetailTexture = NULL;
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