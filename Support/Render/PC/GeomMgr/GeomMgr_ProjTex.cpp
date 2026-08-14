//==============================================================================
//
//  GeomMgr_ProjTex.cpp
//
//  Projected texture utilities for the PC geom manager
//
//==============================================================================

//==============================================================================
//  BASE INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GeomMgr.hpp"

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool GeomMgr::InitProjTextures( void )
{
    x_DebugMsg( "GeomMgr: Initializing projection texture resources\n" );

    if ( !g_ProjectionAtlas.Init() )
    {
        return FALSE;
    }

    x_DebugMsg( "GeomMgr: Projection texture resources initialized\n" );
    return TRUE;
}

//==============================================================================

void GeomMgr::KillProjTextures( void )
{
    g_ProjectionAtlas.Kill();
    x_DebugMsg( "GeomMgr: Projection texture resources released\n" );
}

//==============================================================================

xbool GeomMgr::ResetProjTextures( void )
{
    ShaderBindingLayout const* pBindings = GetShaderBindings( m_activeShaderKind );
    if ( !pBindings )
    {
        return FALSE;
    }

    ProjectionTextureConstants constants;
    x_memset( &constants, 0, sizeof( constants ) );
    return shader_PushUniformData( SHADER_STAGE_PIXEL, pBindings->ProjTexturesPixel, &constants, sizeof( constants ) );
}

//==============================================================================

xbool GeomMgr::UpdateProjTextures( void )
{
    ShaderBindingLayout const* pBindings = GetShaderBindings( m_activeShaderKind );
    if ( !pBindings )
    {
        return FALSE;
    }

    ProjectionTextureConstants constants;
    x_memset( &constants, 0, sizeof( constants ) );

    s32 const nProjLights = MIN( g_ProjTextureMgr.GetProjLightCount(), ProjTextureMgr::MaxLightProjectionCount );
    for ( s32 i = 0; i < nProjLights; ++i )
    {
        matrix4        projMatrix;
        texture const* pTexture = NULL;
        g_ProjTextureMgr.GetProjLight( i, projMatrix, pTexture );
        if ( !pTexture )
        {
            return FALSE;
        }

        ProjectionAtlasRegion region;
        if ( !g_ProjectionAtlas.GetRegion( g_ProjTextureMgr.GetProjLightTexture( i ), ProjectionAtlasEncoding::BlueMask,
                                           region ) )
        {
            return FALSE;
        }

        s32 const dest = constants.ProjLightCount++;
        constants.ProjLightMatrix[dest] = projMatrix;
        constants.ProjLightAtlas[dest] = region.m_uvScaleBias;
        constants.ProjLightInfo[dest].Set( static_cast<f32>( region.m_layer ), region.m_maxMip, 0.0f, 0.0f );
    }

    s32 const nProjShadows = MIN( g_ProjTextureMgr.GetProjShadowCount(), ProjTextureMgr::MaxShadowProjectionCount );
    for ( s32 i = 0; i < nProjShadows; ++i )
    {
        matrix4        projMatrix;
        texture const* pTexture = NULL;
        g_ProjTextureMgr.GetProjShadow( i, projMatrix, pTexture );
        if ( !pTexture )
        {
            return FALSE;
        }

        ProjectionAtlasRegion region;
        if ( !g_ProjectionAtlas.GetRegion( g_ProjTextureMgr.GetProjShadowTexture( i ),
                                           ProjectionAtlasEncoding::BlueMask, region ) )
        {
            return FALSE;
        }

        s32 const dest = constants.ProjShadowCount++;
        constants.ProjShadowMatrix[dest] = projMatrix;
        constants.ProjShadowAtlas[dest] = region.m_uvScaleBias;
        constants.ProjShadowInfo[dest].Set( static_cast<f32>( region.m_layer ), region.m_maxMip, 0.0f, 0.0f );
    }

    return shader_PushUniformData( SHADER_STAGE_PIXEL, pBindings->ProjTexturesPixel, &constants, sizeof( constants ) );
}
